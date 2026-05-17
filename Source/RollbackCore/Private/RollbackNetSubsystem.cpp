// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "RollbackNetSubsystem.h"
#include "RollbackStateComponent.h"

#include "AddressInfoTypes.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace RollbackNet
{
    static constexpr uint32 PacketMagic = 0x52424350; // RBCP
    static constexpr uint8 PacketVersion = 1;

    enum class EWirePacketType : uint8
    {
        Hello = 1,
        Input = 2,
        Ack = 3,
        Heartbeat = 4,
    };
}

void URollbackNetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &URollbackNetSubsystem::TickFunction));
    PerfLastBandwidthTime = FPlatformTime::Seconds();
}

void URollbackNetSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    StopTransport();
    Super::Deinitialize();
}

bool URollbackNetSubsystem::StartUdpPeer(const FRollbackTransportConfig& InConfig, FString& OutError)
{
    StopTransport();

    Config = InConfig;
    Stats = FRollbackTransportStats();
    PerfStats = FRollbackPerformanceStats();
    Peers.Reset();
    RemoteInputBuffer.Reset();
    LocalInputHistory.Reset();
    DelayedIncomingPackets.Reset();
    DelayedOutgoingPackets.Reset();
    PerfSimulationTimes.Reset();
    PerfRollbackTimes.Reset();
    PerfSerializeTimes.Reset();
    PerfStateSizes.Reset();
    PerfBytesSentLastWindow = 0;
    PerfBytesReceivedLastWindow = 0;
    PerfLastBandwidthTime = FPlatformTime::Seconds();
    PerfRollbackFrameTimestamps.Reset();

    const FName RequestedSubsystem = Config.SocketSubsystemName.IsNone() ? PLATFORM_SOCKETSUBSYSTEM : Config.SocketSubsystemName;
    SocketSubsystem = ISocketSubsystem::Get(RequestedSubsystem);
    if (!SocketSubsystem)
    {
        OutError = FString::Printf(TEXT("Socket subsystem '%s' is not available."), *RequestedSubsystem.ToString());
        OnTransportError.Broadcast(OutError);
        return false;
    }

    Socket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("RollbackCore UDP Transport"), NAME_None);
    if (!Socket)
    {
        OutError = TEXT("Failed to create UDP socket.");
        OnTransportError.Broadcast(OutError);
        return false;
    }

    Socket->SetNonBlocking(true);
    Socket->SetReuseAddr(true);

    int32 NewBufferSize = 0;
    Socket->SetSendBufferSize(Config.SendBufferBytes, NewBufferSize);
    Socket->SetReceiveBufferSize(Config.ReceiveBufferBytes, NewBufferSize);

    TSharedRef<FInternetAddr> BindAddress = SocketSubsystem->CreateInternetAddr();
    BindAddress->SetAnyAddress();
    BindAddress->SetPort(Config.LocalPort);

    if (!Socket->Bind(*BindAddress))
    {
        OutError = FString::Printf(TEXT("Failed to bind UDP socket to port %d."), Config.LocalPort);
        OnTransportError.Broadcast(OutError);
        StopTransport();
        return false;
    }

    TSharedRef<FInternetAddr> BoundAddress = SocketSubsystem->CreateInternetAddr();
    Socket->GetAddress(*BoundAddress);
    Config.LocalPort = BoundAddress->GetPort();

    if (!Config.RemoteHost.IsEmpty() && Config.RemotePort > 0)
    {
        if (!ConnectToPeer(-1, Config.RemoteHost, Config.RemotePort, OutError))
        {
            return false;
        }
    }

    return true;
}

void URollbackNetSubsystem::StopTransport()
{
    for (FRemotePeer& Peer : Peers)
    {
        if (Peer.bConnected)
        {
            Peer.bConnected = false;
            OnPeerDisconnected.Broadcast(Peer.PlayerId, TEXT("Transport stopped."));
        }
    }
    Peers.Reset();

    if (Socket && SocketSubsystem)
    {
        Socket->Close();
        SocketSubsystem->DestroySocket(Socket);
    }

    Socket = nullptr;
    SocketSubsystem = nullptr;
    DelayedIncomingPackets.Reset();
    DelayedOutgoingPackets.Reset();
}

bool URollbackNetSubsystem::ConnectToPeer(int32 PlayerId, const FString& RemoteHost, int32 RemotePort, FString& OutError)
{
    if (!Socket || !SocketSubsystem)
    {
        OutError = TEXT("StartUdpPeer must succeed before connecting to a remote peer.");
        OnTransportError.Broadcast(OutError);
        return false;
    }

    if (Peers.Num() >= Config.MaxPeers)
    {
        OutError = FString::Printf(TEXT("Maximum peer count (%d) reached."), Config.MaxPeers);
        OnTransportError.Broadcast(OutError);
        return false;
    }

    if (PlayerId >= 0)
    {
        if (FRemotePeer* Existing = FindPeerByPlayerId(PlayerId))
        {
            OutError = FString::Printf(TEXT("Already connected to player %d."), PlayerId);
            return false;
        }
    }

    TSharedPtr<FInternetAddr> Endpoint;
    if (!ResolveEndpoint(RemoteHost, RemotePort, Endpoint, OutError))
    {
        OnTransportError.Broadcast(OutError);
        return false;
    }

    FRemotePeer NewPeer;
    NewPeer.PlayerId = PlayerId;
    NewPeer.Endpoint = Endpoint;
    NewPeer.bConnected = true;
    NewPeer.LastActivityTime = FPlatformTime::Seconds();
    NewPeer.LastHeartbeatSentTime = FPlatformTime::Seconds();
    Peers.Add(MoveTemp(NewPeer));

    Stats.ConnectedPeerCount = Peers.Num();

    FRemotePeer& Peer = Peers.Last();
    const FString EndpointStr = Peer.Endpoint->ToString(true);
    OnTransportConnected.Broadcast(EndpointStr);

    return SendHello(Peer);
}

void URollbackNetSubsystem::DisconnectPeer(int32 PlayerId)
{
    int32 Index = INDEX_NONE;
    for (int32 i = 0; i < Peers.Num(); ++i)
    {
        if (Peers[i].PlayerId == PlayerId)
        {
            Index = i;
            break;
        }
    }

    if (Index == INDEX_NONE)
    {
        return;
    }

    FRemotePeer Peer = MoveTemp(Peers[Index]);
    Peers.RemoveAtSwap(Index);
    Stats.ConnectedPeerCount = Peers.Num();

    OnPeerDisconnected.Broadcast(Peer.PlayerId, TEXT("Disconnected by local request."));
}

bool URollbackNetSubsystem::SendInputFrame(int32 PlayerId, int32 Frame, FRollbackInput Input, bool bReliable)
{
    if (!Socket || Peers.Num() == 0)
    {
        return false;
    }

    LocalInputHistory.Add(Frame, Input);
    LocalInputHistory.Remove(Frame - FMath::Max(Config.InputRedundancyFrames + 120, 180));

    TArray<FNetworkInputFrame> Frames;
    for (int32 HistoryFrame = Frame - Config.InputRedundancyFrames; HistoryFrame <= Frame; ++HistoryFrame)
    {
        if (const FRollbackInput* FoundInput = LocalInputHistory.Find(HistoryFrame))
        {
            FNetworkInputFrame FrameInput;
            FrameInput.PlayerId = PlayerId;
            FrameInput.Frame = HistoryFrame;
            FrameInput.Input = *FoundInput;
            Frames.Add(FrameInput);
        }
    }

    bool bAllSent = true;
    for (FRemotePeer& Peer : Peers)
    {
        if (Peer.bConnected && Peer.Endpoint.IsValid())
        {
            if (!SendInputPacket(Peer, Frames, bReliable))
            {
                bAllSent = false;
            }
        }
    }

    return bAllSent;
}

bool URollbackNetSubsystem::ConsumeRemoteInput(int32 PlayerId, int32 Frame, FRollbackInput& OutInput) const
{
    if (const TMap<int32, FRollbackInput>* PlayerInputs = RemoteInputBuffer.Find(PlayerId))
    {
        if (const FRollbackInput* FoundInput = PlayerInputs->Find(Frame))
        {
            OutInput = *FoundInput;
            return true;
        }
    }

    OutInput = FRollbackInput();
    return false;
}

void URollbackNetSubsystem::BufferRemoteInputForRollback(int32 PlayerId, int32 Frame, FRollbackInput Input)
{
    CacheRemoteInput(PlayerId, Frame, Input);
}

void URollbackNetSubsystem::ApplyBufferedInputsToState(URollbackStateComponent* StateComponent, int32 PlayerId, int32 FromFrame, int32 ToFrame, bool& bOutChanged, int32& OutEarliestChangedFrame)
{
    bOutChanged = false;
    OutEarliestChangedFrame = -1;

    if (!StateComponent)
    {
        return;
    }

    const TMap<int32, FRollbackInput>* PlayerInputs = RemoteInputBuffer.Find(PlayerId);
    if (!PlayerInputs)
    {
        return;
    }

    const int32 StartFrame = FMath::Min(FromFrame, ToFrame);
    const int32 EndFrame = FMath::Max(FromFrame, ToFrame);
    for (int32 Frame = StartFrame; Frame <= EndFrame; ++Frame)
    {
        if (const FRollbackInput* BufferedInput = PlayerInputs->Find(Frame))
        {
            if (StateComponent->GetInputForFrame(Frame) != *BufferedInput)
            {
                StateComponent->InjectInputForFrame(Frame, *BufferedInput);
                bOutChanged = true;
                if (OutEarliestChangedFrame < 0)
                {
                    OutEarliestChangedFrame = Frame;
                }
            }
        }
    }
}

bool URollbackNetSubsystem::IsConnectedToPeer() const
{
    for (const FRemotePeer& Peer : Peers)
    {
        if (Peer.bConnected)
        {
            return true;
        }
    }
    return false;
}

FRollbackTransportStats URollbackNetSubsystem::GetTransportStats() const
{
    FRollbackTransportStats CurrentStats = Stats;
    int32 TotalPending = 0;
    for (const FRemotePeer& Peer : Peers)
    {
        TotalPending += Peer.PendingReliable.Num();
    }
    CurrentStats.PendingReliablePackets = TotalPending;
    CurrentStats.ConnectedPeerCount = Peers.Num();
    return CurrentStats;
}

FString URollbackNetSubsystem::GetRemoteEndpointString() const
{
    for (const FRemotePeer& Peer : Peers)
    {
        if (Peer.bConnected && Peer.Endpoint.IsValid())
        {
            return Peer.Endpoint->ToString(true);
        }
    }
    return FString();
}

FString URollbackNetSubsystem::GetLocalEndpointString() const
{
    if (!Socket || !SocketSubsystem)
    {
        return FString();
    }

    TSharedRef<FInternetAddr> LocalAddress = SocketSubsystem->CreateInternetAddr();
    Socket->GetAddress(*LocalAddress);
    return LocalAddress->ToString(true);
}

TArray<int32> URollbackNetSubsystem::GetConnectedPeerIds() const
{
    TArray<int32> Ids;
    for (const FRemotePeer& Peer : Peers)
    {
        if (Peer.bConnected && Peer.PlayerId >= 0)
        {
            Ids.Add(Peer.PlayerId);
        }
    }
    return Ids;
}

TArray<FRollbackPeerInfo> URollbackNetSubsystem::GetAllPeerInfo() const
{
    TArray<FRollbackPeerInfo> Infos;
    const double Now = FPlatformTime::Seconds();
    for (const FRemotePeer& Peer : Peers)
    {
        FRollbackPeerInfo Info;
        Info.PlayerId = Peer.PlayerId;
        Info.EndpointString = Peer.Endpoint.IsValid() ? Peer.Endpoint->ToString(true) : FString();
        Info.bIsConnected = Peer.bConnected;
        Info.LastActivitySecondsAgo = Peer.bConnected ? static_cast<float>(Now - Peer.LastActivityTime) : 0.0f;
        Info.RoundTripMs = Peer.LastRoundTripMs;
        Info.PacketsSentToPeer = Peer.PacketsSentToPeer;
        Info.PacketsReceivedFromPeer = Peer.PacketsReceivedFromPeer;
        Infos.Add(MoveTemp(Info));
    }
    return Infos;
}

void URollbackNetSubsystem::FlushTransport()
{
    const double NowSeconds = FPlatformTime::Seconds();
    ReleaseDelayedOutgoing(NowSeconds);
    PumpSocket(NowSeconds);
    ReleaseDelayedIncoming(NowSeconds);
    ResendReliablePackets(NowSeconds);
    CheckPeerTimeouts(NowSeconds);
    SendHeartbeats(NowSeconds);
    UpdatePerformanceStats(NowSeconds);

    for (FRemotePeer& Peer : Peers)
    {
        if (Peer.bAckDirty)
        {
            SendAck(Peer);
            Peer.bAckDirty = false;
        }
    }
}

bool URollbackNetSubsystem::TickFunction(float DeltaTime)
{
    FlushTransport();
    return true;
}

void URollbackNetSubsystem::PumpSocket(double NowSeconds)
{
    if (!Socket || !SocketSubsystem)
    {
        return;
    }

    static constexpr int32 MaxPacketsPerPump = 64;
    for (int32 PacketIndex = 0; PacketIndex < MaxPacketsPerPump; ++PacketIndex)
    {
        TArray<uint8> Payload;
        Payload.SetNumUninitialized(Config.MaxPacketBytes);

        int32 BytesRead = 0;
        TSharedRef<FInternetAddr> Sender = SocketSubsystem->CreateInternetAddr();
        if (!Socket->RecvFrom(Payload.GetData(), Payload.Num(), BytesRead, *Sender) || BytesRead <= 0)
        {
            break;
        }

        Payload.SetNum(BytesRead);
        Stats.PacketsReceived++;
        Stats.BytesReceived += BytesRead;
        PerfBytesReceivedLastWindow += BytesRead;

        if (Config.bEnablePacketLossSimulation && FMath::FRandRange(0.0f, 100.0f) < Config.SimulatedIncomingPacketLossPercent)
        {
            Stats.PacketsDroppedBySimulation++;
            continue;
        }

        const int32 MaxLatencyMs = FMath::Max(Config.SimulatedMinLatencyMs, Config.SimulatedMaxLatencyMs);
        if (Config.bEnablePacketLossSimulation && MaxLatencyMs > 0)
        {
            const int32 DelayMs = FMath::RandRange(FMath::Min(Config.SimulatedMinLatencyMs, MaxLatencyMs), MaxLatencyMs);
            FDelayedPacket Delayed;
            Delayed.Payload = MoveTemp(Payload);
            Delayed.Endpoint = Sender->Clone();
            Delayed.ReleaseSeconds = NowSeconds + static_cast<double>(DelayMs) / 1000.0;
            DelayedIncomingPackets.Add(MoveTemp(Delayed));
            continue;
        }

        ProcessIncomingPayload(Payload, Sender, NowSeconds);
    }
}

void URollbackNetSubsystem::ProcessIncomingPayload(const TArray<uint8>& Payload, TSharedRef<FInternetAddr> Sender, double NowSeconds)
{
    FMemoryReader Reader(Payload);

    uint32 Magic = 0;
    uint8 Version = 0;
    uint8 PacketType = 0;
    uint32 Sequence = 0;
    uint32 AckSequence = 0;
    int32 InputCount = 0;

    Reader << Magic;
    Reader << Version;
    Reader << PacketType;
    Reader << Sequence;
    Reader << AckSequence;
    Reader << InputCount;

    if (Reader.IsError() || Magic != RollbackNet::PacketMagic || Version != RollbackNet::PacketVersion || InputCount < 0 || InputCount > 64)
    {
        return;
    }

    FRemotePeer* Peer = FindPeerByEndpoint(*Sender);

    if (!Peer && Config.bAcceptFirstRemotePeer && Peers.Num() < Config.MaxPeers)
    {
        FRemotePeer NewPeer;
        NewPeer.PlayerId = -1;
        NewPeer.Endpoint = Sender->Clone();
        NewPeer.bConnected = true;
        NewPeer.LastActivityTime = NowSeconds;
        NewPeer.LastHeartbeatSentTime = NowSeconds;
        Peers.Add(MoveTemp(NewPeer));
        Peer = &Peers.Last();
        Stats.ConnectedPeerCount = Peers.Num();
        OnTransportConnected.Broadcast(Sender->ToString(true));
    }

    if (!Peer)
    {
        return;
    }

    Peer->LastActivityTime = NowSeconds;
    Peer->PacketsReceivedFromPeer++;
    Stats.LastReceivedSequence = Sequence;

    RemoveAckedPackets(*Peer, AckSequence, NowSeconds);
    MarkSequenceReceived(*Peer, Sequence);

    if (PacketType == static_cast<uint8>(RollbackNet::EWirePacketType::Hello))
    {
        Peer->bAckDirty = true;
        return;
    }

    if (PacketType == static_cast<uint8>(RollbackNet::EWirePacketType::Heartbeat))
    {
        Peer->bAckDirty = true;
        return;
    }

    if (PacketType == static_cast<uint8>(RollbackNet::EWirePacketType::Ack))
    {
        return;
    }

    if (PacketType == static_cast<uint8>(RollbackNet::EWirePacketType::Input))
    {
        for (int32 Index = 0; Index < InputCount; ++Index)
        {
            FNetworkInputFrame InputFrame;
            float AxisX = 0.0f;
            float AxisY = 0.0f;
            float AxisZ = 0.0f;
            Reader << InputFrame.PlayerId;
            Reader << InputFrame.Frame;
            Reader << InputFrame.Input.Buttons;
            Reader << AxisX;
            Reader << AxisY;
            Reader << AxisZ;

            if (Reader.IsError())
            {
                return;
            }

            InputFrame.Input.Axes = FVector(AxisX, AxisY, AxisZ);

            if (Peer->PlayerId < 0 && InputFrame.PlayerId >= 0)
            {
                Peer->PlayerId = InputFrame.PlayerId;
            }

            CacheRemoteInput(InputFrame.PlayerId, InputFrame.Frame, InputFrame.Input);
        }

        Peer->bAckDirty = true;
    }
}

void URollbackNetSubsystem::ReleaseDelayedIncoming(double NowSeconds)
{
    for (int32 Index = DelayedIncomingPackets.Num() - 1; Index >= 0; --Index)
    {
        if (DelayedIncomingPackets[Index].ReleaseSeconds <= NowSeconds && DelayedIncomingPackets[Index].Endpoint.IsValid())
        {
            FDelayedPacket Delayed = MoveTemp(DelayedIncomingPackets[Index]);
            DelayedIncomingPackets.RemoveAtSwap(Index);
            ProcessIncomingPayload(Delayed.Payload, Delayed.Endpoint.ToSharedRef(), NowSeconds);
        }
    }
}

void URollbackNetSubsystem::ReleaseDelayedOutgoing(double NowSeconds)
{
    for (int32 Index = DelayedOutgoingPackets.Num() - 1; Index >= 0; --Index)
    {
        if (DelayedOutgoingPackets[Index].ReleaseSeconds <= NowSeconds && DelayedOutgoingPackets[Index].Endpoint.IsValid())
        {
            FDelayedPacket Delayed = MoveTemp(DelayedOutgoingPackets[Index]);
            DelayedOutgoingPackets.RemoveAtSwap(Index);
            SendRawPacketNow(Delayed.Payload, *Delayed.Endpoint);
        }
    }
}

void URollbackNetSubsystem::ResendReliablePackets(double NowSeconds)
{
    if (!Socket)
    {
        return;
    }

    for (FRemotePeer& Peer : Peers)
    {
        TArray<uint32> ExceededSequences;

        for (TPair<uint32, FPendingReliablePacket>& Pending : Peer.PendingReliable)
        {
            FPendingReliablePacket& Packet = Pending.Value;
            if (Packet.Destination.IsValid() && NowSeconds - Packet.LastSentSeconds >= Config.ResendAfterSeconds)
            {
                if (Packet.SendAttempts >= Config.MaxReliableRetryCount)
                {
                    ExceededSequences.Add(Pending.Key);
                    continue;
                }

                SendRawPacket(Packet.Payload, Packet.Destination.ToSharedRef(), true);
                Packet.LastSentSeconds = NowSeconds;
                Packet.SendAttempts++;
                Stats.PacketsResent++;
                Peer.PacketsSentToPeer++;
            }
        }

        for (uint32 Seq : ExceededSequences)
        {
            Peer.PendingReliable.Remove(Seq);
            OnPeerMaxRetriesExceeded.Broadcast(Peer.PlayerId, static_cast<int32>(Seq));
        }
    }
}

void URollbackNetSubsystem::CheckPeerTimeouts(double NowSeconds)
{
    for (int32 i = Peers.Num() - 1; i >= 0; --i)
    {
        FRemotePeer& Peer = Peers[i];
        if (Peer.bConnected && (NowSeconds - Peer.LastActivityTime) >= static_cast<double>(Config.PeerTimeoutSeconds))
        {
            const int32 DisconnectedPlayerId = Peer.PlayerId;
            OnPeerDisconnected.Broadcast(DisconnectedPlayerId, TEXT("Peer timed out (no activity)."));
            Peers.RemoveAtSwap(i);
            Stats.ConnectedPeerCount = Peers.Num();
        }
    }
}

void URollbackNetSubsystem::SendHeartbeats(double NowSeconds)
{
    for (FRemotePeer& Peer : Peers)
    {
        if (Peer.bConnected && (NowSeconds - Peer.LastHeartbeatSentTime) >= static_cast<double>(Config.HeartbeatIntervalSeconds))
        {
            SendHeartbeat(Peer);
            Peer.LastHeartbeatSentTime = NowSeconds;
        }
    }
}

bool URollbackNetSubsystem::SendHello(FRemotePeer& Peer)
{
    static const TArray<FNetworkInputFrame> EmptyInputs;
    return SendPacket(Peer, static_cast<uint8>(RollbackNet::EWirePacketType::Hello), EmptyInputs, true);
}

bool URollbackNetSubsystem::SendHeartbeat(FRemotePeer& Peer)
{
    static const TArray<FNetworkInputFrame> EmptyInputs;
    return SendPacket(Peer, static_cast<uint8>(RollbackNet::EWirePacketType::Heartbeat), EmptyInputs, false);
}

bool URollbackNetSubsystem::SendAck(FRemotePeer& Peer)
{
    static const TArray<FNetworkInputFrame> EmptyInputs;
    return SendPacket(Peer, static_cast<uint8>(RollbackNet::EWirePacketType::Ack), EmptyInputs, false);
}

bool URollbackNetSubsystem::SendInputPacket(FRemotePeer& Peer, const TArray<FNetworkInputFrame>& InputFrames, bool bReliable)
{
    return SendPacket(Peer, static_cast<uint8>(RollbackNet::EWirePacketType::Input), InputFrames, bReliable);
}

bool URollbackNetSubsystem::SendPacket(FRemotePeer& Peer, uint8 PacketType, const TArray<FNetworkInputFrame>& InputFrames, bool bReliable)
{
    if (!Peer.Endpoint.IsValid())
    {
        return false;
    }

    TArray<uint8> Payload;
    FMemoryWriter Writer(Payload);

    uint32 Magic = RollbackNet::PacketMagic;
    uint8 Version = RollbackNet::PacketVersion;
    uint32 Sequence = Peer.NextOutgoingSequence++;
    uint32 AckSequence = Peer.HighestContiguousReceivedSequence;
    int32 InputCount = InputFrames.Num();

    Writer << Magic;
    Writer << Version;
    Writer << PacketType;
    Writer << Sequence;
    Writer << AckSequence;
    Writer << InputCount;

    for (const FNetworkInputFrame& InputFrame : InputFrames)
    {
        int32 PId = InputFrame.PlayerId;
        int32 Frame = InputFrame.Frame;
        int32 Buttons = InputFrame.Input.Buttons;
        float AxisX = InputFrame.Input.Axes.X;
        float AxisY = InputFrame.Input.Axes.Y;
        float AxisZ = InputFrame.Input.Axes.Z;

        Writer << PId;
        Writer << Frame;
        Writer << Buttons;
        Writer << AxisX;
        Writer << AxisY;
        Writer << AxisZ;
    }

    if (Payload.Num() > Config.MaxPacketBytes)
    {
        const FString Error = FString::Printf(TEXT("Rollback packet is %d bytes, above MaxPacketBytes=%d."), Payload.Num(), Config.MaxPacketBytes);
        OnTransportError.Broadcast(Error);
        return false;
    }

    const double NowSeconds = FPlatformTime::Seconds();
    if (bReliable)
    {
        FPendingReliablePacket PendingPacket;
        PendingPacket.Payload = Payload;
        PendingPacket.Destination = Peer.Endpoint->Clone();
        PendingPacket.LastSentSeconds = NowSeconds;
        PendingPacket.SendAttempts = 1;
        Peer.PendingReliable.Add(Sequence, MoveTemp(PendingPacket));
    }

    Stats.LastSentSequence = Sequence;
    Peer.PacketsSentToPeer++;
    return SendRawPacket(Payload, Peer.Endpoint.ToSharedRef(), true);
}

bool URollbackNetSubsystem::SendRawPacket(const TArray<uint8>& Payload, TSharedRef<FInternetAddr> Destination, bool bApplySimulation)
{
    if (!Socket)
    {
        return false;
    }

    if (bApplySimulation && Config.bEnablePacketLossSimulation && FMath::FRandRange(0.0f, 100.0f) < Config.SimulatedOutgoingPacketLossPercent)
    {
        Stats.PacketsDroppedBySimulation++;
        return true;
    }

    const int32 MaxLatencyMs = FMath::Max(Config.SimulatedMinLatencyMs, Config.SimulatedMaxLatencyMs);
    if (bApplySimulation && Config.bEnablePacketLossSimulation && MaxLatencyMs > 0)
    {
        const int32 DelayMs = FMath::RandRange(FMath::Min(Config.SimulatedMinLatencyMs, MaxLatencyMs), MaxLatencyMs);
        FDelayedPacket Delayed;
        Delayed.Payload = Payload;
        Delayed.Endpoint = Destination->Clone();
        Delayed.ReleaseSeconds = FPlatformTime::Seconds() + static_cast<double>(DelayMs) / 1000.0;
        DelayedOutgoingPackets.Add(MoveTemp(Delayed));
        return true;
    }

    return SendRawPacketNow(Payload, *Destination);
}

bool URollbackNetSubsystem::SendRawPacketNow(const TArray<uint8>& Payload, const FInternetAddr& Destination)
{
    int32 BytesSent = 0;
    const bool bSent = Socket && Socket->SendTo(Payload.GetData(), Payload.Num(), BytesSent, Destination);
    if (bSent)
    {
        Stats.PacketsSent++;
        Stats.BytesSent += BytesSent;
        PerfBytesSentLastWindow += BytesSent;
    }
    return bSent && BytesSent == Payload.Num();
}

void URollbackNetSubsystem::MarkSequenceReceived(FRemotePeer& Peer, uint32 Sequence)
{
    if (Sequence == 0)
    {
        return;
    }

    Peer.ReceivedSequences.Add(Sequence);
    while (Peer.ReceivedSequences.Contains(Peer.HighestContiguousReceivedSequence + 1))
    {
        Peer.HighestContiguousReceivedSequence++;
        Peer.ReceivedSequences.Remove(Peer.HighestContiguousReceivedSequence - 512);
    }
}

void URollbackNetSubsystem::RemoveAckedPackets(FRemotePeer& Peer, uint32 AckSequence, double NowSeconds)
{
    TArray<uint32> AckedSequences;
    for (const TPair<uint32, FPendingReliablePacket>& Pending : Peer.PendingReliable)
    {
        if (Pending.Key <= AckSequence)
        {
            AckedSequences.Add(Pending.Key);
            Peer.LastRoundTripMs = static_cast<float>((NowSeconds - Pending.Value.LastSentSeconds) * 1000.0);
        }
    }

    for (uint32 AckedSequence : AckedSequences)
    {
        Peer.PendingReliable.Remove(AckedSequence);
    }

    if (AckSequence > static_cast<uint32>(Stats.LastAckedSequence))
    {
        Stats.LastAckedSequence = AckSequence;
    }

    float MaxRtt = 0.0f;
    for (const FRemotePeer& P : Peers)
    {
        MaxRtt = FMath::Max(MaxRtt, P.LastRoundTripMs);
    }
    Stats.LastRoundTripMs = MaxRtt;
}

void URollbackNetSubsystem::CacheRemoteInput(int32 PlayerId, int32 Frame, const FRollbackInput& Input)
{
    TMap<int32, FRollbackInput>& PlayerInputs = RemoteInputBuffer.FindOrAdd(PlayerId);
    const FRollbackInput* ExistingInput = PlayerInputs.Find(Frame);
    if (!ExistingInput || *ExistingInput != Input)
    {
        PlayerInputs.Add(Frame, Input);
        Stats.LastReceivedFrame = FMath::Max(Stats.LastReceivedFrame, Frame);
        OnRemoteInputReceived.Broadcast(PlayerId, Frame, Input);
    }

    PlayerInputs.Remove(Frame - 600);
}

URollbackNetSubsystem::FRemotePeer* URollbackNetSubsystem::FindPeerByEndpoint(const FInternetAddr& Addr)
{
    const FString AddrStr = Addr.ToString(true);
    for (FRemotePeer& Peer : Peers)
    {
        if (Peer.Endpoint.IsValid() && Peer.Endpoint->ToString(true) == AddrStr)
        {
            return &Peer;
        }
    }
    return nullptr;
}

URollbackNetSubsystem::FRemotePeer* URollbackNetSubsystem::FindPeerByPlayerId(int32 PlayerId)
{
    for (FRemotePeer& Peer : Peers)
    {
        if (Peer.PlayerId == PlayerId)
        {
            return &Peer;
        }
    }
    return nullptr;
}

bool URollbackNetSubsystem::ResolveEndpoint(const FString& Host, int32 Port, TSharedPtr<FInternetAddr>& OutEndpoint, FString& OutError) const
{
    if (!SocketSubsystem)
    {
        OutError = TEXT("Socket subsystem is not initialized.");
        return false;
    }

    if (Host.IsEmpty() || Port <= 0 || Port > 65535)
    {
        OutError = FString::Printf(TEXT("Invalid endpoint '%s:%d'."), *Host, Port);
        return false;
    }

    OutEndpoint = SocketSubsystem->GetAddressFromString(Host);
    if (!OutEndpoint.IsValid())
    {
        FAddressInfoResult AddressInfo = SocketSubsystem->GetAddressInfo(*Host, nullptr, EAddressInfoFlags::Default, NAME_None, ESocketType::SOCKTYPE_Datagram);
        if (AddressInfo.Results.Num() > 0)
        {
            OutEndpoint = AddressInfo.Results[0].Address->Clone();
        }
    }

    if (!OutEndpoint.IsValid())
    {
        OutError = FString::Printf(TEXT("Could not resolve host '%s'."), *Host);
        return false;
    }

    OutEndpoint->SetPort(Port);
    return true;
}

void URollbackNetSubsystem::UpdatePerformanceStats(double NowSeconds)
{
    const double Elapsed = NowSeconds - PerfLastBandwidthTime;
    if (Elapsed >= 1.0)
    {
        PerfStats.BytesSentPerSecond = static_cast<float>(PerfBytesSentLastWindow) / static_cast<float>(Elapsed);
        PerfStats.BytesReceivedPerSecond = static_cast<float>(PerfBytesReceivedLastWindow) / static_cast<float>(Elapsed);
        PerfBytesSentLastWindow = 0;
        PerfBytesReceivedLastWindow = 0;
        PerfLastBandwidthTime = NowSeconds;
    }

    if (PerfSimulationTimes.Num() > 0)
    {
        float Sum = 0.0f;
        float Max = 0.0f;
        for (float V : PerfSimulationTimes)
        {
            Sum += V;
            if (V > Max) Max = V;
        }
        PerfStats.AvgSimulationTimeMs = Sum / static_cast<float>(PerfSimulationTimes.Num());
        PerfStats.MaxSimulationTimeMs = Max;
    }

    if (PerfRollbackTimes.Num() > 0)
    {
        float Sum = 0.0f;
        float Max = 0.0f;
        for (float V : PerfRollbackTimes)
        {
            Sum += V;
            if (V > Max) Max = V;
        }
        PerfStats.AvgRollbackTimeMs = Sum / static_cast<float>(PerfRollbackTimes.Num());
        PerfStats.MaxRollbackTimeMs = Max;
    }

    if (PerfSerializeTimes.Num() > 0)
    {
        float Sum = 0.0f;
        float Max = 0.0f;
        for (float V : PerfSerializeTimes)
        {
            Sum += V;
            if (V > Max) Max = V;
        }
        PerfStats.AvgStateSerializeTimeMs = Sum / static_cast<float>(PerfSerializeTimes.Num());
        PerfStats.MaxStateSerializeTimeMs = Max;
    }

    if (PerfStateSizes.Num() > 0)
    {
        float Sum = 0.0f;
        for (float V : PerfStateSizes) Sum += V;
        PerfStats.AvgStateSnapshotBytes = Sum / static_cast<float>(PerfStateSizes.Num());
    }

    const int32 CurrentSecond = static_cast<int32>(NowSeconds);
    int32 CountThisSecond = 0;
    for (int32 Ts : PerfRollbackFrameTimestamps)
    {
        if (Ts >= CurrentSecond) CountThisSecond++;
    }
    PerfStats.RollbacksInLastSecond = CountThisSecond;

    PerfStats.ConnectedPeerCount = Peers.Num();
    PerfStats.DesyncCount = PerfStats.DesyncCount;
}

void URollbackNetSubsystem::RecordSimulationTime(float Ms)
{
    PerfSimulationTimes.Add(Ms);
    if (PerfSimulationTimes.Num() > PerfHistorySize) PerfSimulationTimes.RemoveAt(0);
    PerfStats.FramesSimulated++;
}

void URollbackNetSubsystem::RecordRollbackTime(float Ms, int32 DepthFrames)
{
    PerfRollbackTimes.Add(Ms);
    if (PerfRollbackTimes.Num() > PerfHistorySize) PerfRollbackTimes.RemoveAt(0);
    PerfRollbackFrameTimestamps.Add(static_cast<int32>(FPlatformTime::Seconds()));
    if (PerfRollbackFrameTimestamps.Num() > 120) PerfRollbackFrameTimestamps.RemoveAt(0);
    PerfStats.MaxRollbackDepthFrames = FMath::Max(PerfStats.MaxRollbackDepthFrames, DepthFrames);
    PerfStats.TotalRollbackCount++;
    PerfStats.FramesWithRollback++;
}

void URollbackNetSubsystem::RecordSerializeTime(float Ms, int32 StateBytes)
{
    PerfSerializeTimes.Add(Ms);
    if (PerfSerializeTimes.Num() > PerfHistorySize) PerfSerializeTimes.RemoveAt(0);
    PerfStateSizes.Add(StateBytes);
    if (PerfStateSizes.Num() > PerfHistorySize) PerfStateSizes.RemoveAt(0);
}

void URollbackNetSubsystem::RecordDesync()
{
    PerfStats.DesyncCount++;
}

void URollbackNetSubsystem::SetEntityCount(int32 Count)
{
    PerfStats.RegisteredEntityCount = Count;
}

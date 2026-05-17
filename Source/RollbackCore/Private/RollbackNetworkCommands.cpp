// Copyright (c) 2026 GregOrigin. MIT Licensed - see LICENSE for details.

#include "RollbackNetSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "RollbackManager.h"

namespace
{
    int32 ReadIntArg(const TArray<FString>& Args, int32 Index, int32 DefaultValue)
    {
        return Args.IsValidIndex(Index) ? FCString::Atoi(*Args[Index]) : DefaultValue;
    }

    float ReadFloatArg(const TArray<FString>& Args, int32 Index, float DefaultValue)
    {
        return Args.IsValidIndex(Index) ? FCString::Atof(*Args[Index]) : DefaultValue;
    }

    void LogRollbackNet(const FString& Message)
    {
        UE_LOG(LogTemp, Display, TEXT("RollbackCore: %s"), *Message);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Cyan, Message);
        }
    }

    URollbackNetSubsystem* GetRollbackNetSubsystem(UWorld* World)
    {
        return World ? World->GetSubsystem<URollbackNetSubsystem>() : nullptr;
    }
}

static FAutoConsoleCommandWithWorldAndArgs CVarRollbackNetHost(
    TEXT("Rollback.NetHost"),
    TEXT("Starts UDP transport on a local port. Args: [LocalPort=7777] [LossPercent=0] [MaxPeers=4]"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        URollbackNetSubsystem* NetSubsystem = GetRollbackNetSubsystem(World);
        if (!NetSubsystem)
        {
            LogRollbackNet(TEXT("NetHost failed: RollbackNetSubsystem unavailable."));
            return;
        }

        const int32 LocalPort = ReadIntArg(Args, 0, 7777);
        const float LossPercent = ReadFloatArg(Args, 1, 0.0f);
        const int32 MaxPeers = ReadIntArg(Args, 2, 4);

        FRollbackTransportConfig Config;
        Config.LocalPort = LocalPort;
        Config.MaxPeers = MaxPeers;
        Config.bEnablePacketLossSimulation = LossPercent > 0.0f;
        Config.SimulatedIncomingPacketLossPercent = LossPercent;
        Config.SimulatedOutgoingPacketLossPercent = LossPercent;

        FString Error;
        if (!NetSubsystem->StartUdpPeer(Config, Error))
        {
            LogRollbackNet(FString::Printf(TEXT("NetHost transport failed: %s"), *Error));
            return;
        }

        LogRollbackNet(FString::Printf(TEXT("NetHost ready on %s, max peers %d"), *NetSubsystem->GetLocalEndpointString(), MaxPeers));
    })
);

static FAutoConsoleCommandWithWorldAndArgs CVarRollbackNetClient(
    TEXT("Rollback.NetClient"),
    TEXT("Starts UDP transport and connects to a host. Args: <RemoteHost> <RemotePort> [LocalPort=7778] [PlayerId=-1] [LossPercent=0]"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (Args.Num() < 2)
        {
            LogRollbackNet(TEXT("Usage: Rollback.NetClient <RemoteHost> <RemotePort> [LocalPort=7778] [PlayerId=-1] [LossPercent=0]"));
            return;
        }

        URollbackNetSubsystem* NetSubsystem = GetRollbackNetSubsystem(World);
        if (!NetSubsystem)
        {
            LogRollbackNet(TEXT("NetClient failed: RollbackNetSubsystem unavailable."));
            return;
        }

        const FString RemoteHost = Args[0];
        const int32 RemotePort = ReadIntArg(Args, 1, 7777);
        const int32 LocalPort = ReadIntArg(Args, 2, 7778);
        const int32 PlayerId = ReadIntArg(Args, 3, -1);
        const float LossPercent = ReadFloatArg(Args, 4, 0.0f);

        FRollbackTransportConfig Config;
        Config.LocalPort = LocalPort;
        Config.RemoteHost = RemoteHost;
        Config.RemotePort = RemotePort;
        Config.bEnablePacketLossSimulation = LossPercent > 0.0f;
        Config.SimulatedIncomingPacketLossPercent = LossPercent;
        Config.SimulatedOutgoingPacketLossPercent = LossPercent;

        FString Error;
        if (!NetSubsystem->StartUdpPeer(Config, Error))
        {
            LogRollbackNet(FString::Printf(TEXT("NetClient failed: %s"), *Error));
            return;
        }

        if (!NetSubsystem->ConnectToPeer(PlayerId, RemoteHost, RemotePort, Error))
        {
            LogRollbackNet(FString::Printf(TEXT("NetClient connect failed: %s"), *Error));
            return;
        }

        LogRollbackNet(FString::Printf(TEXT("NetClient connected local %s -> remote %s:%d (PlayerId=%d)"), *NetSubsystem->GetLocalEndpointString(), *RemoteHost, RemotePort, PlayerId));
    })
);

static FAutoConsoleCommandWithWorldAndArgs CVarRollbackNetConnect(
    TEXT("Rollback.NetConnect"),
    TEXT("Connects to an additional peer for multi-player. Args: <RemoteHost> <RemotePort> <PlayerId>"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (Args.Num() < 3)
        {
            LogRollbackNet(TEXT("Usage: Rollback.NetConnect <RemoteHost> <RemotePort> <PlayerId>"));
            return;
        }

        URollbackNetSubsystem* NetSubsystem = GetRollbackNetSubsystem(World);
        if (!NetSubsystem)
        {
            LogRollbackNet(TEXT("NetConnect failed: RollbackNetSubsystem unavailable."));
            return;
        }

        const FString RemoteHost = Args[0];
        const int32 RemotePort = ReadIntArg(Args, 1, 7777);
        const int32 PlayerId = ReadIntArg(Args, 2, -1);

        FString Error;
        if (!NetSubsystem->ConnectToPeer(PlayerId, RemoteHost, RemotePort, Error))
        {
            LogRollbackNet(FString::Printf(TEXT("NetConnect failed: %s"), *Error));
            return;
        }

        LogRollbackNet(FString::Printf(TEXT("Connected to PlayerId=%d at %s:%d. Peers: %d"), PlayerId, *RemoteHost, RemotePort, NetSubsystem->GetConnectedPeerIds().Num()));
    })
);

static FAutoConsoleCommandWithWorldAndArgs CVarRollbackNetDisconnect(
    TEXT("Rollback.NetDisconnect"),
    TEXT("Disconnects a specific peer by PlayerId. Args: <PlayerId>"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (Args.Num() < 1)
        {
            LogRollbackNet(TEXT("Usage: Rollback.NetDisconnect <PlayerId>"));
            return;
        }

        URollbackNetSubsystem* NetSubsystem = GetRollbackNetSubsystem(World);
        if (!NetSubsystem)
        {
            LogRollbackNet(TEXT("NetDisconnect failed: RollbackNetSubsystem unavailable."));
            return;
        }

        const int32 PlayerId = FCString::Atoi(*Args[0]);
        NetSubsystem->DisconnectPeer(PlayerId);
        LogRollbackNet(FString::Printf(TEXT("Disconnected PlayerId=%d. Remaining peers: %d"), PlayerId, NetSubsystem->GetConnectedPeerIds().Num()));
    })
);

static FAutoConsoleCommandWithWorld CVarRollbackNetPeers(
    TEXT("Rollback.NetPeers"),
    TEXT("Lists all connected peers."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        URollbackNetSubsystem* NetSubsystem = GetRollbackNetSubsystem(World);
        if (!NetSubsystem)
        {
            LogRollbackNet(TEXT("NetPeers: subsystem unavailable."));
            return;
        }

        TArray<FRollbackPeerInfo> Peers = NetSubsystem->GetAllPeerInfo();
        if (Peers.Num() == 0)
        {
            LogRollbackNet(TEXT("No peers connected."));
            return;
        }

        for (const FRollbackPeerInfo& Peer : Peers)
        {
            LogRollbackNet(FString::Printf(TEXT("  Peer PlayerId=%d Endpoint=%s Connected=%s RTT=%.1fms Sent=%d Recv=%d Activity=%.1fs ago"),
                Peer.PlayerId, *Peer.EndpointString, Peer.bIsConnected ? TEXT("true") : TEXT("false"),
                Peer.RoundTripMs, Peer.PacketsSentToPeer, Peer.PacketsReceivedFromPeer, Peer.LastActivitySecondsAgo));
        }
    })
);

static FAutoConsoleCommandWithWorld CVarRollbackPerf(
    TEXT("Rollback.Perf"),
    TEXT("Dumps rollback performance statistics."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        URollbackNetSubsystem* NetSubsystem = GetRollbackNetSubsystem(World);
        URollbackManager* Manager = World ? World->GetSubsystem<URollbackManager>() : nullptr;
        if (!NetSubsystem || !Manager)
        {
            LogRollbackNet(TEXT("Rollback.Perf: subsystems unavailable."));
            return;
        }

        const FRollbackPerformanceStats Perf = NetSubsystem->GetPerformanceStats();
        LogRollbackNet(FString::Printf(TEXT("=== Rollback Performance Stats ===")));
        LogRollbackNet(FString::Printf(TEXT("  Frames simulated: %d (%d with rollback)"), Perf.FramesSimulated, Perf.FramesWithRollback));
        LogRollbackNet(FString::Printf(TEXT("  Sim time: avg=%.3fms max=%.3fms"), Perf.AvgSimulationTimeMs, Perf.MaxSimulationTimeMs));
        LogRollbackNet(FString::Printf(TEXT("  Rollback time: avg=%.3fms max=%.3fms depth=%d total=%d last_sec=%d"),
            Perf.AvgRollbackTimeMs, Perf.MaxRollbackTimeMs, Perf.MaxRollbackDepthFrames, Perf.TotalRollbackCount, Perf.RollbacksInLastSecond));
        LogRollbackNet(FString::Printf(TEXT("  State serialize: avg=%.3fms max=%.3fms avg_bytes=%.0f"), Perf.AvgStateSerializeTimeMs, Perf.MaxStateSerializeTimeMs, Perf.AvgStateSnapshotBytes));
        LogRollbackNet(FString::Printf(TEXT("  Bandwidth: up=%.0f B/s down=%.0f B/s"), Perf.BytesSentPerSecond, Perf.BytesReceivedPerSecond));
        LogRollbackNet(FString::Printf(TEXT("  Entities: %d  Peers: %d  Desyncs: %d"), Perf.RegisteredEntityCount, Perf.ConnectedPeerCount, Perf.DesyncCount));
    })
);

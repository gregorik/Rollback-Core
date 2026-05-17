// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "RollbackNetworkBlueprintLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "RollbackManager.h"
#include "RollbackNetSubsystem.h"

FRollbackTransportConfig URollbackNetworkBlueprintLibrary::MakeLoopbackTransportConfig(int32 LocalPort, float PacketLossPercent, int32 MinLatencyMs, int32 MaxLatencyMs, int32 InputRedundancyFrames, float ResendAfterSeconds)
{
    FRollbackTransportConfig Config;
    Config.LocalPort = LocalPort;
    Config.RemoteHost = TEXT("127.0.0.1");
    Config.RemotePort = LocalPort;
    Config.InputRedundancyFrames = InputRedundancyFrames;
    Config.ResendAfterSeconds = ResendAfterSeconds;
    Config.bAcceptFirstRemotePeer = true;
    Config.bEnablePacketLossSimulation = PacketLossPercent > 0.0f || MaxLatencyMs > 0;
    Config.SimulatedOutgoingPacketLossPercent = PacketLossPercent;
    Config.SimulatedIncomingPacketLossPercent = PacketLossPercent;
    Config.SimulatedMinLatencyMs = MinLatencyMs;
    Config.SimulatedMaxLatencyMs = FMath::Max(MinLatencyMs, MaxLatencyMs);
    return Config;
}

bool URollbackNetworkBlueprintLibrary::StartLoopbackPacketLossTransport(const UObject* WorldContextObject, int32 LocalPort, float PacketLossPercent, int32 MinLatencyMs, int32 MaxLatencyMs, FString& OutError)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World)
    {
        OutError = TEXT("No world was available for rollback transport.");
        return false;
    }

    URollbackNetSubsystem* NetSubsystem = World->GetSubsystem<URollbackNetSubsystem>();
    if (!NetSubsystem)
    {
        OutError = TEXT("RollbackNetSubsystem is not available.");
        return false;
    }

    return NetSubsystem->StartUdpPeer(MakeLoopbackTransportConfig(LocalPort, PacketLossPercent, MinLatencyMs, MaxLatencyMs), OutError);
}

bool URollbackNetworkBlueprintLibrary::SendRollbackInputForCurrentFrame(const UObject* WorldContextObject, int32 PlayerId, FRollbackInput Input, bool bReliable, FString& OutError)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World)
    {
        OutError = TEXT("No world was available for rollback input send.");
        return false;
    }

    URollbackNetSubsystem* NetSubsystem = World->GetSubsystem<URollbackNetSubsystem>();
    URollbackManager* RollbackManager = World->GetSubsystem<URollbackManager>();
    if (!NetSubsystem || !RollbackManager)
    {
        OutError = TEXT("Rollback network or manager subsystem is not available.");
        return false;
    }

    if (!NetSubsystem->SendInputFrame(PlayerId, RollbackManager->CurrentFrame, Input, bReliable))
    {
        OutError = TEXT("Rollback input send failed; transport is not connected.");
        return false;
    }

    OutError.Reset();
    return true;
}

bool URollbackNetworkBlueprintLibrary::ApplyBufferedInputsAndRollback(const UObject* WorldContextObject, URollbackStateComponent* RemoteStateComponent, int32 RemotePlayerId, int32 FromFrame, int32 ToFrame, bool& bOutRolledBack, int32& OutRollbackFrame)
{
    bOutRolledBack = false;
    OutRollbackFrame = -1;

    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World || !RemoteStateComponent)
    {
        return false;
    }

    URollbackNetSubsystem* NetSubsystem = World->GetSubsystem<URollbackNetSubsystem>();
    URollbackManager* RollbackManager = World->GetSubsystem<URollbackManager>();
    if (!NetSubsystem || !RollbackManager)
    {
        return false;
    }

    NetSubsystem->ApplyBufferedInputsToState(RemoteStateComponent, RemotePlayerId, FromFrame, ToFrame, bOutRolledBack, OutRollbackFrame);
    if (bOutRolledBack && OutRollbackFrame >= 0)
    {
        RollbackManager->RollbackToFrame(OutRollbackFrame, OutRollbackFrame);
    }

    return true;
}

bool URollbackNetworkBlueprintLibrary::ConnectToRemotePeer(const UObject* WorldContextObject, int32 PlayerId, const FString& RemoteHost, int32 RemotePort, FString& OutError)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World)
    {
        OutError = TEXT("No world was available for peer connection.");
        return false;
    }

    URollbackNetSubsystem* NetSubsystem = World->GetSubsystem<URollbackNetSubsystem>();
    if (!NetSubsystem)
    {
        OutError = TEXT("RollbackNetSubsystem is not available.");
        return false;
    }

    return NetSubsystem->ConnectToPeer(PlayerId, RemoteHost, RemotePort, OutError);
}

void URollbackNetworkBlueprintLibrary::DisconnectRemotePeer(const UObject* WorldContextObject, int32 PlayerId)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World) return;

    URollbackNetSubsystem* NetSubsystem = World->GetSubsystem<URollbackNetSubsystem>();
    if (NetSubsystem)
    {
        NetSubsystem->DisconnectPeer(PlayerId);
    }
}

TArray<int32> URollbackNetworkBlueprintLibrary::GetConnectedPeerIds(const UObject* WorldContextObject)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World) return TArray<int32>();

    URollbackNetSubsystem* NetSubsystem = World->GetSubsystem<URollbackNetSubsystem>();
    return NetSubsystem ? NetSubsystem->GetConnectedPeerIds() : TArray<int32>();
}

TArray<FRollbackPeerInfo> URollbackNetworkBlueprintLibrary::GetAllPeerInfo(const UObject* WorldContextObject)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World) return TArray<FRollbackPeerInfo>();

    URollbackNetSubsystem* NetSubsystem = World->GetSubsystem<URollbackNetSubsystem>();
    return NetSubsystem ? NetSubsystem->GetAllPeerInfo() : TArray<FRollbackPeerInfo>();
}

FRollbackPerformanceStats URollbackNetworkBlueprintLibrary::GetPerformanceStats(const UObject* WorldContextObject)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World) return FRollbackPerformanceStats();

    URollbackNetSubsystem* NetSubsystem = World->GetSubsystem<URollbackNetSubsystem>();
    return NetSubsystem ? NetSubsystem->GetPerformanceStats() : FRollbackPerformanceStats();
}

bool URollbackNetworkBlueprintLibrary::SendRollbackInputToAllPeers(const UObject* WorldContextObject, int32 PlayerId, FRollbackInput Input, bool bReliable, FString& OutError)
{
    return SendRollbackInputForCurrentFrame(WorldContextObject, PlayerId, Input, bReliable, OutError);
}

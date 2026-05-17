// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RollbackStateComponent.h"
#include "RollbackNetTypes.generated.h"

USTRUCT(BlueprintType)
struct FRollbackTransportConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network")
    int32 LocalPort = 7777;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network")
    FString RemoteHost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network")
    int32 RemotePort = 7778;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network")
    FName SocketSubsystemName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network")
    int32 SendBufferBytes = 2 * 1024 * 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network")
    int32 ReceiveBufferBytes = 2 * 1024 * 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network")
    int32 MaxPacketBytes = 1200;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network", meta = (ClampMin = "0", ClampMax = "32"))
    int32 InputRedundancyFrames = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network", meta = (ClampMin = "0.01", ClampMax = "2.0"))
    float ResendAfterSeconds = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network")
    bool bAcceptFirstRemotePeer = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network", meta = (ClampMin = "1", ClampMax = "8"))
    int32 MaxPeers = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network|Reliability", meta = (ClampMin = "1", ClampMax = "100"))
    int32 MaxReliableRetryCount = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network|Reliability", meta = (ClampMin = "1.0", ClampMax = "120.0"))
    float PeerTimeoutSeconds = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network|Reliability", meta = (ClampMin = "0.1", ClampMax = "30.0"))
    float HeartbeatIntervalSeconds = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network|Packet Loss")
    bool bEnablePacketLossSimulation = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network|Packet Loss", meta = (ClampMin = "0", ClampMax = "100"))
    float SimulatedOutgoingPacketLossPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network|Packet Loss", meta = (ClampMin = "0", ClampMax = "100"))
    float SimulatedIncomingPacketLossPercent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network|Packet Loss", meta = (ClampMin = "0", ClampMax = "2000"))
    int32 SimulatedMinLatencyMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Network|Packet Loss", meta = (ClampMin = "0", ClampMax = "2000"))
    int32 SimulatedMaxLatencyMs = 0;
};

USTRUCT(BlueprintType)
struct FRollbackTransportStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 PacketsSent = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 PacketsReceived = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 PacketsResent = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 PacketsDroppedBySimulation = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 BytesSent = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 BytesReceived = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 LastSentSequence = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 LastReceivedSequence = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 LastAckedSequence = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 PendingReliablePackets = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 LastReceivedFrame = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    float LastRoundTripMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 ConnectedPeerCount = 0;
};

USTRUCT(BlueprintType)
struct FRollbackPeerInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 PlayerId = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    FString EndpointString;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    bool bIsConnected = false;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    float LastActivitySecondsAgo = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    float RoundTripMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 PacketsSentToPeer = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Network")
    int32 PacketsReceivedFromPeer = 0;
};

USTRUCT(BlueprintType)
struct FRollbackPerformanceStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    float AvgSimulationTimeMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    float MaxSimulationTimeMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    float AvgRollbackTimeMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    float MaxRollbackTimeMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    int32 RollbacksInLastSecond = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    float AvgStateSnapshotBytes = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    float AvgStateSerializeTimeMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    float MaxStateSerializeTimeMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    float BytesSentPerSecond = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    float BytesReceivedPerSecond = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    int32 ConnectedPeerCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    int32 RegisteredEntityCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    int32 MaxRollbackDepthFrames = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    int32 TotalRollbackCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    int32 DesyncCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    float AvgFrameBudgetMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    int32 FramesSimulated = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Performance")
    int32 FramesWithRollback = 0;
};

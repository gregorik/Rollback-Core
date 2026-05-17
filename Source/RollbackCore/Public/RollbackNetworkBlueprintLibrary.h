// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RollbackNetTypes.h"
#include "RollbackNetworkBlueprintLibrary.generated.h"

class URollbackStateComponent;

UCLASS()
class ROLLBACKCORE_API URollbackNetworkBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Rollback|Network", meta = (AdvancedDisplay = "InputRedundancyFrames,ResendAfterSeconds"))
    static FRollbackTransportConfig MakeLoopbackTransportConfig(int32 LocalPort, float PacketLossPercent, int32 MinLatencyMs, int32 MaxLatencyMs, int32 InputRedundancyFrames = 8, float ResendAfterSeconds = 0.08f);

    UFUNCTION(BlueprintCallable, Category = "Rollback|Network", meta = (WorldContext = "WorldContextObject"))
    static bool StartLoopbackPacketLossTransport(const UObject* WorldContextObject, int32 LocalPort, float PacketLossPercent, int32 MinLatencyMs, int32 MaxLatencyMs, FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "Rollback|Network", meta = (WorldContext = "WorldContextObject"))
    static bool SendRollbackInputForCurrentFrame(const UObject* WorldContextObject, int32 PlayerId, FRollbackInput Input, bool bReliable, FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "Rollback|Network", meta = (WorldContext = "WorldContextObject"))
    static bool ApplyBufferedInputsAndRollback(const UObject* WorldContextObject, URollbackStateComponent* RemoteStateComponent, int32 RemotePlayerId, int32 FromFrame, int32 ToFrame, bool& bOutRolledBack, int32& OutRollbackFrame);

    UFUNCTION(BlueprintCallable, Category = "Rollback|Network", meta = (WorldContext = "WorldContextObject"))
    static bool ConnectToRemotePeer(const UObject* WorldContextObject, int32 PlayerId, const FString& RemoteHost, int32 RemotePort, FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "Rollback|Network", meta = (WorldContext = "WorldContextObject"))
    static void DisconnectRemotePeer(const UObject* WorldContextObject, int32 PlayerId);

    UFUNCTION(BlueprintPure, Category = "Rollback|Network", meta = (WorldContext = "WorldContextObject"))
    static TArray<int32> GetConnectedPeerIds(const UObject* WorldContextObject);

    UFUNCTION(BlueprintPure, Category = "Rollback|Network", meta = (WorldContext = "WorldContextObject"))
    static TArray<FRollbackPeerInfo> GetAllPeerInfo(const UObject* WorldContextObject);

    UFUNCTION(BlueprintPure, Category = "Rollback|Performance", meta = (WorldContext = "WorldContextObject"))
    static FRollbackPerformanceStats GetPerformanceStats(const UObject* WorldContextObject);

    UFUNCTION(BlueprintCallable, Category = "Rollback|Network", meta = (WorldContext = "WorldContextObject"))
    static bool SendRollbackInputToAllPeers(const UObject* WorldContextObject, int32 PlayerId, FRollbackInput Input, bool bReliable, FString& OutError);
};

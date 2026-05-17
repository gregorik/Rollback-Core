// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RollbackEntity.h"
#include "RollbackStateComponent.h"
#include "Containers/Ticker.h"
#include "RollbackManager.generated.h"

USTRUCT(BlueprintType)
struct FRollbackDebugFrameRecord
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 Frame = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    FString EntityName;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    bool bStateAvailable = false;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 SavedByteCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 SavedChecksum = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    FRollbackInput Input;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRollbackDesyncSignature, int32, Frame, const FString&, EntityName, int32, ChecksumMismatch);

UCLASS()
class ROLLBACKCORE_API URollbackManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
    void Tick(float DeltaTime);

    UFUNCTION(BlueprintCallable, Category = "Rollback")
    void RegisterEntity(TScriptInterface<IRollbackEntity> Entity);
    
    UFUNCTION(BlueprintCallable, Category = "Rollback")
    void UnregisterEntity(TScriptInterface<IRollbackEntity> Entity);

    UFUNCTION(BlueprintCallable, Category = "Rollback")
    void AdvanceFrame();

    UFUNCTION(BlueprintCallable, Category = "Rollback")
    void RollbackToFrame(int32 Frame, int32 EarliestMismatchFrame = -1);

    UFUNCTION(BlueprintCallable, Category = "Rollback|Debug")
    void DrawDebugState(int32 Frame);

    UFUNCTION(BlueprintCallable, Category = "Rollback|Debug")
    TArray<FRollbackDebugFrameRecord> GetDebugFrameRecords(int32 Frame) const;

    UFUNCTION(BlueprintCallable, Category = "Rollback|Debug")
    TArray<int32> GetAvailableDebugFrames() const;

    UFUNCTION(BlueprintPure, Category = "Rollback|Debug")
    int32 GetOldestAvailableDebugFrame() const;

    UFUNCTION(BlueprintPure, Category = "Rollback|Debug")
    int32 GetNewestAvailableDebugFrame() const;

    UFUNCTION(BlueprintCallable, Category = "Rollback|Debug")
    bool SetDebugScrubFrame(int32 Frame);

    UFUNCTION(BlueprintCallable, Category = "Rollback|Debug")
    bool StepDebugScrubFrame(int32 FrameDelta);

    UFUNCTION(BlueprintCallable, Category = "Rollback|Debug")
    void SetDebugScrubFollowLive(bool bInFollowLive);

    UPROPERTY(BlueprintReadOnly, Category = "Rollback")
    int32 CurrentFrame;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Debug")
    bool bEnableVisualDebugging = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Debug", meta = (ClampMin = "0"))
    int32 DebugLiveFrameLag = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Debug")
    bool bDebugScrubFollowsLive = true;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 DebugScrubFrame = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 RollbackCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 LastRollbackFrame = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 LastRollbackEndFrame = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 LastRollbackFramesReplayed = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 LastRollbackRestoredStates = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 LastRollbackSavedStates = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Desync")
    int32 DesyncCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Desync")
    int32 LastDesyncFrame = -1;

    UPROPERTY(BlueprintAssignable, Category = "Rollback|Desync")
    FRollbackDesyncSignature OnDesyncDetected;

private:
    void SimulateFrame(int32 Frame);

    UPROPERTY()
    TArray<TScriptInterface<IRollbackEntity>> RegisteredEntities;
    
    FTSTicker::FDelegateHandle TickHandle;
    bool TickFunction(float DeltaTime);
    
    float Accumulator = 0.0f;
    float FixedTimeStep = 1.0f / 60.0f;
    bool bIsReplayingRollback = false;
};

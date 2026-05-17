// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RollbackEntity.h"
#include "UObject/Interface.h"
#include "RollbackStateComponent.generated.h"

USTRUCT(BlueprintType)
struct FRollbackInput
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Rollback")
    int32 Buttons = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Rollback")
    FVector Axes = FVector::ZeroVector;

    bool operator==(const FRollbackInput& Other) const
    {
        return Buttons == Other.Buttons && Axes == Other.Axes;
    }

    bool operator!=(const FRollbackInput& Other) const
    {
        return !(*this == Other);
    }
};

UINTERFACE(MinimalAPI)
class URollbackInputProvider : public UInterface
{
    GENERATED_BODY()
};

class ROLLBACKCORE_API IRollbackInputProvider
{
    GENERATED_BODY()

public:
    virtual bool GetRollbackInput(FRollbackInput& OutInput) { return false; }
};

USTRUCT(BlueprintType)
struct FRollbackFrameState
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<uint8> ActorData;

    UPROPERTY()
    FVector Location;

    UPROPERTY()
    FQuat Rotation;

    UPROPERTY()
    FVector Velocity;
    
    FRollbackFrameState() : Location(FVector::ZeroVector), Rotation(FQuat::Identity), Velocity(FVector::ZeroVector) {}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRollbackTickSignature, float, DeltaTime, int32, Frame, FRollbackInput, Input);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ROLLBACKCORE_API URollbackStateComponent : public UActorComponent, public IRollbackEntity
{
	GENERATED_BODY()

public:	
	URollbackStateComponent();

    virtual void BeginPlay() override;
    virtual void RollbackTick(float DeltaTime, int32 Frame) override;
    virtual void SaveRollbackState(int32 Frame) override;
    virtual void LoadRollbackState(int32 Frame) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback")
    int32 MaxBufferSize = 60;

    // Buffer for states
    UPROPERTY()
    TMap<int32, FRollbackFrameState> StateBuffer;

    // Buffer for inputs
    UPROPERTY()
    TMap<int32, FRollbackInput> InputBuffer;

    // The input to be used for the current local frame before tick
    UPROPERTY(BlueprintReadWrite, Category = "Rollback|Input")
    FRollbackInput CurrentLocalInput;

    UFUNCTION(BlueprintCallable, Category = "Rollback|Input")
    void InjectInputForFrame(int32 Frame, FRollbackInput Input);

    UFUNCTION(BlueprintPure, Category = "Rollback|Input")
    FRollbackInput GetInputForFrame(int32 Frame) const;

    UFUNCTION(BlueprintPure, Category = "Rollback|Debug")
    int32 GetTrackedPropertyCount() const { return TrackedProperties.Num(); }

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 LastSavedFrame = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 LastRestoredFrame = -1;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 LastSavedByteCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 LastRestoredByteCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 LastSavedChecksum = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rollback|Debug")
    int32 LastRestoredChecksum = 0;

    // Event fired during a deterministic tick, where BP applies inputs
    UFUNCTION(BlueprintImplementableEvent, Category = "Rollback")
    void OnRollbackTick(float DeltaTime, int32 Frame, FRollbackInput Input);

    UPROPERTY(BlueprintAssignable, Category = "Rollback")
    FOnRollbackTickSignature OnRollbackTickDelegate;

private:
    void SaveActorVariables(TArray<uint8>& OutData);
    void LoadActorVariables(const TArray<uint8>& InData);

    TArray<FProperty*> TrackedProperties;
};

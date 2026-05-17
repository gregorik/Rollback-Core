// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RollbackStateComponent.h"
#include "RollbackDemoPawn.generated.h"

class UStaticMeshComponent;
class URollbackMovementComponent;

UCLASS()
class ROLLBACKCORE_API ARollbackDemoPawn : public APawn, public IRollbackInputProvider
{
	GENERATED_BODY()

public:
	ARollbackDemoPawn();

protected:
	virtual void BeginPlay() override;

public:	
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual bool GetRollbackInput(FRollbackInput& OutInput) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rollback Demo")
    UStaticMeshComponent* MeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rollback Demo")
    URollbackStateComponent* StateComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rollback Demo")
    URollbackMovementComponent* MoveComp;

    // This proves the auto-state saver works
    UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Rollback Demo")
    FVector SimulatedVelocity;

    UFUNCTION()
    void HandleRollbackTick(float DeltaTime, int32 Frame, FRollbackInput Input);

    void SetColor(FLinearColor Color);

private:
    void UpdateLocalInputFromController();
    void MoveForwardPressed();
    void MoveForwardReleased();
    void MoveBackwardPressed();
    void MoveBackwardReleased();
    void MoveRightPressed();
    void MoveRightReleased();
    void MoveLeftPressed();
    void MoveLeftReleased();

    FVector BoundInputAxes = FVector::ZeroVector;
};

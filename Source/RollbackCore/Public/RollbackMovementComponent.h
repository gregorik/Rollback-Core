// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RollbackMovementComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ROLLBACKCORE_API URollbackMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	URollbackMovementComponent();
    
    // Moves the actor with simple deterministic math using the fixed timestep from URollbackManager
    UFUNCTION(BlueprintCallable, Category = "Rollback|Movement")
    void DeterministicMove(FVector InputVector);

    // Moves the actor with simple deterministic math using an explicit fixed delta time
    UFUNCTION(BlueprintCallable, Category = "Rollback|Movement")
    void DeterministicMoveForStep(FVector InputVector, float FixedDeltaTime);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Movement")
    float MoveSpeed = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rollback|Movement")
    bool bSweepForCollision = true;
};
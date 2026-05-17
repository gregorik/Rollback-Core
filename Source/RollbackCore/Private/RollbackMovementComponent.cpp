// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "RollbackMovementComponent.h"
#include "GameFramework/Actor.h"

URollbackMovementComponent::URollbackMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URollbackMovementComponent::DeterministicMove(FVector InputVector)
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    // Use a fixed time step explicitly for deterministic movement (e.g. 1/60th of a second)
    const float FixedDeltaTime = 0.016666f;
    
    // Truncate to avoid floating point drift, simulating fixed-point math
    InputVector.X = FMath::RoundToFloat(InputVector.X * 1000.f) / 1000.f;
    InputVector.Y = FMath::RoundToFloat(InputVector.Y * 1000.f) / 1000.f;
    InputVector.Z = FMath::RoundToFloat(InputVector.Z * 1000.f) / 1000.f;

    FVector DeltaMove = InputVector * MoveSpeed * FixedDeltaTime;
    Owner->AddActorWorldOffset(DeltaMove, true); // true = sweep for collision
}
// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "RollbackMovementComponent.h"
#include "GameFramework/Actor.h"
#include "RollbackManager.h"
#include "Engine/World.h"

URollbackMovementComponent::URollbackMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URollbackMovementComponent::DeterministicMove(FVector InputVector)
{
    const UWorld* World = GetWorld();
    const URollbackManager* Manager = World ? World->GetSubsystem<URollbackManager>() : nullptr;
    const float FixedDeltaTime = Manager ? Manager->GetFixedTimeStep() : (1.0f / 60.0f);
    DeterministicMoveForStep(InputVector, FixedDeltaTime);
}

void URollbackMovementComponent::DeterministicMoveForStep(FVector InputVector, float FixedDeltaTime)
{
    AActor* Owner = GetOwner();
    if (!Owner || !FMath::IsFinite(FixedDeltaTime) || FixedDeltaTime <= 0.0f || !FMath::IsFinite(MoveSpeed))
    {
        return;
    }

    if (!FMath::IsFinite(InputVector.X) || !FMath::IsFinite(InputVector.Y) || !FMath::IsFinite(InputVector.Z))
    {
        return;
    }

    // Clamp input vector magnitude to 1.0 to prevent speeding on diagonals
    InputVector = InputVector.GetClampedToMaxSize(1.0);

    // Truncate/quantize to avoid floating point drift across compilers/architectures
    InputVector.X = FMath::RoundToDouble(InputVector.X * 1000.0) / 1000.0;
    InputVector.Y = FMath::RoundToDouble(InputVector.Y * 1000.0) / 1000.0;
    InputVector.Z = FMath::RoundToDouble(InputVector.Z * 1000.0) / 1000.0;

    const FVector DeltaMove = InputVector * MoveSpeed * FixedDeltaTime;
    Owner->AddActorWorldOffset(DeltaMove, bSweepForCollision);
}
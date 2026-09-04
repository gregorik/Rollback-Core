// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "RollbackStateComponent.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "RollbackManager.h"
#include "RollbackNetSubsystem.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

URollbackStateComponent::URollbackStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URollbackStateComponent::BeginPlay()
{
    Super::BeginPlay();
    
    TrackedProperties.Reset();
    AActor* Owner = GetOwner();
    if (Owner)
    {
        // Automatically cache all Blueprint variables marked as "SaveGame"
        for (TFieldIterator<FProperty> It(Owner->GetClass()); It; ++It)
        {
            FProperty* Prop = *It;
            if (Prop && Prop->HasAnyPropertyFlags(CPF_SaveGame))
            {
                if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
                {
                    continue;
                }
                if (CastField<FObjectPropertyBase>(Prop) || CastField<FInterfaceProperty>(Prop) ||
                    CastField<FDelegateProperty>(Prop) || CastField<FMulticastDelegateProperty>(Prop))
                {
                    continue;
                }
                TrackedProperties.Add(Prop);
            }
        }

        // Sort tracked properties alphabetically by FName for cross-platform/compiler determinism
        TrackedProperties.Sort([](const FProperty& A, const FProperty& B)
        {
            return A.GetFName().LexicalLess(B.GetFName());
        });
    }

    if (UWorld* World = GetWorld())
    {
        if (URollbackManager* Manager = World->GetSubsystem<URollbackManager>())
        {
            Manager->RegisterEntity(this);
        }
    }
}

void URollbackStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        if (URollbackManager* Manager = World->GetSubsystem<URollbackManager>())
        {
            Manager->UnregisterEntity(this);
        }
    }

    ResetBuffers();
    TrackedProperties.Reset();
    Super::EndPlay(EndPlayReason);
}

void URollbackStateComponent::ResetBuffers()
{
    StateBuffer.Reset();
    InputBuffer.Reset();
}

void URollbackStateComponent::TrimStateBuffer(int32 NewestFrame)
{
    const int32 Capacity = FMath::Max(2, MaxBufferSize);
    const int32 OldestAllowedFrame = NewestFrame - Capacity + 1;
    for (auto It = StateBuffer.CreateIterator(); It; ++It)
    {
        if (It.Key() < OldestAllowedFrame)
        {
            It.RemoveCurrent();
        }
    }

    while (StateBuffer.Num() > Capacity)
    {
        int32 OldestFrame = MAX_int32;
        for (const TPair<int32, FRollbackFrameState>& Pair : StateBuffer)
        {
            OldestFrame = FMath::Min(OldestFrame, Pair.Key);
        }
        if (OldestFrame == MAX_int32)
        {
            break;
        }
        StateBuffer.Remove(OldestFrame);
    }
}

void URollbackStateComponent::TrimInputBuffer(int32 NewestFrame)
{
    const int32 Capacity = FMath::Max(2, MaxBufferSize * 2);
    const int32 OldestAllowedFrame = NewestFrame - Capacity + 1;
    for (auto It = InputBuffer.CreateIterator(); It; ++It)
    {
        if (It.Key() < OldestAllowedFrame)
        {
            It.RemoveCurrent();
        }
    }

    while (InputBuffer.Num() > Capacity)
    {
        int32 OldestFrame = MAX_int32;
        for (const TPair<int32, FRollbackInput>& Pair : InputBuffer)
        {
            OldestFrame = FMath::Min(OldestFrame, Pair.Key);
        }
        if (OldestFrame == MAX_int32)
        {
            break;
        }
        InputBuffer.Remove(OldestFrame);
    }
}

void URollbackStateComponent::RollbackTick(float DeltaTime, int32 Frame)
{
    FRollbackInput InputToUse;
    if (const FRollbackInput* Found = InputBuffer.Find(Frame))
    {
        InputToUse = *Found;
    }
    else
    {
        if (AActor* Owner = GetOwner())
        {
            if (IRollbackInputProvider* InputProvider = Cast<IRollbackInputProvider>(Owner))
            {
                InputProvider->GetRollbackInput(CurrentLocalInput);
            }
        }

        CurrentLocalInput.QuantizeAxes();
        InputToUse = CurrentLocalInput;
        InputBuffer.Add(Frame, InputToUse); // Record local prediction
        TrimInputBuffer(Frame);
    }

    OnRollbackTick(DeltaTime, Frame, InputToUse);
    OnRollbackTickDelegate.Broadcast(DeltaTime, Frame, InputToUse);
}

void URollbackStateComponent::InjectInputForFrame(int32 Frame, FRollbackInput Input)
{
    if (Frame < 0)
    {
        return;
    }

    Input.QuantizeAxes();
    InputBuffer.Add(Frame, Input);
    TrimInputBuffer(Frame);
}

FRollbackInput URollbackStateComponent::GetInputForFrame(int32 Frame) const
{
    if (const FRollbackInput* FoundInput = InputBuffer.Find(Frame))
    {
        return *FoundInput;
    }
    return FRollbackInput();
}

void URollbackStateComponent::SaveRollbackState(int32 Frame)
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    const double SerializeStart = FPlatformTime::Seconds();

    FRollbackFrameState NewState;
    NewState.Location = Owner->GetActorLocation();
    NewState.Rotation = Owner->GetActorQuat();
    NewState.Velocity = Owner->GetVelocity();

    SaveActorVariables(NewState.ActorData);
    LastSavedFrame = Frame;
    LastSavedByteCount = NewState.ActorData.Num();
    LastSavedChecksum = NewState.ActorData.Num() > 0 ? static_cast<int32>(FCrc::MemCrc32(NewState.ActorData.GetData(), NewState.ActorData.Num())) : 0;

    StateBuffer.Add(Frame, NewState);
    TrimStateBuffer(Frame);

    const double SerializeEnd = FPlatformTime::Seconds();
    const float SerializeMs = static_cast<float>((SerializeEnd - SerializeStart) * 1000.0);

    if (UWorld* World = GetWorld())
    {
        if (URollbackNetSubsystem* Net = World->GetSubsystem<URollbackNetSubsystem>())
        {
            Net->RecordSerializeTime(SerializeMs, NewState.ActorData.Num());
        }
    }
}

void URollbackStateComponent::LoadRollbackState(int32 Frame)
{
    if (const FRollbackFrameState* FoundState = StateBuffer.Find(Frame))
    {
        AActor* Owner = GetOwner();
        if (Owner)
        {
            Owner->SetActorLocationAndRotation(FoundState->Location, FoundState->Rotation, false, nullptr, ETeleportType::TeleportPhysics);

            if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
            {
                if (RootPrimitive->IsSimulatingPhysics())
                {
                    RootPrimitive->SetPhysicsLinearVelocity(FoundState->Velocity);
                }
            }

            LoadActorVariables(FoundState->ActorData);
            LastRestoredFrame = Frame;
            LastRestoredByteCount = FoundState->ActorData.Num();
            LastRestoredChecksum = FoundState->ActorData.Num() > 0 ? static_cast<int32>(FCrc::MemCrc32(FoundState->ActorData.GetData(), FoundState->ActorData.Num())) : 0;
            LastRestoredVelocity = FoundState->Velocity;
            OnRollbackStateLoadedDelegate.Broadcast(Frame, FoundState->Velocity);
        }
    }
}

void URollbackStateComponent::SaveActorVariables(TArray<uint8>& OutData)
{
    AActor* Owner = GetOwner();
    if (!Owner || TrackedProperties.Num() == 0) return;

    FMemoryWriter MemWriter(OutData, true);
    FObjectAndNameAsStringProxyArchive Ar(MemWriter, true);
    Ar.ArIsSaveGame = true;

    for (FProperty* Prop : TrackedProperties)
    {
        Prop->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(), Prop->ContainerPtrToValuePtr<void>(Owner));
    }
}

void URollbackStateComponent::LoadActorVariables(const TArray<uint8>& InData)
{
    AActor* Owner = GetOwner();
    if (!Owner || InData.Num() == 0 || TrackedProperties.Num() == 0) return;

    FMemoryReader MemReader(InData, true);
    FObjectAndNameAsStringProxyArchive Ar(MemReader, true);
    Ar.ArIsSaveGame = true;

    for (FProperty* Prop : TrackedProperties)
    {
        Prop->SerializeItem(FStructuredArchiveFromArchive(Ar).GetSlot(), Prop->ContainerPtrToValuePtr<void>(Owner));
    }
}

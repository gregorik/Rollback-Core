// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "RollbackStateComponent.h"
#include "GameFramework/Actor.h"
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
    
    AActor* Owner = GetOwner();
    if (Owner)
    {
        // Automatically cache all Blueprint variables marked as "SaveGame"
        for (TFieldIterator<FProperty> It(Owner->GetClass()); It; ++It)
        {
            if (It->HasAnyPropertyFlags(CPF_SaveGame))
            {
                TrackedProperties.Add(*It);
            }
        }
    }

    if (UWorld* World = GetWorld())
    {
        if (URollbackManager* Manager = World->GetSubsystem<URollbackManager>())
        {
            Manager->RegisterEntity(this);
        }
    }
}

void URollbackStateComponent::RollbackTick(float DeltaTime, int32 Frame)
{
    FRollbackInput InputToUse;
    if (InputBuffer.Contains(Frame))
    {
        InputToUse = InputBuffer[Frame];
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

        InputToUse = CurrentLocalInput;
        InputBuffer.Add(Frame, InputToUse); // Record local prediction
    }

    OnRollbackTick(DeltaTime, Frame, InputToUse);
    OnRollbackTickDelegate.Broadcast(DeltaTime, Frame, InputToUse);
}

void URollbackStateComponent::InjectInputForFrame(int32 Frame, FRollbackInput Input)
{
    InputBuffer.Add(Frame, Input);
    
    if (InputBuffer.Num() > MaxBufferSize * 2)
    {
        InputBuffer.Remove(Frame - (MaxBufferSize * 2));
    }
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

    if (StateBuffer.Num() > MaxBufferSize)
    {
        int32 OldestFrame = Frame - MaxBufferSize;
        StateBuffer.Remove(OldestFrame);
    }

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
    if (StateBuffer.Contains(Frame))
    {
        FRollbackFrameState& State = StateBuffer[Frame];
        AActor* Owner = GetOwner();
        if (Owner)
        {
            Owner->SetActorLocationAndRotation(State.Location, State.Rotation);
            LoadActorVariables(State.ActorData);
            LastRestoredFrame = Frame;
            LastRestoredByteCount = State.ActorData.Num();
            LastRestoredChecksum = State.ActorData.Num() > 0 ? static_cast<int32>(FCrc::MemCrc32(State.ActorData.GetData(), State.ActorData.Num())) : 0;
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

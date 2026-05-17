// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "RollbackManager.h"
#include "Algo/Sort.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/Crc.h"
#include "RollbackCoreSettings.h"
#include "RollbackDemoEnvironment.h"
#include "RollbackNetSubsystem.h"

void URollbackManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    CurrentFrame = 0;

    if (const URollbackCoreSettings* Settings = GetDefault<URollbackCoreSettings>())
    {
        FixedTimeStep = Settings->FixedTickRateHz > 0
            ? 1.0f / static_cast<float>(Settings->FixedTickRateHz)
            : 1.0f / 60.0f;
        bEnableVisualDebugging = Settings->bEnableVisualDebuggingByDefault;
        DebugLiveFrameLag = Settings->DebugLiveFrameLag;
    }

    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &URollbackManager::TickFunction));
}

void URollbackManager::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    Super::Deinitialize();
}

bool URollbackManager::TickFunction(float DeltaTime)
{
    Tick(DeltaTime);
    return true;
}

void URollbackManager::Tick(float DeltaTime)
{
    Accumulator += DeltaTime;
    while (Accumulator >= FixedTimeStep)
    {
        AdvanceFrame();
        Accumulator -= FixedTimeStep;
    }
}

void URollbackManager::RegisterEntity(TScriptInterface<IRollbackEntity> Entity)
{
    if (!RegisteredEntities.Contains(Entity))
    {
        RegisteredEntities.Add(Entity);
    }

    if (UWorld* World = GetWorld())
    {
        if (URollbackNetSubsystem* Net = World->GetSubsystem<URollbackNetSubsystem>())
        {
            Net->SetEntityCount(RegisteredEntities.Num());
        }
    }
}

void URollbackManager::UnregisterEntity(TScriptInterface<IRollbackEntity> Entity)
{
    RegisteredEntities.Remove(Entity);

    if (UWorld* World = GetWorld())
    {
        if (URollbackNetSubsystem* Net = World->GetSubsystem<URollbackNetSubsystem>())
        {
            Net->SetEntityCount(RegisteredEntities.Num());
        }
    }
}

void URollbackManager::AdvanceFrame()
{
    const double SimStart = FPlatformTime::Seconds();
    SimulateFrame(CurrentFrame);
    const double SimEnd = FPlatformTime::Seconds();
    const float SimMs = static_cast<float>((SimEnd - SimStart) * 1000.0);

    if (UWorld* World = GetWorld())
    {
        if (URollbackNetSubsystem* Net = World->GetSubsystem<URollbackNetSubsystem>())
        {
            Net->RecordSimulationTime(SimMs);
        }
    }

    if (bDebugScrubFollowsLive)
    {
        DebugScrubFrame = FMath::Max(0, CurrentFrame - DebugLiveFrameLag);
    }
    CurrentFrame++;
}

void URollbackManager::SimulateFrame(int32 Frame)
{
    for (auto& Entity : RegisteredEntities)
    {
        if (Entity)
        {
            Entity->RollbackTick(FixedTimeStep, Frame);
        }
    }
    
    for (auto& Entity : RegisteredEntities)
    {
        if (Entity)
        {
            Entity->SaveRollbackState(Frame);
            if (bIsReplayingRollback)
            {
                LastRollbackSavedStates++;
            }
        }
    }
    
    if (bEnableVisualDebugging)
    {
        DrawDebugState(Frame - 5);
    }
}

void URollbackManager::RollbackToFrame(int32 Frame, int32 EarliestMismatchFrame)
{
    if (Frame >= CurrentFrame || Frame < 0) return;

    const double RollbackStart = FPlatformTime::Seconds();

    TMap<FString, TMap<int32, int32>> PreRollbackChecksums;
    for (auto& Entity : RegisteredEntities)
    {
        URollbackStateComponent* StateComp = Cast<URollbackStateComponent>(Entity.GetObject());
        if (!StateComp) continue;

        FString EntityName = StateComp->GetName();
        TMap<int32, int32>& EntityChecksums = PreRollbackChecksums.FindOrAdd(EntityName);
        for (int32 CFrame = Frame; CFrame < CurrentFrame; ++CFrame)
        {
            if (const FRollbackFrameState* State = StateComp->StateBuffer.Find(CFrame))
            {
                int32 Checksum = State->ActorData.Num() > 0
                    ? static_cast<int32>(FCrc::MemCrc32(State->ActorData.GetData(), State->ActorData.Num()))
                    : 0;
                EntityChecksums.Add(CFrame, Checksum);
            }
        }
    }

    RollbackCount++;
    LastRollbackFrame = Frame;
    LastRollbackEndFrame = CurrentFrame;
    LastRollbackFramesReplayed = CurrentFrame - Frame;
    LastRollbackRestoredStates = 0;
    LastRollbackSavedStates = 0;

    for (auto& Entity : RegisteredEntities)
    {
        if (Entity)
        {
            Entity->LoadRollbackState(Frame);
            LastRollbackRestoredStates++;
        }
    }
    
    bIsReplayingRollback = true;
    for (int32 SimFrame = Frame; SimFrame < CurrentFrame; ++SimFrame)
    {
        SimulateFrame(SimFrame);

        if (EarliestMismatchFrame < 0 || SimFrame < EarliestMismatchFrame)
        {
            for (auto& Entity : RegisteredEntities)
            {
                URollbackStateComponent* StateComp = Cast<URollbackStateComponent>(Entity.GetObject());
                if (!StateComp) continue;

                FString EntityName = StateComp->GetName();
                const TMap<int32, int32>* EntityChecksums = PreRollbackChecksums.Find(EntityName);
                if (!EntityChecksums) continue;

                const int32* PreChecksum = EntityChecksums->Find(SimFrame);
                if (!PreChecksum) continue;

                if (const FRollbackFrameState* State = StateComp->StateBuffer.Find(SimFrame))
                {
                    int32 PostChecksum = State->ActorData.Num() > 0
                        ? static_cast<int32>(FCrc::MemCrc32(State->ActorData.GetData(), State->ActorData.Num()))
                        : 0;
                    if (PostChecksum != *PreChecksum)
                    {
                        DesyncCount++;
                        LastDesyncFrame = SimFrame;
                        OnDesyncDetected.Broadcast(SimFrame, EntityName, PostChecksum ^ (*PreChecksum));

                        if (UWorld* World = GetWorld())
                        {
                            if (URollbackNetSubsystem* Net = World->GetSubsystem<URollbackNetSubsystem>())
                            {
                                Net->RecordDesync();
                            }
                        }
                    }
                }
            }
        }
    }
    bIsReplayingRollback = false;

    const double RollbackEnd = FPlatformTime::Seconds();
    const float RollbackMs = static_cast<float>((RollbackEnd - RollbackStart) * 1000.0);

    if (UWorld* World = GetWorld())
    {
        if (URollbackNetSubsystem* Net = World->GetSubsystem<URollbackNetSubsystem>())
        {
            Net->RecordRollbackTime(RollbackMs, CurrentFrame - Frame);
        }
    }
}

void URollbackManager::DrawDebugState(int32 Frame)
{
    UWorld* World = GetWorld();
    if (!World) return;

    for (auto& Entity : RegisteredEntities)
    {
        URollbackStateComponent* StateComp = Cast<URollbackStateComponent>(Entity.GetObject());
        if (StateComp && StateComp->StateBuffer.Contains(Frame))
        {
            FRollbackFrameState& State = StateComp->StateBuffer[Frame];
            DrawDebugBox(World, State.Location, FVector(50.f), State.Rotation, FColor::Cyan, false, 1.0f, 0, 2.f);
        }
    }
}

TArray<FRollbackDebugFrameRecord> URollbackManager::GetDebugFrameRecords(int32 Frame) const
{
    TArray<FRollbackDebugFrameRecord> Records;

    for (const TScriptInterface<IRollbackEntity>& Entity : RegisteredEntities)
    {
        const URollbackStateComponent* StateComp = Cast<URollbackStateComponent>(Entity.GetObject());
        if (!StateComp)
        {
            continue;
        }

        FRollbackDebugFrameRecord Record;
        Record.Frame = Frame;
        if (const AActor* Owner = StateComp->GetOwner())
        {
            Record.EntityName = Owner->GetName();
        }
        else
        {
            Record.EntityName = StateComp->GetName();
        }

        Record.Input = StateComp->GetInputForFrame(Frame);
        if (const FRollbackFrameState* State = StateComp->StateBuffer.Find(Frame))
        {
            Record.bStateAvailable = true;
            Record.Location = State->Location;
            Record.Rotation = State->Rotation.Rotator();
            Record.Velocity = State->Velocity;
            Record.SavedByteCount = State->ActorData.Num();
            Record.SavedChecksum = State->ActorData.Num() > 0
                ? static_cast<int32>(FCrc::MemCrc32(State->ActorData.GetData(), State->ActorData.Num()))
                : 0;
        }

        Records.Add(MoveTemp(Record));
    }

    return Records;
}

TArray<int32> URollbackManager::GetAvailableDebugFrames() const
{
    TSet<int32> UniqueFrames;
    for (const TScriptInterface<IRollbackEntity>& Entity : RegisteredEntities)
    {
        const URollbackStateComponent* StateComp = Cast<URollbackStateComponent>(Entity.GetObject());
        if (!StateComp)
        {
            continue;
        }

        for (const TPair<int32, FRollbackFrameState>& StatePair : StateComp->StateBuffer)
        {
            UniqueFrames.Add(StatePair.Key);
        }
    }

    TArray<int32> Frames = UniqueFrames.Array();
    Algo::Sort(Frames);
    return Frames;
}

int32 URollbackManager::GetOldestAvailableDebugFrame() const
{
    const TArray<int32> Frames = GetAvailableDebugFrames();
    return Frames.Num() > 0 ? Frames[0] : -1;
}

int32 URollbackManager::GetNewestAvailableDebugFrame() const
{
    const TArray<int32> Frames = GetAvailableDebugFrames();
    return Frames.Num() > 0 ? Frames.Last() : -1;
}

bool URollbackManager::SetDebugScrubFrame(int32 Frame)
{
    const int32 OldestFrame = GetOldestAvailableDebugFrame();
    const int32 NewestFrame = GetNewestAvailableDebugFrame();
    if (OldestFrame < 0 || NewestFrame < 0)
    {
        DebugScrubFrame = -1;
        return false;
    }

    DebugScrubFrame = FMath::Clamp(Frame, OldestFrame, NewestFrame);
    bDebugScrubFollowsLive = false;
    return DebugScrubFrame == Frame;
}

bool URollbackManager::StepDebugScrubFrame(int32 FrameDelta)
{
    const int32 BaseFrame = DebugScrubFrame >= 0 ? DebugScrubFrame : GetNewestAvailableDebugFrame();
    if (BaseFrame < 0)
    {
        return false;
    }

    return SetDebugScrubFrame(BaseFrame + FrameDelta);
}

void URollbackManager::SetDebugScrubFollowLive(bool bInFollowLive)
{
    bDebugScrubFollowsLive = bInFollowLive;
    if (bDebugScrubFollowsLive)
    {
        DebugScrubFrame = FMath::Max(0, CurrentFrame - DebugLiveFrameLag);
    }
}

static FAutoConsoleCommandWithWorld CVarSpawnRollbackDemo(
    TEXT("Rollback.SpawnDemo"),
    TEXT("Spawns the Rollback Core Spectacular Demo Environment"),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (World)
        {
            World->SpawnActor<ARollbackDemoEnvironment>(FVector(0,0,100), FRotator::ZeroRotator);
        }
    })
);

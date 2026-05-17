// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "RollbackDemoEnvironment.h"
#include "RollbackDemoPawn.h"
#include "RollbackManager.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

ARollbackDemoEnvironment::ARollbackDemoEnvironment()
{
	PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>("Root");

    FloorComp = CreateDefaultSubobject<UStaticMeshComponent>("Floor");
    FloorComp->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'"));
    if (CubeMesh.Succeeded())
    {
        FloorComp->SetStaticMesh(CubeMesh.Object);
    }
    FloorComp->SetRelativeScale3D(FVector(15.f, 15.f, 0.1f));
    FloorComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    CameraComp = CreateDefaultSubobject<UCameraComponent>("Camera");
    CameraComp->SetupAttachment(RootComponent);
    CameraComp->SetRelativeLocationAndRotation(FVector(-1200.f, 0.f, 1000.f), FRotator(-45.f, 0.f, 0.f));

    LightComp = CreateDefaultSubobject<UDirectionalLightComponent>("Light");
    LightComp->SetupAttachment(RootComponent);
    LightComp->SetRelativeRotation(FRotator(-60.f, 45.f, 0.f));

    TextComp = CreateDefaultSubobject<UTextRenderComponent>("Text");
    TextComp->SetupAttachment(RootComponent);
    TextComp->SetRelativeLocation(FVector(0.f, 0.f, 400.f));
    TextComp->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
    TextComp->SetTextRenderColor(FColor::Yellow);
    TextComp->SetWorldSize(72.f);
    TextComp->SetHorizontalAlignment(EHTA_Center);
    TextComp->SetText(FText::FromString("ROLLBACK CORE: SPECTACULAR DEMO\nP1: Blue (WASD) | P2: Red (15 Frames Lag)"));

    MetricsTextComp = CreateDefaultSubobject<UTextRenderComponent>("MetricsText");
    MetricsTextComp->SetupAttachment(RootComponent);
    MetricsTextComp->SetRelativeLocation(FVector(-350.f, -720.f, 280.f));
    MetricsTextComp->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
    MetricsTextComp->SetTextRenderColor(FColor::White);
    MetricsTextComp->SetWorldSize(34.f);
    MetricsTextComp->SetHorizontalAlignment(EHTA_Left);

    RollbackEventTextComp = CreateDefaultSubobject<UTextRenderComponent>("RollbackEventText");
    RollbackEventTextComp->SetupAttachment(RootComponent);
    RollbackEventTextComp->SetRelativeLocation(FVector(-350.f, 520.f, 280.f));
    RollbackEventTextComp->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
    RollbackEventTextComp->SetTextRenderColor(FColor::Green);
    RollbackEventTextComp->SetWorldSize(34.f);
    RollbackEventTextComp->SetHorizontalAlignment(EHTA_Left);
}

void ARollbackDemoEnvironment::BeginPlay()
{
	Super::BeginPlay();

    UWorld* World = GetWorld();
    if (World)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const FVector PawnSpawnOrigin = GetActorLocation() + FVector(0.f, 0.f, 75.f);
        LocalPawn = World->SpawnActor<ARollbackDemoPawn>(PawnSpawnOrigin + FVector(0.f, -300.f, 0.f), FRotator::ZeroRotator, SpawnParams);
        RemotePawn = World->SpawnActor<ARollbackDemoPawn>(PawnSpawnOrigin + FVector(0.f, 300.f, 0.f), FRotator::ZeroRotator, SpawnParams);

        if (LocalPawn) LocalPawn->SetColor(FLinearColor::Blue);
        if (RemotePawn) RemotePawn->SetColor(FLinearColor::Red);

        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (LocalPawn)
            {
                PC->Possess(LocalPawn);
                PC->SetViewTarget(this);
                PC->SetInputMode(FInputModeGameOnly());
                PC->bShowMouseCursor = false;
            }
            else
            {
                PC->Possess(this);
            }
        }
    }
}

void ARollbackDemoEnvironment::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    CorrectionMarkerTimeRemaining = FMath::Max(0.0f, CorrectionMarkerTimeRemaining - DeltaTime);

    URollbackManager* Manager = GetWorld()->GetSubsystem<URollbackManager>();
    if (!Manager || !LocalPawn || !RemotePawn) return;

    // 1. Gather Local Input
    // The possessed blue pawn owns local WASD sampling. Keep this fallback for editor cases
    // where the environment exists but possession has not completed.
    if (!LocalPawn->IsPlayerControlled())
    {
        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        FRollbackInput LocalInput;
        if (PC)
        {
            if (PC->IsInputKeyDown(EKeys::W)) LocalInput.Axes.X += 1.f;
            if (PC->IsInputKeyDown(EKeys::S)) LocalInput.Axes.X -= 1.f;
            if (PC->IsInputKeyDown(EKeys::D)) LocalInput.Axes.Y += 1.f;
            if (PC->IsInputKeyDown(EKeys::A)) LocalInput.Axes.Y -= 1.f;
        }
        LocalPawn->StateComp->CurrentLocalInput = LocalInput;
    }

    // 2. Generate True Remote Input
    FRollbackInput TrueRemoteInput;
    int32 Phase = (Manager->CurrentFrame / 45) % 4; // Change direction every 45 frames
    if (Phase == 0) TrueRemoteInput.Axes.Y = 1.f;
    else if (Phase == 2) TrueRemoteInput.Axes.Y = -1.f;
    
    P2TrueInputs.Add(Manager->CurrentFrame, TrueRemoteInput);

    // 3. Simulate Late Arrival (GGPO Rollback trigger)
    int32 FrameToReceive = Manager->CurrentFrame - SimulatedLatencyFrames;
    if (FrameToReceive >= 0 && P2TrueInputs.Contains(FrameToReceive))
    {
        FRollbackInput ReceivedInput = P2TrueInputs[FrameToReceive];

        RemotePawn->StateComp->InjectInputForFrame(FrameToReceive, ReceivedInput);

        if (ReceivedInput != LastReceivedP2Input)
        {
            LastPredictedRemoteLocation = RemotePawn->GetActorLocation();
            Manager->RollbackToFrame(FrameToReceive);
            LastCorrectedRemoteLocation = RemotePawn->GetActorLocation();
            LastCorrectionDistance = FVector::Dist(LastPredictedRemoteLocation, LastCorrectedRemoteLocation);
            LastCorrectedFrame = FrameToReceive;
            CorrectionMarkerTimeRemaining = 2.0f;
            LastReceivedP2Input = ReceivedInput;
        }

        P2TrueInputs.Remove(FrameToReceive - 100); // Cleanup old state
    }

    // 4. Update ongoing prediction (assume they keep doing what they last did)
    RemotePawn->StateComp->CurrentLocalInput = LastReceivedP2Input;

    UpdateDemoHud(Manager);
    DrawCorrectionMarkers();
}

void ARollbackDemoEnvironment::UpdateDemoHud(const URollbackManager* Manager)
{
    if (!Manager || !LocalPawn || !RemotePawn || !MetricsTextComp || !RollbackEventTextComp)
    {
        return;
    }

    const FRollbackInput LocalInput = LocalPawn->StateComp ? LocalPawn->StateComp->CurrentLocalInput : FRollbackInput();
    const URollbackStateComponent* RemoteState = RemotePawn->StateComp;
    const int32 TrackedProperties = RemoteState ? RemoteState->GetTrackedPropertyCount() : 0;
    const int32 LastSavedFrame = RemoteState ? RemoteState->LastSavedFrame : -1;
    const int32 LastSavedBytes = RemoteState ? RemoteState->LastSavedByteCount : 0;
    const int32 LastSavedChecksum = RemoteState ? RemoteState->LastSavedChecksum : 0;
    const int32 LastRestoredFrame = RemoteState ? RemoteState->LastRestoredFrame : -1;
    const int32 LastRestoredBytes = RemoteState ? RemoteState->LastRestoredByteCount : 0;
    const int32 LastRestoredChecksum = RemoteState ? RemoteState->LastRestoredChecksum : 0;

    const FString MetricsText = FString::Printf(
        TEXT("LOCAL INPUT: X=%+.0f Y=%+.0f\nFRAME: %d  LATENCY: %d FRAMES\nROLLBACKS: %d  LAST CORRECTED: %d\nAUTO STATE: %d SaveGame props | saved f%d %d bytes #%08X\nRESTORE: f%d %d bytes #%08X"),
        LocalInput.Axes.X,
        LocalInput.Axes.Y,
        Manager->CurrentFrame,
        SimulatedLatencyFrames,
        Manager->RollbackCount,
        Manager->LastRollbackFrame,
        TrackedProperties,
        LastSavedFrame,
        LastSavedBytes,
        LastSavedChecksum,
        LastRestoredFrame,
        LastRestoredBytes,
        LastRestoredChecksum);
    MetricsTextComp->SetText(FText::FromString(MetricsText));

    if (Manager->RollbackCount > LastShownRollbackCount)
    {
        LastShownRollbackCount = Manager->RollbackCount;
    }

    const FString EventText = Manager->RollbackCount > 0
        ? FString::Printf(
            TEXT("ROLLBACK EVENT #%d\ncorrected frame %d -> replayed %d frames\nrestored %d states -> resaved %d states\nprediction error %.1f uu\nRED MARKER: before  GREEN MARKER: after"),
            Manager->RollbackCount,
            Manager->LastRollbackFrame,
            Manager->LastRollbackFramesReplayed,
            Manager->LastRollbackRestoredStates,
            Manager->LastRollbackSavedStates,
            LastCorrectionDistance)
        : FString(TEXT("WAITING FOR DELAYED REMOTE INPUT\nred pawn predicts, then corrects when late input arrives"));

    RollbackEventTextComp->SetText(FText::FromString(EventText));
    RollbackEventTextComp->SetTextRenderColor(CorrectionMarkerTimeRemaining > 0.0f ? FColor::Green : FColor::Cyan);
}

void ARollbackDemoEnvironment::DrawCorrectionMarkers() const
{
    UWorld* World = GetWorld();
    if (!World || CorrectionMarkerTimeRemaining <= 0.0f)
    {
        return;
    }

    DrawDebugSphere(World, LastPredictedRemoteLocation, 70.f, 16, FColor::Red, false, 0.0f, 0, 4.f);
    DrawDebugSphere(World, LastCorrectedRemoteLocation, 80.f, 16, FColor::Green, false, 0.0f, 0, 4.f);
    DrawDebugDirectionalArrow(World, LastPredictedRemoteLocation, LastCorrectedRemoteLocation, 100.f, FColor::Yellow, false, 0.0f, 0, 3.f);
}

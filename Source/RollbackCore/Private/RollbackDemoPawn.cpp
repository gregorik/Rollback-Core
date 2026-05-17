// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#include "RollbackDemoPawn.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "RollbackMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

ARollbackDemoPawn::ARollbackDemoPawn()
{
	PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Disabled;

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
    RootComponent = MeshComp;
    MeshComp->SetMobility(EComponentMobility::Movable);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("StaticMesh'/Engine/BasicShapes/Sphere.Sphere'"));
    if (SphereMesh.Succeeded())
    {
        MeshComp->SetStaticMesh(SphereMesh.Object);
    }

    StateComp = CreateDefaultSubobject<URollbackStateComponent>("StateComp");
    StateComp->bAutoActivate = true;
    MoveComp = CreateDefaultSubobject<URollbackMovementComponent>("MoveComp");
    MoveComp->bAutoActivate = true;
}

void ARollbackDemoPawn::BeginPlay()
{
	Super::BeginPlay();
    StateComp->OnRollbackTickDelegate.AddDynamic(this, &ARollbackDemoPawn::HandleRollbackTick);
}

void ARollbackDemoPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    UpdateLocalInputFromController();
}

void ARollbackDemoPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (!PlayerInputComponent)
    {
        return;
    }

    PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &ARollbackDemoPawn::MoveForwardPressed);
    PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &ARollbackDemoPawn::MoveForwardReleased);
    PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &ARollbackDemoPawn::MoveBackwardPressed);
    PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &ARollbackDemoPawn::MoveBackwardReleased);
    PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &ARollbackDemoPawn::MoveRightPressed);
    PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &ARollbackDemoPawn::MoveRightReleased);
    PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &ARollbackDemoPawn::MoveLeftPressed);
    PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &ARollbackDemoPawn::MoveLeftReleased);
}

bool ARollbackDemoPawn::GetRollbackInput(FRollbackInput& OutInput)
{
    if (!IsPlayerControlled())
    {
        return false;
    }

    OutInput = FRollbackInput();
    OutInput.Axes = BoundInputAxes;

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (PC)
    {
        if (PC->IsInputKeyDown(EKeys::W)) OutInput.Axes.X += 1.f;
        if (PC->IsInputKeyDown(EKeys::S)) OutInput.Axes.X -= 1.f;
        if (PC->IsInputKeyDown(EKeys::D)) OutInput.Axes.Y += 1.f;
        if (PC->IsInputKeyDown(EKeys::A)) OutInput.Axes.Y -= 1.f;
    }

    OutInput.Axes.X = FMath::Clamp(OutInput.Axes.X, -1.f, 1.f);
    OutInput.Axes.Y = FMath::Clamp(OutInput.Axes.Y, -1.f, 1.f);
    OutInput.Axes.Z = 0.f;
    return true;
}

void ARollbackDemoPawn::SetColor(FLinearColor Color)
{
    UMaterialInstanceDynamic* MID = MeshComp->CreateAndSetMaterialInstanceDynamic(0);
    if (MID)
    {
        MID->SetVectorParameterValue("Color", Color);
    }
}

void ARollbackDemoPawn::HandleRollbackTick(float DeltaTime, int32 Frame, FRollbackInput Input)
{
    MoveComp->DeterministicMove(Input.Axes);
    SimulatedVelocity = Input.Axes; // Tracked via SaveGame property flag
}

void ARollbackDemoPawn::UpdateLocalInputFromController()
{
    if (!StateComp || !IsPlayerControlled())
    {
        return;
    }

    FRollbackInput LocalInput;
    if (!GetRollbackInput(LocalInput))
    {
        return;
    }

    StateComp->CurrentLocalInput = LocalInput;
}

void ARollbackDemoPawn::MoveForwardPressed()
{
    BoundInputAxes.X += 1.f;
}

void ARollbackDemoPawn::MoveForwardReleased()
{
    BoundInputAxes.X -= 1.f;
}

void ARollbackDemoPawn::MoveBackwardPressed()
{
    BoundInputAxes.X -= 1.f;
}

void ARollbackDemoPawn::MoveBackwardReleased()
{
    BoundInputAxes.X += 1.f;
}

void ARollbackDemoPawn::MoveRightPressed()
{
    BoundInputAxes.Y += 1.f;
}

void ARollbackDemoPawn::MoveRightReleased()
{
    BoundInputAxes.Y -= 1.f;
}

void ARollbackDemoPawn::MoveLeftPressed()
{
    BoundInputAxes.Y -= 1.f;
}

void ARollbackDemoPawn::MoveLeftReleased()
{
    BoundInputAxes.Y += 1.f;
}

// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RollbackStateComponent.h"
#include "RollbackDemoEnvironment.generated.h"

class UCameraComponent;
class UTextRenderComponent;
class UDirectionalLightComponent;
class ARollbackDemoPawn;
class URollbackManager;

UCLASS()
class ROLLBACKCORE_API ARollbackDemoEnvironment : public APawn
{
	GENERATED_BODY()

public:
	ARollbackDemoEnvironment();

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, Category = "Rollback Demo")
    UStaticMeshComponent* FloorComp;

    UPROPERTY(VisibleAnywhere, Category = "Rollback Demo")
    UCameraComponent* CameraComp;

    UPROPERTY(VisibleAnywhere, Category = "Rollback Demo")
    UTextRenderComponent* TextComp;

    UPROPERTY(VisibleAnywhere, Category = "Rollback Demo")
    UTextRenderComponent* MetricsTextComp;

    UPROPERTY(VisibleAnywhere, Category = "Rollback Demo")
    UTextRenderComponent* RollbackEventTextComp;

    UPROPERTY(VisibleAnywhere, Category = "Rollback Demo")
    UDirectionalLightComponent* LightComp;

    UPROPERTY()
    ARollbackDemoPawn* LocalPawn;

    UPROPERTY()
    ARollbackDemoPawn* RemotePawn;

    int32 SimulatedLatencyFrames = 15; // 250ms of artificial latency
    
    UPROPERTY()
    TMap<int32, FRollbackInput> P2TrueInputs;
    
    FRollbackInput LastReceivedP2Input;

private:
    void UpdateDemoHud(const URollbackManager* Manager);
    void DrawCorrectionMarkers() const;

    int32 LastShownRollbackCount = 0;
    int32 LastCorrectedFrame = -1;
    float CorrectionMarkerTimeRemaining = 0.0f;
    FVector LastPredictedRemoteLocation = FVector::ZeroVector;
    FVector LastCorrectedRemoteLocation = FVector::ZeroVector;
    float LastCorrectionDistance = 0.0f;
};

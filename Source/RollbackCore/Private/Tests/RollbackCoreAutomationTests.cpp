// Copyright (c) 2026 GregOrigin. MIT Licensed - see LICENSE for details.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"
#include "RollbackDemoPawn.h"
#include "RollbackManager.h"
#include "RollbackNetSubsystem.h"
#include "RollbackStateComponent.h"

namespace RollbackCore::Tests
{
    UWorld* CreateTestWorld()
    {
        const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), NAME_None, EUniqueObjectNameOptions::GloballyUnique);
        FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
        check(World);
        World->AddToRoot();
        WorldContext.SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        World->BeginPlay();
        return World;
    }

    void DestroyTestWorld(UWorld* World)
    {
        if (!World)
        {
            return;
        }

        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World);
        World->RemoveFromRoot();
        World->DestroyWorld(false);
    }

    ARollbackDemoPawn* SpawnDemoPawn(UWorld* World, const FVector& Location)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ARollbackDemoPawn* Pawn = World->SpawnActor<ARollbackDemoPawn>(Location, FRotator::ZeroRotator, SpawnParams);
        if (Pawn && !Pawn->HasActorBegunPlay())
        {
            Pawn->DispatchBeginPlay();
        }
        return Pawn;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollbackStateSaveRestoreTest, "RollbackCore.State.SaveRestore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRollbackStateSaveRestoreTest::RunTest(const FString& Parameters)
{
    UWorld* World = RollbackCore::Tests::CreateTestWorld();
    ON_SCOPE_EXIT { RollbackCore::Tests::DestroyTestWorld(World); };

    ARollbackDemoPawn* Pawn = RollbackCore::Tests::SpawnDemoPawn(World, FVector::ZeroVector);
    TestNotNull(TEXT("Demo pawn spawned"), Pawn);
    TestNotNull(TEXT("Demo pawn has rollback state component"), Pawn ? Pawn->StateComp : nullptr);
    if (!Pawn || !Pawn->StateComp)
    {
        return false;
    }

    FRollbackInput Input;
    Input.Axes = FVector(1.f, 0.f, 0.f);
    Pawn->StateComp->CurrentLocalInput = Input;
    Pawn->StateComp->RollbackTick(1.0f / 60.0f, 0);
    Pawn->StateComp->SaveRollbackState(0);

    const FVector SavedLocation = Pawn->GetActorLocation();
    const FVector SavedVelocity = Pawn->SimulatedVelocity;

    Pawn->SetActorLocation(FVector(500.f, 500.f, 500.f), false, nullptr, ETeleportType::TeleportPhysics);
    Pawn->SimulatedVelocity = FVector(-9.f, -9.f, -9.f);
    Pawn->StateComp->LoadRollbackState(0);

    TestTrue(TEXT("Transform restored from rollback state"), Pawn->GetActorLocation().Equals(SavedLocation, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("SaveGame velocity restored from rollback state"), Pawn->SimulatedVelocity.Equals(SavedVelocity, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("State component tracked at least one SaveGame property"), Pawn->StateComp->GetTrackedPropertyCount() > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollbackLateInputCorrectionTest, "RollbackCore.Rollback.LateInputCorrection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRollbackLateInputCorrectionTest::RunTest(const FString& Parameters)
{
    UWorld* World = RollbackCore::Tests::CreateTestWorld();
    ON_SCOPE_EXIT { RollbackCore::Tests::DestroyTestWorld(World); };

    URollbackManager* Manager = World->GetSubsystem<URollbackManager>();
    ARollbackDemoPawn* Pawn = RollbackCore::Tests::SpawnDemoPawn(World, FVector::ZeroVector);
    TestNotNull(TEXT("Rollback manager exists"), Manager);
    TestNotNull(TEXT("Demo pawn spawned"), Pawn);
    if (!Manager || !Pawn || !Pawn->StateComp)
    {
        return false;
    }

    for (int32 Frame = 0; Frame < 12; ++Frame)
    {
        Pawn->StateComp->CurrentLocalInput = FRollbackInput();
        Manager->AdvanceFrame();
    }

    const FVector PredictedLocation = Pawn->GetActorLocation();

    FRollbackInput LateInput;
    LateInput.Axes = FVector(0.f, 1.f, 0.f);
    Pawn->StateComp->InjectInputForFrame(4, LateInput);
    Manager->RollbackToFrame(4);

    TestEqual(TEXT("Rollback count incremented"), Manager->RollbackCount, 1);
    TestEqual(TEXT("Last rollback frame recorded"), Manager->LastRollbackFrame, 4);
    TestTrue(TEXT("Late input changed the resimulated location"), !Pawn->GetActorLocation().Equals(PredictedLocation, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Corrected input is stored"), Pawn->StateComp->GetInputForFrame(4) == LateInput);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollbackNetworkBufferApplyTest, "RollbackCore.Network.BufferApply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRollbackNetworkBufferApplyTest::RunTest(const FString& Parameters)
{
    UWorld* World = RollbackCore::Tests::CreateTestWorld();
    ON_SCOPE_EXIT { RollbackCore::Tests::DestroyTestWorld(World); };

    URollbackNetSubsystem* NetSubsystem = World->GetSubsystem<URollbackNetSubsystem>();
    ARollbackDemoPawn* Pawn = RollbackCore::Tests::SpawnDemoPawn(World, FVector::ZeroVector);
    TestNotNull(TEXT("Rollback net subsystem exists"), NetSubsystem);
    TestNotNull(TEXT("Demo pawn spawned"), Pawn);
    if (!NetSubsystem || !Pawn || !Pawn->StateComp)
    {
        return false;
    }

    Pawn->StateComp->InjectInputForFrame(8, FRollbackInput());

    FRollbackInput AuthoritativeInput;
    AuthoritativeInput.Axes = FVector(1.f, 0.f, 0.f);
    NetSubsystem->BufferRemoteInputForRollback(2, 8, AuthoritativeInput);

    bool bChanged = false;
    int32 EarliestChangedFrame = -1;
    NetSubsystem->ApplyBufferedInputsToState(Pawn->StateComp, 2, 8, 8, bChanged, EarliestChangedFrame);

    TestTrue(TEXT("Buffered authoritative input changed prediction"), bChanged);
    TestEqual(TEXT("Earliest changed frame"), EarliestChangedFrame, 8);
    TestTrue(TEXT("Authoritative input injected into state component"), Pawn->StateComp->GetInputForFrame(8) == AuthoritativeInput);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollbackDebugHistoryScrubTest, "RollbackCore.Debug.HistoryScrub", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRollbackDebugHistoryScrubTest::RunTest(const FString& Parameters)
{
    UWorld* World = RollbackCore::Tests::CreateTestWorld();
    ON_SCOPE_EXIT { RollbackCore::Tests::DestroyTestWorld(World); };

    URollbackManager* Manager = World->GetSubsystem<URollbackManager>();
    ARollbackDemoPawn* Pawn = RollbackCore::Tests::SpawnDemoPawn(World, FVector::ZeroVector);
    TestNotNull(TEXT("Rollback manager exists"), Manager);
    TestNotNull(TEXT("Demo pawn spawned"), Pawn);
    if (!Manager || !Pawn || !Pawn->StateComp)
    {
        return false;
    }

    for (int32 Frame = 0; Frame < 8; ++Frame)
    {
        FRollbackInput Input;
        Input.Axes = FVector(Frame % 2 == 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
        Pawn->StateComp->CurrentLocalInput = Input;
        Manager->AdvanceFrame();
    }

    const TArray<int32> AvailableFrames = Manager->GetAvailableDebugFrames();
    TestTrue(TEXT("Debug history contains saved frames"), AvailableFrames.Num() >= 8);
    TestEqual(TEXT("Oldest debug frame"), Manager->GetOldestAvailableDebugFrame(), 0);
    TestEqual(TEXT("Newest debug frame"), Manager->GetNewestAvailableDebugFrame(), 7);

    const TArray<FRollbackDebugFrameRecord> Records = Manager->GetDebugFrameRecords(3);
    TestTrue(TEXT("Scrub frame has records"), Records.Num() > 0);
    if (Records.Num() > 0)
    {
        TestEqual(TEXT("Record frame matches scrub frame"), Records[0].Frame, 3);
        TestTrue(TEXT("Record has a saved rollback state"), Records[0].bStateAvailable);
        TestTrue(TEXT("Record has a named entity"), !Records[0].EntityName.IsEmpty());
        TestTrue(TEXT("Record exposes saved byte count"), Records[0].SavedByteCount > 0);
        TestTrue(TEXT("Record exposes historical input"), FMath::IsNearlyEqual(Records[0].Input.Axes.X, -1.0));
    }

    TestTrue(TEXT("Exact scrub frame can be selected"), Manager->SetDebugScrubFrame(3));
    TestEqual(TEXT("Scrub frame recorded"), Manager->DebugScrubFrame, 3);
    TestTrue(TEXT("Scrub step can be applied"), Manager->StepDebugScrubFrame(2));
    TestEqual(TEXT("Scrub frame stepped"), Manager->DebugScrubFrame, 5);

    Manager->SetDebugScrubFollowLive(true);
    TestTrue(TEXT("Scrub can return to live follow mode"), Manager->bDebugScrubFollowsLive);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollbackUdpLoopbackSmokeTest, "RollbackCore.Network.UdpLoopbackSmoke", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRollbackUdpLoopbackSmokeTest::RunTest(const FString& Parameters)
{
    UWorld* WorldA = RollbackCore::Tests::CreateTestWorld();
    UWorld* WorldB = RollbackCore::Tests::CreateTestWorld();
    ON_SCOPE_EXIT
    {
        RollbackCore::Tests::DestroyTestWorld(WorldB);
        RollbackCore::Tests::DestroyTestWorld(WorldA);
    };

    URollbackNetSubsystem* NetA = WorldA->GetSubsystem<URollbackNetSubsystem>();
    URollbackNetSubsystem* NetB = WorldB->GetSubsystem<URollbackNetSubsystem>();
    TestNotNull(TEXT("Rollback net subsystem A exists"), NetA);
    TestNotNull(TEXT("Rollback net subsystem B exists"), NetB);
    if (!NetA || !NetB)
    {
        return false;
    }

    const int32 BasePort = 23000 + FMath::RandRange(0, 2000);
    FRollbackTransportConfig ConfigA;
    ConfigA.LocalPort = BasePort;
    ConfigA.InputRedundancyFrames = 2;

    FRollbackTransportConfig ConfigB;
    ConfigB.LocalPort = BasePort + 1;
    ConfigB.InputRedundancyFrames = 2;

    FString Error;
    if (!TestTrue(TEXT("Peer B transport starts"), NetB->StartUdpPeer(ConfigB, Error)))
    {
        AddError(FString::Printf(TEXT("Peer B transport start error: %s"), *Error));
        return false;
    }
    Error.Reset();
    if (!TestTrue(TEXT("Peer A transport starts"), NetA->StartUdpPeer(ConfigA, Error)))
    {
        AddError(FString::Printf(TEXT("Peer A transport start error: %s"), *Error));
        return false;
    }
    Error.Reset();
    if (!TestTrue(TEXT("Peer B connects to peer A"), NetB->ConnectToPeer(1, TEXT("127.0.0.1"), BasePort, Error)))
    {
        AddError(FString::Printf(TEXT("Peer B connect error: %s"), *Error));
        return false;
    }
    Error.Reset();
    if (!TestTrue(TEXT("Peer A connects to peer B"), NetA->ConnectToPeer(2, TEXT("127.0.0.1"), BasePort + 1, Error)))
    {
        AddError(FString::Printf(TEXT("Peer A connect error: %s"), *Error));
        return false;
    }

    FRollbackInput SentInput;
    SentInput.Axes = FVector(0.f, -1.f, 0.f);
    TestTrue(TEXT("Peer A input send succeeds"), NetA->SendInputFrame(2, 14, SentInput, true));

    FRollbackInput ReceivedInput;
    for (int32 Attempt = 0; Attempt < 30 && !NetB->ConsumeRemoteInput(2, 14, ReceivedInput); ++Attempt)
    {
        FPlatformProcess::Sleep(0.01f);
        NetA->FlushTransport();
        NetB->FlushTransport();
    }

    const FRollbackTransportStats StatsA = NetA->GetTransportStats();
    const FRollbackTransportStats StatsB = NetB->GetTransportStats();
    if (!NetB->ConsumeRemoteInput(2, 14, ReceivedInput))
    {
        AddError(FString::Printf(
            TEXT("UDP loopback diagnostics: A local=%s remote=%s sent=%d recv=%d lastSent=%d lastRecv=%d pending=%d; B local=%s remote=%s sent=%d recv=%d lastSent=%d lastRecv=%d pending=%d"),
            *NetA->GetLocalEndpointString(),
            *NetA->GetRemoteEndpointString(),
            StatsA.PacketsSent,
            StatsA.PacketsReceived,
            StatsA.LastSentSequence,
            StatsA.LastReceivedSequence,
            StatsA.PendingReliablePackets,
            *NetB->GetLocalEndpointString(),
            *NetB->GetRemoteEndpointString(),
            StatsB.PacketsSent,
            StatsB.PacketsReceived,
            StatsB.LastSentSequence,
            StatsB.LastReceivedSequence,
            StatsB.PendingReliablePackets));
    }

    TestTrue(TEXT("Peer B received UDP input"), NetB->ConsumeRemoteInput(2, 14, ReceivedInput));
    TestTrue(TEXT("Peer B input matches payload"), ReceivedInput == SentInput);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollbackMultiPeerConnectTest, "RollbackCore.Network.MultiPeerConnect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRollbackMultiPeerConnectTest::RunTest(const FString& Parameters)
{
    UWorld* WorldHost = RollbackCore::Tests::CreateTestWorld();
    UWorld* WorldP2 = RollbackCore::Tests::CreateTestWorld();
    UWorld* WorldP3 = RollbackCore::Tests::CreateTestWorld();
    ON_SCOPE_EXIT
    {
        RollbackCore::Tests::DestroyTestWorld(WorldP3);
        RollbackCore::Tests::DestroyTestWorld(WorldP2);
        RollbackCore::Tests::DestroyTestWorld(WorldHost);
    };

    URollbackNetSubsystem* HostNet = WorldHost->GetSubsystem<URollbackNetSubsystem>();
    URollbackNetSubsystem* P2Net = WorldP2->GetSubsystem<URollbackNetSubsystem>();
    URollbackNetSubsystem* P3Net = WorldP3->GetSubsystem<URollbackNetSubsystem>();
    TestNotNull(TEXT("Host net exists"), HostNet);
    TestNotNull(TEXT("P2 net exists"), P2Net);
    TestNotNull(TEXT("P3 net exists"), P3Net);
    if (!HostNet || !P2Net || !P3Net)
    {
        return false;
    }

    const int32 BasePort = 26000 + FMath::RandRange(0, 1000);

    FRollbackTransportConfig HostConfig;
    HostConfig.LocalPort = BasePort;
    HostConfig.MaxPeers = 4;

    FRollbackTransportConfig P2Config;
    P2Config.LocalPort = BasePort + 1;
    P2Config.MaxPeers = 4;

    FRollbackTransportConfig P3Config;
    P3Config.LocalPort = BasePort + 2;
    P3Config.MaxPeers = 4;

    FString Error;
    TestTrue(TEXT("Host transport starts"), HostNet->StartUdpPeer(HostConfig, Error));
    Error.Reset();
    TestTrue(TEXT("P2 transport starts"), P2Net->StartUdpPeer(P2Config, Error));
    Error.Reset();
    TestTrue(TEXT("P3 transport starts"), P3Net->StartUdpPeer(P3Config, Error));
    Error.Reset();

    TestTrue(TEXT("P2 connects to host"), P2Net->ConnectToPeer(1, TEXT("127.0.0.1"), BasePort, Error));
    Error.Reset();
    TestTrue(TEXT("P3 connects to host"), P3Net->ConnectToPeer(1, TEXT("127.0.0.1"), BasePort, Error));
    Error.Reset();
    TestTrue(TEXT("Host connects to P2"), HostNet->ConnectToPeer(2, TEXT("127.0.0.1"), BasePort + 1, Error));
    Error.Reset();
    TestTrue(TEXT("Host connects to P3"), HostNet->ConnectToPeer(3, TEXT("127.0.0.1"), BasePort + 2, Error));

    TestEqual(TEXT("Host has 2 connected peers"), HostNet->GetConnectedPeerIds().Num(), 2);

    FRollbackInput InputFromP2;
    InputFromP2.Axes = FVector(1.f, 2.f, 3.f);
    P2Net->SendInputFrame(2, 5, InputFromP2, true);

    FRollbackInput InputFromP3;
    InputFromP3.Axes = FVector(4.f, 5.f, 6.f);
    P3Net->SendInputFrame(3, 5, InputFromP3, true);

    for (int32 Attempt = 0; Attempt < 30; ++Attempt)
    {
        HostNet->FlushTransport();
        P2Net->FlushTransport();
        P3Net->FlushTransport();
        FPlatformProcess::Sleep(0.01f);
    }

    FRollbackInput ReceivedP2, ReceivedP3;
    const bool bGotP2 = HostNet->ConsumeRemoteInput(2, 5, ReceivedP2);
    const bool bGotP3 = HostNet->ConsumeRemoteInput(3, 5, ReceivedP3);

    TestTrue(TEXT("Host received P2 input"), bGotP2);
    TestTrue(TEXT("Host received P3 input"), bGotP3);
    if (bGotP2)
    {
        TestTrue(TEXT("P2 input matches"), ReceivedP2 == InputFromP2);
    }
    if (bGotP3)
    {
        TestTrue(TEXT("P3 input matches"), ReceivedP3 == InputFromP3);
    }

    HostNet->DisconnectPeer(3);
    TestEqual(TEXT("Host has 1 peer after disconnect"), HostNet->GetConnectedPeerIds().Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollbackPeerDisconnectTest, "RollbackCore.Network.PeerDisconnect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRollbackPeerDisconnectTest::RunTest(const FString& Parameters)
{
    UWorld* WorldA = RollbackCore::Tests::CreateTestWorld();
    UWorld* WorldB = RollbackCore::Tests::CreateTestWorld();
    ON_SCOPE_EXIT
    {
        RollbackCore::Tests::DestroyTestWorld(WorldB);
        RollbackCore::Tests::DestroyTestWorld(WorldA);
    };

    URollbackNetSubsystem* NetA = WorldA->GetSubsystem<URollbackNetSubsystem>();
    URollbackNetSubsystem* NetB = WorldB->GetSubsystem<URollbackNetSubsystem>();
    if (!NetA || !NetB)
    {
        return false;
    }

    const int32 BasePort = 27000 + FMath::RandRange(0, 1000);
    FRollbackTransportConfig ConfigA;
    ConfigA.LocalPort = BasePort;
    ConfigA.PeerTimeoutSeconds = 1.0f;
    ConfigA.HeartbeatIntervalSeconds = 0.2f;

    FRollbackTransportConfig ConfigB;
    ConfigB.LocalPort = BasePort + 1;

    FString Error;
    TestTrue(TEXT("A transport starts"), NetA->StartUdpPeer(ConfigA, Error));
    Error.Reset();
    TestTrue(TEXT("B transport starts"), NetB->StartUdpPeer(ConfigB, Error));
    Error.Reset();
    TestTrue(TEXT("B connects to A"), NetB->ConnectToPeer(1, TEXT("127.0.0.1"), BasePort, Error));

    TestTrue(TEXT("A is connected after B joins"), NetA->IsConnectedToPeer());

    NetB->StopTransport();

    for (int32 i = 0; i < 120 && NetA->IsConnectedToPeer(); ++i)
    {
        NetA->FlushTransport();
        FPlatformProcess::Sleep(0.025f);
    }

    TestFalse(TEXT("A detects B timeout after stop"), NetA->IsConnectedToPeer());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRollbackPerformanceStatsTest, "RollbackCore.Performance.Stats", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRollbackPerformanceStatsTest::RunTest(const FString& Parameters)
{
    UWorld* World = RollbackCore::Tests::CreateTestWorld();
    ON_SCOPE_EXIT { RollbackCore::Tests::DestroyTestWorld(World); };

    URollbackManager* Manager = World->GetSubsystem<URollbackManager>();
    URollbackNetSubsystem* NetSubsystem = World->GetSubsystem<URollbackNetSubsystem>();
    ARollbackDemoPawn* Pawn = RollbackCore::Tests::SpawnDemoPawn(World, FVector::ZeroVector);
    TestNotNull(TEXT("Manager exists"), Manager);
    TestNotNull(TEXT("Net subsystem exists"), NetSubsystem);
    if (!Manager || !NetSubsystem || !Pawn || !Pawn->StateComp)
    {
        return false;
    }

    for (int32 Frame = 0; Frame < 30; ++Frame)
    {
        FRollbackInput Input;
        Input.Axes = FVector(1.f, 0.f, 0.f);
        Pawn->StateComp->CurrentLocalInput = Input;
        Manager->AdvanceFrame();
    }

    const FRollbackPerformanceStats Perf = NetSubsystem->GetPerformanceStats();

    TestTrue(TEXT("Frames simulated > 0"), Perf.FramesSimulated > 0);
    TestTrue(TEXT("Avg simulation time >= 0"), Perf.AvgSimulationTimeMs >= 0.0f);
    TestTrue(TEXT("Avg state snapshot bytes >= 0"), Perf.AvgStateSnapshotBytes >= 0.0f);
    TestTrue(TEXT("Registered entity count is 1"), Perf.RegisteredEntityCount == 1);

    Pawn->StateComp->InjectInputForFrame(5, FRollbackInput());
    Manager->RollbackToFrame(5, 10);

    const FRollbackPerformanceStats PerfAfterRollback = NetSubsystem->GetPerformanceStats();
    TestTrue(TEXT("Rollback recorded"), PerfAfterRollback.TotalRollbackCount > 0);
    TestTrue(TEXT("Max rollback depth tracked"), PerfAfterRollback.MaxRollbackDepthFrames > 0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

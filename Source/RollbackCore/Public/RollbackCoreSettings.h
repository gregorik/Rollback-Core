// Copyright (c) 2026 GregOrigin. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RollbackCoreSettings.generated.h"

UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "Rollback Core"))
class ROLLBACKCORE_API URollbackCoreSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    URollbackCoreSettings();

    UPROPERTY(EditAnywhere, Config, Category = "Simulation",
        meta = (ClampMin = "15", ClampMax = "240", DisplayName = "Fixed Tick Rate (Hz)",
                ToolTip = "Deterministic simulation rate. 60 is standard for fighting games. Must match across all peers."))
    int32 FixedTickRateHz = 60;

    UPROPERTY(EditAnywhere, Config, Category = "Simulation",
        meta = (ClampMin = "0", ClampMax = "240", DisplayName = "Max Rollback Depth (frames)",
                ToolTip = "Soft target for the deepest rollback the simulation will perform. Larger values tolerate worse latency at the cost of CPU."))
    int32 MaxRollbackDepthFrames = 12;

    UPROPERTY(EditAnywhere, Config, Category = "Networking",
        meta = (ClampMin = "1024", ClampMax = "65535", DisplayName = "Default Local Port"))
    int32 DefaultLocalPort = 7777;

    UPROPERTY(EditAnywhere, Config, Category = "Networking",
        meta = (ClampMin = "0", ClampMax = "32", DisplayName = "Default Input Redundancy (frames)",
                ToolTip = "Number of past input frames each packet repeats. Higher values recover from packet loss faster at the cost of bandwidth."))
    int32 DefaultInputRedundancyFrames = 8;

    UPROPERTY(EditAnywhere, Config, Category = "Networking",
        meta = (ClampMin = "1", ClampMax = "8", DisplayName = "Default Max Peers"))
    int32 DefaultMaxPeers = 4;

    UPROPERTY(EditAnywhere, Config, Category = "Networking",
        meta = (ClampMin = "0.01", ClampMax = "2.0", DisplayName = "Reliable Resend After (seconds)"))
    float DefaultResendAfterSeconds = 0.08f;

    UPROPERTY(EditAnywhere, Config, Category = "Networking|Reliability",
        meta = (ClampMin = "0.1", ClampMax = "30.0", DisplayName = "Heartbeat Interval (seconds)"))
    float DefaultHeartbeatIntervalSeconds = 2.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Networking|Reliability",
        meta = (ClampMin = "1.0", ClampMax = "120.0", DisplayName = "Peer Timeout (seconds)"))
    float DefaultPeerTimeoutSeconds = 10.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Debug",
        meta = (DisplayName = "Enable Visual Debugging by Default",
                ToolTip = "When true, URollbackManager renders correction trails, ghost states, and rollback markers in the editor."))
    bool bEnableVisualDebuggingByDefault = true;

    UPROPERTY(EditAnywhere, Config, Category = "Debug",
        meta = (ClampMin = "0", ClampMax = "60", DisplayName = "Debug Live Frame Lag",
                ToolTip = "Frames the visual debugger trails behind the live frame so corrections are visible."))
    int32 DebugLiveFrameLag = 5;

    UPROPERTY(EditAnywhere, Config, Category = "Debug",
        meta = (DisplayName = "Auto-Open Stats Panel in PIE",
                ToolTip = "If true, the Rollback Stats panel opens automatically the first time you press Play in Editor each session."))
    bool bAutoOpenStatsPanelInPIE = false;

    UPROPERTY(EditAnywhere, Config, Category = "Onboarding",
        meta = (DisplayName = "Show Setup Wizard On Startup",
                ToolTip = "If true, the Rollback Setup Wizard opens automatically once when the project loads. Uncheck the 'Don't show again' option in the wizard to clear this."))
    bool bShowSetupWizardOnStartup = true;
};

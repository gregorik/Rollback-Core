# Rollback Core

Open-source deterministic rollback netcode for Unreal Engine 5.

Rollback Core is a runtime plugin that gives any UE5 project a fighting-game-style rollback simulation: a fixed-step deterministic tick, automatic per-frame state capture, UDP input transport with redundancy and reliable ACKs, and a one-call rollback-to-frame API. It is built around `SaveGame`-flagged property reflection, so any property you mark `SaveGame` is automatically snapshotted and restored across rollbacks — no manual serialization code per actor.

This is the open-source distribution. A commercial superset, [Rollback Core Pro](#related), adds a visual frame-scrubber debugger, desync inspection tooling, OnlineSubsystem matchmaking, multi-engine-version packages, and additional demos.

## Features

- **Deterministic fixed-step tick** via `URollbackManager` (default 60 Hz, configurable in Project Settings).
- **Auto-state saving** through `URollbackStateComponent`: any `UPROPERTY(SaveGame)` is captured and restored automatically.
- **Rollback API:** `RollbackToFrame(int32 Frame)` re-simulates from any historical frame using buffered inputs.
- **UDP transport** with input redundancy (configurable per-packet replay frames), reliable ACK/resend, heartbeats, and peer-timeout detection.
- **Multi-peer support** up to 8 peers.
- **Simulated packet loss + latency** for local testing without leaving the editor.
- **Blueprint surface** via `URollbackNetworkBlueprintLibrary` for projects that prefer BP wiring.
- **Performance instrumentation** (`FRollbackPerformanceStats`) — avg/max sim & rollback times, snapshot sizes, bandwidth.
- **Basic top-down demo** (`RC_BasicDemo.umap`) showing two pawns with artificial latency and live rollback corrections.
- **Console commands** for hosting, connecting, and inspecting state without writing any Blueprint glue.

## Installation

Clone (or submodule) into your project's `Plugins/` directory:

```sh
cd YourProject/Plugins
git clone https://github.com/GregOrigin/RollbackCore.git
```

Then regenerate project files and rebuild. The plugin is enabled automatically by Unreal once present.

## Quick start (C++)

```cpp
// Host
FRollbackTransportConfig Config;
Config.LocalPort = 7777;
FString Error;
GetWorld()->GetSubsystem<URollbackNetSubsystem>()->StartUdpPeer(Config, Error);

// Client
Config.LocalPort = 7778;
Config.RemoteHost = TEXT("127.0.0.1");
Config.RemotePort = 7777;
GetWorld()->GetSubsystem<URollbackNetSubsystem>()->StartUdpPeer(Config, Error);
```

## Quick start (console)

```
Rollback.NetHost 7777
Rollback.NetClient 127.0.0.1 7777 7778
Rollback.NetPeers
Rollback.Perf
```

## Marking gameplay state for rollback

Inherit `IRollbackInputProvider` on the pawn you want to drive, and add an `URollbackStateComponent`. Any `UPROPERTY` flagged `SaveGame` on the owning actor is captured automatically:

```cpp
UCLASS()
class AMyFighter : public APawn, public IRollbackInputProvider
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere) URollbackStateComponent* StateComp;

    // Rolled back automatically because of the SaveGame flag
    UPROPERTY(SaveGame) FVector Velocity;
    UPROPERTY(SaveGame) int32 Health;
    UPROPERTY(SaveGame) int32 HitstunFramesRemaining;

    virtual bool GetRollbackInput(FRollbackInput& OutInput) override;
};
```

## Project Settings

`Project Settings → Plugins → Rollback Core` exposes:
- Fixed tick rate (15-240 Hz)
- Max rollback depth (frames)
- Default UDP port, input redundancy, max peers
- Reliable resend interval, heartbeat interval, peer timeout

## Tests

Automation tests live in `Source/RollbackCore/Private/Tests/` under `WITH_DEV_AUTOMATION_TESTS`. Run from a commandlet:

```
UnrealEditor-Cmd.exe YourProject.uproject -ExecCmds="Automation RunTests RollbackCore; Quit" -unattended -nop4 -nosplash -nullrhi
```

## Determinism: what you must and must not do

Rollback only works when the same frame range, replayed from the same inputs, produces the same state. The plugin enforces this for its own snapshot/replay path; you are responsible for keeping gameplay code deterministic. Do not put inside a rollback-authoritative actor:

- `CharacterMovementComponent`, Mover, or Chaos physics on the rolled-back actor
- Latent Blueprint actions, timelines, or `FTimerManager` callbacks
- `FMath::Rand*` without seeding from a frame-derived seed
- Wall-clock time (`FDateTime::Now`, `FPlatformTime::Seconds`) as gameplay input

See `Docs/network-prediction-mover.md` for how to combine Rollback Core with Unreal's Network Prediction or Mover plugins.

## Related

- **Rollback Core Pro** — commercial superset on the Unreal Fab marketplace with a visual frame-scrubber debugger, desync inspection, OnlineSubsystem (LAN/EOS/Steam) matchmaking, multi-engine-version support (5.4–5.7), and additional demos (network packet-loss simulator, 2D viking fighter).

## License

MIT. See [LICENSE](LICENSE).

## Author

[GregOrigin](https://github.com/GregOrigin)

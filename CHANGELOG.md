# Changelog

All notable changes to Rollback Core (OSS) are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-09-04

### Fixed & Updated

- **Determinism & Build**: Added `FPSemanticsMode.Precise` in `RollbackCore.Build.cs` to enforce bit-identical float semantics across compiler optimization levels and platforms.
- **Entity Lifecycle & Safety**: Implemented `URollbackStateComponent::EndPlay` and entity validation in `URollbackManager`, safely unregistering destroyed entities and preventing crashes or dangling pointers.
- **Desync Detection**: Corrected the desync check in `URollbackManager::RollbackToFrame` to only verify frames before `EarliestMismatchFrame`, eliminating false-positive desync notifications on valid rollback corrections.
- **Simulation Stability**: Added spiral-of-death accumulator limiting in `URollbackManager::Tick` and world pause checking in `TickFunction`. Added `MaxRollbackDepthFrames` bound enforcement.
- **State Serialization**: Added deterministic alphabetical property sorting by `FName` to guarantee matching reflection schemas across machines. Filtered out transient and pointer/delegate types.
- **State Buffer Management**: Replaced fragile single-frame removals with full ring buffer pruning in `TrimStateBuffer` and `TrimInputBuffer`. Added `ResetBuffers()`.
- **Physics & Movement Restore**: Updated `LoadRollbackState` to use `ETeleportType::TeleportPhysics`, restore linear velocity on physics-simulating root components, expose `LastRestoredVelocity`, and broadcast `OnRollbackStateLoadedDelegate`.
- **Deterministic Movement**: Updated `URollbackMovementComponent::DeterministicMove` to dynamically pull the simulation timestep from `URollbackManager`. Added `DeterministicMoveForStep(InputVector, FixedDeltaTime)`, `bSweepForCollision`, and NaN/INF sanitization.
- **Input Sanitization**: Added `FRollbackInput::QuantizeAxes()` with NaN fallback and [-1.0, 1.0] clamping.
- **Transport Subsystem**:
  - Fixed unsigned integer underflow and memory leak in `MarkSequenceReceived`.
  - Added `InitialSentSeconds` to pending reliable packets for accurate RTT tracking across retries.
  - Replaced single-frame removals in `LocalInputHistory` and `RemoteInputBuffer` with iterator-based window pruning.
  - Accurately counted active connected peers in performance telemetry.
- **Demo Environment**: Fixed `RollbackDemoEnvironment` passing mismatch frame index to `RollbackToFrame`, pruned `P2TrueInputs` properly, and updated `RollbackDemoPawn` to use `DeterministicMoveForStep`.
- **Settings Cleanup**: Removed unused Pro-only setting properties from `URollbackCoreSettings`.
- **Automation Tests**: Added `RollbackCore.State.EntityLifecycle`, `RollbackCore.Movement.DeterministicStep`, and `RollbackCore.Input.Quantization` tests.

## [1.0.0] - 2026-05-17

Initial open-source release. Derived from the Rollback Core Pro commercial
plugin; this OSS distribution contains the core rollback simulation, UDP
transport, and basic top-down demo. Pro-only features (visual frame-scrubber
debugger, desync inspection tooling, OnlineSubsystem matchmaking,
network-packet-loss demo, 2D viking fighter demo, multi-engine-version
packages) are not included.

### Runtime

- `URollbackManager` — deterministic fixed-step simulation loop with
  `AdvanceFrame()` / `RollbackToFrame()` and a per-frame debug history API.
- `URollbackStateComponent` — automatic per-frame snapshot/restore of any
  `UPROPERTY(SaveGame)` on the owning actor, plus transform & velocity.
- `URollbackMovementComponent` — deterministic axis-input integration.
- `URollbackNetSubsystem` — UDP transport with input redundancy, reliable
  ACK/resend, heartbeats, peer-timeout detection, multi-peer (up to 8),
  simulated packet loss + latency.
- `URollbackNetworkBlueprintLibrary` — Blueprint surface for the transport
  and rollback APIs.
- `URollbackCoreSettings` (`UDeveloperSettings`) — Project Settings entries
  for fixed tick rate, max rollback depth, default UDP port, input
  redundancy, max peers, resend/heartbeat/timeout intervals.
- Performance instrumentation (`FRollbackPerformanceStats`) — avg/max sim
  and rollback times, snapshot sizes, bandwidth, rollbacks-per-second.
- Console commands: `Rollback.NetHost`, `Rollback.NetClient`,
  `Rollback.NetConnect`, `Rollback.NetDisconnect`, `Rollback.NetPeers`,
  `Rollback.Perf`.

### Demo

- `RC_BasicDemo.umap` — two pawns with artificial latency and live
  rollback corrections.

### Tests

- Automation tests under `RollbackCore.*` for state save/restore, late
  input correction, network buffer apply, debug history scrub, UDP
  loopback smoke, multi-peer connect, peer disconnect detection, and
  performance stats.

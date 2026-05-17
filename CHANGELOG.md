# Changelog

All notable changes to Rollback Core (OSS) are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

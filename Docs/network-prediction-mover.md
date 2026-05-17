# Network Prediction and Mover Guidance

Rollback Core is a standalone deterministic rollback layer. It does not wrap Unreal's Network Prediction plugin or Mover. The plugin owns its own fixed-step simulation loop, input history, state snapshots, UDP input transport, and rollback replay path.

Use Rollback Core when the gameplay state is compact and deterministic:

- Fighting game actors
- Platform fighter actors
- Deterministic projectiles
- Hit windows and cooldowns
- Small replicated simulation objects
- Prototype rollback feel tests

Use Network Prediction or Mover when the project needs Unreal-native predicted movement, server-authoritative movement models, or direct alignment with Epic's movement stack.

Do not place `CharacterMovementComponent`, Mover, Chaos physics, latent Blueprint actions, timelines, or unseeded random gameplay inside rollback authority unless every mutated value can be snapshotted and replayed deterministically.

Safe integration patterns:

1. Deterministic core plus visual shell: Rollback Core simulates compact gameplay state. Meshes, animation, VFX, audio, and camera effects follow the corrected state after rollback.
2. Separated systems: Mover or Network Prediction owns locomotion. Rollback Core owns deterministic combat, projectile, hit-validation, or frame-data logic.
3. Prototype migration: use Rollback Core to validate rollback feel, then migrate production movement to Network Prediction or Mover if the project needs that stack.

A mixed architecture is safe only when resimulating the same frame range from the same input history produces the same gameplay state under packet loss, latency, and repeated rollback.

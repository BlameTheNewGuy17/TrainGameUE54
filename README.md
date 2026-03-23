# TrainGame UE5 Rail Network Plugin

A reusable rail network plugin for Unreal Engine 5. Provides a data-driven rail graph, editor tools for track placement, movement along rails, and multiplayer replication support.

---

## Overview

The plugin is built in two layers:

**Core Graph (`FNetworkGraph`)** — Plain C++ class. Owns node/edge connectivity, transforms, and pathfinding. Designed to be reusable for any network type (rail, road, etc.).

**Rail Network Subsystem (`URailNetworkSubsystem`)** — UWorldSubsystem. Owns all rail-specific logic on top of the graph: tangent resolution, topology validation, curve radius checks, movement, blocks, signals, save/load, and replication helpers.

### Key design decisions
- The subsystem is the sole authority on topology. Editor tools and gameplay code pass *intent* — the subsystem validates and executes.
- Node types (Control, Switch, Crossover) are derived automatically from edge topology. You don't set them manually.
- Multiplayer state is managed via two components attached to GameState and PlayerController. The subsystem itself does not replicate.
- Save/load uses UE's built-in `USaveGame` system. All rail data is serialized automatically via `UPROPERTY(SaveGame)`.

---

## Architecture

```
FNetworkGraph                    — connectivity + transforms (plain C++)
URailNetworkSubsystem            — rail logic, validation, movement (UWorldSubsystem)
UTrainNetworkStateComponent      — server → all clients replication (on GameState)
UTrainNetworkPlayerComponent     — client → server RPCs (on PlayerController)
ATrainNetworkGameMode            — triggers late-join snapshot push (optional base class)
```

### Node Types
Nodes are typed automatically based on their connected edges:

| Edges | Topology | Type |
|-------|----------|------|
| 0 | — | Pruned automatically |
| 1 | Dead end | Control |
| 2 | Opposing tangents (through-route) | Control |
| 2 | Same-direction tangents (branch) | Switch |
| 3+ | Single angle family | Switch |
| 3+ | Multiple angle families | Crossover |

Type is locked once set, until edges are removed and the topology changes.

---

## Plugin Contents

| File | Purpose |
|------|---------|
| `RailNetworkTypes.h` | All structs, enums, and ID types |
| `RailMath.h` | Hermite curve math (position, tangent, second derivative) |
| `NetworkGraph.h/.cpp` | Generic graph — connectivity, pathfinding |
| `RailNetworkSubsystem.h/.cpp` | Core rail logic |
| `RailNetworkSaveGame.h` | SaveGame container |
| `TrainNetworkStateComponent.h/.cpp` | GameState replication component |
| `TrainNetworkPlayerComponent.h/.cpp` | PlayerController replication component |
| `TrainNetworkGameMode.h/.cpp` | Optional GameMode base class |
| `BuildTrackTool.h/.cpp` | Editor tool — place and remove track |
| `ModifyTrackTool.h/.cpp` | Editor tool — modify existing track |

---

## Singleplayer vs Multiplayer

**Singleplayer** — You only need the subsystem. No replication components required. The editor tools work out of the box.

**Multiplayer** — You need both replication components and a GameMode that calls `PushSnapshotToClient` on player join. See [PLUGIN_SETUP.md](PLUGIN_SETUP.md) for full setup instructions.

---

## Movement

Train movement is handled via `AdvanceAlongRails` on the subsystem. Pass a `FRailLocation` (edge + distance + direction) and a delta distance, get back a new `FRailLocation`. Switch routing is handled automatically via `SelectNextEdge`.

```cpp
FRailMoveContext Ctx;
FRailTravelResult Result = RailNetwork->AdvanceAlongRails(CurrentLocation, DeltaS, Ctx);
FTransform NewTransform = RailNetwork->GetTransformAtDistance(Result.Edge, Result.S);
```

---

## Save / Load

Save and load are exposed as `CallInEditor` functions on the subsystem, and as `BlueprintCallable` for runtime use.

```cpp
RailNetwork->SaveNetwork(TEXT("SlotName"));
RailNetwork->LoadNetwork(TEXT("SlotName"));
```

Derived values (edge length, speed limit) are recomputed on load. Blocks and signals are saved but their runtime state (occupied, aspect) is transient and resets on load.

---

## Known Limitations / To Do

- Curve radius check uses geometric circle fitting (three-point circumradius). The minimum radius is configurable via `MinCurveRadiusCm` on the subsystem.
- `TrainSimulationSubsystem` (frame-by-frame train movement replication) is not yet implemented.
- Block and signal systems are stubbed — data structures exist but gameplay logic is not implemented.
- Plugin rename pending.

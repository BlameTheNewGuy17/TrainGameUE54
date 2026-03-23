# Plugin Setup Guide

This guide covers how to integrate the TrainGame Rail Network Plugin into your project. Both C++ and Blueprint workflows are covered.

---

## Singleplayer Setup

For singleplayer you only need the subsystem, which is registered automatically as a `UWorldSubsystem`. No additional setup is required — just start using it.

```cpp
URailNetworkSubsystem* RailNetwork = GetWorld()->GetSubsystem<URailNetworkSubsystem>();
```

In Blueprint, use the **Get World Subsystem** node with class set to `RailNetworkSubsystem`.

---

## Multiplayer Setup

Multiplayer requires three things:
1. `UTrainNetworkStateComponent` on your GameState
2. `UTrainNetworkPlayerComponent` on your PlayerController
3. A GameMode that calls `PushSnapshotToClient` when a player joins

---

### Option A — C++

#### 1. GameState

Add the component to your GameState constructor:

```cpp
// MyGameState.h
#include "TrainNetworkStateComponent.h"

UCLASS()
class AMyGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    AMyGameState();

    UPROPERTY(VisibleAnywhere) UTrainNetworkStateComponent* RailNetworkState;
};
```

```cpp
// MyGameState.cpp
AMyGameState::AMyGameState()
{
    RailNetworkState = CreateDefaultSubobject<UTrainNetworkStateComponent>(TEXT("RailNetworkState"));
}
```

#### 2. PlayerController

```cpp
// MyPlayerController.h
#include "TrainNetworkPlayerComponent.h"

UCLASS()
class AMyPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AMyPlayerController();

    UPROPERTY(VisibleAnywhere) UTrainNetworkPlayerComponent* RailNetworkPlayer;
};
```

```cpp
// MyPlayerController.cpp
AMyPlayerController::AMyPlayerController()
{
    RailNetworkPlayer = CreateDefaultSubobject<UTrainNetworkPlayerComponent>(TEXT("RailNetworkPlayer"));
}
```

#### 3. GameMode

Either inherit from `ATrainNetworkGameMode` directly:

```cpp
UCLASS()
class AMyGameMode : public ATrainNetworkGameMode
{
    GENERATED_BODY()
};
```

Or if you have an existing GameMode, add this to your `PostLogin` override:

```cpp
void AMyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    AGameStateBase* GameState = GetGameState<AGameStateBase>();
    if (!GameState) return;

    UTrainNetworkStateComponent* StateComp = GameState->FindComponentByClass<UTrainNetworkStateComponent>();
    if (StateComp)
    {
        StateComp->PushSnapshotToClient(NewPlayer);
    }
}
```

#### 4. Register your classes

In your GameMode or project settings, set:
- Game State Class → `MyGameState`
- Player Controller Class → `MyPlayerController`
- Game Mode Class → `MyGameMode`

---

### Option B — Blueprint

#### 1. GameState

- Create a Blueprint subclass of `GameStateBase` (or your existing GameState)
- In the Components panel, click **Add** and search for `TrainNetworkStateComponent`
- Add it to the actor

#### 2. PlayerController

- Create a Blueprint subclass of `PlayerController` (or your existing one)
- In the Components panel, click **Add** and search for `TrainNetworkPlayerComponent`
- Add it to the actor

#### 3. GameMode

- Open your GameMode Blueprint
- Override the `Post Login` event
- Call the parent (`Super`) first
- From the `New Player` pin, **Get Player Controller** → **Get Component by Class** with class set to `TrainNetworkStateComponent`

Wait — `PostLogin` gives you the new `APlayerController` directly. From it, get the GameState component:

```
Event Post Login
→ Call Parent
→ Get Game State (cast to your GameState)
→ Get TrainNetworkStateComponent
→ Push Snapshot To Client (pass New Player as the PC pin)
```

#### 4. Register your classes

In Project Settings → Maps & Modes, or in your GameMode Blueprint defaults, set:
- Game State Class → your GameState Blueprint
- Player Controller Class → your PlayerController Blueprint

---

## Placing Track at Runtime (Multiplayer)

In multiplayer, clients should never call the subsystem directly. Route all requests through the player component:

**C++:**
```cpp
UTrainNetworkPlayerComponent* PlayerComp = GetComponentByClass<UTrainNetworkPlayerComponent>();
if (PlayerComp)
{
    FEdgePlacementRequest Request;
    // ... fill out request ...
    PlayerComp->Server_RequestPlaceEdge(Request);
}
```

**Blueprint:**
- Get `TrainNetworkPlayerComponent` from your PlayerController
- Call `Server Request Place Edge` with a filled `FEdgePlacementRequest`

The server will validate the request, apply it, and multicast the result to all clients automatically.

---

## Save / Load

Save and load can be called from anywhere with authority (server or singleplayer):

**C++:**
```cpp
URailNetworkSubsystem* RailNetwork = GetWorld()->GetSubsystem<URailNetworkSubsystem>();
RailNetwork->SaveNetwork(TEXT("YourSlotName"));
RailNetwork->LoadNetwork(TEXT("YourSlotName"));
```

**Blueprint:**
- Get `RailNetworkSubsystem` via **Get World Subsystem**
- Call **Save Network** or **Load Network** with your slot name

In the editor, **Save Network** and **Load Network** are also available as **Call In Editor** buttons on the subsystem details panel for quick testing.

---

## Configuration

The following properties are exposed on `URailNetworkSubsystem`:

| Property | Default | Description |
|----------|---------|-------------|
| `MinCurveRadiusCm` | 200.0 | Minimum allowed curve radius in cm. Set to 0 to disable the check. |

---

## Notes

- The subsystem is the sole authority on topology. Always go through `CanPlaceEdge` / `RequestPlaceEdge` rather than calling internal functions directly.
- Node types are assigned automatically — do not attempt to set them manually.
- In multiplayer, never call `RequestPlaceEdge` or `RequestRemoveEdge` directly on a client. Always use `UTrainNetworkPlayerComponent`.
- Save files do not persist runtime block/signal state (occupied status, signal aspect). Only graph structure is saved.

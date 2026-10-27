# Unit state contract (FOC-004)

This document records the minimal domain boundary for [Issue #10](https://github.com/Hazernit/fog-of-command/issues/10). Actual Windows verification status is in the [diagnostic report](diagnostics/FOC-004-windows-unit-state.md).

The types belong to the existing FogOfCommand module's Domain folders and depend on Unreal Core value/container/archive types. They do not own an Actor, UObject, world, controller, Tick, renderer or UI. A position update is a value-state transition, not navigation or scene movement.

## Identity and lifecycle

- UnitId and OrderId are distinct strong GUID types; a default/zero GUID is invalid. Newly allocated IDs use the platform GUID generator. A copied/serialized identity denotes the same entity, not a new entity.
- SideId is a strong positive integer; zero/negative values are invalid. It identifies a side without implementing diplomacy or visibility.
- Initialization requires a valid complete state. An initialized state slot keeps its UnitId: retrying initialization with that same ID never refills resources, resets position, or revives a destroyed unit. Another ID conflicts. Creation is first-wins at this single-entity boundary; a global entity registry/duplicate-world reconciliation is not introduced here.
- The slot cannot be assigned/reset through C++ copy/move assignment. Copy construction denotes the same identity and values; allocating a different entity requires a new ID and an empty slot. The snapshot DTO remains assignable for constructing/validating candidate input.
- Destruction is terminal, idempotent and retains identity, side, position and ammunition. Health becomes zero; an optional opaque current OrderId is cleared. Positive health implies alive, zero health implies destroyed; inconsistent imported states are rejected.
- Move/damage/ammunition consumption preserve UnitId and side. Damage and consumption are individual operations per call, not network request deduplication; repeated calls must still obey resource bounds. Repeated destruction is a no-op. No healing, automatic refill or resurrection is provided.

## Validation and atomic failure

Positions and health values must be finite. Maximum health is positive, health lies in [0, maximum]; ammunition/capacity are nonnegative integers with ammunition <= capacity. Negative damage/consumption, NaN/infinity, invalid IDs/sides, overspending, impossible lifecycle state and unsupported serialized versions must return explicit failure. Rejected operations leave the previous state unchanged; values are not silently clamped into valid input.

The serialized value boundary carries a schema version and preserves all IDs, position, health, ammunition/capacities, lifecycle and optional OrderId. Loading validates the complete candidate before committing it; truncated/invalid/unknown-version input must not partly replace an existing valid state. This is a small testable value format, not SaveGame/file storage, replication, schema migration or an event-sourcing system.

`Deserialize` uses the same first-creation rule as `Initialize`: an empty slot accepts a valid snapshot, the same existing ID returns AlreadyExists without overwriting state, another ID conflicts. It cannot roll back damage, restore ammunition or resurrect an existing tombstone. Import a snapshot into an empty slot to represent that saved identity; authoritative global uniqueness is the responsibility of a later owner/registry, not these value types.

## Minimal API

Headers: `Domain/UnitIdentifiers.h` and `Domain/UnitState.h`, namespace `FogOfCommand::Domain`. `FUnitStateSnapshot` is the mutable import/export DTO; the initialized slot exposes only a const snapshot. Every operation returns `EUnitStateResult`, so a caller can distinguish validation failures, AlreadyExists/AlreadyDestroyed, IdentityConflict, Destroyed/NotInitialized and insufficient ammunition.

```cpp
using namespace FogOfCommand::Domain;
FUnitStateSnapshot initial;
initial.UnitId = FUnitId::Create();
initial.SideId = FSideId::FromValue(1);
initial.AmmoCapacity = 30;
initial.Ammunition = 30;
FUnitState unit;
// Check each returned result before consuming the new state.
const EUnitStateResult created = unit.Initialize(initial);
const EUnitStateResult moved = unit.MoveTo(FVector(100.0, 0.0, 0.0));
const EUnitStateResult damaged = unit.ApplyDamage(10.0);
const EUnitStateResult consumed = unit.ConsumeAmmunition(1);
TArray<uint8> bytes;
const EUnitStateResult encoded = unit.Serialize(bytes);
FUnitState imported;
const EUnitStateResult decoded = imported.Deserialize(bytes);
// imported has the same identity and values; it is not a newly allocated unit.
```

Position is an Unreal coordinate value in centimetres; no world/Actor is consulted. V1 encodes 90 little-endian bytes: uint32 version, 16-byte Unit GUID, int32 side, three position doubles, health/max-health doubles, int32 ammunition/capacity, uint8 life, uint8 optional-order flag, 16-byte Order GUID (zero when absent). Invalid flags, noncanonical absent order data, bad values, wrong size/trailing bytes and unknown versions are rejected. This is the initial local value format; cross-platform save-system compatibility and migrations are not claimed.

## Scope and verification

FOC-004 introduced OrderId as identity metadata. [FOC-005](ORDERS.md) now adds the Core-only MOVE/HOLD lifecycle and a narrow `CompareExchangeCurrentOrderId(ExpectedUnitId, Expected, Desired)` API. It requires a living initialized unit, validates IDs/expected link and changes only CurrentOrderId; no mutable snapshot is exposed. The owner in ORDERS.md coordinates record cancellation with destruction/lethal damage. Standalone UnitState destruction still clears its link without owning any order history. V1 serialization is unchanged.

WorldState/PerceivedWorldState, resources beyond this unit's health/ammunition, order execution/navigation, RTS, AI and debug UI remain outside these value-state contracts.

Six Automation tests cover identifiers, lifecycle, resources, invalid data, repeated operations and serialization alongside the existing three FogOfCommand.Smoke tests. Coverage includes a 512-ID sample for each GUID type, 64 repeated lifecycles, zero/INT32_MAX resource bounds, NaN/Inf, all 90 truncated frame lengths, unsupported versions and malformed fields. This is a test sample, not a mathematical proof of global GUID uniqueness. Run the combined project gate using [TESTING.md](TESTING.md); no Unreal PASS is claimed before actual Windows execution.

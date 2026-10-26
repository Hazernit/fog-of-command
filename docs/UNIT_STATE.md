# Unit state contract (FOC-004)

This document records the minimal domain boundary for [Issue #10](https://github.com/Hazernit/fog-of-command/issues/10). Implementation and Windows evidence are in progress; see the [diagnostic report](diagnostics/FOC-004-windows-unit-state.md).

The types belong to the existing FogOfCommand module's Domain folders and depend on Unreal Core value/container/archive types. They do not own an Actor, UObject, world, controller, Tick, renderer or UI. A position update is a value-state transition, not navigation or scene movement.

## Identity and lifecycle

- UnitId and OrderId are distinct strong GUID types; a default/zero GUID is invalid. Newly allocated IDs use the platform GUID generator. A copied/serialized identity denotes the same entity, not a new entity.
- SideId is a strong positive integer; zero/negative values are invalid. It identifies a side without implementing diplomacy or visibility.
- Initialization requires a valid complete state. An initialized state slot keeps its UnitId: retrying initialization with that same ID never refills resources, resets position, or revives a destroyed unit. Another ID conflicts. Creation is first-wins at this single-entity boundary; a global entity registry/duplicate-world reconciliation is not introduced here.
- Destruction is terminal, idempotent and retains identity, side, position and ammunition. Health becomes zero; an optional opaque current OrderId is cleared. Positive health implies alive, zero health implies destroyed; inconsistent imported states are rejected.
- Move/damage/ammunition consumption preserve UnitId and side. Damage and consumption are individual operations per call, not network request deduplication; repeated calls must still obey resource bounds. Repeated destruction is a no-op. No healing, automatic refill or resurrection is provided.

## Validation and atomic failure

Positions and health values must be finite. Maximum health is positive, health lies in [0, maximum]; ammunition/capacity are nonnegative integers with ammunition <= capacity. Negative damage/consumption, NaN/infinity, invalid IDs/sides, overspending, impossible lifecycle state and unsupported serialized versions must return explicit failure. Rejected operations leave the previous state unchanged; values are not silently clamped into valid input.

The serialized value boundary carries a schema version and preserves all IDs, position, health, ammunition/capacities, lifecycle and optional OrderId. Loading validates the complete candidate before committing it; truncated/invalid/unknown-version input must not partly replace an existing valid state. This is a small testable value format, not SaveGame/file storage, replication, schema migration or an event-sourcing system.

## Scope and verification

OrderId is only identity metadata. Order kinds, queues, timestamps, execution and cancellation belong to FOC-005 and are not implemented. WorldState/PerceivedWorldState, resources beyond this unit's health/ammunition, RTS, AI and debug UI remain outside FOC-004.

Automation will cover identifiers, lifecycle, resources, invalid data, repeated operations and serialization alongside the existing three FogOfCommand.Smoke tests. Run the combined project gate using [TESTING.md](TESTING.md); no Unreal PASS is claimed before actual Windows execution.

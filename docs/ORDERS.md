# MOVE/HOLD domain order contract (FOC-005)

Scope: [Issue #11](https://github.com/Hazernit/fog-of-command/issues/11). Actual verification status: [Windows report](diagnostics/FOC-005-windows-orders.md).

Orders describe intent and lifecycle in Core-only data. No Actor, controller, Tick, UI, navigation, AI execution, global registry, networking or SaveGame system is introduced. A single-unit owner coordinates its private UnitState and order records; consumers receive const views. Keeping that owner alive across a controller handoff preserves the same unit/order identity, timestamps and resources. Actual controller/Possess integration is not part of this issue.

## Validated intent and time

- An order has strong OrderId and UnitId, kind MOVE/HOLD, state, timestamps and a typed reason. Both IDs must be valid and UnitId must identify this initialized, living owner's unit.
- MOVE requires a finite position target in Unreal centimetres. HOLD has no position argument; it represents holding rather than a movement target. Unknown kinds, missing/unexpected targets and NaN/Inf are rejected before any mutation. World reachability and map bounds require a future executor and are not inferred here.
- Time is a caller-supplied **nonnegative int64 simulation tick**, with one agreed epoch/unit per owner. No wall clock, locale or time-zone dependency. Actual mutations cannot go backwards; equal ticks are allowed. The model never increments time internally, avoiding arithmetic overflow at INT64_MAX.
- Creation, optional start, update and optional finish times must agree with lifecycle. Replaying an unchanged request or already-applied outcome does not reset its timestamps or advance time. Negative/invalid inputs are still errors. Operation rejection is separate from accepting an explicit Failed outcome; invalid input does not partly create a Failed order.

## Lifecycle

| From | Accepted operation | To / effect |
|---|---|---|
| No active order | Submit validated MOVE/HOLD | Pending; attach CurrentOrderId |
| Pending | Start | Executing; record start time |
| Executing | Complete | Completed; record explicit completion reason/time and clear matching link |
| Pending / Executing | Cancel / Fail | Cancelled / Failed with compatible reason and finish time; clear matching link |
| Pending / Executing | Explicit validated replacement | Old order Cancelled (replaced), new order Pending; one atomic link replacement |
| Living unit with active order | Coordinated Destroy / lethal damage | Unit destroyed, active order Cancelled (unit destroyed), link cleared atomically |
| Terminal order | Same already-applied terminal outcome | No mutation; timestamps/reason stay stable |
| Terminal order | Conflicting outcome / restart | Explicit rejection; no resurrection |

Only one Pending or Executing order can be active for the unit. Order history retains terminal IDs/intents for duplicate detection during the owner's lifetime; it is not an execution queue or a global unit/order registry. The same ID with changed intent is a conflict. Stale cancellation/replacement must never clear a newer CurrentOrderId. Replacement first validates the new intent, expected previous ID and time; failures preserve the old order and link.

`Complete` records an external executor's outcome; it does not move the unit or prove a MOVE target has been reached. `Fail` records explicit ExecutionFailed, while invalid arguments return an operation error without creating/changing an order. Public cancellation accepts CancelledByRequest; Replaced and UnitDestroyed are reserved for their coordinated operations. Terminal history is retained without eviction and validation scans that history; large-scale performance and retention policy are not claimed.

## UnitState boundary and destruction

UnitState gets only a narrow validated current-order compare/exchange API: expected UnitId, expected current optional OrderId and desired optional OrderId. It checks identity, valid IDs, lifecycle and the expected link. It exposes no mutable snapshot or general state setter. Health, ammunition, side, position and identity remain unaffected by order assignment/removal; its existing v1 value format is retained.

The order owner exposes no mutable UnitState, so coordinated destruction/damage can validate time/inputs first, cancel the active record and destroy the unit together. Existing standalone UnitState destruction still clears its opaque link; callers that need order-record consistency must use the coordinating owner. There is no callback/event bus or automatic Tick reconciliation.

## API boundary

`Domain/Orders.h`, namespace `FogOfCommand::Domain`, defines FOrderRequest, FOrderSnapshot and the noncopyable/nonassignable FUnitOrders owner. Initialize takes a valid unit snapshot without an orphan CurrentOrderId. Submit creates Pending only when there is no active order; Replace requires the expected current ID. Start/Complete/Cancel/Fail address explicit UnitId/OrderId and time. DestroyUnit/ApplyDamage coordinate health/death with the active record. GetUnit/GetHistory/FindOrder expose only const values; a borrowed FindOrder pointer must not be retained across commands that can grow the history map.

UnitState's API is `CompareExchangeCurrentOrderId(ExpectedUnitId, Expected, Desired)`, with optional expected/desired IDs. It requires a living initialized unit, validates the expected identity and both IDs, and changes only the link when Expected matches. A stale CAS is rejected even if it might otherwise be a repeated assignment; higher-level command retries are deduplicated by the owner before calling CAS. UnitState's version1 90-byte format remains unchanged.

## Verification and persistence limits

Eight Orders Automation groups cover Validation, Transitions, Assignment, Replacement, Idempotency, Destruction, Timestamps and Serialization (UnitState's existing current-link value round trip). The default combined gate must retain all nine prior tests and require all eight new ones. Source-level coverage is not a UE PASS until the real Windows build/run recorded in the report.

Order-history/book serialization is not required for this issue and is not implemented. Separate unit values with an orphaned CurrentOrderId cannot be adopted as a coherent new order owner without the corresponding record; that import must be rejected. Full save/load, cross-owner uniqueness, history retention/eviction policy, concurrency and path execution remain outside this contract.

# Simulation event journal (FOC-006)

Scope: [Issue #12](https://github.com/Hazernit/fog-of-command/issues/12).
Actual evidence: [Windows report](diagnostics/FOC-006-windows-event-log.md).

## Owner and time

`Domain/SimulationEvents.h` adds Core-only `FSimulationEvent` and a per-owner
`FSimulationEventLog`. The existing `FUnitOrders` owns the journal alongside its
private UnitState and order history. All authoritative changes needing a trace
must use this owner: timed `Initialize`, `MoveUnit`, `ConsumeAmmunition`,
`ApplyDamage`, `DestroyUnit` and the existing order commands. These preserve
validation, stable identity and one active order. Standalone `FUnitState` stays a
pure value/serialization boundary; its direct operations do not emit events.
No mutable snapshot, public append API, observer callback or global bus is added.

Every event has a 1-based uint64 sequence, explicit nonnegative int64 simulation
tick and stable UnitId. Equal ticks retain sequence order. Order events carry
OrderId, kind, optional validated MOVE target, previous/new status and typed
reason. Unit resource/position events carry full copied before/after values and
the active OrderId when present. Idle unit events have no invented OrderId.
OrderReplaced also carries the new ID as RelatedOrderId; its OrderId is the old ID.
No wall clock, arbitrary reason text, path, username, pointer or file IO is stored.

Creation accepts `Initialize(snapshot, At = 0)`. The creation tick constrains
future mutations. The legacy `GetLastMutationTime()` still excludes initialization;
`GetEventLog().GetLastSimulationTick()` includes it. Unchanged retries return their
existing AlreadyExists/AlreadyApplied result without changing either clock.
Valid tombstone import emits UnitCreated for adoption into this owner, without
inventing damage/destruction history. Invalid imports emit nothing.

## Event semantics

| Event | Accepted change / reason |
|---|---|
| UnitCreated | First valid owner initialization / Created |
| UnitMoved | Position value actually changed / PositionChanged |
| UnitDamaged | Health actually decreased / DamageApplied |
| UnitDestroyed | Alive became terminal / ExplicitDestruction or DamageApplied |
| AmmunitionConsumed | Stock actually decreased / AmmunitionSpent |
| OrderSubmitted | New validated Pending intent / None |
| OrderStarted | Pending to Executing / None |
| OrderCompleted | Executing to Completed / Completed |
| OrderCancelled | Active to Cancelled / CancelledByRequest, Replaced or UnitDestroyed |
| OrderFailed | Explicit accepted Failed outcome / ExecutionFailed |
| OrderReplaced | Old intent replaced with new intent / Replaced; both IDs |

Replacement emits **OrderCancelled, OrderSubmitted, OrderReplaced**. Explicit
unit destruction emits **OrderCancelled (if active), UnitDestroyed**. Lethal damage
emits **UnitDamaged, OrderCancelled (if active), UnitDestroyed**. Each compound
operation publishes after all unit/order changes commit, at the same tick, in
this order. Lethal damage's two unit records describe different facets of one
operation and share its full before/final-after values; they are not instructions
to apply damage twice. Consumers must not replay these diagnostic records as commands.

Invalid input, stale IDs, impossible transitions, regressing mutation times,
duplicate intents, repeated terminal outcomes, unchanged positions, zero ammo
consumption and zero/rounded-away damage produce **no event** and do not advance
time. Sequence capacity is checked for the complete batch before mutation; normal
validation refusals leave unit, orders, clocks and journal unchanged. Serial
callers only: this is not thread synchronization; allocation failure remains a
process-level failure, not a recoverable domain result.

## Retention and cost

The journal retains the latest **256** records in a fixed-capacity ring. Sequence
numbers never reset during the owner's lifetime; total and evicted counts expose
gaps. `GetEvents()` returns chronological value copies; editing them cannot alter
the journal. A batch crossing the retention boundary can evict earlier batch
records, so this is a recent diagnostic window, not persistent event sourcing.
Append and active-link checks take constant work; commands no longer scan all
retained order history. Explicit `Validate()` remains the full history audit.
Order history itself is still retained for deduplication without eviction, as in
FOC-005; this issue does not introduce a large-world retention policy.

There is no frame Tick registration, automatic UE logging or repeated polling.
Only explicit successful changes append. Reading/dumping never appends.
UnitState v1 remains 90 bytes; journal/history serialization, SaveGame, global
ordering across units, networking, movement execution and scene Actors are outside
this contract. The journal contains authoritative state and must not become an
unfiltered future player/network UI feed.

## Developer commands

Commands are compiled only under `!UE_BUILD_SHIPPING && !UE_BUILD_TEST` and
registered only when the process explicitly starts with `-FocEventDebug`.
Ordinary launches register neither command. Module shutdown unregisters both and
releases only the isolated developer state.

- `foc.EventLog.RunScenario` creates a fresh isolated owner, runs validated
  transitions covering all 11 kinds and checks 21 events plus a no-op death retry.
  It never reads/modifies live scene units or filesystem data.
- `foc.EventLog.Dump` prints up to 256 retained isolated records with tick, sequence,
  IDs, statuses and reasons, plus resulting unit values. No arguments are accepted;
  invalid arguments are rejected without copying them into custom output or
  changing the isolated session. Dump before a scenario reports EMPTY.

In the Development Editor, open the existing Startup map and use the Output Log
console after launching with `-FocEventDebug`. For an unattended minimal-map check,
use the existing accepted engine root and a freshly built ASCII checkout:

```powershell
$editor = Join-Path $env:UE_ROOT 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project = Join-Path $PWD 'FogOfCommand.uproject'
# Set $localLog to a NEW external file outside every Git checkout.
& $editor $project /Game/Maps/Startup -unattended -nop4 -nosplash -nullrhi `
  -FocEventDebug '-ExecCmds=foc.EventLog.RunScenario;foc.EventLog.Dump;Quit' `
  "-abslog=$localLog"
```

This command has no Automation verdict: inspect native exit, scenario PASS,
exactly 21 sequential FOC_EVENT records, all 11 types, Startup map load and absence
of new project errors. Repeat without `-FocEventDebug`: both commands must be
unrecognized and no FOC_EVENT records/scenario may appear. Full 23-test gate
commands and honest hosted CI limits are in [TESTING.md](TESTING.md). Raw logs
remain local; publish only sanitized findings. A NullRHI map check does not prove
GUI/PIE/rendering/GPU stability or a Shipping binary build.
# Simulation event journal (FOC-006)

Scope: [Issue #12](https://github.com/Hazernit/fog-of-command/issues/12).
This journal makes validated domain transitions observable. It does not execute
orders, move scene Actors, introduce a world registry or implement event sourcing.

## Implementation and verification plan

- Add structured Core-only events for UnitCreated, UnitMoved, UnitDamaged,
  UnitDestroyed, AmmunitionConsumed, OrderSubmitted, OrderStarted,
  OrderCompleted, OrderCancelled, OrderFailed and OrderReplaced.
- The single-unit FUnitOrders owner records committed state changes with explicit
  nonnegative simulation ticks, stable IDs, typed reasons and ordered sequence
  numbers. Its private UnitState stays behind validated operations; the standalone
  UnitState value/serialization API remains independent of diagnostic storage.
- Rejections and unchanged retries produce no events. Replacement and destruction
  commit their coordinated changes before publishing their ordered event batch.
- Keep bounded recent event retention and avoid new scans of retained order
  history on each command. No frame callback, file output or arbitrary text
  payload belongs in the domain journal.
- Add explicit opt-in developer commands using isolated test state, excluded from
  Shipping/Test builds. Commands must call the same validated domain API and
  never mutate a live scene or inspect private machine/user information.
- Add six Events Automation groups and retain all 17 existing project tests.
  Update the exact-name Windows gate and hosted static checks.

The initial plan is saved before implementation. Final API details and actual
PASS/FAIL/NOT RUN evidence will be updated after implementation and a fresh
Windows clone/build/test run. See the [diagnostic report](diagnostics/FOC-006-windows-event-log.md).

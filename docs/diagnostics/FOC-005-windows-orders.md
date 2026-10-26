# FOC-005: MOVE/HOLD domain orders and Windows verification

Date: 2026-10-26 (Europe/Moscow). Issue: [#11](https://github.com/Hazernit/fog-of-command/issues/11). Branch: `feat/foc-005-orders`.

## Preparation

Read current AGENTS, ARCHITECTURE, UNIT_STATE, TESTING, Windows guide and Issue #11. FOC-004 is merged; local main fast-forwarded to `37ab9232fe37403379a504dddb22d9da0f85a7eb` before branching. Initial working tree was clean. Git LFS initialized locally, pull/fsck PASS; Startup map is unchanged.

Implement Core-only MOVE/HOLD intent/state/time/reasons, one active order for one owned unit, narrow validated CurrentOrderId assignment/removal, coherent replacement and destruction with atomic failures. No global registry, Actor/Tick/UI, AI/navigation/order execution, network/save-game subsystem or neighboring Issue.

Fresh inventory: installed UE **5.8.4 CL58902052**, MSVC **14.44.35229** (file product 14.44.35229.0), SDK **10.0.22621.0**. Installed VS2022 IDE **17.14.43** differs from historical pin17.14.42; this task did not update software or modify ENGINE_VERSION.md. Actual UBT selection will be verified in the new build. No toolchain/locale changes.

Use a new ASCII checkout outside OneDrive and external local evidence. Known Engine startup failures #55 and location-specific PCH C1083 #57 remain separate, without culture override or cause-isolation experiments.

## Stage results

| Check | Status | Evidence |
|---|---|---|
| Main / dependency / branch / LFS | PASS | FOC-004 merged, clean tree, separate feature branch, pull/fsck |
| Orders / UnitState API implementation | Implemented; runtime NOT RUN | Core-only owner/history/lifecycle and narrow CAS; old UnitState v1 format unchanged |
| Combined report validator / parser | PASS (synthetic/static) | 65 cases in Windows PowerShell 5.1 and PowerShell 7; All requires 9 old + 8 new tests |
| Local static checks | PASS | Project/LFS/domain boundary, 21 Python regression cases, 65 PowerShell report cases, parser/backlog/diff/privacy |
| Hosted static CI | Runner-stage PASS | [40319117205](https://github.com/Hazernit/fog-of-command/actions/runs/40319117205); full implementation pending push |
| New clean Windows C++ build | NOT RUN | Pending new remote clone |
| All old/new Unreal Automation Tests | NOT RUN | Pending compiled implementation |
| Hosted UE build / Automation | NOT RUN | Hosted runner has no UE; actual Windows manual gate required |
| GUI/PIE/Possess/GPU, packaging, Engine rebuild/cold-cache | NOT RUN | Domain/Automation scope |
| Owner review / merge | NOT RUN | Draft PR; no automatic merge |

Contract: [ORDERS.md](../ORDERS.md). Commands: [TESTING.md](../TESTING.md). Only sanitized summaries go into Git. Raw logs/JSON/commands, private paths and generated caches remain local outside Git. MyProject/Engine source are unchanged, installations are not run, and neighboring Issues are not started.

## Implementation and publication

Orders.h/.cpp define MOVE/HOLD intent, lifecycle and explicit results/reasons. A noncopyable single-unit owner holds a private UnitState and per-unit terminal history. Only const unit/order views escape. New order IDs cannot replace an active order implicitly; explicit Replace validates the old link, new intent and time before either record/link changes. UnitState gets only a validated expected-identity/expected-link CAS, without mutable snapshots or changes to health/resources/position/identity.

DestroyUnit and lethal ApplyDamage prevalidate arguments/time and cancel the active order with UnitDestroyed while creating the unit tombstone. No-op request/outcome retries preserve timestamps and do not advance the clock. Existing terminal callbacks cannot clear a later active order. Scalar finite target checks precede copying invalid FVector input; TMap record pointers are not reused after Add can reallocate storage. Core include paths were checked against the actual installed UE headers.

Eight new Orders Automation groups include full unit/history/clock comparisons after rejection and eight concise Checked-invariants info markers. New static required-file/dependency checks also cover Orders.h/.cpp. These remain implementation/coverage claims until actual Windows compilation and test execution below.

The GitHub connector transport became unavailable during early PR publication. Git push still worked; the PR was created/verified using Windows HTTPS and the existing Git Credential Manager authentication, without displaying/persisting credential values or changing connection settings. Draft PR [#60](https://github.com/Hazernit/fog-of-command/pull/60) is attached; publication helper/HTTP raw data stay outside Git.

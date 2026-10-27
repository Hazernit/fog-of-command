# FOC-005: MOVE/HOLD domain orders and Windows verification

Date: 2026-10-26 (Europe/Moscow). Issue: [#11](https://github.com/Hazernit/fog-of-command/issues/11). Branch: `feat/foc-005-orders`.

## Preparation

Read current AGENTS, ARCHITECTURE, UNIT_STATE, TESTING, Windows guide and Issue #11. FOC-004 is merged; local main fast-forwarded to `37ab9232fe37403379a504dddb22d9da0f85a7eb` before branching. Initial working tree was clean. Git LFS initialized locally, pull/fsck PASS; Startup map is unchanged.

Implement Core-only MOVE/HOLD intent/state/time/reasons, one active order for one owned unit, narrow validated CurrentOrderId assignment/removal, coherent replacement and destruction with atomic failures. No global registry, Actor/Tick/UI, AI/navigation/order execution, network/save-game subsystem or neighboring Issue.

Fresh inventory: installed UE **5.8.4 CL58902052**, MSVC **14.44.35229** (file product 14.44.35229.0), SDK **10.0.22621.0**. Installed VS2022 IDE **17.14.43** differs from historical pin17.14.42; this task did not update software or modify ENGINE_VERSION.md. Actual UBT selection is confirmed in the build section below. No toolchain/locale changes.

Use a new ASCII checkout outside OneDrive and external local evidence. Known Engine startup failures #55 and location-specific PCH C1083 #57 remain separate, without culture override or cause-isolation experiments.

## Stage results

| Check | Status | Evidence |
|---|---|---|
| Main / dependency / branch / LFS | PASS | FOC-004 merged, clean tree, separate feature branch, pull/fsck |
| Orders / UnitState API implementation | PASS | Compiled Core-only owner/history/lifecycle/CAS; all eight Orders groups passed in real UE; UnitState v1 unchanged |
| Combined report validator / parser | PASS (synthetic/static) | 65 cases in Windows PowerShell 5.1 and PowerShell 7; All requires 9 old + 8 new tests |
| Local static checks | PASS | Project/LFS/domain boundary, 21 Python regression cases, 65 PowerShell report cases, parser/backlog/diff/privacy |
| Hosted static CI | PASS | [40320642061](https://github.com/Hazernit/fog-of-command/actions/runs/40320642061) on f0f6132, Ubuntu + Windows; hosted UE NOT RUN |
| New clean Windows C++ build | PASS | f0f6132, native/wrapper 0, 79.740 s wall / 78.44 s UBT, 15 actions, zero compiler/linker warnings/errors |
| All old/new Unreal Automation Tests | PASS | One combined All run: 17/17, native/wrapper 0, 54.320 s wall; zero project warnings/errors |
| Hosted UE build / Automation | NOT RUN | Hosted runner has no UE; actual Windows manual gate required |
| GUI/PIE/Possess/GPU, packaging, Engine rebuild/cold-cache | NOT RUN | Domain/Automation scope |
| Owner review / merge | NOT RUN | Draft PR; no automatic merge |

Contract: [ORDERS.md](../ORDERS.md). Commands: [TESTING.md](../TESTING.md). Only sanitized summaries go into Git. Raw logs/JSON/commands, private paths and generated caches remain local outside Git. MyProject/Engine source are unchanged, installations are not run, and neighboring Issues are not started.

## Implementation and publication

Orders.h/.cpp define MOVE/HOLD intent, lifecycle and explicit results/reasons. A noncopyable single-unit owner holds a private UnitState and per-unit terminal history. Only const unit/order views escape. New order IDs cannot replace an active order implicitly; explicit Replace validates the old link, new intent and time before either record/link changes. UnitState gets only a validated expected-identity/expected-link CAS, without mutable snapshots or changes to health/resources/position/identity.

DestroyUnit and lethal ApplyDamage prevalidate arguments/time and cancel the active order with UnitDestroyed while creating the unit tombstone. No-op request/outcome retries preserve timestamps and do not advance the clock. Existing terminal callbacks cannot clear a later active order. Scalar finite target checks precede copying invalid FVector input; TMap record pointers are not reused after Add can reallocate storage. Core include paths were checked against the actual installed UE headers.

Eight new Orders Automation groups include full unit/history/clock comparisons after rejection and eight concise Checked-invariants info markers. New static required-file/dependency checks also cover Orders.h/.cpp. These remain implementation/coverage claims until actual Windows compilation and test execution below.

The GitHub connector transport became unavailable during early PR publication. Git push still worked; the PR was created/verified using Windows HTTPS and the existing Git Credential Manager authentication, without displaying/persisting credential values or changing connection settings. Draft PR [#60](https://github.com/Hazernit/fog-of-command/pull/60) is attached; publication helper/HTTP raw data stay outside Git.

## Actual clean Windows C++ build

A new remote GitHub clone downloaded `f0f6132d2fe64d9b63505553e3c5f82f13a015f9`; source/binaries were not copied from an earlier acceptance project. LFS pull/fsck PASS. Before build **Binaries=false, Intermediate=false, Saved=false, DerivedDataCache=false**. Startup map remains 26285 bytes with SHA256 `8a9a48b6dcfbb9e0b7e6d90cd5fc459745c1cd675aae025fc78c55537ae014b8`, matching the existing LFS pointer/object.

**15:11:57.453–15:13:17.208 +03:00:** native/wrapper **0**, **79.740 s wall / 78.44 s UBT**, **15 actions**. UBT selected **VS2022 / MSVC 14.44.35229 / Windows SDK 10.0.22621.0**; compiler/linker warnings **0**, errors **0**. Orders implementation, UnitState CAS and all old/new Automation C++ sources compiled and linked into the Editor target. Shared Engine/UBA/DDC caches were not cleared; this is a clean project build on the installed binary UE, not an Engine rebuild/cold-cache benchmark.

## Actual combined Unreal Automation run

Same downloaded commit `f0f6132`, explicit `-Suite All`, Windows PowerShell 5.1, unattended NullRHI, no culture override. **15:14:16.576–15:15:10.973 +03:00:** actual native **0**, wrapper **0**, fresh report verdict **PASS**, **54.320 s wall**, no wrapper timeout/launch error. All **17 exact tests** are Success, with project errors/warnings **0**. Root counters: succeeded17; succeededWithWarnings/failed/notRun/inProcess0. Test durations total **0.296593 s**; wall time also includes Editor startup/discovery/reporting.

| Full test name | State | Duration |
|---|---|---|
| FogOfCommand.Orders.Validation | Success | 0.008558 s |
| FogOfCommand.Orders.Transitions | Success | 0.007884 s |
| FogOfCommand.Orders.Assignment | Success | 0.007822 s |
| FogOfCommand.Orders.Replacement | Success | 0.007643 s |
| FogOfCommand.Orders.Idempotency | Success | 0.007356 s |
| FogOfCommand.Orders.Destruction | Success | 0.008080 s |
| FogOfCommand.Orders.Timestamps | Success | 0.008210 s |
| FogOfCommand.Orders.Serialization | Success | 0.008373 s |
| FogOfCommand.Smoke.Configuration | Success | 0.008657 s |
| FogOfCommand.Smoke.Module | Success | 0.008444 s |
| FogOfCommand.Smoke.StartupMap | Success | 0.166166 s |
| FogOfCommand.UnitState.Identifiers | Success | 0.007976 s |
| FogOfCommand.UnitState.Lifecycle | Success | 0.008224 s |
| FogOfCommand.UnitState.Resources | Success | 0.008489 s |
| FogOfCommand.UnitState.Validation | Success | 0.008186 s |
| FogOfCommand.UnitState.RepeatedOperations | Success | 0.008600 s |
| FogOfCommand.UnitState.Serialization | Success | 0.007926 s |

The log has **14 Checked-invariants markers**: eight Orders plus six retained UnitState tests. This is one complete combined run, not merged subset reports. The Orders.Serialization group proves the existing UnitState v1 current-link round trip/CAS/orphan rejection, **not** order-history serialization; its TESTING table wording was corrected after review.

## Runtime diagnostics and working-tree evidence

Startup still reports **428 named Engine self-tests: 425 PASS, 3 FAIL**. The failures are exactly the known `FUnifiedErrorTest_CreateErrorMessage`, `FUnifiedErrorTest_CreateErrorMessageWithContext`, `FStructuredLogFormatTest` from #55, with **17 AutomationTest Error lines**. Other Error lines **0**; detected fatal/assert/ensure/device-removed/GPU-crash markers **0**. No locale change or suppression; the project PASS does not mean #55 is fixed or the full startup log is clean.

Two non-project startup warnings were observed: **ZenServer AutoLaunch initialization timed out waiting for health after 30.862 s**, and LayoutService reported an older layout key. Project tests still completed with zero warnings/errors. Zen/service/cache health and the layout migration are not investigated or repaired by FOC-005; no service, security or network configuration was changed. No claim about cold-start performance is made.

After run, tracked project/Config/Content diff is empty. The map hash is unchanged and matches its LFS pointer. Editor created **untracked Config/DefaultInput.ini** only in the disposable clone; it is left local, not copied/staged/published. No generated folders or raw reports were transferred into the development checkout.

## CI and acceptance limits

The actual hosted [PR CI run 40320642061](https://github.com/Hazernit/fog-of-command/actions/runs/40320642061) for f0f6132 succeeded on Ubuntu and Windows. It covers LFS, static project/domain boundaries, **21 Python regression cases**, backlog, plus Windows PowerShell syntax/**65 synthetic report cases**. Hosted **UE build / Unreal Automation / Editor-PIE remain NOT RUN**: the real UE results above were obtained locally. Existing hosted Actions/runner migration warnings are not claimed fixed.

The guarantee is for serial callers and one owner's private unit/history. There is no global registry/cross-owner uniqueness, thread synchronization, history eviction, world/path reachability, AI/order execution or controller/Actor integration. Complete only records an external executor's outcome. History is retained for duplicate detection; large-scale performance is not measured. No neighboring Issue is started.

**NOT RUN:** GUI/PIE/Possess/reopen/input/GPU rendering/stress, Game target/Shipping/packaging/standalone, Engine rebuild/cold-cache benchmark, order-book/history serialization (not implemented), cross-platform codec, live subset/timeout-kill paths, #57 cause isolation, Zen/layout repair/diagnosis, owner review/merge. The PR remains Draft and Issue open for the owner's decision.

# FOC-004: domain UnitState and Windows verification

Date: 2026-10-26 (Europe/Moscow). Issue: [#10](https://github.com/Hazernit/fog-of-command/issues/10).
Branch: `feat/foc-004-unit-state`.

## Preparation and scope

Current AGENTS, ARCHITECTURE, ROADMAP, TESTING, Windows guide and Issue #10 were read. FOC-003 is accepted and merged; local main fast-forwarded to `4e24f8da0b940c25734d9bf471de8b2cd719aff4` before branching. Initial tree was clean; Git LFS pull/fsck PASS.

Implement Core-only UnitId, SideId, OrderId and UnitState, explicit invalid-state/resource results, stable identity through movement/damage/destruction, idempotent initialization/destruction and a small versioned value-state serialization boundary. No Actors, Tick, UI, command execution, WorldState simulation, save-game subsystem or FOC-005.

Fresh local inventory: UE **5.8.4 CL58902052**, actual MSVC **14.44.35229** (file product version 14.44.35229.0), SDK **10.0.22621.0** present. Current VS2022 IDE is **17.14.43**, differing from historical 17.14.42; this task did not update it or change ENGINE_VERSION.md. Actual UBT selection is confirmed in the build results below. No installs/updates or toolchain changes.

Use a new ASCII checkout outside OneDrive for the real clean build, preserving raw evidence outside Git. Known startup tests #55 and location-specific C1083 #57 remain separate; no locale override or cause-isolation experiments.

## Stage results

| Check | Status | Evidence |
|---|---|---|
| Main / dependency / branch / LFS | PASS | FOC-003 merged, fast-forward main, clean tree, LFS pull/fsck |
| Domain implementation and Automation Tests | PASS | Four Core-only domain files compiled; all six invariant test groups passed in real UE |
| Combined report gate / PowerShell syntax | PASS (synthetic/static) | 43 report cases on Windows PowerShell 5.1 and PowerShell 7; parser PASS; default All requires 3 old + 6 new exact tests |
| Local static checks | PASS | Project/LFS/domain-boundary validator, 19 Python regression cases, 43 PowerShell report cases, backlog validator and staged diff/privacy review |
| Hosted static CI | PASS | [40313251839](https://github.com/Hazernit/fog-of-command/actions/runs/40313251839) on b97406c, Ubuntu + Windows; UE NOT RUN |
| First new clean Windows C++ build | FAIL | c704a3b, native/wrapper 6, 67.771 s wall / 66.01 s UBT; missing Optional header include |
| Corrected clean Windows C++ build | PASS | b97406c, native/wrapper 0, 64.659 s wall / 63.08 s UBT, no compiler/linker errors or warnings |
| Old and new Unreal Automation Tests | PASS | One combined All run: 3 old Smoke + 6 new UnitState, native/wrapper 0, 24.705 s wall; zero project warnings/errors |
| Hosted UE build / Automation | NOT RUN | No configured UE on hosted runner; local Windows manual gate |
| GUI/PIE/GPU, packaging/Shipping, Engine rebuild/cold-cache | NOT RUN | Domain/Automation scope; no new runtime scene claims |
| Owner review / merge | NOT RUN | Draft PR; no automatic merge |

Only sanitized summaries belong in Git. Raw UBT/Editor logs, JSON reports, local absolute paths, generated caches and auto-created local input Config remain outside Git. MyProject/Engine source are unchanged; FOC-005 and other Issues are not started.

## Implementation and review

Strong GUID UnitId/OrderId, positive SideId and a single-entity state slot live in Core-only Domain files. No new module/dependency, Actor/world/Tick/UI or Order execution is introduced. Initialize/Deserialize validate a full candidate before first creation; same-ID retries cannot heal/refill/revive, another ID conflicts. Public assignment/reset is disabled; state is exposed as a const snapshot. Destroy creates a terminal tombstone while preserving identity/side/position/ammunition and clearing only the optional OrderId.

V1 is a fixed 90-byte little-endian value frame with explicit schema version. Failure preserves both destination state and serialization output buffer. Review caught and prevented a FVector constructor diagnostic/sanitization path: raw decoded coordinates are checked for finiteness before constructing a vector. Invalid data uses explicit result values rather than asserts, silent correction or partial mutation.

Six new Automation groups cover strong/valid/unique ID allocation (512 per GUID type), stable lifecycle IDs, zero/INT32_MAX resources, negative/NaN/Inf and incoherent imports, 64 repeated lifecycles, all 90 truncated frame prefixes, bad version/trailing/malformed fields, and alive/no-order/destroyed round trips. Each emits a concise Checked-invariants info entry. These source-level claims require the actual build/run below before being counted PASS.

## First actual clean build and include correction

A new remote GitHub clone of `c704a3ba5d08055248d636dca3ac7be99c6bef5d` downloaded the unchanged Startup map through LFS; pull/fsck PASS. Binaries, Intermediate, Saved and DerivedDataCache were absent before the build. **14:09:25.586–14:10:33.372 +03:00**, native/wrapper **6**, **67.771 s wall / 66.01 s UBT**, 13 actions. UBT selected **VS2022 / MSVC 14.44.35229 / Windows SDK 10.0.22621.0**, matching the compiler/SDK pins.

Two compiler failures (UnitState.cpp and its tests) had the same source-header cause:

```text
UnitState.h(6): fatal error C1083: Cannot open include file: Templates/Optional.h: No such file or directory
Result: Failed (OtherCompilationError)
```

The installed UE 5.8 Core headers confirm `Misc/Optional.h` (also used by CoreMinimal.h and AutomationTest.h), not Templates/Optional.h. This is an incorrect new source include, distinct from the location-specific PCH C1083 in #57. The include is corrected; Engine files/toolchain are unchanged. Raw evidence and the failed clone are preserved outside Git; corrected clean build and actual Automation remain **NOT RUN** until rerun. Hosted static CI passed, illustrating that it does not prove C++ compilation.

## Corrected clean C++ build

A second new remote clone downloaded `b97406ce558acd16cc7d973cee7fcf767a4be11e` from GitHub. No source/binaries/generated folders were copied from the failed clone. LFS pull/fsck PASS; the 26285-byte Startup map SHA256 is unchanged: `8a9a48b6dcfbb9e0b7e6d90cd5fc459745c1cd675aae025fc78c55537ae014b8`. Binaries/Intermediate/Saved/DerivedDataCache were all absent before build.

**14:12:35.715–14:13:40.389 +03:00:** native/wrapper **0**, **64.659 s wall / 63.08 s UBT**. UBT selected **VS2022 / MSVC 14.44.35229 / SDK 10.0.22621.0**; compiler/linker errors **0**, warnings **0**. Domain implementation and both old/new Automation source compiled into the Editor target. Shared Engine/UBA/DDC caches remain uncontrolled; this is a clean project build on the installed binary UE, not an Engine rebuild or cold-cache benchmark.

## Actual combined Unreal Automation run

On the same remote clone/commit `b97406c`, Windows PowerShell 5.1 launched the checked-in runner with `-Suite All`, unattended NullRHI and no culture override. **14:14:44.095–14:15:08.862 +03:00:** actual native **0**, wrapper **0**, **24.705 s wall**, no timeout/launch error, fresh JSON verdict **PASS**. This is one combined project run, not a merge of partial reports.

Portable reproduction (local variables, no private paths):

```powershell
$evidence = Join-Path $env:TEMP 'FOC004-local-evidence'
./scripts/windows/build_fog_of_command.ps1 -EngineRoot $env:UE_ROOT -EvidenceRoot $evidence
./scripts/windows/run_fog_of_command_tests.ps1 -EngineRoot $env:UE_ROOT -EvidenceRoot $evidence -Suite All -TimeoutSeconds 600
```

The runner executes `Automation RunTests StartsWith:FogOfCommand.;Quit`; its exact native command and raw reports remain in the external local evidence directory. Fresh report: succeeded **9**, succeededWithWarnings/failed/notRun/inProcess **0**, every test state Success with errors/warnings **0**. Test durations sum to **0.234428 s**; process wall time includes Editor startup, discovery and reporting.

| Full test name | State | Duration |
|---|---|---|
| FogOfCommand.Smoke.Configuration | Success | 0.007748 s |
| FogOfCommand.Smoke.Module | Success | 0.008032 s |
| FogOfCommand.Smoke.StartupMap | Success | 0.178827 s |
| FogOfCommand.UnitState.Identifiers | Success | 0.001639 s |
| FogOfCommand.UnitState.Lifecycle | Success | 0.008083 s |
| FogOfCommand.UnitState.Resources | Success | 0.006483 s |
| FogOfCommand.UnitState.Validation | Success | 0.007615 s |
| FogOfCommand.UnitState.RepeatedOperations | Success | 0.008172 s |
| FogOfCommand.UnitState.Serialization | Success | 0.007828 s |

After run, tracked project/Config/Content diff is empty; Startup map SHA256 still matches the committed LFS object. Editor created untracked `Config/DefaultInput.ini` in the disposable clone. It is left local, not copied/staged/published. The main development checkout remains separate; no generated folders were transferred back.

Six `Checked:` info markers in the real Automation log describe the domain invariants exercised. The startup phase still emitted **428 named Engine self-tests: 425 PASS, 3 FAIL**, exactly `FUnifiedErrorTest_CreateErrorMessage`, `FUnifiedErrorTest_CreateErrorMessageWithContext`, and `FStructuredLogFormatTest` from #55. There are **17 AutomationTest Error lines**, **0 other Error lines**, and no detected fatal/assert/ensure/device-removed/GPU-crash marker. The single Warning is LayoutService's legacy layout-key migration; it was not repaired. No culture override/global locale change was used. Project 9/9 PASS does not mean this known startup defect is fixed or the entire log is error-free.

## Hosted CI and static evidence

The actual [PR CI run 40313251839](https://github.com/Hazernit/fog-of-command/actions/runs/40313251839) for the compiled/tested code commit b97406c completed successfully on Ubuntu and Windows. Both jobs passed LFS, project/target/map/domain-boundary checks, **19 Python regression cases** and backlog validation. Windows additionally passed PowerShell parsing and **43 synthetic report-verdict cases**; Linux skips those Windows-only steps as designed. Local report tests also passed in Windows PowerShell 5.1 and PowerShell 7.

The domain dependency check is deliberately narrow/static, not C++ semantic analysis. The real native build/run above provides the compiler/runtime evidence. Hosted **UE build / Unreal Automation / Editor-PIE are NOT RUN**: no UE is configured on those runners and a successful static job is not a successful engine build. Existing hosted Actions/runner migration warnings are not presented as fixed by this issue.

## Limits and acceptance

The proved scope is a single-entity Core value/slot contract and real project Automation, not a global authoritative unit registry, collision-proof GUID mathematics, gameplay/Actor/Possess integration, navigation, command execution or a SaveGame system. IDs were tested over the documented allocation sample and preserved through lifecycle/value serialization. Assignment reset and import-based refilling/resurrection are rejected by contract. OrderId remains opaque; FOC-005 is not started.

**NOT RUN:** GUI/PIE/reopen/input/GPU rendering/stress for this issue, separate Game target/Shipping/packaging/standalone, Engine rebuild, cold-cache performance, cross-platform/big-endian serialization, live individual subset CLI selectors, live timeout-kill integration, original #57 cause isolation, owner acceptance/merge. Synthetic report cases and hosted static checks do not count as UE execution. Review/merge remains the owner's decision; the agent leaves the PR Draft and Issue open.

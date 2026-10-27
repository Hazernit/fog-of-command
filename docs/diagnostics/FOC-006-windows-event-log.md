# FOC-006 Windows event journal verification

Issue: [#12](https://github.com/Hazernit/fog-of-command/issues/12).
Draft PR: [#61](https://github.com/Hazernit/fog-of-command/pull/61).
Branch: `feat/foc-006-event-log`. Base: `1e8c87c` (accepted FOC-005).
Date: **2026-10-27**, all times below Europe/Moscow (UTC+03:00).

## Implemented boundary

[EVENT_LOG.md](../EVENT_LOG.md) documents all 11 event kinds, copied values,
nonnegative simulation ticks, stable IDs, typed reasons, batch order and recent
256-record ring retention. FUnitOrders is the journalled authoritative boundary;
standalone UnitState remains a pure value API with unchanged v1 bytes. Refusals
and no-op retries preserve state, clock, history and journal. Normal commands
check the private active slot without scanning the entire retained order history;
explicit Validate remains the full audit. No Actor movement, order execution,
Tick registration, persistence or world registry is implemented.

## Reproduction and source provenance

- PASS: clean main synchronized with origin/main; read current AGENTS, architecture,
  UnitState, orders, testing, Windows guide, roadmap and relevant GDD; dependencies
  accepted. Only FOC-006 is in scope.
- PASS: native Git LFS3.7.0, local hooks/pull/fsck. Existing Startup map unchanged:
  26,285 bytes, SHA256
  `8a9a48b6dcfbb9e0b7e6d90cd5fc459745c1cd675aae025fc78c55537ae014b8`.
- Clean native build and Automation tested source **775e9c1** from a new GitHub
  clone in an external ASCII directory, outside OneDrive and every other checkout.
  Binaries, Intermediate, Saved and DerivedDataCache were all absent before build.
  Existing Engine/UBA/shared DDC caches were not cleared; this is a clean project
  build on installed binary UE, not an Engine rebuild/cold-cache benchmark.
- **6ad6e64** adds the reproducible debug script and documentation; C++ sources
  remain exactly the compiled/tested775e9c1 revision. Later documentation/script
  fixes do not claim another C++ build.
- Raw build/Editor/Automation/debug logs, full JSON reports, local command lines
  and generated files remain outside Git. Only this reviewed summary is published.

Actual toolchain:

| Component | Observed |
|---|---|
| Unreal Engine |5.8.4 CL58902052, installed binary build|
| Visual Studio2022 |17.14.43 currently installed; historical accepted IDE display17.14.42|
| UBT-selected MSVC |14.44.35229, installed toolset folder14.44.35207|
| UBT-selected Windows SDK |10.0.22621.0|

Agent did not install/update/downgrade/select another toolchain. Project target
pins and ENGINE_VERSION.md are unchanged. IDE inventory drift is reported rather
than silently rewriting the accepted baseline.

## Native clean C++ build — PASS

Command from the clean clone (EngineRoot is the user's existing local UE root;
EvidenceRoot must be external):

```powershell
./scripts/windows/build_fog_of_command.ps1 `
  -EngineRoot $env:UE_ROOT -EvidenceRoot $evidence
```

Run ID `FOC002-build-20261027-144215-d792243b` (legacy shared script prefix).
**14:42:15.270–14:44:14.275**; native/wrapper exit **0/0**; wall **118.989s**,
UBT **115.61s**, **18 actions**. `Result: Succeeded`; compiler/linker warnings
**0**, errors **0**. Selected MSVC/SDK validation PASS. First C++ build passed;
no engine source or compile workaround was needed.

## Unreal Automation — PASS, all 23

```powershell
./scripts/windows/run_fog_of_command_tests.ps1 `
  -EngineRoot $env:UE_ROOT -EvidenceRoot $evidence -TimeoutSeconds 600
```

Run ID `FOC003-tests-20261027-144426-dc102f8a` (legacy shared script prefix).
**14:44:26.452–14:46:01.262**; native/wrapper exit **0/0**, **94.731s** wall,
no timeout/launch error. Fresh JSON report contains **23 unique Success** values:

| Group | Actual result |
|---|---|
| Smoke, existing |3/3 PASS|
| UnitState, existing |6/6 PASS|
| Orders, existing |8/8 PASS|
| Events, new |6/6 PASS|

New groups: Transitions, Ordering, Atomicity, ReplacementAndDestruction,
RepeatedOperations, Retention. All 17 prior tests remain unchanged and ran once.
Project failed/notRun/inProcess/succeededWithWarnings **0**; per-test warnings
and errors **0**. Report test-duration sum **0.386937082s**, distinct from native
startup/wall duration. **20 Checked markers** appear in JSON and log (6Events,
8Orders,6UnitState). Exact manifest gate rejects old17-test or incomplete reports.

Tests cover all 11 kinds/payloads/reasons/IDs, equal/nonzero/INT64_MAX ticks,
complete before/after state/history/clock/journal comparisons on refusal,
replacement and Pending/Executing lethal/explicit death, stale callbacks,
64-loop no-op cases, copied reads, bounded retention and sequence gaps. This is
NullRHI Editor Automation, not GUI/PIE/input/rendering/GPU validation.

## Developer commands on Startup — PASS

```powershell
./scripts/windows/run_fog_of_command_event_debug.ps1 `
  -EngineRoot $env:UE_ROOT -ProjectRoot $cleanClone `
  -EvidenceRoot $evidence -TimeoutSeconds 180
```

Final helper source **ecc8264**, using the existing clean-clone 775e9c1 C++ module.
Run ID `FOC006-debug-20261027-145844-56a47099`; wrapper **0/PASS**, native
**0/0**, no timeout, launch or cleanup errors. Both processes load the saved
Startup map, create its world, complete Editor initialization and close normally
through QUIT_EDITOR. No live player/scene state is mutated by the commands.

| Case | Moscow time | Wall | Registry | Domain output | Result |
|---|---|---|---|---|---|
| Ordinary launch |14:58:44.788–14:59:33.338|47.853s|0 commands|0 events/scenarios|PASS|
| Explicit -FocEventDebug |14:59:33.352–15:00:25.477|51.478s|exactly Dump + RunScenario|2 identical dumps,21 records each,11 kinds|PASS|

Enabled case: one successful isolated scenario, one EMPTY read before creation,
two fixed invalid-argument refusals, stable UnitId, exact sequence/ticks/reasons
and identical repeated dumps (42 output lines, **21 journal entries**, no second
mutation batch). Final isolated unit is destroyed with health 0, ammunition 8,
position(100,0,0), no CurrentOrderId. No output is generated automatically per
frame; only explicit commands/transitions record/dump, also covered by the 64-loop
no-op tests and absence of domain Tick registration. A continuous rendering or
frame-rate benchmark was not performed.

Two earlier helper failures are retained as evidence: ordinary Quit did not exit
Editor and game-style map/unknown-command assumptions were wrong; that owned
harness/Editor alone was stopped. A second native0/0 invocation verified the
scenario but its wrapper falsely rejected timestamp/frame-prefixed registry
names. Source-based fixes plus eight PS5.1/7 regression fixtures corrected the
helper; the final fresh end-to-end run above passed. No failed invocation was
rewritten into a PASS or omitted from local evidence. These were helper failures,
not C++/domain test failures.

Final debug logs were also audited manually, independently of helper markers:
MapCheck reports **0 errors/0 warnings** in both cases; each log contains only
17 Error-severity lines from the same three known startup tests, **0 errors after
commands**, and **0 fatal/assert/ensure/device/debug-failure markers**. Default
case has2 warnings (AssetRegistry platform file journal unavailable, Zen health
wait30.698s); enabled has3 (same journal warning, Zen30.665s, EOS HTTP cancellation).
No layout warning appears in the final debug run. These startup/shutdown warnings
remain observations and were not repaired or claimed caused by event code.
## Static checks and hosted CI

- PASS: `python scripts/validate_project.py`, `python scripts/validate_backlog.py`
  (45task issues/6epics), Git/LFS asset checks and staged privacy/output checks.
- PASS: `python -B -m unittest discover -s scripts/tests -p 'test_*.py'`, **25/25**.
- PASS: report-verdict **81 synthetic cases** in native Windows PowerShell5.1 and
  PowerShell7. Missing Events, old17-report, partial/skipped/duplicate/warning/
  malformed reports fail the new All 23 gate. Synthetic cases are not UE tests.
- PASS: PowerShell parser, including the developer helper, under5.1 and7. The additional 8 registry-output
  regression cases pass in both shells.
- PASS: [code PR CI](https://github.com/Hazernit/fog-of-command/actions/runs/40497970183)
  on 775e9c1 and [script-stage PR CI](https://github.com/Hazernit/fog-of-command/actions/runs/40498675048)
  on 6ad6e64 and [final helper CI](https://github.com/Hazernit/fog-of-command/actions/runs/40500176949)
  on ecc8264, both Windows/Ubuntu jobs. Hosted UE build/Automation/Editor-PIE are
  explicitly **NOT RUN**, no UE installation or raw-artifact upload.

## Other diagnostics and limits

Automation startup still reports **425/428 success,3 failures** tracked in
[#55](https://github.com/Hazernit/fog-of-command/issues/55):
FUnifiedErrorTest_CreateErrorMessage, FUnifiedErrorTest_CreateErrorMessageWithContext,
FStructuredLogFormatTest. There are **17 LogAutomationTest Error-severity lines**,
all before the project queue; additional Error-severity lines **0**, project-queue
and later errors **0**. Fatal/assert/ensure/GPU-crash markers **0**. A Zen HTTP
connect diagnostic has the word Error in a Display message; it is not an extra
Error-severity result.

Two Warning-severity observations in Automation: Zen AutoLaunch health timeout
**30.516s**, and legacy layout key v1.5/v1.6. They were not repaired or assigned a
new cause; startup/log is not claimed clean. The interrupted first debug launch
also observed an HTTP timeout while left running; no network/security change was
made. System locale and process culture were not overridden.

- NOT RUN: GUI/PIE/Possess/reopen/input, GPU rendering/stress, Game/Shipping/Test
  binaries/packaging, Engine rebuild, cross-platform runtime/codec, journal/history
  serialization, concurrency/load testing and uint64 sequence-exhaustion runtime
  injection. Source-level Shipping/Test guards are not binary build evidence.
- NOT RUN: #55/#57 cause isolation or Zen/layout repair. No adjacent Issue started.
- The acceptance clone has unchanged tracked Config/Content/C++ values; UE generated
  only an untracked local Config/DefaultInput.ini besides ignored outputs. It was
  not staged/copied/published. The development feature checkout stays clean after
  commits. MyProject, Engine sources, main, system/network/security settings were
  not modified. Draft PR remains unmerged; Issue acceptance belongs to the owner.
Final evidence and parser fixes are documentation/PowerShell changes only;
all C++ and asset content is byte-for-byte unchanged from the clean-built 775e9c1
revision. Review/acceptance and merge remain with the owner.

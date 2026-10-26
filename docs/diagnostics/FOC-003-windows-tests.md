# FOC-003: Windows build and Automation Tests

Date: 2026-10-26 (Europe/Moscow). Issue: [#9](https://github.com/Hazernit/fog-of-command/issues/9).
Branch: `feat/foc-003-tests`.

## Preparation

- Read current AGENTS, architecture, roadmap, Windows development guide, engine pin and Issue #9.
- FOC-002 is merged in main at `280f8b5d0ad73aaea26c12a8a9440ff123f9f21d`; local main fast-forwarded to it before creating this branch. Initial working tree was clean.
- Git LFS 3.7.0: local hooks initialized, pull and fsck PASS. Existing Startup map remains an LFS asset; no new binary assets are planned.
- Use the accepted UE 5.8.4 CL58902052 / VS2022 17.14.42 / MSVC 14.44.35229 / Windows SDK 10.0.22621.0. No installers or toolchain changes.
- Fresh local inventory differs from the historical IDE pin: VS2022 is now **17.14.43** (installation 17.14.37824.6). UE **5.8.4 CL58902052**, actual MSVC **14.44.35229** (folder 14.44.35207) and SDK **10.0.22621.0** are still installed. This task does not change ENGINE_VERSION.md or approve a new baseline; record actual UBT selection after the new build. The agent did not install/update the IDE.
- Build and run from a new ASCII checkout outside OneDrive. The original location-specific C1083 remains [#57](https://github.com/Hazernit/fog-of-command/issues/57); its cause is not investigated here.
- Known ru-RU startup self-test failures remain [#55](https://github.com/Hazernit/fog-of-command/issues/55). Do not change locale or suppress them. Project Automation results and engine startup diagnostics are separate evidence.

## Results

| Check | Status | Evidence |
|---|---|---|
| Main synchronization / feature branch / LFS | PASS | Clean tree, fast-forward main, LFS pull/fsck |
| Windows runner syntax / report failure handling | PASS (static/synthetic) | PowerShell parser; 19 report validator cases, including zero/partial/skipped/failed/wrong-suite/malformed-type reports, in Windows PowerShell 5.1 and PowerShell 7 |
| Local static project validator / Python negative tests | PASS (static/synthetic) | Descriptor/module/targets/maps/LFS contract; 16 Python cases; backlog validator |
| First actual project Automation run | Suite PASS, wrapper FAIL | Three exact report tests Success with zero warnings/errors; native exit unavailable to WinPS5 wrapper, so overall FAIL |
| Corrected command-line runner | PASS | Native/wrapper 0; all three project tests Success, zero project warnings/errors; 18.212 s wall |
| Controlled bad-Config negative gate | PASS (expected test FAIL) | Wrong GameDefaultMap: Configuration Fail, other two Success; native 255 / wrapper 1; original Config restored byte-for-byte |
| Final restored run | PASS | Commit 306c864, native/wrapper 0, 3/3 Success, zero project warnings/errors, 18.476 s wall |
| First new clean Windows C++ build | FAIL | Native/wrapper 6, 62.653 s wall / 61.30 s UBT; C2672 in test line 143; selected pinned MSVC/SDK PASS |
| Corrected clean Windows C++ build | PASS | New remote clone 623d543, native/wrapper 0, 63.619 s wall / 62.70 s UBT; no compiler/linker warnings/errors |
| Hosted static checks | PASS | [PR run 40306728289](https://github.com/Hazernit/fog-of-command/actions/runs/40306728289) on 306c864, Ubuntu + Windows; UE NOT RUN |
| Hosted UE build / Automation | NOT RUN | Hosted runners have no configured licensed UE installation; local Windows gate required |
| Editor GUI / PIE / GPU rendering | NOT RUN | FOC-003 targets headless Automation; prior FOC-002 observations are not new tests |
| Owner review / merge | NOT RUN | Draft PR; no automatic merge |

Only reviewed summaries belong here. Raw UBT/Editor logs, JSON reports, command files, generated outputs and private absolute paths stay in a local evidence directory outside Git. MyProject and Unreal Engine source are not changed. FOC-004 is not started.

## First actual C++ build and correction

A new remote clone of `0a9ea586d55e7a10259723d9db67dd07e53949b3` downloaded the existing LFS map; pull/fsck and map hash PASS. Before build, Binaries/Intermediate/Saved/DerivedDataCache were all absent. Build ran 13:11:27.178–13:12:29.846 +03:00: native/wrapper **6**, **62.653 s wall / 61.30 s UBT**. UBT selected **Visual Studio 2022 / MSVC 14.44.35229 / Windows SDK 10.0.22621.0**, matching the compiler/SDK pins despite the observed IDE patch drift.

The test failed compilation with **C2672**, not a toolchain or map failure:

```text
FogOfCommandSmokeTests.cpp(143): error C2672: FAutomationTestBase::TestNotNull: no matching overload
TestNotNull(..., Mesh->GetStaticMesh());
Cannot deduce const ValueType* from TObjectPtr<UStaticMesh>.
Result: Failed (OtherCompilationError)
```

This excerpt is translated/sanitized. The installed UE header confirms `GetStaticMesh()` returns `TObjectPtr<UStaticMesh>`. Pass `.Get()` to the pointer assertion; no Engine edit. A new clean clone/build is required after this fix; runtime Automation remains **NOT RUN** at this stage. Raw evidence is preserved outside Git; the failed clone is not cleaned/deleted.

Hosted static checks on this commit actually passed on both Ubuntu and Windows, including LFS, project/backlog contracts, 16 Python cases and Windows PowerShell parser/19 synthetic report cases. This does not imply the C++ compilation passed.

Reproduction commands and interpretation: [TESTING.md](../TESTING.md). Synthetic validator tests are not Unreal tests.

## Corrected clean Windows build

A second, new remote clone downloaded `623d5436aee1bb1b47259b025c51f3ed1c80a24b`; no source, binaries or cache folders were copied from the failed clone. Git LFS pull/fsck PASS. Startup map SHA256 remained `8a9a48b6dcfbb9e0b7e6d90cd5fc459745c1cd675aae025fc78c55537ae014b8`. Binaries, Intermediate, Saved and DerivedDataCache were absent before the build.

**13:14:25.757–13:15:29.395 +03:00:** native/wrapper **0**, **63.619 s wall / 62.70 s UBT**, 10 actions. UBT selected **VS2022 / MSVC 14.44.35229 / SDK 10.0.22621.0**; compiler/linker warnings **0**, errors **0**. The new Automation C++ source was compiled and the Editor module linked. Shared Engine/UBA caches were not cleared; this is a clean project build on the installed binary engine, not an Engine rebuild or cold-cache benchmark.

The current VS2022 IDE patch is **17.14.43**; this observation does not rewrite the historical baseline of 17.14.42. No software was installed, upgraded or downgraded by this task; actual compiler/SDK remained pinned.

## First actual Automation run and Windows PowerShell correction

Run **13:15:51.656–13:16:13.083 +03:00**, 21.382 s, loaded the real C++ tests in unattended NullRHI and exported all three exact project tests as Success, with zero project warnings/errors. However, Windows PowerShell 5.1 returned a null process ExitCode after asynchronous Start-Process/redirects/WaitForExit. The wrapper correctly returned **1 / FAIL** rather than interpreting the missing native code as success. This is not a fully passing command-line run; the log's completion marker is not a substitute for a native exit observation.

A small local Windows PowerShell 5.1.26100.9711 probe reproduced the issue using an owned child that delays 500 ms and exits 37: without reading its handle before waiting, ExitCode was null; with its handle cached while alive, ExitCode was 37. The runner now caches the owned handle before the bounded wait. No execution-policy/global settings changed; no log-based exit fallback was introduced. A new real Automation run is required to validate the correction.

## Corrected real command-line run

The acceptance clone fast-forwarded from GitHub to `6c17d6986c1cc89fdc4a5e2873b783a498be290b` (only runner/docs changed after the clean C++ build). **13:19:40.152–13:19:58.409 +03:00:** native **0**, wrapper **0**, fresh report validation **PASS**, **18.212 s wall**, no timeout/launch error. All three exact project tests were Success with zero project warnings/errors. This verifies the actual native exit collection and end-to-end Windows command.

The first two logs use the default AutomationTest category verbosity: 17 AutomationTest error lines are visible, but named startup-result totals are not emitted at that verbosity. Do not derive exact per-test startup counts from an absent summary. The runner now requests process-local `LogAutomationTest Log` so the upcoming controlled negative/restored runs also preserve names/totals; no locale or saved settings change.

## Controlled negative gate and restored final run

The same acceptance clone fast-forwarded from GitHub to `306c8643582c3edd29fc54bc0a776dd615a24288`. Source/assets did not change after the clean build. Only in this disposable clone, a local backup preserved the exact original bytes of Config/DefaultEngine.ini. Temporarily change **GameDefaultMap only** from `/Game/Maps/Startup.Startup` to `/Engine/Maps/Entry.Entry`; EditorStartupMap, native module and saved map remain intact. The original Config is restored in a finally block and its SHA256 compared before/after. The negative setting is never committed or copied to the main working checkout.

| Run (+03:00) | Actual project result | Native / wrapper | Wall time |
|---|---|---|---|
| Negative: 13:21:43.922–13:22:02.820 | Configuration **Fail** (1 expected assertion), Module/StartupMap Success | **255 / 1** | 18.850 s |
| Restored: 13:22:03.620–13:22:22.153 | All three **Success**, 0 project errors/warnings/skipped/in-progress | **0 / 0** | 18.476 s |

Thus the actual Unreal test, Automation exit verdict and wrapper reject a broken configuration, then pass after restoration; this is not only a synthetic report-parser check.

Final fresh `index.json` contains exactly:

| Full test name | State | Test duration | Errors / warnings |
|---|---|---|---|
| FogOfCommand.Smoke.Configuration | Success | 0.007561 s | 0 / 0 |
| FogOfCommand.Smoke.Module | Success | 0.007948 s | 0 / 0 |
| FogOfCommand.Smoke.StartupMap | Success | 0.151046 s | 0 / 0 |

Report counters: succeeded **3**, succeededWithWarnings/failed/notRun/inProcess **0**; total test duration **0.166555 s**. The 18.476 s process wall time includes Editor startup/discovery/reporting. NullRHI is confirmed; these are real C++ Editor Automation/asset checks, not rendering or PIE.

After restoration and execution, tracked Source/Config/Content have no diff and the Startup map SHA256 is unchanged. Editor created **untracked Config/DefaultInput.ini** in the disposable clone; it is left local, not copied, staged or published. This generated file is not evidence that the entire working tree is clean; only tracked project files are unchanged.

## Startup diagnostics remain separate (#55)

The final negative/restored runs with process-local test logging each emitted **428 named Engine startup results: 425 successful, 3 failed**. The failures are exactly the known `FUnifiedErrorTest_CreateErrorMessage`, `FUnifiedErrorTest_CreateErrorMessageWithContext`, and `FStructuredLogFormatTest`, with **7 + 4 + 6 = 17 CHECK/error lines**. No culture override, global locale change or error suppression was used; #55 is not fixed.

The restored positive log contains those 17 AutomationTest Error lines and **0 other Error lines**. The intentional negative log additionally contains two AutomationController error lines for its expected project failure. Neither log contains a detected fatal/assert/ensure/device-removed/GPU-crash marker. Two non-project warnings are visible: AssetRegistry's file journal is unavailable on the engine volume (uncached discovery), and LayoutService reports an older layout key. These warnings were not repaired; the full startup log is not claimed warning/error-free. Each final project report still has zero project warnings, and the positive report zero project errors.

## CI and remaining boundaries

Actual hosted PR CI for `306c864` passed on Ubuntu and Windows: [run 40306728289](https://github.com/Hazernit/fog-of-command/actions/runs/40306728289). LFS object verification, static descriptor/module/targets/maps checks, 16 Python regression cases and backlog checks passed on both; PowerShell syntax/19 synthetic report cases passed on Windows. Linux correctly skips the Windows-only steps. The summary explicitly states hosted **Unreal build / Automation / Editor-PIE: NOT RUN**; it installs no UE and uploads no raw logs.

Hosted CI annotations warn about the Node20 runtime of checkout@v4/setup-python@v5 (GitHub forces Node24), plus an Ubuntu runner migration notice. These are CI-platform warnings, not UE results; action-runtime migration is not claimed as tested/fixed here.

**NOT RUN:** GUI Editor/PIE/reopen/input/GPU rendering or stress for this issue; standalone/Game target/Shipping/packaging; Engine rebuild; cold-cache benchmark; live timeout-kill integration (bounded code path reviewed, synthetic failure cases run); original Unicode/OneDrive cause isolation (#57); owner review/merge. FOC-004 is not started. The Windows CLI/config/module/saved-map tests and clean project build are the actual scope proved above.

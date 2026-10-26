# Tests and Windows build gate (FOC-003 / FOC-004)

Use the pinned environment in [ENGINE_VERSION.md](ENGINE_VERSION.md). Work in an
ASCII checkout outside OneDrive; the location-specific C1083 is tracked in
[#57](https://github.com/Hazernit/fog-of-command/issues/57). Do not change the
engine/compiler/SDK or system locale for a test. No new software is installed by
these scripts. See [actual results](diagnostics/FOC-003-windows-tests.md), including
any difference between the accepted IDE version and current local inventory.

## Static checks (no Unreal required)

From the checkout root with Python 3, Git and Git LFS available:

```powershell
git lfs install --local
git lfs pull
git lfs fsck
python scripts/validate_backlog.py
python scripts/validate_project.py
python -m unittest discover -s scripts/tests -p "test_*.py"
powershell.exe -NoProfile -File scripts/tests/test_automation_report.ps1
```

These check repository contracts and failure handling. Passing them does **not**
mean the C++ module was compiled, the binary map loaded in UE, or Unreal tests ran.
Synthetic report-validator cases are not Unreal Automation Tests.

## Real clean C++ build on Windows

Clone the branch being reviewed into a **new** ASCII directory outside OneDrive,
then pull LFS. Before the build, Binaries, Intermediate, Saved and DerivedDataCache
must be absent to claim a clean project build. The scripts do not delete old
outputs or shared Engine/UBA/DDC caches. A clean project build is not an Engine
rebuild or a cold-cache performance measurement.

Set `$env:UE_ROOT` to the local installed UE root. The evidence directory below
must remain outside **all** Git checkouts and outside Engine/project directories.

```powershell
$evidence = Join-Path $env:TEMP 'FOC003-local-evidence'
./scripts/windows/build_fog_of_command.ps1 `
  -EngineRoot $env:UE_ROOT -EvidenceRoot $evidence
# Check native/wrapper exit codes and selected MSVC/SDK in build-result.json.
if ($LASTEXITCODE -ne 0) { throw 'C++ build failed; preserve local evidence.' }
```

## Real Unreal Automation from the Windows terminal

After a successful Development Editor / Win64 build, the default `All` selection
runs the combined **nine-test** project gate (three Smoke + six UnitState):

```powershell
./scripts/windows/run_fog_of_command_tests.ps1 `
  -EngineRoot $env:UE_ROOT -EvidenceRoot $evidence -TimeoutSeconds 600
if ($LASTEXITCODE -ne 0) { throw 'Automation failed or did not complete; inspect local evidence.' }
```

For diagnosis, append `-Suite Smoke` (three tests) or `-Suite UnitState` (six tests)
to a separate invocation. Each subset validates its own exact set; a subset PASS
does not replace an `All` run. The shared expected-name manifest is
`Get-FocExpectedTests` in `scripts/windows/automation_report.ps1`. Additions or
renames must update that manifest, the compiled tests and the documentation.

Each invocation creates a unique external directory containing the exact command,
raw Editor/stdout/stderr logs, `AutomationReport/index.json` and `test-result.json`.
The runner refuses a missing built module or wrong UE patch/CL. On timeout it
terminates only its own unattended process, preserves evidence and returns failure.
It never closes another Editor session or deletes project/cache files.

The native command is equivalent to the following (each `$report` must be a **new**
external directory; this raw command alone does not validate the exported report):

```powershell
$editor = Join-Path $env:UE_ROOT 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$project = Join-Path $PWD 'FogOfCommand.uproject'
# Set $report and $localLog to fresh locations outside Git first.
& $editor $project -unattended -nop4 -nosplash -nullrhi -stdout -FullStdOutLogOutput `
  '-LogCmds=LogAutomationTest Log' `
  -TestLoops=1 '-ExecCmds=Automation RunTests StartsWith:FogOfCommand.;Quit' `
  "-ReportExportPath=$report" "-abslog=$localLog"
```

`StartsWith:FogOfCommand.` selects the project namespace. Subsets instead use
`StartsWith:FogOfCommand.Smoke.` or `StartsWith:FogOfCommand.UnitState.`. `;Quit` is part of the Automation
command queue, so it exits after the report is finished with the Automation
verdict. Do not append an immediate standalone engine Quit or treat a
`-TestExit` log phrase as a successful test verdict. No `-culture=en` is used.
The installed UE source was checked for these command/report semantics.
The process-local LogAutomationTest verbosity preserves named engine startup
test results for diagnosis; it does not suppress errors or change saved preferences.

Expected tests use EditorContext + EngineFilter. Smoke tests require editor
development automation; Core-only UnitState tests use WITH_DEV_AUTOMATION_TESTS
and are selected in the Editor context. No additional gameplay subsystem:

| Full test name | Contract |
|---|---|
| `FogOfCommand.Smoke.Configuration` | Default maps, native GameMode and project configuration |
| `FogOfCommand.Smoke.Module` | Loaded native C++ module, GameMode and standard DefaultPawn |
| `FogOfCommand.Smoke.StartupMap` | Load the saved LFS map in UE; check floor/props, collision, spawn and lighting |
| `FogOfCommand.UnitState.Identifiers` | Identifier creation, validity and uniqueness |
| `FogOfCommand.UnitState.Lifecycle` | Supported lifecycle transitions and stable identity |
| `FogOfCommand.UnitState.Resources` | Resource changes and bounds |
| `FogOfCommand.UnitState.Validation` | State validation and rejection of invalid data |
| `FogOfCommand.UnitState.RepeatedOperations` | Repeated operations preserve required state invariants |
| `FogOfCommand.UnitState.Serialization` | Serialization round trip and persisted identity/state |

The wrapper requires native exit **0**, all nine exact names once for `All` (or
the exact three/six for the explicitly selected subset), every state
`Success`, zero project warnings/errors and complete consistent report counters.
Missing/malformed/partial/zero-test reports, duplicates, wrong suites, skipped or
in-progress tests fail even if the process exits 0. Reports are never reused.
Old Smoke-only reports cannot satisfy the default combined gate. Names in this
table describe test intent, not an assertion that the new tests were compiled or
run. FOC-004 Windows results must be recorded separately when actually obtained.

This is **NullRHI editor Automation**: actual module/configuration/asset loading,
not rendering, PIE, input controls or GPU stability. GUI/PIE remains the manual
smoke described in [Windows development](WINDOWS_DEVELOPMENT.md).

## Known engine diagnostics and honest CI

The ru-RU startup self-tests in [#55](https://github.com/Hazernit/fog-of-command/issues/55)
can emit three FAIL / 17 CHECK errors before the project suite. Record those
separately; a project-suite PASS does not mean the startup log is clean or #55 is
fixed. Inspect all new errors/fatals and preserve local raw evidence. Do not change
locale, discard logs or count unselected built-in tests as project successes.

GitHub Actions runs the available static checks on hosted Linux/Windows runners,
including PowerShell parser/report-validator checks on Windows. Its summary must
explicitly say **UE C++ build / Unreal Automation / Editor-PIE: NOT RUN**. No UE is
installed/downloaded there; no successful hosted job is evidence of an Unreal
build. The required real Windows build/Automation gate is manual until a licensed,
pinned UE runner is separately configured. No untrusted PR runs on this PC.

Use the PR template with **PASS / FAIL / NOT RUN**, exact commit/toolchain/commands
and a sanitized report link. Never upload raw logs, full Automation reports,
machine identifiers, private paths or auto-generated local Config. Keep generated
folders out of Git. Packaging, Shipping, Engine rebuild and owner acceptance need
their own evidence; do not infer them from this suite.

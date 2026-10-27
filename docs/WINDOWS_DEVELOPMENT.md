# Windows — Fog of Command: verified setup and first UE project

**Baseline accepted 2026-10-26:** Unreal Engine **5.8.4 CL58902052**, Visual Studio **2022 17.14.42**, MSVC **14.44.35229** (installed toolset folder `14.44.35207`), Windows SDK **10.0.22621.0**. Use this known-working combination for FOC-002. Other installed toolchains exist but are not the project baseline.

## Evidence and reproducibility

- [Pinned environment and caveats](ENGINE_VERSION.md)
- [Full sanitized two-run Windows smoke report](diagnostics/FOC-046-windows-smoke.md)
- [Portable C++ clean smoke script](../scripts/windows/run_foc046_clean_smoke.ps1)
- [Portable GPU telemetry script](../scripts/windows/collect_foc046_gpu_telemetry.ps1)
- [FOC-047: 3 ru-RU startup self-test failures](https://github.com/Hazernit/fog-of-command/issues/55) — nonblocking, not fixed.

The completed 2026-10-26 external Blank C++ smoke used UBT auto-selected VS2022/MSVC 14.44/SDK22621, reported **exit 0 in 87.041s**, loaded the Editor C++ module, ran 3 short PIE cycles and 30m30.839s continuous PIE, and reopened the Editor successfully. This checks that configuration on the tested PC, **not** all future maps, SM6/Lumen/Nanite, or an independent GPU stress test.

## FOC-002: Windows developer setup

1. Work **locally on Windows** with a clean checkout of `Hazernit/fog-of-command` and latest `main`. Check `git status` before touching files. Do not edit old `MyProject` or disposable `Foc046Smoke`.
2. Verify the **actual** Unreal Engine installation via `Engine/Build/Build.version`, Visual Studio 2022 C++ toolchain and installed Windows SDK. Engine installation paths differ per PC: **never hardcode** an example drive letter or copy a private local path into Git.
3. Verify `git lfs version` and `git lfs install` before writing `.uasset` or `.umap`; `.gitattributes` specifies LFS filters. If unavailable, stop before committing binary assets and report a blocker; don't upload broken LFS pointers.
4. Start from a basic **Games → Blank → C++** Unreal project using the pinned UE version; name it **FogOfCommand**, in this repository. Create minimal code module and GameMode, a minimal level built from simple licensed primitives, and a clear startup-map configuration. Do not import Starter Content or paid marketplace assets.
5. Never commit generated `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, local `.sln` or raw Editor/UBT logs. Review staged diffs for secrets, personal paths and stray binaries.
6. Use the installed `Engine/Build/BatchFiles/Build.bat` to compile `FogOfCommandEditor Win64 Development -Project=<absolute_path_to_FogOfCommand.uproject> -WaitMutex`; record the **real** exit code, duration and compiler chosen by UBT. Do not claim a clean checkout PASS without actually doing the test from a clean checkout.
7. Open Editor, load the minimal level, Play In Editor, Stop, close and reopen. Record real evidence. The pre-existing ru-RU startup self-test failures are separate from build/PIE result; report them accurately and do not globally change the user's locale to conceal them.

## Git workflow

- One issue / one feature branch: `feat/foc-002-unreal-project`, one early **Draft PR** referencing [FOC-002 #8](https://github.com/Hazernit/fog-of-command/issues/8).
- Commit and push **each completed logical step** in the feature branch. Do not push to `main` or merge yourself.
- Commit source, Config, project descriptor, reproducible map assets (with LFS), tests, docs, and sanitized test reports. Keep raw diagnostics local outside Git.
- PR must state what is working, test commands, `PASS`/`FAIL`/`NOT RUN`, third-party asset provenance, and remaining issues.

## FOC-003: build and Automation gate

Follow [TESTING.md](TESTING.md) for static checks, a new ASCII clone build and
`run_fog_of_command_tests.ps1`. By default it runs all twenty-three
`FogOfCommand` tests (three Smoke, six UnitState, eight Orders, six Events) with NullRHI and requires a complete successful JSON
report plus native exit 0. It does not claim Editor GUI/PIE/GPU rendering coverage.
Known startup failures #55 remain separate and visible; the location-specific
build problem is tracked in [#57](https://github.com/Hazernit/fog-of-command/issues/57).
Record actual versions/results in the [FOC-003 report](diagnostics/FOC-003-windows-tests.md).
Hosted CI has no configured UE installation: real UE build/tests are a local manual
Windows gate, explicitly **NOT RUN** on hosted runners.

FOC-004 adds Core-only [UnitState values and lifecycle](UNIT_STATE.md). Use the
default `-Suite All` combined gate when reviewing it; optional Smoke/UnitState
subsets are diagnostics and cannot substitute for that gate. Current domain/build
results and known limitations are in the [FOC-004 report](diagnostics/FOC-004-windows-unit-state.md).

FOC-005 adds the [single-unit MOVE/HOLD lifecycle](ORDERS.md), without navigation
or scene execution. Review it with default `-Suite All` (23 tests); `-Suite Orders`
selects its eight tests only for diagnosis and cannot replace the full gate.
The [FOC-005 report](diagnostics/FOC-005-windows-orders.md) records the real build,
Automation results and publication limitations separately from hosted static CI.

## Troubleshooting boundaries

### Existing FOC-002 project

The repository now includes `FogOfCommand.uproject`, native GameMode and the LFS
`Content/Maps/Startup.umap`. Follow the [clone/build/Play instructions](../README.md#сборка-и-запуск-windows)
and [actual FOC-002 results](diagnostics/FOC-002-windows-smoke.md). Use an ASCII
checkout outside OneDrive: the original Unicode/OneDrive checkout failed to open
a PCH intermediate file (C1083), while fresh ASCII clones built successfully.
The exact cause of that location-specific difference has not been isolated.
Do not copy generated folders or local auto-generated input preferences into Git.
If Windows displays a firewall permission prompt, the owner handles it manually;
do not automatically grant network access for this local-only scene.

### Diagnostic boundaries

- If UBT unexpectedly selects MSVC 14.51 or another SDK, stop and diagnose version selection before adding code workarounds. Do not patch engine source or install another IDE/SDK without approval.
- If a GPU/device crash occurs, stop and capture a sanitized report; do not assume the game code caused the crash.
- If a project crashes on opening, avoid random cache deletion; first preserve evidence and isolate the broken project state.

FOC-006 adds the [domain event journal and opt-in isolated debug commands](EVENT_LOG.md).
Run all23 tests for acceptance; -Suite Events is a six-test diagnostic subset.
GUI/PIE and Shipping require separate evidence; developer commands never mutate live units.

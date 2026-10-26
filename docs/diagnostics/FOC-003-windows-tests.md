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
| Windows runner syntax / report failure handling | PASS (static/synthetic) | PowerShell parser; 12 report validator cases, including zero/partial/skipped/failed/wrong-suite reports, in Windows PowerShell 5.1 and PowerShell 7 |
| New project Automation Tests | NOT RUN | Implementation and actual Windows execution pending |
| New clean Windows C++ build | NOT RUN | Pending new remote clone |
| Hosted static checks | NOT RUN | Workflow pending |
| Hosted UE build / Automation | NOT RUN | Hosted runners have no configured licensed UE installation; local Windows gate required |
| Editor GUI / PIE / GPU rendering | NOT RUN | FOC-003 targets headless Automation; prior FOC-002 observations are not new tests |
| Owner review / merge | NOT RUN | Draft PR; no automatic merge |

Only reviewed summaries belong here. Raw UBT/Editor logs, JSON reports, command files, generated outputs and private absolute paths stay in a local evidence directory outside Git. MyProject and Unreal Engine source are not changed. FOC-004 is not started.

Reproduction commands and interpretation: [TESTING.md](../TESTING.md). Project tests have not yet run at this stage; synthetic validator tests are not Unreal tests.

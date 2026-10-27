# FOC-006 Windows event journal verification

Issue: [#12](https://github.com/Hazernit/fog-of-command/issues/12).
Branch: `feat/foc-006-event-log`.
Base: `1e8c87c` (accepted FOC-005 on main).

## Preparation

- PASS: synchronized clean local main with origin/main using fast-forward.
- PASS: read current AGENTS, architecture, UnitState, orders, testing, Windows
  development guide, roadmap and relevant GDD sections; dependency FOC-004/005
  is accepted. Work is confined to FOC-006.
- PASS: Git LFS 3.7.0 available; `git lfs fsck` passed.
- The [event contract](../EVENT_LOG.md) records the implementation boundary.

## Checks pending implementation

| Check | Status |
|---|---|
| Clean Windows Development Editor C++ build | PASS: native/wrapper0, 118.989s, UBT115.61s,18 actions,0 compiler warnings/errors |
| All 17 prior project Automation Tests plus new Events tests | NOT RUN |
| Developer commands on minimal startup map | NOT RUN |
| Static validators and negative test cases | PASS: Python25/25, PowerShell report81 cases on5.1 and7; parser/project/backlog/LFS |
| Hosted static CI | PASS: [code PR run](https://github.com/Hazernit/fog-of-command/actions/runs/40497970183), Windows+Ubuntu; UE build/Automation NOT RUN there |
| Editor GUI / PIE / GPU / packaging / Shipping build | NOT RUN |

No new software/toolchain is installed or selected, and no Engine source,
MyProject, system locale, networking or security settings are modified. Raw local
evidence and generated folders remain outside Git. This report will be replaced
with measured results; planned coverage is not a test PASS.

Clean build tested source commit775e9c1 from a new external ASCII GitHub clone on
2026-10-27,14:42:15.269 to14:44:14.275 Moscow time. Binaries/Intermediate/Saved/
DerivedDataCache absent before launch, shared Engine/UBA/DDC caches retained.
UE5.8.4CL58902052; UBT selected VS2022/MSVC14.44.35229, SDK10.0.22621.0.
Current installed VS2022 display17.14.43 differs from historical accepted17.14.42;
agent did not install/update/downgrade it. Toolchain target pins are unchanged.
GUI/PIE/GPU/Shipping/packaging/Engine rebuild remain NOT RUN.

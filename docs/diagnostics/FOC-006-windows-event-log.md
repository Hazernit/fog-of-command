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
| Clean Windows Development Editor C++ build | NOT RUN |
| All 17 prior project Automation Tests plus new Events tests | NOT RUN |
| Developer commands on minimal startup map | NOT RUN |
| Static validators and negative test cases | NOT RUN |
| Hosted static CI | NOT RUN |
| Editor GUI / PIE / GPU / packaging / Shipping build | NOT RUN |

No new software/toolchain is installed or selected, and no Engine source,
MyProject, system locale, networking or security settings are modified. Raw local
evidence and generated folders remain outside Git. This report will be replaced
with measured results; planned coverage is not a test PASS.

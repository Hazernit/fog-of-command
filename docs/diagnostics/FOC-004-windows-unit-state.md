# FOC-004: domain UnitState and Windows verification

Date: 2026-10-26 (Europe/Moscow). Issue: [#10](https://github.com/Hazernit/fog-of-command/issues/10).
Branch: `feat/foc-004-unit-state`.

## Preparation and scope

Current AGENTS, ARCHITECTURE, ROADMAP, TESTING, Windows guide and Issue #10 were read. FOC-003 is accepted and merged; local main fast-forwarded to `4e24f8da0b940c25734d9bf471de8b2cd719aff4` before branching. Initial tree was clean; Git LFS pull/fsck PASS.

Implement Core-only UnitId, SideId, OrderId and UnitState, explicit invalid-state/resource results, stable identity through movement/damage/destruction, idempotent initialization/destruction and a small versioned value-state serialization boundary. No Actors, Tick, UI, command execution, WorldState simulation, save-game subsystem or FOC-005.

Fresh local inventory: UE **5.8.4 CL58902052**, actual MSVC **14.44.35229** (file product version 14.44.35229.0), SDK **10.0.22621.0** present. Current VS2022 IDE is **17.14.43**, differing from historical 17.14.42; this task did not update it or change ENGINE_VERSION.md. Actual UBT-selected compiler/SDK will be recorded after build. No installs/updates or toolchain changes.

Use a new ASCII checkout outside OneDrive for the real clean build, preserving raw evidence outside Git. Known startup tests #55 and location-specific C1083 #57 remain separate; no locale override or cause-isolation experiments.

## Stage results

| Check | Status | Evidence |
|---|---|---|
| Main / dependency / branch / LFS | PASS | FOC-003 merged, fast-forward main, clean tree, LFS pull/fsck |
| Domain implementation and Automation Tests | NOT RUN | Development in progress |
| Static checks / CI for this branch | NOT RUN | Pending implementation |
| New clean Windows C++ build | NOT RUN | Pending new remote clone |
| Old and new Unreal Automation Tests | NOT RUN | Pending compiled domain model |
| Hosted UE build / Automation | NOT RUN | No configured UE on hosted runner; local Windows manual gate |
| GUI/PIE/GPU, packaging/Shipping, Engine rebuild/cold-cache | NOT RUN | Domain/Automation scope; no new runtime scene claims |
| Owner review / merge | NOT RUN | Draft PR; no automatic merge |

Only sanitized summaries belong in Git. Raw UBT/Editor logs, JSON reports, local absolute paths, generated caches and auto-created local input Config remain outside Git. MyProject/Engine source are unchanged; FOC-005 and other Issues are not started.

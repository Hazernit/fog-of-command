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
| Domain implementation and Automation Tests | Implemented; runtime NOT RUN | Four Core-only domain files, six invariant test groups; compilation/runtime pending |
| Combined report gate / PowerShell syntax | PASS (synthetic/static) | 43 report cases on Windows PowerShell 5.1 and PowerShell 7; parser PASS; default All requires 3 old + 6 new exact tests |
| Local static checks | PASS | Project/LFS/domain-boundary validator, 19 Python regression cases, 43 PowerShell report cases, backlog validator and staged diff/privacy review |
| Hosted static CI | PASS | [40312839893](https://github.com/Hazernit/fog-of-command/actions/runs/40312839893) on c704a3b, Ubuntu + Windows; UE NOT RUN |
| First new clean Windows C++ build | FAIL | c704a3b, native/wrapper 6, 67.771 s wall / 66.01 s UBT; missing Optional header include |
| Corrected clean Windows C++ build | NOT RUN | Pending new remote clone after include correction |
| Old and new Unreal Automation Tests | NOT RUN | Pending compiled domain model |
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

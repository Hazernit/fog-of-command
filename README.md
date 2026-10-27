# Fog of Command

**Статус:** минимальный Unreal C++ проект FOC-002: стартовая карта, native GameMode и штатная тестовая камера. Реальные результаты clean clone/build/Editor/PIE — в [отчёте FOC-002](docs/diagnostics/FOC-002-windows-smoke.md). Дальнейшие игровые системы пока не реализованы. Рабочее название.

Военная стратегия с тремя масштабами: управление государством, тактические RTS-операции и выборочное прямое управление солдатами, техникой и дронами. Центральная механика — **разница между реальным состоянием мира и тем, что игрок знает по разведданным**.

> Проект ориентирован на вымышленный мир и игровые абстракции вооружений. Первая цель — **маленький играбельный PC-прототип**, а не глобальный MMO, мобильная версия или магазин скинов.

## Главное

- [Полный GDD v0.1 (Markdown)](docs/Fog_of_Command_GDD_v0.1.md) · [PDF](docs/Fog_of_Command_GDD_v0.1.pdf) · [Word](docs/Fog_of_Command_GDD_v0.1.docx)
- [Дорожная карта](docs/ROADMAP.md) — этапы и критерии выхода
- [Закреплённый Windows toolchain](docs/ENGINE_VERSION.md) · [Windows setup](docs/WINDOWS_DEVELOPMENT.md) · [проверенный smoke](docs/diagnostics/FOC-046-windows-smoke.md)
- [Архитектура](docs/ARCHITECTURE.md) — состояние мира, разведка, команды, инварианты
- [Правила для Codex](AGENTS.md) · [Как передавать задачи](docs/CODEX_WORKFLOW.md)
- [Локальный backlog](backlog/README.md) — задачи, которые создаются на GitHub автоматически
- [Публикация на GitHub](docs/PUBLISHING.md) — одна команда после установки `git` и GitHub CLI

## Первое игровое доказательство

На тестовой 3D-карте игрок управляет несколькими подразделениями в RTS-режиме, отправляет разведывательный БПЛА, получает **неидеальные наблюдения**, а затем переключается на одного солдата или дрон. Остальные подразделения продолжают выполнять приказы. Сохраняется единое состояние сущностей и ресурсов.

## Решения

| Область | Решение |
|---|---|
| Первичная платформа | Windows PC |
| Движок | **Unreal Engine 5.8.4 CL58902052** (закреплён Windows baseline; VS2022 17.14.42 / MSVC 14.44.35229 / SDK 22621) |
| Языки | C++ (правила/состояние), Blueprints/UMG (сцены, UI) |
| Первая реализация | Одиночная тактическая операция |
| Далее | Глобальная кампания → PvP → мобильная адаптация/Roblox после отдельной оценки |
| Монетизация | Не pay-to-win. **В MVP её нет.** |

## Начало разработки

1. **FOC-001 / FOC-046**: Windows toolchain и C++/PIE smoke приняты как базовая конфигурация; незавершённые ru-RU startup tests отслеживаются отдельно в [FOC-047](https://github.com/Hazernit/fog-of-command/issues/55).
2. **FOC-002**: минимальный C++ проект, native GameMode и карта проверены на Windows и приняты после [ревью PR #56](https://github.com/Hazernit/fog-of-command/pull/56); [отчёт](docs/diagnostics/FOC-002-windows-smoke.md).
3. **FOC-003/004** приняты после [PR #58](https://github.com/Hazernit/fog-of-command/pull/58) и [PR #59](https://github.com/Hazernit/fog-of-command/pull/59): Automation gate и [доменный UnitState](docs/UNIT_STATE.md). **Текущая FOC-005** добавляет [доменный lifecycle MOVE/HOLD](docs/ORDERS.md), без Actors/Tick/UI или исполнения приказов; [результаты проверок](docs/diagnostics/FOC-005-windows-orders.md). Соседние задачи не реализуются.

## Сборка и запуск Windows

Требуются UE **5.8.4 CL58902052**, VS2022 **17.14.42**, MSVC **14.44.35229**, SDK **10.0.22621.0** и Git LFS. Новые версии автоматически не устанавливаются. `EngineAssociation=5.8` использует стандартную регистрацию Epic; скрипт сборки дополнительно проверяет точный patch/CL, а Target rules закрепляют compiler/SDK.

```powershell
# Выберите локальный ASCII-каталог вне OneDrive для воспроизведения сборки.
Set-Location $env:FOC_WORKSPACE
git clone https://github.com/Hazernit/fog-of-command.git
Set-Location fog-of-command
# После merge FOC-002 игровое основание доступно в main.
git lfs install --local
git lfs pull
```

Задайте `$env:UE_ROOT` локальным корнем установленной UE. Из корня репозитория:

```powershell
$evidence = Join-Path $env:TEMP 'FOC002-local-evidence'
./scripts/windows/build_fog_of_command.ps1 `
  -EngineRoot $env:UE_ROOT -EvidenceRoot $evidence
& (Join-Path $env:UE_ROOT 'Engine/Binaries/Win64/UnrealEditor.exe') `
  (Join-Path $PWD 'FogOfCommand.uproject') -dx12 -log
```

Editor должен открыть `/Game/Maps/Startup`: земля, три простых объекта, освещение и PlayerStart. Нажмите **Play → Selected Viewport**, щёлкните по сцене: штатная свободная камера Unreal — WASD, Q/E и мышь. **Esc** или **Stop** останавливает PIE; повторный Play запускает новую сессию. Это тестовая камера, игровое RTS-управление ещё не реализуется. Генерировать карту после clone не требуется: `.umap` хранится через LFS.

Для Visual Studio можно локально создать solution через контекстное меню `.uproject` → **Generate Visual Studio project files**; выбрать `Development Editor / Win64`. `.sln`, `Binaries`, `Intermediate`, `Saved`, `DerivedDataCache` в Git не входят. Скрипт не удаляет старые outputs: чистая сборка проверяется в новом clone, где этих каталогов ещё нет. Raw logs содержат локальные пути и остаются вне Git.

Известные три ru-RU startup FAIL отслеживаются в [FOC-047](https://github.com/Hazernit/fog-of-command/issues/55). Не менять системную локаль для их сокрытия. Подробные команды и результаты: [Windows development](docs/WINDOWS_DEVELOPMENT.md), [FOC-002 smoke](docs/diagnostics/FOC-002-windows-smoke.md). Сборка самой UE и packaging игры — **NOT RUN**.

## Проверки изменений

[FOC-003: команды тестов и Windows gate](docs/TESTING.md) описывает статические
проверки, чистую C++ сборку и изолированный запуск `FogOfCommand.Smoke` через
UnrealEditor-Cmd. Фактические результаты: [FOC-003 report](docs/diagnostics/FOC-003-windows-tests.md).
Успешный hosted static CI не означает сборку UE: на hosted runners она **NOT RUN**.

## Лицензирование

Публичная доступность репозитория **не означает** разрешения использовать код или ассеты. Лицензия пока не выбрана. Перед публикацией игры требуется решение по лицензии, правам на ассеты и торговому названию.

FOC-006: [доменный журнал событий и opt-in developer-команды](docs/EVENT_LOG.md).
Полный Automation gate:23 теста (прежние17 +6 Events); реальные результаты и
ограничения — [отчёт Windows](docs/diagnostics/FOC-006-windows-event-log.md).

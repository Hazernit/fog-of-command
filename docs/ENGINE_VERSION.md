# Версия Unreal и проверка Windows toolchain — FOC-001

**Issue:** [#7 / FOC-001](https://github.com/Hazernit/fog-of-command/issues/7).

**Дата подготовки процедуры:** 2026-10-23.

**Статус:** Windows smoke **NOT RUN**; проверенная версия движка **не закреплена**.

Эта сессия выполнялась на macOS 26.6.2 (Darwin 25.6.0, arm64). Доступа к
целевой Windows-машине и результатов её запуска нет. Наличие стабильного релиза
в документации поставщика не подтверждает работу на нашем ПК. Не закрывать
FOC-001 и не начинать зависимую FOC-002 до подтверждения критериев владельцем.

## Границы и зависимости

У FOC-001 нет входящих зависимостей — проверены GitHub Issue и
`backlog/issues.json`. FOC-002 зависит от FOC-001. Здесь проверяется одноразовый
пустой C++ проект **вне Git checkout**; `FogOfCommand.uproject`, игровые модули,
карта игры и новые плагины относятся к FOC-002 или следующим задачам.

## Конфигурация

Кандидат для первого запуска и фактически проверенная конфигурация учитываются
раздельно. После smoke записать точные версии из файлов и build log; несовпадение
с кандидатом объяснить до принятия pin. Автоматически обновлять движок, IDE или
драйвер между измерениями нельзя.

| Параметр | Кандидат / целевая среда | Фактически проверено |
|---|---|---|
| Unreal Engine | **5.8.4**, stable hotfix от 2026-10-20 | NOT RUN; pin отсутствует |
| IDE | **Visual Studio 2026 18.11.1 Stable**, build **12315.225**, от 2026-10-20 | NOT RUN |
| MSVC | **14.50 x64/x86**; полный servicing patch и версия `cl.exe` ещё не известны | NOT RUN; уточнить при установке и по UBT log |
| Windows SDK | **10.0.26100.0**, target SDK | NOT RUN |
| .NET | **10.0** для VS 2026 | NOT RUN |
| OS | Windows 11 x64; полный build ОС записать перед smoke | NOT RUN |
| CPU / GPU / RAM | i9-9900K / RTX 4070 / 64 ГБ, со слов владельца | Аппаратно не проверено |
| GPU driver / BIOS / частоты | Версии и настройки записать на Windows | NOT RUN |

Источники проверены 2026-10-23:

- [Epic: выпуск UE 5.8.4](https://forums.unrealengine.com/t/5-8-4-hotfix-released/2842775).
- [Microsoft: Visual Studio release history](https://learn.microsoft.com/en-us/visualstudio/releases/2026/release-history)
  подтверждает версию, build и канал IDE.
- [Epic: совместимость UE 5.8 с Visual Studio](https://dev.epicgames.com/documentation/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.8)
  рекомендует для общей разработки VS 2026, MSVC 14.50, SDK 10.0.26100+ и .NET 10.0.
- [Microsoft: установка MSVC](https://learn.microsoft.com/en-us/cpp/overview/acquire-msvc?view=msvc-170):
  компонент `Microsoft.VisualStudio.Component.VC.14.50.18.0.x86.x64` выбирает
  семейство toolset, а не полный servicing patch.

Это конкретный кандидат по опубликованной совместимости, а не сертификация
комбинации на нашем ПК и не утверждение о самом новом выпуске на любую дату.
Полный MSVC patch нельзя вывести из номера IDE. Не подменять его выдуманным
номером ради заполнения таблицы. Пока он и реальные результаты отсутствуют,
критерий точной проверенной конфигурации FOC-001 не выполнен.

## Подготовка чистого запуска на Windows

Все команды ниже — **процедура для Windows, NOT RUN в этой сессии**.
Использовать PowerShell на основной Windows-машине. При ошибке команды
остановиться, сохранить вывод и устранить причину перед следующим шагом.

1. Установить выбранный стабильный UE через Epic Games Launcher, проверить
   установку через **Verify**. Не использовать Preview и не менять minor/patch
   молча. Записать полный `Engine/Build/Build.version` и скрин версии Editor.
2. В Visual Studio Installer включить **Game development with C++** и
   **Desktop development with C++**, .NET 10.0, указанный MSVC x64/x86
   и Windows SDK. MSVC 14.50 установить явно через Individual components:
   обновлённая IDE может предлагать иной toolset по умолчанию.
   Версии установленных компонентов и реально выбранные
   UBT версии — разные сведения; окончательные значения брать из build log.
3. Вне репозитория создать новую папку `C:\UE-Smoke\FOC001`. В Editor выбрать
   **Games → Blank → C++**, Desktop, Scalable, без Starter Content и ray tracing;
   имя проекта `Foc001Smoke`, папка `C:\UE-Smoke\FOC001\Foc001Smoke`.
   Сохранить пустую карту как `/Game/Smoke`. Закрыть Editor и IDE перед сборкой;
   Live Coding не использовать как проверку чистой сборки.
4. Создать отдельную папку результатов для каждого прогона. Не загружать в Git
   проект smoke, `Binaries`, `Intermediate`, `Saved`, DDC, полные `dxdiag`/Event Logs.
   Перед прикреплением доказательств проверить персональные пути и данные машины.

### 1. Сведения о среде и драйвере

Путь `$UE` заменить на фактическую установку. Проверить, что `$ProjectDir`
указывает на одноразовый проект, а не на репозиторий или папку Engine.

```powershell
$ErrorActionPreference = 'Continue' # Native stderr не должен прервать запись build log.
$UE = 'C:\Program Files\Epic Games\UE_5.8'
$ProjectDir = 'C:\UE-Smoke\FOC001\Foc001Smoke'
$Project = Join-Path $ProjectDir 'Foc001Smoke.uproject'
$Out = Join-Path 'C:\UE-Smoke\FOC001' ('evidence-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $Out -ErrorAction Stop | Out-Null
if (!(Test-Path $Project)) { throw "Missing smoke project: $Project" }

Get-Date -Format o | Out-File "$Out\started.txt"
Get-Content "$UE\Engine\Build\Build.version" -ErrorAction Stop | Tee-Object "$Out\Build.version.txt"
Get-Content $Project -ErrorAction Stop | Out-File "$Out\uproject.txt"
Get-CimInstance Win32_OperatingSystem |
    Select-Object Caption, Version, BuildNumber | Out-File "$Out\os.txt"
Get-CimInstance Win32_Processor |
    Select-Object Name | Out-File "$Out\cpu.txt"
Get-CimInstance Win32_ComputerSystem |
    Select-Object TotalPhysicalMemory | Out-File "$Out\ram.txt"
Get-CimInstance Win32_BIOS |
    Select-Object SMBIOSBIOSVersion, ReleaseDate | Out-File "$Out\bios.txt"
Get-CimInstance Win32_VideoController |
    Select-Object Name, DriverVersion, DriverDate | Out-File "$Out\gpu.txt"

$VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path -LiteralPath $VsWhere -PathType Leaf)) { throw "Missing vswhere: $VsWhere" }
$LASTEXITCODE = $null
& $VsWhere -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json |
    Out-File "$Out\visual-studio.json"
$VsExit = $LASTEXITCODE
if ($null -eq $VsExit -or $VsExit -ne 0) { throw 'vswhere failed' }
Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\Include" -Directory |
    Select-Object Name | Out-File "$Out\installed-sdk.txt"
Start-Process dxdiag.exe -ArgumentList "/t `"$Out\dxdiag.txt`"" -Wait
```

Вручную записать маркетинговую версию GPU-драйвера из NVIDIA System Information,
выбранный RHI, разрешение, режим питания, наличие разгона/undervolt/XMP и фоновых
нагрузок. `dxdiag` и CIM не являются тестом стабильности GPU. Если `vswhere`
возвращает пустой список, сначала исправить установку C++ workload.

### 2. Чистая C++ сборка и длительность

Editor и IDE закрыты. В одноразовом `$ProjectDir` удалить только проектные
`Binaries` и `Intermediate` через Explorer; Engine и общие кэши не удалять.
Это чистая сборка **проекта на установленном движке**, не сборка Engine из исходников.
Указать, был ли DDC прогрет, и отдельно учитывать время создания проекта,
сборки и компиляции шейдеров. Не выдавать повторный no-op build за clean build.

```powershell
$Build = Join-Path $UE 'Engine\Build\BatchFiles\Build.bat'
if (!(Test-Path -LiteralPath $Build -PathType Leaf)) { throw "Missing Build.bat: $Build" }
$LASTEXITCODE = $null
$Timer = [Diagnostics.Stopwatch]::StartNew()
& $Build Foc001SmokeEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE 2>&1 |
    Tee-Object "$Out\build.txt"
$BuildExit = $LASTEXITCODE
$Timer.Stop()
"ExitCode=$BuildExit`nElapsedSeconds=$($Timer.Elapsed.TotalSeconds)" |
    Out-File "$Out\build-result.txt"
if ($null -eq $BuildExit -or $BuildExit -ne 0) {
    throw "Build failed or did not start: $BuildExit; see $Out\build.txt"
}
```

Сохранить из `build.txt` строки с выбранными MSVC/toolchain, Windows SDK и итогом
UBT; записать полный номер компилятора из выбранного `cl.exe` (баннер `/Bv`).
Вызов `cl.exe /Bv` без исходника может вернуть ненулевой код из-за отсутствия
входного файла: это сбор сведений, а не отдельный build test.
Если фактическая версия отличается от кандидата, не отмечать конфигурацию как
проверенную без объяснения. Код 0 подтверждает только сборку; PIE проверяется отдельно.
При необходимости solution генерировать штатным пунктом **Generate Visual Studio
project files** у `.uproject` после регистрации выбранного движка; это не замена сборке.

### 3. Editor / PIE и повторный запуск

```powershell
$Editor = Join-Path $UE 'Engine\Binaries\Win64\UnrealEditor.exe'
if (!(Test-Path -LiteralPath $Editor -PathType Leaf)) { throw "Missing Editor: $Editor" }
& $Editor $Project /Game/Smoke -dx12 -log "-abslog=$Out\editor-dx12.log"
```

1. Дождаться окончания шейдеров. Снять версию Editor и пустую карту; убедиться,
   что загружен собранный C++ модуль. В Output Log проверить ошибки загрузки.
2. Выбрать **Play → Selected Viewport** (PIE). Сделать 3 цикла Play/Stop, затем
   оставить PIE работать 30 минут. Записать фактические начало/конец, разрешение,
   RHI, зависания, артефакты, driver reset и fatal/device-removed сообщения.
   Успешный старт процесса или скрин Editor не подтверждает запуск PIE.
3. Завершить PIE и Editor штатно. Открыть тот же проект повторно, запустить/остановить
   PIE, закрыть. Сохранить второй лог с другим именем и скрин PIE. Отсутствие сбоя
   в пустом проекте ограничивает вывод этим smoke; это не performance gate игры.

### 4. GPU/Windows проверяются отдельно

Ранее упоминались GPU-сбои, но в репозитории нет воспроизводимых логов или точного
диагноза. В этой сессии ни сбой, ни стабильность Windows/GPU не подтверждены.

- До UE и после smoke проверить **Reliability Monitor** (`perfmon /rel`) и
  **Event Viewer → Windows Logs → System** за интервал прогона: Display/драйвер,
  WHEA-Logger, Kernel-Power; записать время, Event ID и точный текст. Одна запись
  Kernel-Power не определяет причину сбоя.
- Отдельно от UE выполнить 30 минут уже доступной владельцу известной графической
  нагрузки. Записать название/версию, настройки, длительность, наблюдаемые
  температуры и результат. Если такого теста нет, оставить **NOT RUN**;
  не заменять его `dxdiag`, сборкой C++ или успешным PIE.
- При артефактах, зависании, BSOD, driver reset или WHEA остановить прогон и
  сохранить логи/дампы/скрин. Создать отдельную GitHub Issue-блокер с версиями
  ОС/BIOS/драйвера, условиями, частотой воспроизведения и ссылкой из FOC-001/PR.
  Сбой вне UE — основание исследовать ПК/драйвер; сбой только в UE ещё не
  доказывает баг движка или неисправность GPU.
- Для диагностики можно повторить с `-dx11` вместо `-dx12`, записав отдельный лог.
  Успех DX11 не засчитывать как успех DX12. Менять один фактор за раз; после
  изменения драйвера/настроек повторить соответствующие проверки и записать версии.

## Фактические результаты

| Проверка | Результат | Доказательство / ограничение |
|---|---|---|
| Зависимости FOC-001 | PASS | GitHub #7 и локальный backlog: `depends_on: []`; FOC-002 зависит от FOC-001 |
| `python3 scripts/validate_backlog.py` | PASS | `OK: 45 task issues, 6 epics, all dependencies and GDD files present` на macOS |
| `git diff --check` | PASS | Проверка diff документа на macOS; не проверяет PowerShell или Unreal |
| Версии UE/IDE/MSVC/SDK на Windows | NOT RUN | Нет доступа к Windows, `Build.version`, UBT log и вывода `cl.exe` |
| Чистый C++ build | NOT RUN | Exit code и длительность не измерены |
| Editor / PIE / повторное открытие | NOT RUN | Логи и скриншоты отсутствуют |
| Отдельная GPU/driver проверка | NOT RUN | Ранее упомянутые сбои не воспроизводились в этой сессии |

Windows-команды выше не исполнялись и не проверялись PowerShell на этой машине.
Отдельный аппаратный blocker сейчас не создан: наблюдаемого сбоя и доказательств
нет; отсутствие доступа к Windows не является аппаратной неисправностью.

## Что заполнить после реального smoke

| Поле отчёта | Текущее значение |
|---|---|
| Дата/часовой пояс, оператор, идентификатор прогона | NOT RUN |
| UE Major.Minor.Patch, Changelist/BranchName, источник установки | NOT RUN |
| IDE полная версия, MSVC toolset и `cl.exe`, выбранный UBT SDK | NOT RUN |
| OS build, BIOS, CPU/GPU/RAM, драйвер, настройки частот, RHI | NOT RUN |
| Clean build: команда, exit code, время в секундах, состояние DDC | NOT RUN |
| Editor/PIE: 3 цикла, 30 минут, повторное открытие, ссылки на логи/скрины | NOT RUN |
| Независимая GPU-нагрузка и события Windows до/после | NOT RUN |
| Известные проблемы, воспроизведение, ссылка на blocker (если есть) | Не диагностированы |

После успешной проверки заменить кандидат фактической точной конфигурацией,
приложить доказательства и длительности, объяснить отклонения и получить ревью
владельца. При незавершённых проверках оставить соответствующие `NOT RUN` и
критерии Issue незакрытыми. Дальнейшее обновление принятого pin — отдельный ADR/Issue.

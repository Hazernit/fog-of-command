# FOC-046: Windows Blank C++ smoke

Дата: 2026-10-23, Europe/Moscow. Задачи: [#53](https://github.com/Hazernit/fog-of-command/issues/53), [#7](https://github.com/Hazernit/fog-of-command/issues/7).

Это поэтапный отчёт с реального Windows-ПК. Пока не выполнены build, Editor/PIE и GPU gate, конфигурация не принимается и Issues остаются открытыми. Движок в main по-прежнему TBD; кандидат PR #52 не равен принятому pin.

## Область и сохранность данных

- Проверяется одноразовый Blank C++ проект вне репозитория. FOC-002 не начинается, игровой `.uproject` не создаётся.
- MyProject не изменяется; пользовательские Editor/IDE не закрываются.
- Установки, обновления движка/toolchain/драйвера, изменения BIOS/сети/защиты не выполняются.
- В Git публикуются только переносимая процедура и отредактированные результаты. Raw-логи, скриншоты с личными данными, абсолютные локальные пути, токены и машинные идентификаторы остаются вне Git.

## Этап 1: новая установка и toolchain

Актуальные `AGENTS.md`, `docs/CODEX_WORKFLOW.md`, #53 и #7 прочитаны из GitHub; исходный main: `c693048f687d6a32e77936cbab7d67db67a497ec`. Проверки выполняются локальными Windows-инструментами, а не симулируются по документации.

| Компонент | Фактическое наблюдение |
|---|---|
| Unreal Engine | **5.8.4**, CL **58902052**, compatible CL55116800, branch `++UE5+Release-5.8` |
| Источник версии | `Engine/Build/Build.version` и Epic Launcher manifest; incomplete=false |
| Build tools | InstalledBuild marker, Build.bat, UBT, Editor и Blank C++ template присутствуют |
| .NET UE | Bundled SDK **10.0.203**, runtime **10.0.7**; `dotnet --info` выполнен |
| Visual Studio | Build Tools2019 **16.11.60**; Community2022 **17.14.42**; Community2026 **18.10.4** |
| MSVC toolset directories | **14.29.30133**, **14.44.35207**, **14.51.36231** |
| cl.exe versions | **19.29.30159**, **19.44.35229**, **19.51.36260** |
| Windows SDK | **10.0.19041.0**, **10.0.22621.0**, **10.0.26100.0** |

Именно установленный UE5.8 использует `Engine/Config/Windows/Windows_SDK.json`: MSVC minimum14.38.33130, preferred families14.50.35717–14.50.99999 и14.44.35207–14.44.99999. Важно различать toolset directory и actual compiler version: установленный каталог14.44.35207 содержит cl19.44.35229/UBT-version14.44.35229, выше исключённого диапазона14.44.0–14.44.35210. Установленный14.51 новее preferred. SDK minimum10.0.19041.0, main10.0.22621.0; SDK26100 присутствует. Окончательные selected MSVC/SDK будут зафиксированы по UBT log, наличие компонентов не является PASS сборки.

Старый UE5.3.2/MSVC14.51 запуск ранее завершался C4668/C4067 на `__has_feature`. Это отдельный исторический результат; он не переносится на UE5.8.4.

## Текущие проверки

| Проверка | Статус | Доказательство / ограничение |
|---|---|---|
| Новая UE5.8.4 обнаружена | **PASS** | Два локальных источника версии и build tools |
| MSVC/SDK inventory | **PASS** | vswhere, executable/file versions, SDK headers/libs/tools |
| Clean project C++ build UE5.8.4 | **NOT RUN** | Следующий этап; exit code и elapsed time ещё не измерены |
| Editor / C++ module load / DX12 | **NOT RUN** | Только после успешного build |
| 3 цикла PIE, 30 минут, повторное открытие | **NOT RUN** | Нужны реальные runtime доказательства |
| GPU telemetry/events и независимая нагрузка | **NOT RUN** | Нельзя заменить инвентаризацией драйвера |
| Приёмка версии владельцем | **NOT RUN** | #53/#7 не закрываются, PR остаётся Draft |

## Воспроизведение следующего этапа

Переменные `UE_ROOT`, `SMOKE_PROJECT`, `SMOKE_EVIDENCE` обозначают локальные пути оператора; их значения не публикуются. Проект будет новым, из текстового `TP_Blank` установленного движка; до первой сборки `Binaries` и `Intermediate` должны отсутствовать. Это чистая сборка проектного модуля на установленном движке, не пересборка Engine и не очистка общих DDC.

```powershell
& (Join-Path $env:UE_ROOT 'Engine/Build/BatchFiles/Build.bat') `
  Foc046SmokeEditor Win64 Development "-Project=$env:SMOKE_PROJECT" `
  -WaitMutex -NoHotReloadFromIDE -NoHotReload -MaxParallelActions=4 `
  "-Log=$(Join-Path $env:SMOKE_EVIDENCE 'UnrealBuildTool.log')"
```

При FAIL сначала разбирать фактический UBT log. Если auto-selected14.51 несовместим с политикой/движком, допустима новая чистая попытка с уже установленным14.44, выбранным аргументом `-CompilerVersion=14.44.35229`; это ещё **NOT RUN**, изменения глобального XML или исходников движка не требуются. Ничего не устанавливать автоматически.

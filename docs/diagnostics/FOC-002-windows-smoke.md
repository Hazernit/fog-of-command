# FOC-002: первый Unreal C++ проект — Windows smoke

Дата: 2026-10-26, Europe/Moscow. Issue: [#8](https://github.com/Hazernit/fog-of-command/issues/8). Ветка: `feat/foc-002-unreal-project`.

## Подготовка

Актуальные AGENTS, README, ARCHITECTURE, ROADMAP, ENGINE_VERSION и WINDOWS_DEVELOPMENT прочитаны из main. Локальный main синхронизирован fast-forward с `f0379dadf1cbe0d1c63771d57bd668839f673fb8`; новая feature-ветка создана из него, исходная рабочая копия чистая. FOC-001 принята; отдельная FOC-047 (#55) не блокирует этот проект.

Локально подтверждены UE **5.8.4 CL58902052**, установленный VS2022 toolset14.44 и SDK22621. Фактический выбор компилятора будет записан по журналу новой сборки.

Git LFS **3.7.0** доступен; `git lfs install --local` выполнен **до создания бинарной карты**. `git check-attr` подтверждает `filter/diff/merge=lfs`, `text=unset` для `Content/Maps/Startup.umap`.

## Результаты по этапам

| Проверка | Статус |
|---|---|
| Синхронизация main, отдельная ветка, Git LFS | PASS |
| C++ модуль, GameMode и Config | PASS статически: descriptor/targets/module/GameMode/Config созданы; runtime ещё NOT RUN |
| Первая C++ сборка в исходном checkout | FAIL: native6,35.034с wall /32.85с UBT; C1083 открытия PCH, искажённый Unicode path |
| Чистая C++ сборка нового ASCII clone, код до карты | PASS: native0,wrapper0,109.165с wall /106.38с UBT,9actions |
| Создание карты, первая попытка | FAIL: commandlet-1,49.172с; недоступный Python SystemLibrary.break_soft_object_path |
| Создание карты после исправления API | PASS: native0,13.119с,FOC002_STARTUP_MAP_CREATED |
| Валидатор сохранённой карты, отдельный процесс | PASS: native0,11.103с,FOC002_STARTUP_MAP_VALIDATION_PASS |
| Push/pull бинарной карты через LFS | PASS: upload1/1,новый remote clone/LFS pull/fsck,24300байт/SHA256 совпадает |
| Сборка полного проекта из нового удалённого git clone без generated folders | PASS: native0/wrapper0,128.271с wall /127.19с UBT,точный compiler/SDK |
| Editor / стартовая карта / PIE / Stop / повтор / reopen | NOT RUN |
| Анализ текущих runtime ошибок | NOT RUN |
| Приёмка владельцем / merge | NOT RUN |

## Первая попытка C++ build

08:34:37–08:35:12 +03:00: до build Binaries/Intermediate отсутствовали, UBT создал новый makefile. Реально выбран VS2022/MSVC14.44.35229/SDK10.0.22621.0. Исходный checkout расположен в OneDrive с кириллицей в пути. Native exit6,35.034с wall,32.85с UBT; компилятор завершился на создании shared PCH до компиляции игрового кода:

```text
Using Visual Studio 2022 14.44.35229 toolchain (<VS_ROOT>/VC/Tools/MSVC/14.44.35207) and Windows 10.0.22621.0 SDK (<WINDOWS_KITS_ROOT>).
[1/9] Compile [x64] SharedPCH.UnrealEd.Project.ValApi.ValExpApi.Cpp20.cpp
c1xx: fatal error C1083: cannot open compiler intermediate file: <CHECKOUT_WITH_GARBLED_UNICODE_PATH>/Intermediate/.../SharedPCH....h.pch: No such file or directory
Result: Failed (OtherCompilationError)
```

Строка C1083 выше очищена и переведена по смыслу; private path и испорченные персональные сегменты не публикуются. Причина (Unicode/расположение/UBA и др.) ещё не изолирована. Следующий шаг — новый удалённый git clone в ASCII-пути вне OneDrive. Исходный checkout и caches не удаляются. Первоначальный parser wrapper не распознал дополнительное `2022` в toolchain line; regex уточнён и проверен по фактическому UBT log, raw evidence не переписана.

## Сборка нового удалённого clone в ASCII-пути

Реальный `git clone --single-branch --branch feat/foc-002-unreal-project` с GitHub получил commit `57c0fc6`. В новом checkout до build Binaries=false/Intermediate=false. **08:38:47.522–08:40:36.706 +03:00**, native0/wrapper0, **109.165с wall /106.38с UBT /59.91с UBA**. UHT создал4generatedfiles, выполнены9actions, DLL и target receipt созданы. ToolchainValidation PASS: **VS2022/MSVC14.44.35229/SDK10.0.22621.0**. C++ модуль и GameMode скомпилированы; карта в этом commit ещё не добавлена. Полный clone с картой и его runtime будут отдельной проверкой.

Это подтверждает рабочую сборку в выбранном ASCII checkout вне OneDrive; исходная причина C1083 в другой копии не изолирована. Для воспроизведения используется ASCII-путь. Общие Engine/UBA/DDC caches не очищались, cold-cache benchmark и полная сборка Engine — NOT RUN.

## Исправление инструмента создания карты

Первый реальный Python commandlet08:41:26–08:42:15 завершился native-1/49.172с: `AttributeError: type object 'SystemLibrary' has no attribute 'break_soft_object_path'`. Это ошибка editor tooling, не успешная генерация карты и не проблема C++ модуля. Проверка C++ UFUNCTION signature не гарантировала Python-export name. Короткие read-only probes внутри этой UE установили поддерживаемый `SoftObjectPath/SoftClassPath.export_text()` и уточнили имена API. Исходники Engine и language settings не менялись; raw probe/log файлы остаются вне Git.

Исправленный creator08:48:33–08:48:46: **native0,13.119с**, marker `FOC002_STARTUP_MAP_CREATED`. Независимый load/validator08:49:34–08:49:45: **native0,11.103с**, marker `FOC002_STARTUP_MAP_VALIDATION_PASS`. Создана одна карта `Content/Maps/Startup.umap` (24300байт):80×80м земля, три Engine Basic Shapes, динамическое освещение/небо/туман, один PlayerStart и native GameMode. Validator проверил сохранённую карту,4меша,коллизии земли Pawn/Visibility,свободный spawn и направление взгляда,default maps/GameMode/DefaultPawn. Это asset check с NullRHI, не визуальный PIE PASS. Python включён только в командлетных процессах, не постоянно в проекте; culture override не задавался.

В рабочем clone Editor автоматически добавил конфигурацию Android File Server с локальным security token. Эта конфигурация не копируется/не добавляется в Git, значение не публикуется. Ненужный для Win64 плагин `AndroidFileServer` явно отключён в .uproject; системные настройки сети/защиты не менялись. В основной checkout переносится только проверенная Startup.umap; ASCII/UTF16 privacy checks карты на личные пути/имя и token-field markers —0совпадений.

## Полный чистый clone с LFS-картой

Карта запушена через Git LFS в commit `5157f5f`: upload1/1. Второй **новый удалённый git clone с GitHub**, `git lfs pull` и `git lfs fsck` —PASS. До первой сборки в нём **Binaries=false,Intermediate=false,Saved=false,DerivedDataCache=false**. Полученная карта24300байт имеет SHA256 `5ad91881b385962873b2e9277880e510e21a63202fe74321ee726fa475a96f7b`, совпадает с проверенным исходным ассетом и LFS pointer; проектные файлы не переносились вручную.

Сборка полного clone **08:52:38.011–08:54:46.299 +03:00**: **native0/wrapper0**, **128.271с wall /127.19с UBT /61.27с UBA**,9actions,UHT4generatedfiles. ToolchainValidationPASS:VS2022/MSVC14.44.35229/SDK10.0.22621.0. Созданы DLL/receipt. Это чистая сборка проектного C++ на установленной binaryUE, не пересборка Engine и не cold-cache измерение.

Отдельный validator скачанной LFS-карты: native0,11.115с,markerPASS. После этого Editor запущен **без map override**, только с FogOfCommand.uproject/DX12/log. Startup.umap загрузилась по Config,MapCheck0errors/0warnings,module/D3D12-SM5/RTX4070 подтверждены,initialization39.85с. На первом запуске появилось системное окно Windows Firewall; агент им не управлял. Владелец обработал запрос вручную (сообщил о кнопке Allow/Approve),после чего окно отсутствовало. Выбранные network profiles/правила отдельно **NOT RUN**; никаких изменений firewall/VPN/сети агент не выполнял.

Первый PIE09:00:26.956–09:01:22.219 подтвердил `FOC-002 GameMode BeginPlay: map=UEDPIE_0_Startup, default pawn=DefaultPawn` и штатный Stop. **Визуальная проверка FAIL:** сцена пересвечена,примитивы едва видны. Интенсивность Sun в генераторе уменьшена10000→10; требуется регенерация и повторный UI/PIE. Engine и глобальная экспозиция/локаль не меняются. Editor после первого PIE штатно закрыт09:01:31. Он также создал локальный DefaultInput.ini; автоматически созданные локальные input preferences не переносятся в Git.

## Границы проверки

Только минимальный проект и карта FOC-002. RTS, разведка, экономика, multiplayer и FOC-003 не начинаются. MyProject и исходники Engine не редактируются. Системная локаль и глобальные языковые настройки не меняются; известные ru-RU startup self-test failures учитываются отдельно в #55.

Raw UBT/Editor logs и локальные абсолютные пути остаются вне Git. Здесь публикуются только очищенные результаты и переносимые команды. Установщики и обновления не запускаются. Действия самого Editor с cache/config описываются отдельно при наблюдении.

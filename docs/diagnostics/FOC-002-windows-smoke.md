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
| Создание и проверка стартовой карты | NOT RUN |
| Push/pull бинарной карты через LFS | NOT RUN |
| Сборка из нового удалённого git clone без generated folders | NOT RUN |
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

## Границы проверки

Только минимальный проект и карта FOC-002. RTS, разведка, экономика, multiplayer и FOC-003 не начинаются. MyProject и исходники Engine не редактируются. Системная локаль и глобальные языковые настройки не меняются; известные ru-RU startup self-test failures учитываются отдельно в #55.

Raw UBT/Editor logs и локальные абсолютные пути остаются вне Git. Здесь публикуются только очищенные результаты и переносимые команды. Установщики и обновления не запускаются. Действия самого Editor с cache/config описываются отдельно при наблюдении.

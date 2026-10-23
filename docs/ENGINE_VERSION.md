# Версия Unreal и локальная сборка

**Статус:** Windows build/Editor/PIE/GPU smoke выполнен в FOC-046; есть 3 RU startup test failures. Точный pin FOC-001 пока не принят владельцем.

| Параметр | Значение |
|---|---|
| Unreal Engine | Pin **TBD — FOC-001**; измеренный кандидат **5.8.4 CL58902052** |
| Visual Studio / MSVC | Фактический build: VS2022 **17.14.42**, MSVC **14.44.35229**, toolset directory **14.44.35207** |
| Windows SDK | Фактически выбран **10.0.22621.0** |
| Target | Windows 11 x64, основная рабочая станция |
| CPU / GPU / RAM | Локально: i9-9900K / RTX4070 / **63.85GiB usable**; свежий API installed-RAM недоступен |
| Проверка стабильности драйверов | Runtime observation **PASS**: driver617.42,30мин telemetry,49–51°C,0выбранных событий/GPUfatal; независимая нагрузка **NOT RUN** |
| Проверка PIE | Три коротких Play/Stop, **32мин21.549с** непрерывного PIE и повторное открытие/PIE **PASS** |
| Проверка чистого C++ build | **PASS**, exit0, **93.037с** wall time, **90.70с** UBT; новый Blank C++ модуль вне Git |
| Встроенные startup tests | ru-RU: **425/428 PASS, 3 FAIL**,17CHECK errors; процессный `-culture=en`: **428/428 PASS**,0Error |

Команды, очищенные выдержки и оригинальный проверенный снимок PIE: [smoke отчёт](diagnostics/FOC-046-windows-smoke.md).

Три RU failures атрибутированы тестам `FUnifiedErrorTest_CreateErrorMessage`, `FUnifiedErrorTest_CreateErrorMessageWithContext`, `FStructuredLogFormatTest`. При том же проекте и процессном `-culture=en` все428тестов прошли; сохранённые language/culture preferences не изменились. Сравнение подтверждает зависимость от culture, но не исправление RU baseline. Функциональный Editor/PIE PASS не означает RU-запуска без ошибок.

Независимый тяжёлый GPU stress и принятие pin владельцем — **NOT RUN**. #53/#7 открыты, PR54 Draft; FOC-002 не начинается. Кандидат PR52 с VS2026/MSVC14.50 отличается от фактически собранной конфигурации; автоматического принятия или обновления версии нет.

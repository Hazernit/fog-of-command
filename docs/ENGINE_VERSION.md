# Версия Unreal и локальная сборка

**Статус:** частично проверено в FOC-046; точный pin FOC-001 пока не принят владельцем.

| Параметр | Значение |
|---|---|
| Unreal Engine | Pin **TBD — FOC-001**; измеренный кандидат **5.8.4 CL58902052** |
| Visual Studio / MSVC | Фактический build: VS2022 **17.14.42**, MSVC **14.44.35229**, toolset directory **14.44.35207** |
| Windows SDK | Фактически выбран **10.0.22621.0** |
| Target | Windows 11 x64, основная рабочая станция |
| CPU / GPU / RAM | Локально: i9-9900K / RTX4070 / **63.85GiB usable**; свежий API installed-RAM недоступен |
| Проверка стабильности драйверов | **NOT RUN** как завершённый gate; driver617.42, идёт runtime telemetry |
| Проверка PIE | Три коротких Play/Stop **PASS**; завершённый30min/reopen **NOT RUN** |
| Проверка чистого C++ build | **PASS**, exit0, **93.037с** wall time, **90.70с** UBT; новый Blank C++ модуль вне Git |

Команды, очищенные выдержки и оригинальный проверенный снимок PIE: [smoke отчёт](diagnostics/FOC-046-windows-smoke.md).

Startup automation checks содержат17ошибок `Condition failed`; точная причина пока UNKNOWN. Функциональный Editor/PIE PASS не означает запуска без ошибок. Полный runtime/GPU gate и принятие pin не завершены. Кандидат PR52 с VS2026/MSVC14.50 отличается от фактически собранной конфигурации; автоматического принятия или обновления версии нет.

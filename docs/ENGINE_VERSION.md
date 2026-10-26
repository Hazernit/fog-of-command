# Версия Unreal и локальная сборка

**Статус на 26.10.2026:** новый clean build, Editor, три коротких PIE, повторный 30-минутный PIE/GPU smoke и reopen прошли. Есть 3 RU startup test failures; процессный EN-запуск прошёл428/428. Точный pin FOC-001 пока не принят владельцем. Прогон 23 октября хранится отдельно в отчёте.

| Параметр | Значение |
|---|---|
| Unreal Engine | Pin **TBD — FOC-001**; свежий inventory **5.8.4 CL58902052**, совпадает с 23 октября |
| Visual Studio / MSVC | UBT26.10 автоматически выбрал VS2022 **17.14.42**, MSVC **14.44.35229**, toolset directory **14.44.35207** |
| Windows SDK | UBT26.10 выбрал **10.0.22621.0** |
| Target | Windows 11 x64, основная рабочая станция |
| CPU / GPU / RAM | Свежий inventory26.10: i9-9900K / RTX4070 / **64GiB installed,63.85GiB usable** |
| Проверка стабильности драйверов | 26.10: **PASS как наблюдение этой сцены**, driver617.42,30мин/178проб,43–49°C,0новыхGPUfatal/выбранныхSystem events. Независимая нагрузка **NOT RUN** |
| Проверка PIE | 26.10: **3 коротких Play/Stop,30мин30.839с непрерывного PIE и reopen/PIE PASS**. Прогон23.10:32мин21.549с и reopen PASS |
| Проверка чистого C++ build | 26.10 **PASS**, exit0, **87.041с** wall time, **84.31с** UBT; новый Blank C++ модуль вне Git. Прогон23.10:93.037с/90.70с |
| Встроенные startup tests | 26.10 ru-RU: **425/428 PASS,3 FAIL**,17CHECK errors; `-culture=en`: **428/428 PASS**,0Error. Прогон23.10 дал те же результаты |

Команды, очищенные выдержки и оригинальный проверенный снимок PIE: [smoke отчёт](diagnostics/FOC-046-windows-smoke.md).

Три RU failures атрибутированы тестам `FUnifiedErrorTest_CreateErrorMessage`, `FUnifiedErrorTest_CreateErrorMessageWithContext`, `FStructuredLogFormatTest`; свежие RU запуски26.10 повторили их. Процессный `-culture=en`26.10 дал428/428PASS, как и23.10; сохранённые language/culture preferences не изменились (проверены до/после с учётом новых INI). Это подтверждает culture-зависимость наблюдаемых failures, не исправление RU baseline. Функциональный Editor/PIE PASS не означает RU-запуска без ошибок.

Независимый тяжёлый GPU stress и принятие pin владельцем — **NOT RUN**. #53/#7 открыты, PR54 Draft; FOC-002 не начинается. Кандидат PR52 с VS2026/MSVC14.50 отличается от фактически собранной конфигурации; автоматического принятия или обновления версии нет.

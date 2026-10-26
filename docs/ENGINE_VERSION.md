# Версия Unreal и локальная сборка

**Статус на 26.10.2026:** новый clean build, Editor, три коротких PIE и повторный 30-минутный PIE/GPU smoke прошли; reopen и EN-сравнение в работе. Есть 3 RU startup test failures. Точный pin FOC-001 пока не принят владельцем. Прогон 23 октября хранится отдельно в отчёте.

| Параметр | Значение |
|---|---|
| Unreal Engine | Pin **TBD — FOC-001**; свежий inventory **5.8.4 CL58902052**, совпадает с 23 октября |
| Visual Studio / MSVC | UBT26.10 автоматически выбрал VS2022 **17.14.42**, MSVC **14.44.35229**, toolset directory **14.44.35207** |
| Windows SDK | UBT26.10 выбрал **10.0.22621.0** |
| Target | Windows 11 x64, основная рабочая станция |
| CPU / GPU / RAM | Свежий inventory26.10: i9-9900K / RTX4070 / **64GiB installed,63.85GiB usable** |
| Проверка стабильности драйверов | 26.10: **PASS как наблюдение этой сцены**, driver617.42,30мин/178проб,43–49°C,0новыхGPUfatal/выбранныхSystem events. Независимая нагрузка **NOT RUN** |
| Проверка PIE | 26.10: **3 коротких Play/Stop и30мин30.839с непрерывного PIE PASS**; reopen ещё **NOT RUN как завершённая проверка**. Прогон23.10:32мин21.549с и reopen PASS |
| Проверка чистого C++ build | 26.10 **PASS**, exit0, **87.041с** wall time, **84.31с** UBT; новый Blank C++ модуль вне Git. Прогон23.10:93.037с/90.70с |
| Встроенные startup tests | 26.10 ru-RU: **425/428 PASS,3 FAIL**,17CHECK errors; новое EN-сравнение **NOT RUN**. Прогон23.10 с `-culture=en`:428/428 PASS |

Команды, очищенные выдержки и оригинальный проверенный снимок PIE: [smoke отчёт](diagnostics/FOC-046-windows-smoke.md).

Три RU failures атрибутированы тестам `FUnifiedErrorTest_CreateErrorMessage`, `FUnifiedErrorTest_CreateErrorMessageWithContext`, `FStructuredLogFormatTest`; свежий RU запуск26.10 повторил их. Сравнение23.10 с процессным `-culture=en` дало428/428PASS без изменения сохранённых language/culture preferences; это исторический результат, не текущий EN PASS и не исправление RU baseline. Функциональный Editor/PIE PASS не означает RU-запуска без ошибок.

Независимый тяжёлый GPU stress и принятие pin владельцем — **NOT RUN**. #53/#7 открыты, PR54 Draft; FOC-002 не начинается. Кандидат PR52 с VS2026/MSVC14.50 отличается от фактически собранной конфигурации; автоматического принятия или обновления версии нет.

## Issue

FOC-### / Refs # (использовать Closes # только после выполнения критериев)

## Что изменено

-

## Проверка

| Проверка | Результат (PASS / FAIL / NOT RUN) | Доказательство |
|---|---|---|
| Python static project/backlog checks и negative tests | NOT RUN | Команда, commit и итог; static PASS не равен UE PASS |
| Git LFS / отсутствие tracked generated/raw files | NOT RUN | Pointer/object/hash, результат проверки |
| Unreal C++ build на pinned Windows toolchain | NOT RUN | Commit, UE/CL/MSVC/SDK, native exit, время, clean/incremental |
| Project Automation: FogOfCommand (Smoke + UnitState + Orders) | NOT RUN | Command, selected suite (All by default), exact test names/counts, report verdict, exit; Engine self-tests отдельно |
| Editor / startup map / PIE / Stop / reopen | NOT RUN | Сценарий, фактические результаты и ограничения |
| Hosted CI | NOT RUN | Ссылка на run и commit; явно указать, что UE build/runtime в hosted CI NOT RUN |
| Контроль инвариантов (если затронуты) | NOT RUN | UnitId / ресурсы / наблюдения / продолжение AI |

Для каждого результата поставить **PASS / FAIL / NOT RUN** и доказательство либо причину.
Raw logs, абсолютные локальные пути, токены, пользовательские config и дампы не прикладывать.
Известные ограничения учитывать отдельно: [ru-RU Engine startup tests #55](https://github.com/Hazernit/fog-of-command/issues/55)
и [не блокирующая диагностика Unicode/OneDrive/PCH C1083 #57](https://github.com/Hazernit/fog-of-command/issues/57).
Указать, воспроизведены ли они; не скрывать Engine errors успешными project tests или сменой глобальной локали.

## Что не проверено и почему

-

## Риски и влияние на баланс / совместимость

-

## Скриншоты / видео (если UI или геймплей)

-

## Checklist

- [ ] Критерии Issue проверены
- [ ] Я не изменял несвязанный scope
- [ ] Инварианты состояния не нарушены
- [ ] Нет секретов и нелицензированных ассетов
- [ ] Документация и тесты обновлены

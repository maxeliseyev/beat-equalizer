# Onboarding

Короткий вход для разработчика, который берёт проект без этого чата.
Источник правды — git и документы ниже, не устные договорённости.

## За первые 30 минут

1. Прочитать `AGENTS.md`. Это контракт проекта: инварианты важнее удобной
   реализации.
2. Проверить состояние:

   ```bash
   git fetch origin
   git checkout main
   git pull --ff-only
   gh pr list --state open
   ```

3. Прочитать `docs/status.md` целиком. Если он расходится с git или GitHub —
   сначала исправить его отдельным docs-PR.
4. Прочитать session note из `status.md`, если он указан.
5. Собрать локально:

   ```bash
   make test
   make standalone
   ```

6. Для GUI-задачи открыть Standalone через `make run`. Для Reaper smoke —
   `docs/reaper-testing.md`.

## Что читать перед задачей

| Задача | Документы |
|---|---|
| Ближайший шаг | `docs/status.md`, последняя `docs/sessions/YYYY-MM-DD-*.md` |
| Любая фича этапа 2 | `docs/drum-editor-plan.md`, нужная часть `docs/timing-design-recommendations.md` |
| DSP алгоритм | `AGENTS.md`, `docs/plan.md`, профильный ADR, соседний тест |
| Детектор / события | `docs/detector-design-recommendations.md`, `docs/hit-segmentation-recommendations.md` |
| Export / Reaper | `docs/drum-editor-plan.md`, `docs/reaper-testing.md` |
| Git / PR / handoff | `docs/git-workflow.md`, `docs/handoff.md` |

Не надо читать все session notes подряд. Они объясняют прошлые решения, а не
заменяют текущий статус.

## Как выбрать работу

Если задача не назначена явно, брать только `docs/status.md` → `Next`. Если
`Next` уже сделан или PR слит, обновить `main`, исправить `status.md` и взять
следующий пункт из `docs/drum-editor-plan.md`, этап 2.

Один PR — один связный шаг. Нормальная грань для текущего этапа:

- новый DSP-блок + синтетические тесты;
- adapter `beat_doc -> beat_dsp` без UI;
- Standalone UI/export bridge без нового алгоритма;
- реальный gate-протокол без изменения алгоритма.

Не смешивать эти слои, даже если кажется, что «так быстрее».

## Куда класть код

| Слой | Что здесь живёт | Чего здесь нет |
|---|---|---|
| `src/dsp` | Алгоритмы, offline/realtime DSP, синтетически тестируемые блоки | JUCE GUI, `beat_doc`, файл проекта |
| `src/doc` | Источники, события, задержки, edit regions, признаки, журнал | JUCE, окна, хост |
| `src/plugin` | JUCE adapter: параметры, UI, worker, file player, export bridge | Новый алгоритм до теста в `src/dsp`/`src/doc` |
| `tests` | Catch2 synthetic/GUI smoke/real-kit gated tests | Реальные WAV/AIFF |
| `docs` | Канон, планы, handoff, ADR, протоколы | Дампы чата и звук |

Новая зависимость разрешена только вниз: `plugin -> doc -> dsp`.
Обратная связь `dsp -> doc` или `doc -> plugin` — архитектурный баг.

## Definition Of Done

Перед PR:

1. `make test` зелёный.
2. Если менялся DSP — есть синтетический тест, который мог бы упасть на ошибке.
3. Если менялся GUI/export — `make test-gui` и ручной Standalone smoke.
4. Если задача касается Reaper — пройти `docs/reaper-testing.md`.
5. Если использовался реальный кит — только через `BEAT_REAL_KIT_DIR`; звук не
   коммитить, числа писать в `docs/real-kit-protocol.md`.
6. `docs/status.md` указывает на текущую ветку/PR и один следующий шаг.
7. Нетривиальное решение записано в `docs/sessions/`; долгоживущий запрет или
   архитектурное решение — в `docs/decisions/`.
8. Новое поведение меняет `VERSION` и `CHANGELOG.md`; docs-only PR — patch.

## Что нельзя делать на автопилоте

- Не пушить напрямую в `main`.
- Не создавать `develop` до `v1.0.0`.
- Не добавлять ARA/Celemony SDK до delivery-этапа.
- Не вендорить JUCE/Catch2 или аудиоматериал.
- Не укорачивать room/OH задержку как «ошибку»: return — часть модели.
- Не растягивать атаку одного микрофона ради другого: protected zone общая.
- Не выключать JUCE splash без коммерческой лицензии.

## Как передать работу обратно

Перед паузой или переключением:

1. Закоммитить собирающееся состояние на feature-ветке.
2. Запушить ветку и открыть draft PR.
3. Переписать `docs/status.md`: Branch, PR, Now, Next, Resume.
4. Добавить session note с `Why`, если было решение или тупик.

Следующий разработчик должен суметь продолжить с одного `docs/status.md`.

# 2026-09-04 — Onboarding readiness

## Context

Появился риск временно передать проект второму разработчику. Нужно проверить,
сможет ли он быстро войти в работу без этого чата, не нарушить workflow и
понять будущий план.

## What

Аудит показал четыре практических gap:

- `docs/status.md` после merge PR #46 остался на старой ветке и PR draft;
- не было короткого onboarding-документа для человека: setup, read order, DoD,
  границы слоёв и handoff были разбросаны по нескольким файлам;
- `docs/reaper-testing.md` устарел и говорил, что `Analyze` ещё нет;
- этап 2, шаг 7 был слишком крупным для передачи: “точки реза, кроссфейды,
  WSOLA” без очереди следующих PR.

Ветка `docs/onboarding-readiness` добавляет:

- `docs/onboarding.md`;
- ссылки на onboarding в `README.md`, `docs/README.md` и `AGENTS.md`;
- актуальный Reaper/Standalone smoke в `docs/reaper-testing.md`;
- декомпозицию этапа 2, шага 7 после PR #46 в `docs/drum-editor-plan.md`;
- расширение ADR `editor-in-standalone-ara-is-delivery`: RPP и ARA теперь
  описаны как delivery adapters поверх `beat_doc`;
- generic stage template в `docs/handoff.md`;
- patch bump `0.27.1` и `CHANGELOG.md`.

## Why

Корневой `AGENTS.md` хорошо защищает инварианты, а `handoff.md` хорошо
работает для агентов. Но второму разработчику нужен короткий маршрут: что
читать сначала, как собрать, какой первый task брать, где границы `dsp/doc/plugin`
и что считать готовым PR. Без этого он будет либо читать всю историю sessions,
либо начнёт с устаревшего `status.md`.

Декомпозиция шага 7 нужна не как новый продуктовый план, а как operational
queue: каждый следующий PR имеет чёткую границу и не смешивает DSP, adapter,
UI/export, real-kit gate, sidecar и RPP.

## Status

status.md обновлён: да. Ветка: `docs/onboarding-readiness`.
PR: #47 draft — https://github.com/maxeliseyev/beat-equalizer/pull/47.
Проверено: `rtk git diff --check`, `rtk make test`.

## Next

Проверить PR #47; после merge продолжить этап 2, шаг 7.1:
`EditRegionPlan -> CrossfadeRenderer::EditPoint`.

## Open

- Требуется только review человеком: достаточно ли `docs/onboarding.md`
  совпадает с тем, как вы хотите вводить второго разработчика.

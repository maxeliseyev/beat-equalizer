# 2026-09-04 — Edit region plan

## Context

PR #44 смёржен: YAN9 Advanced diagnostics gate показал, что `GlideRenderer`
упирается в slew-limit и не улучшает source-centric event coherence. Следующий
шаг из плана — этап 2, шаг 7: точки реза, кроссфейды и WSOLA на затухании.

## What

Ветка `feat/edit-region-plan` добавляет первый слой шага 7:

- `src/doc/EditRegionPlan.*` строит промежутки между соседними событиями;
- каждый промежуток использует `protectedZone()` как union атак по всем
  микрофонам, а не reference-only attack;
- `body` остаётся будущему WSOLA на decay, `join` резервируется под
  равноамплитудный crossfade перед следующей protected zone;
- статус региона явно показывает, когда protected zones пересеклись, crossfade
  пришлось укоротить или у события нет наблюдений;
- `kEditCrossfadeMs` задаёт default crossfade в миллисекундах;
- тесты покрывают shared protected zone, budget exhaustion, clamped crossfade и
  стабильность миллисекунд при 48/96 kHz.

Также в `docs/drum-editor-plan.md` зафиксирован Reaper round-trip: standalone
должен уметь генерировать `.RPP` с импортированными стемами, markers/regions и
edit/stretch decisions для ручной доработки инженером. Sidecar остаётся
источником правды; RPP — канал доставки, а не вторая модель состояния.

## Why

Начинать шаг 7 сразу с аудио-рендера рискованно: без проверяемой карты регионов
легко нарушить инвариант 17 и начать растягивать атаку одного из микрофонов.
Поэтому первый PR делает детерминированную модель: что защищено, где есть
место для decay-warp, где можно поставить crossfade и где бюджета уже нет.

Reaper round-trip добавлен в план сейчас, потому что он влияет на форму данных:
regions и edit/stretch decisions должны жить в `beat_doc` и sidecar так, чтобы
потом их можно было экспортировать в RPP без повторного анализа.

## Status

status.md обновлён: да. Ветка: `feat/edit-region-plan`.
PR: #45 draft — https://github.com/maxeliseyev/beat-equalizer/pull/45.
Проверено: `rtk make test`.

## Next

Проверить PR #45; после merge начать crossfade renderer поверх edit regions.

## Open

- WSOLA, аудио-рендер и RPP export не входят в эту ветку.

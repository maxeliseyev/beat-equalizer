# 2026-09-04 — Crossfade edit adapter

## Context

PR #47 смёржен. Следующий шаг по `docs/status.md` и декомпозиции этапа 2,
шага 7 — adapter `EditRegionPlan -> CrossfadeRenderer::EditPoint`.

## What

Ветка `feat/crossfade-edit-adapter` добавляет:

- `src/doc/CrossfadeEditAdapter.*`;
- `buildCrossfadeEditPlan(document, options)`;
- конвертацию `EditRegion.join` в `CrossfadeRenderer::EditPoint` для
  следующего события;
- seed point для предыдущего события, чтобы renderer не применял target delay
  следующего события с начала файла;
- counters для skipped regions и событий без delay rows;
- тесты на seed/target points, skip region без бюджета, skip события без delay
  и интеграцию adapter output с `CrossfadeRenderer`.

## Why

Adapter живёт в `beat_doc`, а не в `beat_dsp` и не в `src/plugin`: только
документ одновременно знает events, edit regions и delay field. `beat_dsp`
остаётся ниже и принимает sample-based edit points, как было решено в PR #46.
Plugin на следующем шаге сможет взять этот план и передать его в renderer без
дублирования логики сборки regions.

Seed point нужен явно: если передать renderer только target point следующего
события, первый point станет начальным состоянием и новая задержка начнёт
действовать с нулевого сэмпла, а не в join-интервале.

## Status

status.md обновлён: да. Ветка: `feat/crossfade-edit-adapter`. PR: #48 draft.
Проверено: `rtk make test`.

## Next

После merge PR #48 сделать Standalone `Export crossfade...` поверх adapter +
`CrossfadeRenderer`.

## Open

- UI/export bridge, YAN9 gate, WSOLA, sidecar и RPP export не входят в эту ветку.

# Status

Updated: 2026-09-04
Stage: 2 (редактор в standalone)
Plan step: этап 2, шаг 7 — DSP crossfade renderer
Branch: `feat/crossfade-renderer`
PR: #46 draft — https://github.com/maxeliseyev/beat-equalizer/pull/46
Blockers: none

## Done

- PR #45 смёржен в `main` squash-коммитом `054a0bf` 2026-09-04:
  `EditRegionPlan` в `beat_doc` строит protected/body/join regions для шага 7,
  а Reaper round-trip зафиксирован в плане.
- Ветка `feat/crossfade-renderer` начата от актуального `main`.
- Добавлен `CrossfadeRenderer` в `beat_dsp`: offline renderer скачка задержки
  под равноамплитудным crossfade.
- Edit points задают общий join-интервал и target delays по каналам; каналы без
  измерения держат base delay.
- `strength = 0` остаётся bit-for-bit bypass.
- `VERSION` поднят до `0.27.0`, `CHANGELOG.md` обновлён.

## Now

- `rtk make test` зелёный.
- Renderer пока не подключён к Standalone/UI/export: текущий PR только добавляет
  DSP-блок и синтетические тесты.

## Next

Проверить PR #46; после merge сделать adapter
`EditRegionPlan -> CrossfadeRenderer::EditPoint` и отдельный Standalone export
для crossfade-render.

## Resume

1. `git fetch && git checkout feat/crossfade-renderer && git pull`
2. Читать `docs/sessions/2026-09-04-crossfade-renderer.md`
3. `rtk make test`
4. Проверить PR #46; после merge подключить crossfade-render к Standalone
   export через edit regions.

## Open

- Crossfade-render ещё не доступен из UI.
- WSOLA на decay ещё не реализован.
- RPP/Reaper round-trip зафиксирован в плане, но не реализован.
- Sidecar JSON для повторного открытия проекта всё ещё предстоит сделать.

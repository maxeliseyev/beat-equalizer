# Status

Updated: 2026-09-04
Stage: 2 (редактор в standalone)
Plan step: этап 2, шаг 7.1 — adapter `EditRegionPlan -> CrossfadeRenderer::EditPoint`
Branch: `feat/crossfade-edit-adapter`
PR: #48 draft — https://github.com/maxeliseyev/beat-equalizer/pull/48
Blockers: none

## Done

- PR #47 смёржен в `main` squash-коммитом `9852bbf` 2026-09-04:
  onboarding/workflow readiness docs обновлены для передачи второму
  разработчику.
- Ветка `feat/crossfade-edit-adapter` начата от актуального `main`.
- Добавлен `CrossfadeEditAdapter` в `beat_doc`: документ строит
  `CrossfadeRenderer::EditPoint` из `EditRegionPlan` и `DelayField`.
- Adapter добавляет seed point для старого target delay, затем target point с
  общим join-интервалом для следующего события.
- Regions без crossfade budget и события без delay rows явно считаются в
  `CrossfadeEditPlan`.
- `VERSION` поднят до `0.28.0`, `CHANGELOG.md` обновлён.

## Now

- `rtk make test` зелёный.
- PR #48 открыт draft.
- UI/export не менялись: crossfade-render пока не доступен из Standalone.

## Next

После merge PR #48 сделать этап 2, шаг 7.2 — Standalone `Export crossfade...`
поверх adapter + `CrossfadeRenderer`.

## Resume

1. `git fetch && git checkout feat/crossfade-edit-adapter && git pull`
2. Читать `docs/sessions/2026-09-04-crossfade-edit-adapter.md`
3. `rtk make test`
4. Проверить PR #48; после merge начать Standalone `Export crossfade...`.

## Open

- Crossfade-render ещё не доступен из UI/export.
- YAN9 crossfade gate ещё не выполнен.
- WSOLA на decay ещё не реализован.
- Sidecar JSON и RPP export зафиксированы в плане, но не реализованы.

# Status

Updated: 2026-09-04
Stage: 2 (редактор в standalone)
Plan step: этап 2, шаг 7 — edit regions для точек реза, кроссфейдов и WSOLA
Branch: `feat/edit-region-plan`
PR: #45 draft — https://github.com/maxeliseyev/beat-equalizer/pull/45
Blockers: none

## Done

- PR #44 смёржен в `main` squash-коммитом `40507a4` 2026-09-04:
  YAN9 Advanced diagnostics gate показал, что glide сам по себе недостаточен.
- Ветка `feat/edit-region-plan` начата от актуального `main`.
- Добавлен `EditRegionPlan` в `beat_doc`: для соседних событий строятся общие
  regions из protected zones, body для будущего WSOLA и join для crossfade.
- План явно помечает нехватку бюджета: missing protected zone, overlapping
  protected zones, clamped crossfade.
- `docs/drum-editor-plan.md` расширен Reaper round-trip: standalone должен
  уметь экспортировать `.RPP` с импортированными стемами, markers/regions и
  edit/stretch decisions, а sidecar остаётся источником правды.
- `VERSION` поднят до `0.26.0`, `CHANGELOG.md` обновлён.

## Now

- `rtk make test` зелёный.
- Аудио-рендер, UI и RPP export в этой ветке не реализованы: текущий PR только
  закладывает проверяемую модель регионов этапа 2, шаг 7.

## Next

Проверить PR #45; после merge идти к DSP-рендеру равноамплитудного crossfade
по готовым edit regions.

## Resume

1. `git fetch && git checkout feat/edit-region-plan && git pull`
2. Читать `docs/sessions/2026-09-04-edit-region-plan.md`
3. `rtk make test`
4. Проверить PR #45 и после merge начать crossfade renderer поверх edit
   regions.

## Open

- WSOLA на decay ещё не реализован.
- RPP/Reaper round-trip зафиксирован в плане, но не реализован.
- Sidecar JSON для повторного открытия проекта всё ещё предстоит сделать.

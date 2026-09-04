# Status

Updated: 2026-09-05
Stage: 2 (редактор в standalone)
Plan step: продуктовая дорожная карта этапов 2+
Branch: `docs/product-roadmap`
PR: #50 draft — https://github.com/maxeliseyev/beat-equalizer/pull/50
Blockers: none

## Done

- PR #49 смёржен в `main` squash-коммитом `f4ce518` 2026-09-04:
  macOS onboarding prerequisites зафиксированы в документации.
- Ветка `docs/product-roadmap` начата от актуального `main`.
- `docs/drum-editor-plan.md` расширен: этапы 2–5 получили продуктовые срезы,
  gates, релизную линию и post-v1 discovery-очередь.
- Reaper `.RPP` export убран из этапа 2 и оставлен delivery-адаптером этапа 4.
- `VERSION` поднят до `0.28.2`, `CHANGELOG.md` и handoff обновлены.
- `rtk git diff --check` и `rtk make test` зелёные.

## Now

- Документационная правка готова; draft PR #50 открыт.
- Код DSP/doc/plugin и поведение приложения не менялись.

## Next

- После merge продолжить этап 2, шаг 7.2: Standalone
  `Export crossfade...` поверх adapter + `CrossfadeRenderer`.

## Resume

1. `git fetch && git checkout docs/product-roadmap && git pull`
2. Читать секцию 9 в `docs/drum-editor-plan.md`
3. `rtk git diff --check && rtk make test`
4. Проверить PR #50; после merge начать Standalone `Export crossfade...`.

## Open

- Crossfade-render ещё не доступен из UI/export.
- YAN9 crossfade gate ещё не выполнен.
- WSOLA на decay ещё не реализован.
- Sidecar JSON и RPP export зафиксированы в плане, но не реализованы.

# Status

Updated: 2026-09-04
Stage: 2 (редактор в standalone)
Plan step: macOS onboarding prerequisites для второго разработчика
Branch: `docs/macos-onboarding-prereqs`
PR: #49 draft — https://github.com/maxeliseyev/beat-equalizer/pull/49
Blockers: none

## Done

- PR #48 смёржен в `main` squash-коммитом `8a58f18` 2026-09-04:
  `CrossfadeEditAdapter` добавлен в `beat_doc`.
- Ветка `docs/macos-onboarding-prereqs` начата от актуального `main`.
- `docs/onboarding.md` дополнен явным macOS dev setup: Command Line Tools,
  Homebrew-пакеты, проверка окружения, `gh auth`, Reaper host-smoke и
  release-only требования.
- `README.md` ссылается на onboarding как источник macOS setup.
- `VERSION` поднят до `0.28.1`, `CHANGELOG.md` обновлён.

## Now

- Документационная правка готовится к draft PR.
- `rtk git diff --check` и `rtk make test` зелёные.
- PR #49 открыт draft.
- Код DSP/doc/plugin не менялся.

## Next

После merge PR #49 вернуться к этапу 2, шагу 7.2 — Standalone
`Export crossfade...` поверх adapter + `CrossfadeRenderer`.

## Resume

1. `git fetch && git checkout docs/macos-onboarding-prereqs && git pull`
2. Читать `docs/onboarding.md`
3. `rtk git diff --check`
4. Проверить PR #49; после merge начать Standalone `Export crossfade...`.

## Open

- Crossfade-render ещё не доступен из UI/export.
- YAN9 crossfade gate ещё не выполнен.
- WSOLA на decay ещё не реализован.
- Sidecar JSON и RPP export зафиксированы в плане, но не реализованы.

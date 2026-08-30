# Status

Updated: 2026-08-30
Stage: 1 (static alignment)
Plan step: chore — Makefile + `docs/` (хвост после merge #3)
Branch: `chore/makefile`
PR: https://github.com/maxeliseyev/beat-equalizer/pull/4
Blockers: none

## Done

- PR 1–3 в `main`: скелет, GCC-PHAT, Lagrange delay + PDC + таблица каналов.
- `DOCUMENTATION/` переименован в `docs/`, курсор — `docs/status.md`.
- В этом PR также `Makefile`.

## Now

Донести #4 в `main`. Не начинать Analyze на этой ветке.

## Next

После merge — **plan PR 4: AlignmentEngine + worker + Analyze** (это не GitHub #4).
Reaper: `docs/reaper-testing.md`. Сборка: `make`.

## Resume

1. `git fetch && git checkout chore/makefile && git pull`
2. `make test`
3. После merge — от свежего `main`: `feat/analyze-engine`.

## Open

- Коммерческая лицензия JUCE vs GPL.
- `Bteq` / `Algn` — заглушки.
- Windows — после macOS VST3.
- GitHub Require PR on `main`.

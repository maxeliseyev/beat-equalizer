# Status

Updated: 2026-08-30
Stage: 1 (static alignment)
Plan step: docs — detector canon + semver 0.1.1
Branch: `docs/detector-design`
PR: https://github.com/maxeliseyev/beat-equalizer/pull/6
Blockers: none

## Done

- Канон детекции вшит в AGENTS / план продукта.
- Версионирование: `VERSION` = 0.1.1, changelog, UI показывает номер.

## Now

Донести PR #6 в `main`. Код детектора не писать.

## Next

Осциллограф (#5) если ещё открыт; затем Analyze по detector-design.

## Resume

1. `git fetch && git checkout docs/detector-design && git pull`
2. `make version` → 0.1.1
3. После merge — Analyze с свежего `main`.

## Open

- GitHub Require PR on `main`.

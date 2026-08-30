# Status

Updated: 2026-08-30
Stage: 1 (static alignment)
Plan step: PR 3 — дробная задержка + PDC
Branch: `feat/fractional-delay`
PR: https://github.com/maxeliseyev/beat-equalizer/pull/3
Blockers: none

## Done

- PR 1–2 в `main`: скелет, GCC-PHAT.
- На этой ветке: `LatencyModel` (applied = max(d)−d, latency = ceil(max)+interpolator),
  `FractionalDelay` Lagrange 5, invert, сглаживание 5 мс.
- `processBlock` читает delayMs / polarity / enabled / A-B, `setLatencySamples`.
- Тесты: 21 cases зелёные. Analyze ещё нет — задержка ручная.
- Минимальный GUI: таблица всех живых каналов, пик входа, PDC, hint для Reaper
  (`DOCUMENTATION/reaper-testing.md`).
- `Makefile`: `make` / `make debug` / `make test` / `make vst3|au|standalone` / `make run`.
  Release уже собран и установлен в `~/Library/Audio/Plug-Ins/`.

## Now

Ждём squash-merge [#3](https://github.com/maxeliseyev/beat-equalizer/pull/3)
и ручную проверку в Reaper по `reaper-testing.md`.
Не подключать GCC-PHAT к processBlock.

## Next

После merge #3 — **PR 4: AlignmentEngine + worker + Analyze** (ring buffer, snapshot).
Проверка в Reaper: `make` (уже Release в Plug-Ins) и `DOCUMENTATION/reaper-testing.md`.

## Resume

1. `git fetch && git checkout feat/fractional-delay && git pull`
2. `make test` или `make` (Release).
3. Reaper: `DOCUMENTATION/reaper-testing.md`. После merge — `feat/analyze-engine` от `main`.

## Open

- Коммерческая лицензия JUCE vs GPL.
- `Bteq` / `Algn` — заглушки.
- Windows — после macOS VST3.
- GitHub Require PR on `main`.

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

## Now

Ждём squash-merge [#3](https://github.com/maxeliseyev/beat-equalizer/pull/3).
Не подключать GCC-PHAT к processBlock.

## Next

После merge — **PR 4: AlignmentEngine + worker + Analyze** (ring buffer, snapshot).

## Resume

1. `git fetch && git checkout feat/fractional-delay && git pull`
2. `cmake --preset debug && cmake --build --preset debug --target beat_tests && ./build/debug/tests/beat_tests`
3. Standalone: Ch 2 delay, Invert, A/B. После merge — `feat/analyze-engine` от `main`.

## Open

- Коммерческая лицензия JUCE vs GPL.
- `Bteq` / `Algn` — заглушки.
- Windows — после macOS VST3.
- GitHub Require PR on `main`.

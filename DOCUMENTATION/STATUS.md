# Status

Updated: 2026-08-30
Stage: 1 (static alignment)
Plan step: PR 2 — GCC-PHAT + синтетические тесты
Branch: `feat/gcc-phat`
PR: opening
Blockers: none

## Done

- PR 1 влит в `main` (#1): скелет JUCE, контракт репо, handoff.
- На этой ветке: `beat::Fft` (radix-2), `beat::GccPhat` (полоса 100 Hz–8 kHz,
  окно τmax, парабола, полярность по невзвешенной корреляции).
- Тесты: целая/дробная задержка, знак лага, инверсия, ложный период 20 мс,
  48/96 kHz. `beat_tests` зелёные (12 cases). Плагин не вызывает GCC-PHAT.

## Now

Открыть PR 2 в `main`. UI / processBlock / worker не трогать.

## Next

После merge — **PR 3: дробная задержка + PDC** в realtime. Не начинать на этой ветке.

## Resume

1. `git fetch && git checkout feat/gcc-phat && git pull`
2. `cmake --preset debug && cmake --build --preset debug --target beat_tests && ./build/debug/tests/beat_tests`
3. Если PR открыт — ревью. Если влит — ветка от `main`: `feat/fractional-delay`.

## Open

- Коммерческая лицензия JUCE vs GPL (на код не влияет).
- `Bteq` / `Algn` — заглушки.
- Windows — после macOS VST3.
- GitHub Require PR on `main` — всё ещё руками.

# Status

Updated: 2026-08-30
Stage: 1 (static alignment)
Plan step: PR 1 — скелет плагина + контракт репо
Branch: `feat/plugin-skeleton`
PR: https://github.com/maxeliseyev/beat-equalizer/pull/1
Blockers: none

## Done

- Продуктовый план: `DOCUMENTATION/drum-editor-plan.md`.
- План MVP: `DOCUMENTATION/plan.md` (PR 1–8).
- Скелет C++/JUCE: CMake, JUCE 8.0.15 FetchContent, `beat_dsp`, N-in/N-out
  passthrough, APVTS 24 слота, Catch2; debug-сборка и `beat_tests` зелёные.
- Контракт репо: `AGENTS.md`, `.clang-format`, git-флоу без `develop` до v1,
  handoff/`STATUS.md`.
- ADR: `mvp-static-nin-nout-no-ara`, `git-trunk-until-v1`.

На `origin/main` пока стартовый коммит. Первый PR открыт.

## Now

Ждём squash-merge [#1](https://github.com/maxeliseyev/beat-equalizer/pull/1).
GCC-PHAT в этот PR не входит.

## Next

После merge в `main` — **PR 2: GCC-PHAT + синтетика** (`src/dsp`, тесты, без UI).
Не начинать PR 2 на `feat/plugin-skeleton`.

## Resume

1. `git fetch && git checkout feat/plugin-skeleton && git pull`
2. `cmake --preset debug && cmake --build --preset debug && ctest --test-dir build/debug --output-on-failure`
3. PR открыт: ревью или merge. После merge — ветка от свежего `main`: `feat/gcc-phat`.

## Open

- Коммерческая лицензия JUCE vs GPL на время разработки (на код не влияет).
- Имя плагина / коды `Bteq` `Algn` — заглушки, живут.
- Windows — после macOS VST3.
- GitHub: включить Require PR on `main`.

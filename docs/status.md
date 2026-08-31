# Status

Updated: 2026-08-31
Stage: 1 (static alignment)
Plan step: PR 5 — олпасс + когерентность + auto-rotate
Branch: `feat/rotator-coherence`
PR: открыть из этой ветки
Blockers: none

## Done

- PR 1–4 плана слиты в `main` (репо-PR 1–9), последний — Analyze engine (0.2.0).
- На ветке: `AllpassRotator`, `Coherence`, auto-rotate на Analyze, ротатор в
  `processBlock` после задержки, строка «Sum coherence до → после» в UI, 0.3.0.

## Now

Ветка готова к PR. Ушами не проверено: ротатор и рост когерентности на реальном
ките — автотесты гоняют синтетику.

## Next

PR 6 — таблица каналов целиком (колонки rotator и corr), коррелометр, Mono Sum,
компактная отрисовка снимка анализа.

## Resume

1. `git fetch && git checkout feat/rotator-coherence && git pull`
2. `make test && make test-gui && make standalone`
3. Standalone: Analyze на паре со сдвигом и инверсией — «Sum coherence» должна
   вырасти; A/B обязан звучать без ротатора.

## Open

- Ротатор не виден в таблице каналов (колонка — в PR 6).
- Сетка перебора ротатора 12 × 4 не калибрована на живых китах (PR 8).
- GitHub Require PR on `main`.

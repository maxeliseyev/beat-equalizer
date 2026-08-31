# Status

Updated: 2026-08-31
Stage: 1 (static alignment)
Plan step: PR 4 — AlignmentEngine + worker + Analyze
Branch: `feat/analyze-engine`
PR: none (в работе)
Blockers: none

## Done

- PR 1–8 в `main`: скелет, GCC-PHAT, Lagrange+PDC, Makefile/docs, осциллограф,
  канон детектора, Time-окно скопа (0.1.1).

## Now

Ветка только заведена, STATUS синхронизирован с git (PR 7 и 8 слиты).
Дальше — ядро анализа: кольцевой буфер сырого входа, AlignmentEngine
(кадры → GCC-PHAT → медиана → полярность → snapshot), worker-поток, кнопки
Analyze / Freeze.

## Next

Собрать `make test` зелёным и открыть PR 4.

## Resume

1. `git fetch && git checkout feat/analyze-engine && git pull`
2. `make test && make test-gui`
3. Standalone: два канала, сдвиг 2.3 мс + инверсия → Analyze находит.

## Open

- GitHub Require PR on `main`.

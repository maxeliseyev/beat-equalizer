# Status

Updated: 2026-08-31
Stage: 1 (static alignment)
Plan step: PR 4 — AlignmentEngine + worker + Analyze
Branch: `feat/analyze-engine`
PR: https://github.com/maxeliseyev/beat-equalizer/pull/9
Blockers: none

## Done

- PR 1–8 в `main`: скелет, GCC-PHAT, Lagrange+PDC, Makefile/docs, осциллограф,
  канон детектора, Time-окно скопа (0.1.1).
- На ветке: кольцевой буфер сырого входа, `AlignmentEngine`, worker-поток,
  кнопки Analyze / Freeze, статус в UI, версия 0.2.0.
  `make test` (38 кейсов) и `make test-gui` (7 кейсов) зелёные.

## Now

PR 9 открыт, ждёт приёмки. Не проверено ушами: реальный кит и Standalone с двумя WAV —
автотесты гоняют синтетику через `processBlock`, но не звук в живом хосте.

## Next

Ручная приёмка в Standalone (2 канала, сдвиг ~2.3 мс + инверсия), затем PR 5 —
олпасс-ротатор и когерентность.

## Resume

1. `git fetch && git checkout feat/analyze-engine && git pull`
2. `make test && make test-gui && make standalone`
3. Standalone: подать материал, нажать Analyze, сверить delay в таблице с
   ожидаемым; Freeze не должен затирать ручную правку.

## Open

- Опора пересчитывает FFT на каждый канал: 8 с × 16 каналов — пара секунд на
  проход. Кэш спектра опоры, если станет мешать.
- GitHub Require PR on `main`.

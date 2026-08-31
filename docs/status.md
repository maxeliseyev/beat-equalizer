# Status

Updated: 2026-08-31
Stage: 1 (static alignment)
Plan step: PR 6 — таблица, коррелометр, Mono Sum
Branch: `feat/table-correlometer`
PR: https://github.com/maxeliseyev/beat-equalizer/pull/11
Blockers: none

## Done

- PR 1–5 плана в `main` (репо-PR 1–10), последний — ротатор и когерентность (0.3.0).
- На ветке: колонки Role / Rotator / Corr, `ChannelColumns`, коррелометр,
  Mono Sum, `beat::correlation`, 0.4.0.

## Now

PR 11 открыт, ждёт приёмки. Ушами не проверено: Mono Sum и коррелометр на живом ките.

## Next

PR 7 — Standalone loader (несколько WAV на каналы), export aligned WAV,
персистентность последних оценок между открытиями проекта.

## Resume

1. `git fetch && git checkout feat/table-correlometer && git pull`
2. `make test && make test-gui && make standalone`
3. Standalone: включить Mono Sum, проверить, что стемы выше 1-2 не сломались;
   инвертировать канал руками — Corr и коррелометр должны уйти в минус.

## Open

- Роль канала — метаданные, алгоритм её не читает (инвариант 8).
- Corr зависит от окна Time скопа: на редком материале цифра шумит.
- GitHub Require PR on `main`.

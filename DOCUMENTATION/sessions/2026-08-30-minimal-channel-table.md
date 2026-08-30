# 2026-08-30 — минимальная таблица каналов

## Context

PR 3 даёт delay/PDC, но GUI показывал только Ch 1–2. В Reaper непонятно,
доходит ли стем и какой канал крутить.

## What

- Строка на каждый живой вход: On, пик входа, Delay ms, Polarity.
- Шапка: N in/out, PDC в сэмплах и мс, A/B, hint по routing.
- `DOCUMENTATION/reaper-testing.md`.
- Добавлено в открытый PR 3: без этого его нельзя проверить в хосте.

## Why

Полный UI (коррелометр, waveform) — PR 6. Сейчас нужен минимальный стенд:
пик = «routing живой», слайдер = «сдвиг слышен», PDC = «хост компенсирует».

Не отдельный PR: иначе #3 остаётся непроверяемым.

## Status

STATUS.md обновлён: да. Ветка: `feat/fractional-delay`.

## Next

Merge #3, руками прогнать Reaper по `reaper-testing.md`, затем PR 4 Analyze.

## Open

Нет.

# 2026-08-30 — fractional delay + PDC

## Context

GCC-PHAT влит, но `processBlock` был passthrough. Нужен слышимый ручной сдвиг.

## What

- `LatencyModel`: TDOA → applied ≥ 0, `reportedLatency = ceil(max) + 2`.
- `FractionalDelay`: Lagrange 5, min delay = interpolator (2 сэмпла), сглаживание 5 мс.
- Processor: delayMs как applied, polarity Invert, A/B = все каналы с max applied
  без инверсии, та же PDC.
- Editor: слайдеры Ch1/Ch2 delay, polarity Ch2.
- GCC-PHAT в realtime не вызывается.

## Why

delayMs в APVTS — applied, не сырой TDOA: до Analyze пользователю незачем
видеть «канал позже на X». Analyze (PR 4) запишет TDOA и прогонит через
`applyTdoa`.

A/B задерживает dry на max(applied), не на 0: иначе bypass прыгает по времени
на величину PDC.

## Status

STATUS.md обновлён: да. Ветка: `feat/fractional-delay`.

## Next

PR 4: worker + Analyze.

## Open

Нет.

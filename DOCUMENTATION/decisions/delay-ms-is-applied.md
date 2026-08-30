# delayMs — applied delay, не сырой TDOA

Status: accepted  
Date: 2026-08-30

## Context

В APVTS уже есть `chNN.delayMs` (0–20). Нужно решить, это сдвиг, который
слышит пользователь, или оценка GCC относительно опоры.

## Decision

`delayMs` = **applied** delay (≥ 0). PDC = ceil(max applied) + interpolator (2).
Сырой TDOA живёт в `AlignmentSnapshot` / Analyze и превращается в applied
через `LatencyModel::applyTdoa`.

A/B bypass: все каналы получают `max(applied)` без инверсии, та же latency.

## Alternatives

- Писать TDOA в `delayMs` (может быть «отрицательным» смыслом vs опора) —
  ломает слайдер 0–20 и ручной монтаж до Analyze.
- Latency = 0 — кит уезжает позже относительно сессии.

## Consequences

PR 4 не меняет parameter ID. Analyze обновляет delayMs уже как applied
(после `applyTdoa`) либо держит TDOA отдельно и пишет applied в параметры.

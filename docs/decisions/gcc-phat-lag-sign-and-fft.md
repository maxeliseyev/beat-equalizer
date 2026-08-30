# GCC-PHAT: знак лага и свой FFT

Status: accepted  
Date: 2026-08-30

## Context

Нужна оценка TDOA в `beat_dsp` с тестами без GUI. Нужна договорённость о знаке
лага для модели PDC (`applied = max(d) - d`).

## Decision

- Положительный `lagSamples` значит: канал (`signal`) **позже** опоры.
  Реализация: `G = Y * conj(X)`.
- FFT — собственный radix-2 в `src/dsp/Fft.cpp`, не `juce::dsp::FFT` в этой либе.
- Полярность — знак невзвешенной корреляции в точке лага.

## Alternatives

- `X * conj(Y)` — даёт противоположный знак; отвергнуто тестом
  `sig[i] = ref[i - D] ⇒ lag = +D`.
- `juce::dsp::FFT` в `beat_dsp` — конфликт/дублирование модуля с plugin-таргетом
  и тяжёлые тесты. pffft оставили на случай нехватки скорости worker'а.

## Consequences

AlignmentEngine (PR 4) берёт `d[i]` как есть. Менять знак только вместе с тестами
и этим ADR.

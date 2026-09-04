# 2026-09-04 — Crossfade renderer

## Context

PR #45 смёржен: `beat_doc` умеет строить edit regions с protected/body/join.
Следующий шаг из `status.md` — DSP-рендер равноамплитудного crossfade поверх
этих regions.

## What

Ветка `feat/crossfade-renderer` добавляет `src/dsp/CrossfadeRenderer.*`:

- renderer принимает sample-based edit points, а не `beat_doc` structs, чтобы
  `beat_dsp` не зависел от документа;
- каждый edit point задаёт общий join-интервал и target delay по каналам;
- missing channel продолжает base delay;
- переход между target delays делается равноамплитудным blend:
  `from + (to - from) * mix`;
- `strength = 0` обходит DSP и копирует исходник bit-for-bit;
- тесты покрывают bypass, отсутствие gain bump на correlated material,
  скачок задержки внутри join, base delay для отсутствующего канала и skip
  edit point без валидного join.

## Why

Crossfade — DSP-примитив, а не часть документа. Если бы `beat_dsp` принимал
`EditRegion`, зависимость пошла бы вверх (`beat_dsp -> beat_doc`) и сломала бы
границу, нужную для будущего ARA/RPP adapter. Поэтому документ позже только
конвертирует свои regions в простые sample-based edit points.

Renderer использует две `FractionalDelay` линии, которые получают один и тот же
input stream: так можно одновременно прочитать старую и новую задержку в join и
смешать их без сглаживания delay glide. Равноамплитудный crossfade выбран по
канону, потому что материал коррелирован и equal-power даёт горб.

## Status

status.md обновлён: да. Ветка: `feat/crossfade-renderer`.
PR: #46 draft — https://github.com/maxeliseyev/beat-equalizer/pull/46.
Проверено: `rtk make test`.

## Next

Проверить PR #46; после merge подключить crossfade-render к Standalone export
через edit regions.

## Open

- UI/export adapter, WSOLA и RPP export не входят в эту ветку.

# Changelog

Формат: [Keep a Changelog](https://keepachangelog.com/). Версия — semver
`major.minor.patch`. Источник правды: файл `VERSION`. Как бампать: `docs/versioning.md`.

## 0.2.0 — 2026-08-31

### Added

- Analyze: кольцевой буфер сырого входа (8 с), worker-поток, оценка задержки и
  полярности на канал против опоры, результат — в параметры каналов.
- Freeze: оценки считаются и показываются, но не применяются.
- Уверенность кадра в GCC-PHAT (пик к медиане окна поиска) — тихие и
  некоррелированные кадры не портят медиану.

### Changed

- Диапазон `chNN.delayMs` выводится из `kMaxDistanceM` (~30 мс), а не литерал
  20 мс: линия задержки обязана покрывать самую дальнюю дистанцию поиска.

## 0.1.1 — 2026-08-30

### Added

- Канон репозитория: `docs/`, `AGENTS.md`, Makefile, handoff.
- Канон детекции ударов (`detector-design-recommendations.md`): лестница без сетей.
- Semver: файл `VERSION`, этот changelog, версия в заголовке плагина.

## 0.1.0 — 2026-08-30

### Added

- N-in / N-out passthrough (2–24), VST3 / AU / Standalone.
- GCC-PHAT, дробная задержка Lagrange, PDC, A/B.
- Таблица каналов: enable, delay, polarity, пик входа.

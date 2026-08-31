# Status

Updated: 2026-08-31
Stage: 1 (static alignment)
Plan step: PR 7 — Standalone loader, export, персистентность
Branch: `feat/standalone-bench`
PR: открыть из этой ветки
Blockers: none

## Done

- PR 1–6 плана в `main` (репо-PR 1–11), последний — таблица и коррелометр (0.4.0).
- На ветке: `FilePlayer`, `Exporter`, версионный блоб оценок, ряд стенда в UI,
  Analyze читает файлы, 0.5.0.

## Now

Ветка готова к PR. Стенд собран, но на живом ките ещё не гонялся — это и есть
следующий шаг плана.

## Next

PR 8 — прогон на реальном ките: 8-16 микрофонов через стенд, калибровка
`kAnalysisMinPeakRatio` и сетки ротатора, протокол ожидаемых задержек в `docs/`.

## Resume

1. `git fetch && git checkout feat/standalone-bench && git pull`
2. `make test && make test-gui && make standalone && make run`
3. В Standalone: Load files… на набор стемов, Play, Analyze, Export aligned…,
   затем сверить экспорт в редакторе.

## Open

- Клип целиком в памяти: 16 дорожек по минуте ≈ 180 МБ.
- Проигрывание ограничено каналами устройства, анализ и экспорт — нет.
- GitHub Require PR on `main`.

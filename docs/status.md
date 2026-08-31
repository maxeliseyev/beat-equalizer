# Status

Updated: 2026-08-31
Stage: 1 (static alignment)
Plan step: между PR 7 и PR 8 — UI-правка перед прогоном на ките
Branch: `feat/per-channel-scopes`
PR: https://github.com/maxeliseyev/beat-equalizer/pull/13
Blockers: none

## Done

- PR 1–7 плана в `main` (репо-PR 1–12), последний — стенд Standalone (0.5.0).
- На ветке: осциллограмма в строке канала, строки по материалу стенда,
  `FilePlayer::readDisplayWindow`, 0.6.0.

## Now

PR 13 открыт, ждёт приёмки. Ветка зелёная: `make test` 61 кейс,
`make test-gui` 28 кейсов.

## Next

PR 8 — прогон на реальном ките: 8-16 микрофонов через стенд, калибровка
`kAnalysisMinPeakRatio` и сетки ротатора, протокол ожидаемых задержек в `docs/`.

## Resume

1. `git fetch && git checkout feat/per-channel-scopes && git pull`
2. `make test && make test-gui && make standalone && make run`
3. В Standalone: Load files… на набор стемов (моно-файлы кладутся по каналам
   подряд, порядок — как в диалоге), Play, Analyze, Export aligned…

## Open

- Клип целиком в памяти: 16 дорожек по минуте ≈ 180 МБ.
- Проигрывание ограничено каналами устройства; анализ, картинка и экспорт — нет.
- Файлы обязаны быть экспортированы от одного нуля сессии: стенд не выравнивает
  старты, он их измеряет.
- Ротатор не участвует в отрисовке трасс (только в звуке и экспорте).
- GitHub Require PR on `main`.

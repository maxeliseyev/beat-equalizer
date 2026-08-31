# Status

Updated: 2026-08-31
Stage: 1 (static alignment)
Plan step: подготовка к этапу 2/3 — темп, сетка и сравнение фаз в интерфейсе
Branch: `feat/tempo-grid-phase`
PR: https://github.com/maxeliseyev/beat-equalizer/pull/15
Blockers: none

## Done

- PR 1–7 плана в `main` (репо-PR 1–14), последний — документ о том, как двигать
  удары во времени.
- На ветке: темп от хоста или вручную, сетка на трассах, колонка Phase %,
  `src/dsp/Grid.h`, 0.7.0.

## Now

Ветка зелёная: `make test` 66 кейсов, `make test-gui` 38 кейсов.

**Важно:** сквош PR #13 захватил только два коммита из трёх — Solo/Mute, имена
дорожек, кнопка Audio… и try-lock отрисовки на `main` не попали. Потерянный
коммит `31b2c95` восстановлен черри-пиком в этой ветке; проверять при мердже,
что он доехал.

## Next

PR 8 плана — прогон на реальном ките: 8-16 микрофонов через стенд, калибровка
`kAnalysisMinPeakRatio` и сетки ротатора, протокол ожидаемых задержек в `docs/`.

Этап 2 начинать по `timing-design-recommendations.md`: события, поле задержек,
glide — и только потом нарезка.

## Resume

1. `git fetch && git checkout feat/tempo-grid-phase && git pull`
2. `make test && make test-gui && make standalone && make run`
3. В Standalone: Audio… (буфер), Load files…, Tempo Manual + Grid 1/8,
   Time 500-1000 мс — линии сетки видны на трассах; Analyze — колонка Phase %.

## Open

- Клип целиком в памяти: 16 дорожек по минуте ≈ 180 МБ.
- Проигрывание ограничено каналами устройства; анализ, картинка и экспорт — нет.
- Файлы обязаны быть экспортированы от одного нуля сессии.
- Ротатор не участвует в отрисовке трасс (только в звуке и экспорте).
- Solo/Mute — мониторинг: в `Export aligned` не попадают.
- Сетка на стенде считается от начала клипа: своей позиции в такте у файла нет.
- GitHub Require PR on `main`.

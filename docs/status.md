# Status

Updated: 2026-09-04
Stage: 2 (редактор в standalone)
Plan step: этап 2, шаг 6 — Basic/Advanced channel table перед source-centric gate UI
Branch: `feat/basic-channel-table`
PR: none
Blockers: none

## Done

- Этап 1 закрыт: статическое выравнивание слышно на реальном ките.
- Этап 2, шаги 1–5 в `main`: `beat_doc`, детектор, сверка по микрофонам,
  калибровка сессии, события/маркеры в standalone.
- PR #36 смёржен в `main` squash-коммитом `141c24b` 2026-09-03.
- PR #37 смёржен в `main` squash-коммитом `0b4658b` 2026-09-03:
  source-centric diagnostic показал, что aggregate coherence не годится как
  gate перед нарезкой.
- PR #38 смёржен в `main` squash-коммитом `4c5356f` 2026-09-03:
  matcher доверяет калиброванным priors и держит snare close-пару на протоколе.
- PR #39 смёржен в `main` squash-коммитом `ad45b3b` 2026-09-03:
  Standalone показывает compact source-centric status, а `d/hit ms` после
  Detect берёт source-owned задержки.
- PR #40 смёржен в `main` squash-коммитом `a866ddc` 2026-09-04:
  Basic/Advanced UI mode добавил `global.uiMode`, header toggle и видимость
  top-level advanced-блоков.

## Now

- Basic/Advanced rollout фиксируется в продуктовых документах как серия PR:
  каркас режима, таблица каналов, Basic outcome/status, Advanced diagnostics v2.
- PR 41 реализован на ветке: `ChannelTableMode` управляет геометрией
  `ChannelColumns`/`ChannelRow`, а `PluginEditor` передаёт режим из
  `global.uiMode`.
- Basic скрывает ручные/диагностические колонки таблицы (`Delay`, `Rot`, `Pol`,
  `Corr`, `Phase`, `d/hit ms`, monitor `Level/Pan`) и отдаёт ширину waveform.
  В Standalone Basic оставляет monitor `S/M`, когда загружен bench material.
- Advanced сохраняет текущую таблицу целиком.
- DSP, glide, source matching, export и APVTS channel parameters не менялись.
- Версия поднята до `0.23.0`; changelog обновлён.
- Проверено: `make test`, `make test-gui`, `git diff --check`.

## Next

Открыть draft PR в `main`; после merge продолжить PR 42 — Basic outcome/status.

## Resume

1. `git fetch && git checkout feat/basic-channel-table && git pull`
2. Читать `docs/sessions/2026-09-04-basic-channel-table.md`
3. `make test`
4. `make test-gui`
5. Открыть или проверить draft PR в `main`.

## Open

- Compact source status есть, но полной таблицы source diagnostics в UI ещё нет.
- Ownership/классификация источника всё ещё грубая: trusted prior держит
  геометрию известных пар, но не доказывает, что каждый snare-owned marker
  действительно снейр.
- Basic/Advanced имеет первый пользовательский переключатель и mode-aware
  таблицу каналов; Basic outcome/status ещё не сделан.
- Ролей каналов в UI нет, поэтому compact status пишет `late Ch`, а не
  `room/OH return`.
- Реальный кит один; пороги не подгонять под него.
- `beat_doc` пока не сериализуется.
- Матрицу профиля и просачивания всё ещё негде смотреть.
- Strength glide общий на все каналы; per-channel strength для комнаты/оверхедов
  ещё нет.
- Full-file glide export после 20-секундного Detect держит первую/последнюю
  цель за пределами окна Detect.
- Мониторинг идёт только в выходы 1-2: кит на восемь выходов карты не разводится.
- Developer ID и нотаризация зависят от локальных сертификатов.

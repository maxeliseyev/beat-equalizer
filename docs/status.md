# Status

Updated: 2026-09-04
Stage: 2 (редактор в standalone)
Plan step: этап 2, шаг 6 — Basic outcome/status перед source-centric gate UI
Branch: `feat/basic-outcome-status`
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
- PR #41 смёржен в `main` squash-коммитом `389d882` 2026-09-04:
  Basic/Advanced таблица каналов скрывает ручные и диагностические колонки в
  Basic, отдавая ширину waveform.

## Now

- Basic/Advanced rollout фиксируется в продуктовых документах как серия PR:
  каркас режима, таблица каналов, Basic outcome/status, Advanced diagnostics v2.
- PR 42 реализован на ветке: Basic показывает один короткий primary status
  (`Load files to begin`, `Ready to Detect`, `Export ready: N hits found`,
  `Aligned. A/B to compare`, `Review in Advanced: no hits found`).
- Basic скрывает detailed `detectStatus` и technical hint; Advanced сохраняет
  подробные строки анализа, detect, source status и Glide preview/coherence.
- Basic bench row больше не показывает `Glide preview ... event coherence`;
  успешный glide export в Basic сворачивается до `Exported <file>`.
- DSP, glide, source matching, export, таблица каналов и APVTS channel
  parameters не менялись.
- Версия поднята до `0.24.0`; changelog обновлён.
- Проверено: `make test`, `make test-gui`, `git diff --check`.

## Next

Открыть draft PR в `main`; после merge продолжить PR 43 — Advanced
diagnostics v2.

## Resume

1. `git fetch && git checkout feat/basic-outcome-status && git pull`
2. Читать `docs/sessions/2026-09-04-basic-outcome-status.md`
3. `make test`
4. `make test-gui`
5. Открыть или проверить draft PR в `main`.

## Open

- Compact source status есть, но полной таблицы source diagnostics в UI ещё нет.
- Ownership/классификация источника всё ещё грубая: trusted prior держит
  геометрию известных пар, но не доказывает, что каждый snare-owned marker
  действительно снейр.
- Basic/Advanced имеет переключатель, mode-aware таблицу каналов и Basic
  outcome/status; Advanced diagnostics v2 ещё не сделан.
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

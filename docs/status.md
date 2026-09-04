# Status

Updated: 2026-09-04
Stage: 2 (редактор в standalone)
Plan step: этап 2, шаг 6 — PR 43 Advanced diagnostics v2
Branch: `feat/advanced-diagnostics-v2`
PR: #43 draft — https://github.com/maxeliseyev/beat-equalizer/pull/43
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
- PR #42 смёржен в `main` squash-коммитом `e5aaf7e` 2026-09-04:
  Basic показывает короткий outcome/status, Advanced сохраняет подробные
  detect/source/glide строки.

## Now

- PR #43 реализован на ветке `feat/advanced-diagnostics-v2`.
- Advanced получил source-centric таблицу по всем активным каналам: роль,
  usage, observations, natural delay, spread, full-align offset, residual и
  calibration flag.
- Advanced channel table показывает role control для существующего
  `chXX.role`; Basic role control и source diagnostics не показывает.
- Compact source status пишет `OH return`/`room return`, когда роль позднего
  канала задана как overhead/room.
- Роли остаются метаданными: они подписывают строки, но не меняют DSP, Detect,
  matcher, glide или export.
- Версия поднята до `0.25.0`; changelog обновлён.
- Проверено: `make test`, `make test-gui`, `git diff --check`.

## Next

После merge PR #43 прогнать Advanced diagnostics на YAN9 и решить gate к
шагу 7: нужны ли точки реза/кроссфейды/WSOLA или glide достаточно.

## Resume

1. `git fetch && git checkout feat/advanced-diagnostics-v2 && git pull`
2. Читать `docs/sessions/2026-09-04-advanced-diagnostics-v2.md`
3. `make test`
4. `make test-gui`
5. Проверить PR #43 и после merge начать gate-прогон на YAN9 от актуального
   `main`.

## Open

- Ownership/классификация источника всё ещё грубая: trusted prior держит
  геометрию известных пар, но не доказывает, что каждый snare-owned marker
  действительно снейр.
- Реальный кит один; пороги не подгонять под него.
- `beat_doc` пока не сериализуется.
- Матрицу профиля и просачивания всё ещё негде смотреть.
- Strength glide общий на все каналы; per-channel strength для комнаты/оверхедов
  ещё нет.
- Full-file glide export после 20-секундного Detect держит первую/последнюю
  цель за пределами окна Detect.
- Мониторинг идёт только в выходы 1-2: кит на восемь выходов карты не разводится.
- Developer ID и нотаризация зависят от локальных сертификатов.

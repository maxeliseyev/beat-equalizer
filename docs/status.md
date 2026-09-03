# Status

Updated: 2026-09-03
Stage: 2 (редактор в standalone)
Plan step: этап 2, шаг 6 — source-centric gate перед нарезкой
Branch: `feat/source-centric-ui`
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

## Now

- `SourceDiagnostic` вынесен в `beat_doc`: source-owned raw/MAD, residual к
  калибровке, full-align offset, natural offset, close/late каналы.
- `DetectWorker` публикует source-centric сводку, `BeatEqualizerAudioProcessor`
  форматирует компактный status `src / close / late / bleed`.
- Standalone показывает source status под Analyze/Detect; `d/hit ms` после
  свежего Detect показывает source-owned задержки, а не общую смесь событий
  канала.
- Basic/Advanced UI зафиксирован ADR `decisions/basic-advanced-ui-modes.md`.
- Проверено: `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 make test`,
  `make test-gui`, hidden YAN9 `[.real-kit-export]` и `[.real-kit-source]`.

## Next

Открыть PR. После merge сделать реальный Basic/Advanced toggle и разложить
существующие контролы по visible-блокам.

## Resume

1. `git fetch && git checkout feat/source-centric-ui && git pull`
2. `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 make test && make test-gui`
3. `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 build/release/tests/beat_gui_tests "[.real-kit-export]" -s`
4. `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 build/release/tests/beat_tests "[.real-kit-source]" -s`
5. Читать `docs/sessions/2026-09-03-source-centric-ui.md`; открыть PR или
   продолжить с Basic/Advanced toggle после merge.

## Open

- Compact source status есть, но полной таблицы source diagnostics в UI ещё нет.
- Ownership/классификация источника всё ещё грубая: trusted prior держит
  геометрию известных пар, но не доказывает, что каждый snare-owned marker
  действительно снейр.
- Basic/Advanced пока ADR, не переключатель в UI.
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

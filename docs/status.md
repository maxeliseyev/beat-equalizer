# Status

Updated: 2026-09-03
Stage: 2 (редактор в standalone)
Plan step: этап 2, шаг 6 — Basic/Advanced foundation перед source-centric gate UI
Branch: `feat/basic-advanced-ui`
PR: #40 ready — https://github.com/maxeliseyev/beat-equalizer/pull/40
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

## Now

- Basic/Advanced rollout фиксируется в продуктовых документах как серия PR:
  каркас режима, таблица каналов, Basic outcome/status, Advanced diagnostics v2.
- Первый PR этой серии реализован на ветке: добавлен `global.uiMode`, header
  toggle и видимость существующих top-level advanced-блоков.
- Basic выбран по умолчанию и скрывает `max distance`/`freeze`, `glide strength`,
  source status, correlometer и grid/time/tempo controls. Advanced возвращает
  текущий инженерный экран.
- DSP, glide, source matching и export не менялись.
- Версия поднята до `0.22.0`; changelog обновлён.
- Проверено: `make test`, `make test-gui`, `git diff --check`.
- PR #40 переведён из draft в ready for review.

## Next

Смёржить PR #40; после merge продолжить PR 41 — mode-aware таблица каналов.

## Resume

1. `git fetch && git checkout feat/basic-advanced-ui && git pull`
2. Читать `docs/sessions/2026-09-03-basic-advanced-ui.md`
3. `make test`
4. `make test-gui`
5. Смёржить PR #40 и после merge начать PR 41 от актуального `main`.

## Open

- Compact source status есть, но полной таблицы source diagnostics в UI ещё нет.
- Ownership/классификация источника всё ещё грубая: trusted prior держит
  геометрию известных пар, но не доказывает, что каждый snare-owned marker
  действительно снейр.
- Basic/Advanced имеет первый пользовательский переключатель, но таблица
  каналов пока остаётся Advanced-таблицей даже в Basic.
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

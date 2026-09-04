# Status

Updated: 2026-09-04
Stage: 2 (редактор в standalone)
Plan step: этап 2, gate после шага 6 — YAN9 Advanced diagnostics
Branch: `test/yan9-advanced-gate`
PR: #44 draft — https://github.com/maxeliseyev/beat-equalizer/pull/44
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
- PR #43 смёржен в `main` squash-коммитом `1ae968c` 2026-09-04:
  Advanced показывает source-centric diagnostics table и role metadata в
  таблице каналов; Basic остаётся компактным.

## Now

- Gate-прогон YAN9 после PR #43 выполнен на ветке `test/yan9-advanced-gate`.
- `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 make test` зелёный.
- Hidden source стенд `[.real-kit-source]`: 124 hits, 91 snare-owned, 608 obs,
  7 calibrated delays; snare bottom 0.281 мс, room 14.667 мс с return.
- Hidden export стенд `[.real-kit-export]`: static 79 % → 92 % sum coherence;
  glide 50 % и 100 % остаётся 81 % → 81 % event coherence, 13/30 limited.
- Gate-вывод: glide сам по себе недостаточен; после merge этого PR идти к
  этапу 2, шагу 7 — точки реза, кроссфейды, WSOLA на затухании.

## Next

После merge PR #44 начать этап 2, шаг 7: точки реза, кроссфейды и WSOLA на
затухании.

## Resume

1. `git fetch && git checkout test/yan9-advanced-gate && git pull`
2. Читать `docs/sessions/2026-09-04-yan9-advanced-gate.md`
3. Проверить `docs/real-kit-protocol.md`
4. `git diff --check`
5. Проверить PR #44 и после merge начать шаг 7 от актуального `main`.

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

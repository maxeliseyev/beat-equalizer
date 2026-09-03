# Status

Updated: 2026-09-03
Stage: 2 (редактор в standalone)
Plan step: этап 2, шаг 6 — real-kit прогон `GlideRenderer` в standalone export
Branch: `test/real-kit-glide-export`
PR: #36 — https://github.com/maxeliseyev/beat-equalizer/pull/36
Blockers: none

## Done

- Этап 1 закрыт: PR 1–8 плана, последний — прогон реального кита через стенд.
- Этап 2, шаги 1–5 в `main`: `beat_doc`, детектор, сверка по микрофонам,
  калибровка сессии, `beat_doc` в плагине.
- PR #33 смёржен в `main` squash-коммитом `70dfb6a` 2026-09-03.
- PR #34 смёржен в `main` squash-коммитом `04124f2` 2026-09-03.
- PR #35 смёржен в `main` squash-коммитом `37aac89` 2026-09-03.
- На `main`: `Export static...`, `Export glide...`, `Glide preview @ N%`,
  `global.glideStrength`, `VERSION` 0.20.0.

## Now

- `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 make test` зелёный:
  real-kit пакет прошёл за 24.39 с.
- `make test-gui` зелёный: 366 assertions в 61 test case.
- Скрытый export smoke `[.real-kit-export]` прогнал 10…30 с YAN9 через
  Load → Analyze → Detect → static/glide export: 33 assertions.
- Числа: Analyze 79 % → 92 %, Detect 59 hits / 218 obs / 4 delays,
  Glide 50 % 83 % → 83 % (29 limited), Glide 100 % 83 % → 82 % (47 limited).
- Временные WAV для прослушивания лежат в системном tmp:
  `/var/folders/bz/n5yr1q2d45n7126lht5djp2c0000gn/T/beat_gui_tests/beat-equalizer-real-kit/yan9-static.wav`,
  `yan9-glide-50.wav`, `yan9-glide-100.wav`.

## Next

Прослушать `yan9-static.wav`, `yan9-glide-50.wav`, `yan9-glide-100.wav`.
Если числа подтвердятся на слух, до шага 7 разбирать per-channel strength и
карту событий, а не переходить сразу к нарезке.

## Resume

1. `git fetch && git checkout test/real-kit-glide-export && git pull`
2. `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 make test && make test-gui`
3. `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 build/release/tests/beat_gui_tests "[.real-kit-export]" -s`
4. Читать `docs/sessions/2026-09-03-glide-renderer.md`; продолжить с ручного
   прослушивания временных export WAV.

## Open

- Реальный кит один; пороги не подгонять под него.
- `beat_doc` пока не сериализуется.
- Матрицу профиля и просачивания всё ещё негде смотреть.
- Strength glide общий на все каналы; per-channel strength для комнаты/оверхедов
  ещё нет.
- Агент не может оценить слуховые артефакты; нужен ручной A/B.
- Full-file glide export после 20-секундного Detect держит первую/последнюю
  цель за пределами окна Detect.
- Мониторинг идёт только в выходы 1-2: кит на восемь выходов карты не разводится.
- Возврат задержки комнаты описан моделью, но рендером не сделан.
- Developer ID и нотаризация зависят от локальных сертификатов.

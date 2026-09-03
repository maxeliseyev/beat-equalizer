# Status

Updated: 2026-09-03
Stage: 2 (редактор в standalone)
Plan step: этап 2, шаг 6 — source-centric gate перед нарезкой
Branch: `feat/source-centric-diagnostics`
PR: none
Blockers: none

## Done

- Этап 1 закрыт: статическое выравнивание слышно на реальном ките.
- Этап 2, шаги 1–5 в `main`: `beat_doc`, детектор, сверка по микрофонам,
  калибровка сессии, события/маркеры в standalone.
- PR #35 смёржен в `main` squash-коммитом `37aac89` 2026-09-03.
- PR #36 смёржен в `main` squash-коммитом `141c24b` 2026-09-03:
  real-kit smoke показал static 79 % → 92 %, glide 50 % 83 % → 83 %,
  glide 100 % 83 % → 82 %.

## Now

- На ветке добавлен скрытый real-kit diagnostic `[.real-kit-source]` для YAN9:
  snare top как источник, snare bottom как close-пара, остальные каналы как
  bleed-наблюдения.
- Канон этапа 2 уточнён: gate перед нарезкой source-centric, а не средняя
  когерентность всей суммы.
- Диагностика YAN9 10…50 с: 124 hits, 87 snare-owned, 537 obs,
  7 calibrated delays.
- Текущая сверка груба для close-пары: snare bottom 0.776 мс вместо
  протокольных 0.281 мс, MAD 0.540 мс.
- Room: 16 obs, raw 15.608 мс; full align даёт offset 0.000 мс,
  `return(room)=1` сохраняет 15.608 мс.

## Next

Улучшить source-centric сверку снейра: close-pair residual должен вернуться к
протоколу до решений о нарезке, кроссфейдах или WSOLA.

## Resume

1. `git fetch && git checkout feat/source-centric-diagnostics && git pull`
2. `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 make test`
3. `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 build/release/tests/beat_tests "[.real-kit-source]" -s`
4. Читать `docs/real-kit-protocol.md` и
   `docs/sessions/2026-09-03-glide-renderer.md`; продолжить с matcher-а.

## Open

- Source-centric diagnostic пока скрытый тест, а не видимая таблица в UI.
- Реальный кит один; пороги не подгонять под него.
- `beat_doc` пока не сериализуется.
- Матрицу профиля и просачивания всё ещё негде смотреть.
- Strength glide общий на все каналы; per-channel strength для комнаты/оверхедов
  ещё нет.
- Full-file glide export после 20-секундного Detect держит первую/последнюю
  цель за пределами окна Detect.
- Мониторинг идёт только в выходы 1-2: кит на восемь выходов карты не разводится.
- Developer ID и нотаризация зависят от локальных сертификатов.

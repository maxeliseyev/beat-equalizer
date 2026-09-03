# Status

Updated: 2026-09-03
Stage: 2 (редактор в standalone)
Plan step: этап 2, шаг 6 — source-centric gate перед нарезкой
Branch: `fix/source-centric-snare-match`
PR: #38 — https://github.com/maxeliseyev/beat-equalizer/pull/38
Blockers: none

## Done

- Этап 1 закрыт: статическое выравнивание слышно на реальном ките.
- Этап 2, шаги 1–5 в `main`: `beat_doc`, детектор, сверка по микрофонам,
  калибровка сессии, события/маркеры в standalone.
- PR #36 смёржен в `main` squash-коммитом `141c24b` 2026-09-03.
- PR #37 смёржен в `main` squash-коммитом `0b4658b` 2026-09-03:
  source-centric diagnostic показал, что aggregate coherence не годится как
  gate перед нарезкой.

## Now

- `MatchContext` различает rough prior и trusted prior из `SessionProfile`.
- `CrossMicMatcher` для trusted prior держится за калиброванную геометрию:
  coarse search узкий, GCC residual принимается только внутри
  `kMatchTrustedPriorSlackMs` или трёх разбросов калибровки.
- YAN9 `[.real-kit-source]` после правки: 124 hits, 91 snare-owned,
  608 obs, 7 calibrated delays.
- Snare bottom вернулся к протоколу: 0.281 мс, MAD 0.000 мс,
  residual 0.000 мс.
- Room вернулся к протоколу: 14.667 мс, MAD 0.000 мс;
  `return(room)=1` сохраняет 14.667 мс.
- Проверено: `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 make test`
  зелёный, hidden `[.real-kit-source]` зелёный, `make test-gui` зелёный.

## Next

Сделать source-centric diagnostics видимыми в standalone UI: инженер должен
видеть close-pair, bleed-наблюдения и room/OH return, а не только среднюю
строку Detect.

## Resume

1. `git fetch && git checkout fix/source-centric-snare-match && git pull`
2. `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 make test`
3. `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 build/release/tests/beat_tests "[.real-kit-source]" -s`
4. Читать `docs/sessions/2026-09-03-source-centric-match.md`; продолжить с
   отображения source-centric diagnostic в standalone.

## Open

- Source-centric diagnostic пока скрытый тест, а не видимая таблица в UI.
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

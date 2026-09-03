# Status

Updated: 2026-09-03
Stage: 2 (редактор в standalone)
Plan step: этап 2, шаг 6 — `GlideRenderer` в standalone export
Branch: `feat/glide-export`
PR: https://github.com/maxeliseyev/beat-equalizer/pull/35
Blockers: none

## Done

- Этап 1 закрыт: PR 1–8 плана, последний — прогон реального кита через стенд.
- Этап 2, шаги 1–5 в `main`: `beat_doc`, детектор, сверка по микрофонам,
  калибровка сессии, `beat_doc` в плагине.
- PR #33 смёржен в `main` squash-коммитом `70dfb6a` 2026-09-03.
- PR #34 смёржен в `main` squash-коммитом `04124f2` 2026-09-03.
- На `main`: `GlideRenderer` в `beat_dsp`, explicit delay API в
  `FractionalDelay`, тесты renderer-а, `VERSION` 0.19.0.

## Now

- `Export static...` оставлен на старом статическом пути.
- `Export glide...` требует свежий Detect и пишет WAV через `GlideRenderer`;
  после экспорта статус показывает hits, event coherence до/после и limited.
- После свежего Detect Standalone показывает `Glide preview @ N%` до записи WAV.
- `Glide Strength` сохраняется как `global.glideStrength` и управляет preview и
  export.
- Ветка запушена, открыт draft PR #35.
- `make test` зелёный; `make test-gui` зелёный. Без `BEAT_REAL_KIT_DIR`
  реальные тесты пропущены.

## Next

Прогнать PR #35 на реальном ките в Standalone: сравнить `Export static...`,
`Glide Strength` 50 % и 100 %, записать числа и слуховые заметки в протокол.

## Resume

1. `git fetch && git checkout feat/glide-export && git pull`
2. `make test && make test-gui`
3. Читать `docs/sessions/2026-09-03-glide-renderer.md`; продолжить с
   real-kit проверки PR #35.

## Open

- Реальный кит один; пороги не подгонять под него.
- `beat_doc` пока не сериализуется.
- Матрицу профиля и просачивания всё ещё негде смотреть.
- Strength glide общий на все каналы; per-channel strength для комнаты/оверхедов
  ещё нет.
- Мониторинг идёт только в выходы 1-2: кит на восемь выходов карты не разводится.
- Возврат задержки комнаты описан моделью, но рендером не сделан.
- Developer ID и нотаризация зависят от локальных сертификатов.

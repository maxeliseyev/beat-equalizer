# Status

Updated: 2026-09-03
Stage: 2 (редактор в standalone)
Plan step: этап 2, шаг 6 — `GlideRenderer`
Branch: `feat/glide-renderer`
PR: none
Blockers: none

## Done

- Этап 1 закрыт: PR 1–8 плана, последний — прогон реального кита через стенд.
- Этап 2, шаги 1–5 в `main`: `beat_doc`, детектор, сверка по микрофонам,
  калибровка сессии, `beat_doc` в плагине.
- PR #33 смёржен в `main` squash-коммитом `70dfb6a` 2026-09-03.
- На этой ветке: `GlideRenderer` в `beat_dsp`, explicit delay API в
  `FractionalDelay`, тесты renderer-а, `VERSION` 0.19.0.

## Now

- `make test` зелёный; `make test-gui` зелёный. Без `BEAT_REAL_KIT_DIR`
  реальные тесты пропущены.
- `Export aligned` пока не переключён на per-hit renderer: это отдельный
  пользовательский режим/статус, а не скрытая смена статического экспорта.
- Локально перенесена однострочная правка warning в
  `src/plugin/OverviewStrip.cpp`: расчёт мышиной позиции больше не делает
  неявный `int -> float`.

## Next

Подключить `GlideRenderer` к standalone-экспорту явным режимом/статусом и
показать event coherence пользователю.

## Resume

1. `git fetch && git checkout feat/glide-renderer && git pull`
2. `make test && make test-gui`
3. Читать `docs/sessions/2026-09-03-glide-renderer.md`; продолжить с
   подключения renderer-а к standalone-экспорту.

## Open

- Реальный кит один; пороги не подгонять под него.
- `beat_doc` пока не сериализуется.
- Матрицу профиля и просачивания всё ещё негде смотреть.
- Нужно решить, где в UI живёт strength glide и как назвать режим экспорта.
- Мониторинг идёт только в выходы 1-2: кит на восемь выходов карты не разводится.
- Возврат задержки комнаты описан моделью, но рендером не сделан.
- Developer ID и нотаризация зависят от локальных сертификатов.

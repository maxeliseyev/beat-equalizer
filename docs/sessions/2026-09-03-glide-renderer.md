# 2026-09-03 — `GlideRenderer`

## Context

PR #33 смёржен, этап 2 дошёл до шага 6: по-ударный рендер без нарезки.
`timing-design-recommendations.md` задаёт первый механизм — glide на
существующей задержке с лимитом скорости и метрикой когерентности на событие.

## What

Добавлен `beat::GlideRenderer` в `src/dsp`: события несут целевую applied delay
по каналам, renderer держит задержку постоянной в защищённой атаке и меняет её
между событиями не быстрее `kGlideMaxSlew`.

`FractionalDelay` получил `processSampleAtDelay`, чтобы offline renderer мог
задавать задержку на каждый сэмпл без realtime smoothing.

Тесты закрывают `strength = 0` как бит-в-бит копию входа, ограничение скорости
glide, достижение цели на длинном затухании и знак задержки в event coherence.

## Why

`GlideRenderer` оставлен в `beat_dsp`, а не в plugin: это алгоритм рендера, и
его надо проверять синтетикой до того, как его дёргает `PluginProcessor`.
Зависимость на `beat_doc` не добавлялась: plugin позже соберёт простые события
renderer-а из `Document` и `DelayField`.

`Export aligned` не переключён на per-hit renderer в этом изменении. Сейчас он
имеет понятный статический контракт: применяет ручные delay/polarity/rotator и
не зависит от последнего Detect. Молча менять экспорт после появления событий
значит смешать новый звук, отсутствие strength UI и старое имя кнопки в одном
PR.

## Status

status.md обновлён: да. Ветка: `feat/glide-renderer`.

## Next

Подключить `GlideRenderer` к standalone-экспорту явным режимом/статусом и
показать event coherence пользователю.

## Open

- Нужно решить, где в UI живёт strength glide и как назвать режим экспорта.
- Пересчёт событий остаётся ручным: Detect не запускается автоматически перед
  per-hit render.

---

## Export bridge

## Context

PR #34 смёржен. Следующий кусок — сделать renderer доступным в Standalone, не
меняя старый статический экспорт молча.

## What

Добавлена отдельная кнопка `Export glide...` рядом с `Export static...`.
`BeatEqualizerAudioProcessor::exportGlide` собирает события renderer-а из
свежего `DetectWorker::Result`: `Document::events()` даёт время и защищённую
зону, `DelayField::applied()` — целевую задержку канала. Каналы без задержки
события остаются на базовой статической задержке.

После `GlideRenderer` применяется та же часть статического рендера, что и
раньше: polarity и rotator. Monitor mix, Solo/Mute, Level и Pan в export не
попадают.

## Why

Старый `Export aligned...` переименован в `Export static...`, а не заменён:
пользователь должен явно выбрать per-hit renderer. Без этого одна и та же
кнопка давала бы разный звук в зависимости от того, нажимали Detect или нет.

Strength пока не добавлен в APVTS: это отдельный UX-выбор, который нельзя
прятать в неочевидном параметре. Текущий per-hit export работает на 100 %, а
статический путь остаётся быстрым сравнением.

## Status

status.md обновлён: да. Ветка: `feat/glide-export`.

## Next

Показать метрику glide до экспорта и добавить strength UI.

## Open

- Strength glide всё ещё не настраивается.
- Detect остаётся ручным; export не запускает анализ автоматически.

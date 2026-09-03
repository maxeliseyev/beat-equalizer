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

---

## Strength and preview

## Context

PR #35 уже открыл отдельный `Export glide...`, но strength оставался
захардкоженным на 100 %, а event coherence появлялась только после записи WAV.

## What

Добавлен APVTS-параметр `global.glideStrength` и slider `Glide` в Standalone.
Он управляет `GlideRenderer::Options::strength` для preview и для export.

Preview считает тот же renderer, но только по окну последнего Detect, а не по
всему файлу. Статус `Glide preview @ N%: ...` появляется сразу после свежего
Detect и обновляется после изменения strength. `Export glide...` использует тот
же расчёт, но рендерит весь материал и пишет WAV.

## Why

Preview не считается в таймере редактора: на длинном ките это превратило бы
линейный offline render в 25 пересчётов в секунду. Окно Detect ограничено
`kDetectSeconds`, поэтому метрика появляется быстро и относится к тем же
событиям, которые пользователь видит маркерами.

Strength хранится в APVTS, а не локально в editor-е, чтобы значение переживало
сохранение проекта и было доступно для automation/host state без отдельной
миграции позже.

## Status

status.md обновлён: да. Ветка: `feat/glide-export`. PR #35.

## Next

Прогнать PR на реальном ките в Standalone: сравнить `Export static...`,
`Glide Strength` 50 % и 100 %, записать числа и слуховые заметки в протокол.

## Open

- Strength общий на все каналы; комнату пока нельзя оставить на меньшем
  strength отдельно.
- Preview-метрика описывает delay glide; polarity/rotator применяются после
  renderer-а, как в export path.

---

## YAN9 real-kit smoke

## Context

PR #35 смёржен в `main`. Нужно проверить новый Standalone export path на
реальном ките YAN9 из `/Users/maximeliseyev/Sound/YAN9`.

## What

Добавлен скрытый GUI-тест `[.real-kit-export]`: он берёт 20 секунд YAN9
(10…30 с), пишет временный 8-канальный WAV в системный `/tmp`, запускает
workflow Load → Analyze → Detect → static/glide export и печатает статусы.
Обычный `ctest` этот тест не запускает.

Результат: `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 make test`
зелёный, около 24 с. Export smoke тоже зелёный. На 10…30 с: Analyze 79 % → 92 %,
Detect 59 hits / 218 obs / 4 delays, Glide 50 % 83 % → 83 % с 29 limited,
Glide 100 % 83 % → 82 % с 47 limited.

## Why

Текущий Detect в Standalone намеренно ограничен `kDetectSeconds` = 20 с.
Если грузить полный четырёхминутный дубль и экспортировать его целиком после
одного Detect, поле событий будет частичным: до первого события renderer
держит первую цель, после последнего — последнюю. Для проверки PR #35 нужен
именно export path, а не спор о full-document детекции, поэтому прогон сделан
на 20-секундном временном WAV той же длины, что окно Detect.

## Status

status.md обновлён: да. Ветка: `test/real-kit-glide-export`.

## Next

Прослушать временные `yan9-static.wav`, `yan9-glide-50.wav` и
`yan9-glide-100.wav`; если числа подтвердятся на слух, разбирать per-channel
strength/карту событий до шага 7.

## Open

- Агент не оценивает звук на слух; временные WAV оставлены для ручного
  прослушивания.
- Full-file glide export после 20-секундного Detect остаётся продуктовым
  ограничением, не дефектом теста.

---

## Source-centric gate

## Context

PR #36 смёржен. После обсуждения стало ясно, что сравнение `static` vs
`glide` по общей когерентности слишком грубое: оно в первую очередь хвалит
совмещение room с close-микрофонами, хотя для будущей квантизации room/OH
надо уметь вернуть к естественной задержке.

## What

Добавлен скрытый real-kit тест `[.real-kit-source]`. Он строит snare-source
срез YAN9: snare top как опора, snare bottom как close-пара, остальные каналы
как наблюдения того же физического удара. Тест печатает raw TDOA, разброс,
residual к протоколу, offset после полного выравнивания и offset при
`return(room)=1`.

Результат PR #37 до правки matcher-а: 124 hits, 87 snare-owned, 537 obs,
7 calibrated delays. Snare bottom давал 0.776 мс вместо протокольных 0.281 мс,
MAD 0.540 мс. Room был виден в 16 событиях: full align сводил его в 0.000 мс,
а `return(room)=1` сохранял 15.608 мс. Следующая правка описана в
`2026-09-03-source-centric-match.md`.

## Why

Этот PR не чинит matcher и не начинает нарезку. Его задача — поставить
правильный gate перед следующей машинерией: сначала доказать, что один и тот
же источник стабильно находится в close-паре и в bleed-наблюдениях, и только
потом выбирать между glide, кроссфейдами и WSOLA.

## Status

status.md обновлён: да. Ветка: `feat/source-centric-diagnostics`.

## Next

Улучшить source-centric сверку снейра: close-pair residual должен вернуться к
протоколу до решений о нарезке.

## Open

- Текущий matcher для snare bottom на YAN9 всё ещё грубее протокола примерно
  на полмиллисекунды.
- Source-centric diagnostic пока скрытый тест, а не видимая таблица в UI.

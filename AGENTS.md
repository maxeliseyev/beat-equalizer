# AGENTS.md — beat-equalizer

Контракт для людей и агентов в этом репозитории. Детали — в `DOCUMENTATION/`;
сюда попадают только инварианты, которые нельзя «улучшить» по ходу задачи.

**Первый файл любой сессии — `DOCUMENTATION/STATUS.md`.** Чат предыдущего
агента может отсутствовать. Если STATUS расходится с git — чини STATUS до кода.

| Зона | Читать сначала |
|---|---|
| Где остановились, ветка, следующий шаг | `DOCUMENTATION/STATUS.md` |
| Протокол смены, что куда писать | `DOCUMENTATION/handoff.md` |
| Продукт, этапы 1–4, алгоритмы | `DOCUMENTATION/drum-editor-plan.md` |
| MVP (этап 1), порядок PR, DSP/UI | `DOCUMENTATION/plan.md` |
| Сборка, форматы, лицензия JUCE | `README.md` |
| Ветки и PR | `DOCUMENTATION/git-workflow.md` |

Не копируй эти документы в чат и не дублируй их в код-комментариях.

---

## Что это

Плагин выравнивания многомикрофонных ударных. C++20, JUCE 8, CMake.
Рабочее имя **Beat Equalizer**. Коды: manufacturer `Bteq`, plugin `Algn`.
Пространство имён DSP: `beat`.

Сейчас делается **этап 1 — статическое выравнивание**: один инстанс,
N-in / N-out (2…24), одна задержка и полярность на канал, без ARA, без нарезки.

Целевой хост MVP: **Reaper** + **Standalone**. Logic и Ableton не блокеры.
Windows — после стабильного macOS VST3, не в том же PR.

Не начинай этап 2 (ARA, онсеты, по-ударные задержки, нарезка), пока этап 1
не слышен на реальном ките и не закрыт по `DOCUMENTATION/plan.md` PR 8.

---

## Сборка

Канон — `make` (обёртка над CMake presets). Toolchain: CMake ≥ 3.22, Ninja,
C++20. JUCE 8.0.15 и Catch2 — FetchContent, в git их нет.

```bash
make              # Release: тесты + форматы этой ОС
make debug
make test
make vst3|au|standalone
make run
```

macOS: VST3+AU+Standalone, копия в `~/Library/Audio/Plug-Ins/`.
Linux/Windows (когда появится): VST3+Standalone, без AU.

Перед тем как считать задачу сделанной: `make test` зелёный. GUI — Standalone
или Reaper (`DOCUMENTATION/reaper-testing.md`), не только компиляция.

---

## Дерево

```
src/dsp/        ядро: TDOA, delay, rotator, coherence. Без UI, без AudioProcessor.
src/plugin/     JUCE: шины, APVTS, processBlock, editor, worker, ring buffer.
tests/          Catch2, линк на beat_dsp. DSP-тесты не поднимают GUI.
DOCUMENTATION/  планы, решения, заметки сессий.
```

Новый алгоритм живёт в `src/dsp` и покрывается тестом **до** того, как его
дергает `PluginProcessor`. Обратный порядок запрещён.

Плагин линкует `beat_dsp`. `beat_dsp` не линкует `juce_gui_*`.
JUCE в DSP допустим только как FFT (`juce::dsp::FFT`). Нужна скорость — **pffft**.
**FFTW не брать** (GPL или платная лицензия).

---

## Инварианты

Нарушить любой пункт = регрессия, даже если «так проще».

1. **`processBlock` не считает FFT, GCC, медиану, не аллоцирует, не лочится.**
   Только чтение снимка состояния и DSP: invert, fractional delay, allpass.
   Анализ — worker thread, публикация lock-free snapshot.
2. **Канал в realtime можно только задержать, не сдвинуть раньше.**
   Сырые TDOA `d[i]` относительно опоры → `applied[i] = max(d) - d[i]` (≥ 0) →
   `latency = ceil(max(d)) + groupDelay интерполятора`. Dry/A-B идёт через ту же
   latency. Без PDC кит уедет относительно остальной сессии.
3. **Один входной и один выходной bus, `in == out`, 2…24 канала.**
   Не плодить именованные моно-шины «Kick In / Snare Top» в MVP.
   Плагин не микширует кит в стерео в основном режиме: N-out passthrough.
   Mono Sum — только мониторинг, не ломает разводку стемов.
4. **Пороги и окна — в метрах, миллисекундах и герцах, не в сэмплах.**
   `τmax = distance_m / 343`. Дефолт 4 м ≈ 12 мс. Sample rate 44.1/48/96
   не должен менять задержку в миллисекундах.
5. **GCC-PHAT ищет пик только в `[-τmax, +τmax]`.** Полоса анализа ~100 Hz…8 kHz.
   Иначе коррелятор залипает на периоде бочки (~20–40 мс). Тест с ложным
   периодом 20 мс при τmax = 12 мс обязан не выбирать его.
6. **Полярность — знак невзвешенной корреляции в точке лага, не знак PHAT-пика.**
7. **ID параметров APVTS стабильны.** Слотов всегда 24, даже если хост открыл 8
   каналов. Схема: `global.*`, `ch01.enabled`, `ch01.delayMs`, `ch01.polarity`,
   `ch01.rotatorAmount`, `ch01.rotatorHz`. Не переименовывать, не сдвигать индексы.
8. **Роли каналов (close / OH / room / hats) в MVP — метаданные**, не ветки алгоритма.
9. **Не включать ARA, Celemony SDK, общий документ между инстансами** до этапа 2.
10. **Не выключать splash JUCE** (`JUCE_DISPLAY_SPLASH_SCREEN=0`) без коммерческой
    лицензии. Сейчас GPL-режим. Закрытый бинарь без лицензии JUCE не шипить.

---

## C++ / JUCE

- C++20, без GNU-расширений (`CMAKE_CXX_EXTENSIONS OFF`).
- `#pragma once`. Namespace `beat` в DSP. В plugin-слое JUCE-классы — как в
  скелете (`BeatEqualizerAudioProcessor`), не `using namespace juce` в заголовках.
- Стиль: Allman, 4 пробела, лимит 100. Формат: `clang-format` (`.clang-format`).
- Не тенить `AudioProcessorEditor::processor` — поле editor называть `audioProcessor`.
- Комментарий только на неочевидное: почему лаг режется по метрам, почему dry
  с той же latency. Не комментировать «increment i».
- Не добавлять `juce_generate_juce_header`. Инклюды модулей напрямую.
- Не вендорить JUCE/Catch2 в дерево. Пин тега — `cmake/Dependencies.cmake`.

---

## Тесты

Цель — тест, который может провалиться на известном дефекте, не coverage.

DSP (обязательны для PR с алгоритмом):

- известная задержка 0 / 0.25 / 1 / 5.5 / 11.3 сэмпла @ 48 и 96 kHz, ошибка < 0.1;
- инверсия канала;
- ложный период 20 мс при τmax = 12 мс;
- `applied >= 0`, latency-модель;
- dry/wet совпадают по времени.

Не тащить в `tests/` реальные сессии на гигабайты. Фикстуры — синтетика;
реальный кит гоняется руками в Standalone (PR 8) и протоколируется в
`DOCUMENTATION/`.

---

## Как работаем

Полный флоу: `DOCUMENTATION/git-workflow.md`. ADR: `decisions/git-trunk-until-v1.md`.

До первой версии в проде **нет ветки `develop`**. Интеграция — только `main`.
Вся работа, включая автора репо, идёт ветка → PR → squash в `main`.
Прямой пуш в `main` запрещён.

Порядок MVP — `DOCUMENTATION/plan.md`, секция «Порядок реализации». Один PR —
один шаг. Не смешивать GCC-PHAT с таблицей UI «чтобы сразу было видно».

Ветки от актуального `main`: `feat/…`, `fix/…`, `chore/…`, `docs/…`, `test/…`.
Коммиты — [Conventional Commits](https://www.conventionalcommits.org/):
`feat(dsp): …`, `fix(plugin): …`, `test(dsp): …`, `docs: …`.

`develop` заводить только после тега первой изданной версии (`v1.0.0`).
До этого не создавать её «на будущее» и не целить PR в неё.

Незаконченная работа живёт на запушенной feature-ветке, имя которой стоит
в `STATUS.md`. Не на `main`, не только в чате, не только в local working tree.

Definition of done:

1. Инварианты выше не нарушены.
2. Сборка debug (+ тесты) зелёная.
3. Если менялся DSP — есть/обновлён синтетический тест.
4. Если менялся GUI — проверен Standalone.
5. `STATUS.md` соответствует git (ветка, Now, Next, Resume).
6. Если решение нетривиальное — session note с **Why**; если ограничит будущее — ADR.

Не коммить `build/`, `_deps/`, `.DS_Store`. Не bump-ай JUCE «заодно».

---

## Смена сессии

Шаблоны: `DOCUMENTATION/handoff.md`. Нарушить пункт ниже = следующий агент
не может продолжить без этого чата.

**Старт** (пока не сделано — не писать фичу):

1. `git status -sb`, ветка, последние коммиты, открытые PR.
2. `DOCUMENTATION/STATUS.md` целиком.
3. Session note, на которую ссылается STATUS, если ссылается.

**Конец** (и когда «ещё не готово»):

1. Код на feature-ветке; собирающееся закоммитить; ветку **push**.
2. Переписать `STATUS.md`: Branch, Now, Next, Resume обязательны.
3. Session note, если было Why / тупик / нетривиальный баг.
4. Канон (`plan.md`, `drum-editor-plan.md`, этот файл, ADR) — в том же
   изменении, что и поведение, не отдельным «потом в доках».

`STATUS.md` перезаписывать, не дописывать летопись. Летопись = git + `sessions/`.

---

## Документация

Один дом на факт — таблица в `handoff.md`. Кратко:

- курсор смены → `STATUS.md`;
- почему в этой сессии → `sessions/`;
- повторно всплывёт → `decisions/`;
- как делать этап 1 → `plan.md`;
- что за продукт → `drum-editor-plan.md`;
- нельзя нарушить → этот файл.

Канон продукта не переписывать сюда. Меняешь поведение этапа 1 — правь
`plan.md` в том же изменении. Меняешь рамку продукта (ARA, нарезка, этап 2+) —
`drum-editor-plan.md`.

Черновики агента, дампы чата и «как я думал» в `src/` не класть.

# 2026-09-03 — Source-centric UI

## Context

PR #38 смёржен: matcher перестал ломать калиброванные close-пары. Следующий
шаг из `status.md` — показать source-centric diagnostic в Standalone, чтобы
инженер видел не только aggregate coherence и общую `d/hit` строку.

Параллельно зафиксировано продуктовое решение: интерфейс нужен в двух режимах,
Basic и Advanced. Текущий экран уже выглядит как Advanced, и дальнейшие
диагностические числа нельзя делать обязательной частью первого экрана.

## What

- `beat_doc` получил `SourceDiagnostic`: статистика по событиям, которыми
  владеет выбранный источник, с raw/MAD, residual к калибровке, full-align
  offset и natural offset.
- `DetectWorker` публикует эту сводку вместе с документом, профилем и match
  report.
- `BeatEqualizerAudioProcessor` форматирует компактную строку
  `src / close / late / bleed`.
- Standalone показывает эту строку под Analyze/Detect и после свежего Detect
  заполняет `d/hit ms` source-owned задержками.
- Добавлен ADR `decisions/basic-advanced-ui-modes.md`; `drum-editor-plan.md`
  ссылается на него как на канон интерфейсных режимов.

## Why

Source-centric summary живёт в `beat_doc`, а не в `PluginEditor`: будущий ARA
адаптер и Standalone должны получать одну и ту же диагностику поверх одного
документа. UI только форматирует и решает видимость.

В compact строке написано `late Ch`, а не `return Ch`: без ролей каналов
алгоритм не имеет права обещать, что самый поздний наблюдаемый канал — именно
room или OH. Возврат room/OH остаётся задачей Advanced UI с ролями каналов.

Basic/Advanced toggle не реализован в этом PR намеренно: сначала нужен один
самостоятельный advanced-блок source diagnostics, который потом можно скрыть
целиком. Полный режим Basic должен быть отдельным UI PR, иначе source
diagnostics смешается с перестройкой всего экрана.

## Status

status.md обновлён: да. Ветка: `feat/source-centric-ui`.
PR: #39 — https://github.com/maxeliseyev/beat-equalizer/pull/39.
Проверено: `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 make test`,
`make test-gui`, hidden YAN9 `[.real-kit-export]` и `[.real-kit-source]`.

## Next

Открыть PR; после merge сделать реальный Basic/Advanced toggle и разложить
существующие контролы по visible-блокам.

## Open

- Compact source status не заменяет полную таблицу source diagnostics.
- Ролей каналов в UI ещё нет, поэтому room/OH return не выделяется отдельно.
- Basic/Advanced пока ADR и архитектурная подготовка, не пользовательский
  переключатель.

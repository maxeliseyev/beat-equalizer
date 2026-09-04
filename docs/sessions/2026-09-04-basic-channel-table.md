# 2026-09-04 — Basic channel table

## Context

PR #40 смёржен: `global.uiMode` и top-level Basic/Advanced visibility уже в
`main`. Следующий пункт rollout — mode-aware таблица каналов.

## What

Текущая ветка делает PR 41:

- Basic-таблица скрывает ручные и инженерные колонки, чтобы первый экран не
  выглядел как стенд диагностики;
- Advanced-таблица сохраняет текущий набор колонок целиком;
- waveform получает ширину, которая освободилась после скрытия advanced
  колонок;
- DSP, Detect, Glide, export и APVTS channel parameters не меняются.

Реализовано:

- добавлен `ChannelTableMode`;
- `ChannelColumns::from()` и `ChannelRow::controlsWidth()` стали mode-aware;
- `ChannelRow` скрывает/показывает S/M, Level/Pan, Delay/Rot/Pol и
  Corr/Phase/d-hit по режиму;
- `PluginEditor` передаёт `global.uiMode` в строки и шапку таблицы;
- минимальная ширина editor-а пересчитывается при смене режима и появлении
  monitor-колонок;
- добавлены GUI-тесты на Basic/Advanced геометрию, видимость колонок и ширину
  waveform.

## Why

После PR #40 Basic уже прячет верхние advanced-блоки, но таблица всё ещё
показывает `Delay`, `Rot`, `Pol`, `Corr`, `Phase` и `d/hit ms`. Это ломает
продуктовую цель Basic: пользователь должен сначала довериться автоматике и
видеть материал/каналы, а не весь набор ручек.

Таблица вынесена в отдельный PR, потому что её геометрия общая для header,
строк и waveform. Любая ошибка здесь сразу ломает маркеры и скриншотные GUI
тесты, поэтому этот шаг должен быть изолирован от текстов статуса и
source-centric diagnostics v2.

## Status

status.md обновлён: да. Ветка: `feat/basic-channel-table`.
PR: #41 draft — https://github.com/maxeliseyev/beat-equalizer/pull/41.
Проверено: `make test`, `make test-gui`, `git diff --check`.

## Next

После merge PR #41 продолжить PR 42 — Basic outcome/status.

## Open

- В этом PR Basic скрывает колонки, но не вводит новый outcome/status.

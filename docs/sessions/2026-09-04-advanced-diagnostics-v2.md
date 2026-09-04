# 2026-09-04 — Advanced diagnostics v2

## Context

PR #42 смёржен: Basic больше не показывает инженерные detect/source/glide
строки, а Advanced сохранил старую диагностику. Следующий пункт Basic/Advanced
rollout из `docs/drum-editor-plan.md` — Advanced diagnostics v2.

## What

Текущая ветка делает PR #43:

- Advanced получает полную source-centric таблицу по каналам;
- таблица показывает роль канала, observations, natural/raw delay, spread,
  full-align offset, calibration residual и классификацию строки;
- роль канала становится видимой ручкой в Advanced channel table;
- compact source status начинает называть поздний room/OH канал как return,
  когда роль задана;
- Basic не получает новые controls/таблицы и остаётся коротким outcome UI.

Реализовано:

- добавлен `SourceDiagnosticTable` в plugin target;
- Advanced chrome показывает source-centric строки для всех активных каналов;
- таблица берёт live `chXX.role` и свежий Detect-result, stale Detect
  сбрасывается в прочерки;
- `ChannelRow` показывает role combo только в Advanced;
- compact source status в processor использует role metadata для `OH return`
  и `room return`;
- добавлены GUI-тесты на Basic/Advanced visibility, role column и room return.

## Why

После source-centric PR #39 инженер видел только компактную строку `src`,
`close`, `late` и счётчик bleed-каналов. Этого мало перед gate к нарезке:
нужно видеть, какие каналы реально наблюдали source-owned события, где
сохраняется естественная задержка комнаты/оверхедов, где full alignment
схлопывает приход к нулю и где residual расходится с калибровкой.

Роли в этом PR используются только как подписи и фильтр восприятия. Менять по
ним matcher опасно: по контракту этапа роли пока метаданные, а не ветки
алгоритма.

## Status

status.md обновлён: да. Ветка: `feat/advanced-diagnostics-v2`.
PR: none.
Проверено: `make test`, `make test-gui`, `git diff --check`.

## Next

Открыть draft PR #43 в `main`.

## Open

- Нужно решить, достаточно ли table-only диагностики или позже понадобится
  отдельная матрица профиля/просачивания.

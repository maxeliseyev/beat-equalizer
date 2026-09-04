# 2026-09-04 — Basic outcome status

## Context

PR #41 смёржен: Basic уже скрывает advanced-колонки таблицы и отдаёт ширину
waveform. Следующий пункт rollout — убрать из Basic инженерные статусные
строки и заменить их коротким итогом/следующим действием.

## What

Текущая ветка делает PR 42:

- Basic показывает один короткий пользовательский статус;
- Advanced сохраняет текущие подробные `analysisStatus`, `detectStatus`,
  source status и diagnostic chrome;
- Detect/Analyze/Glide/export logic не меняются;
- таблица каналов после PR #41 не меняется.

Реализовано:

- в Basic `analysisStatus` стал primary outcome/status;
- Basic скрывает detailed `detectStatus` и technical hint;
- Standalone Basic показывает `Load files to begin`, `Ready to Detect`,
  `Export ready: N hits found` или `Review in Advanced: no hits found`;
- host Basic показывает `Play the kit, then Analyze`, `Aligned. A/B to compare`
  и короткие сообщения для quiet/not-enough/frozen states;
- Basic bench row не показывает detailed `Glide preview ... event coherence`,
  а successful glide export сворачивает до `Exported <file>`;
- Advanced продолжает показывать detailed analysis/detect/source/glide strings.

## Why

После PR #40-41 первый экран уже стал проще визуально, но Basic всё ещё может
показывать строки вида `Detect found N hits, M obs, K delays`, source-owned
цифры и другие диагностические сообщения. Это полезно для Advanced, но в Basic
пользователь должен видеть outcome: что загружено, что посчитано, можно ли
слушать/экспортировать или нужно перейти в Advanced.

Этот шаг отделён от full source-centric table: иначе снова смешаются
копирайтинг первого экрана и инженерная диагностика.

## Status

status.md обновлён: да. Ветка: `feat/basic-outcome-status`.
PR: #42 draft — https://github.com/maxeliseyev/beat-equalizer/pull/42.
Проверено: `make test`, `make test-gui`, `git diff --check`.

## Next

После merge PR #42 продолжить PR 43 — Advanced diagnostics v2.

## Open

- В этом PR Basic status остаётся компактной строкой, без нового visual design.
- Advanced diagnostics v2 с таблицей source-centric остаётся следующим PR.

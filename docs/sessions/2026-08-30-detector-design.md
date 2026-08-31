# 2026-08-30 — канон детекции

## Context

Появился `docs/detector-design-recommendations.md`. Нужно, чтобы агенты
читали его до кода Analyze и не тащили ONNX «на вырост».

## What

Документ в индексе `docs/README.md`, таблице AGENTS, инварианты 11–13,
правка §3 `drum-editor-plan.md` (сеть = ступень 5), ADR.

## Why

§3.3 старого плана прямо звал RTNeural. Без явного канона следующий PR
детекции начнёт с рантайма. Лестница 1–4 закрывает 90% без данных.

## Status

status.md обновлён: да. Ветка: `docs/detector-design`.

## Next

Не писать детектор в этом PR. После scope/#5 — Analyze по этому канону.

## Open

Нет.

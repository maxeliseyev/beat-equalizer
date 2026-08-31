# 2026-08-30 — semver

## Context

Нужен стандартный номер major.minor.patch. База + документы = 0.1.1.

## What

- `VERSION` — единственный источник (сейчас `0.1.1`).
- CMake `project(VERSION)` и `juce_add_plugin(VERSION)` читают файл.
- `CHANGELOG.md`, `docs/versioning.md`, заголовок UI, `make version`.

## Why

Не хардкодить 0.1.0 в CMakeLists: разъедется с DAW и changelog.
Пока 0.x: patch = docs/сборка, minor = новое поведение, 1.0.0 = издание.

Тег не на каждый PR — только когда бинарь отдают.

## Status

status.md обновлён: да. Ветка: `docs/detector-design`.

## Next

Не ставить git-тег с этой ветки.

## Open

Нет.

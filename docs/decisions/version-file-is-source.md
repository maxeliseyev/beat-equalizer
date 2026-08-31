# VERSION — единственный источник номера

Status: accepted  
Date: 2026-08-30

## Context

Нужен semver в DAW, changelog и git. Легко размножить 0.1.0 в CMake, UI и README.

## Decision

Файл `VERSION` (одна строка `major.minor.patch`). CMake читает его в
`project()`, JUCE — `VERSION ${PROJECT_VERSION}`. Правила: `docs/versioning.md`.

Сейчас **0.1.1**: база плагина + канон документации.

## Alternatives

- Только git-теги — DAW не видит номер до релиза.
- Хардкод в CMakeLists — разъедется с changelog.

## Consequences

Бамп версии = правка `VERSION` + запись в `CHANGELOG.md` в том же PR.

# 2026-09-04 — macOS onboarding prerequisites

## Context

После merge PR #48 второй разработчик должен суметь поднять проект без этого
чата. В `README.md` был только краткий build minimum, а `docs/onboarding.md`
не называл явный набор macOS tools.

## What

Ветка `docs/macos-onboarding-prereqs` добавляет в `docs/onboarding.md`:

- `xcode-select --install`;
- Homebrew packages: CMake, Ninja, Git, GitHub CLI, clang-format;
- команды проверки toolchain;
- объяснение, что полный Xcode не обязателен для обычной сборки;
- границу Reaper host-smoke и release-only подписи/нотаризации.

`README.md` теперь коротко отправляет macOS setup в onboarding.

## Why

Это onboarding-факт, а не build logic: второй разработчик должен видеть список
до первого `make test`, но CMake/Makefile не должны пытаться устанавливать
глобальные инструменты или Xcode.

## Status

status.md обновлён: да. Ветка: `docs/macos-onboarding-prereqs`. PR: #49 draft.
Проверено: `rtk git diff --check`, `rtk make test`.

## Next

После merge PR #49 вернуться к Standalone `Export crossfade...`.

# 2026-09-04 — YAN9 Advanced diagnostics gate

## Context

PR #43 смёржен: Advanced теперь показывает source-centric diagnostics table и
role metadata. Следующий шаг из `status.md` — прогнать YAN9 после этого UI и
решить gate перед этапом 2, шагом 7.

## What

Текущая ветка делает gate-протокол:

- запускает ordinary real-kit tests на `/Users/maximeliseyev/Sound/YAN9`;
- запускает скрытый source-centric стенд `[.real-kit-source]`;
- запускает скрытый Standalone export smoke `[.real-kit-export]`;
- фиксирует числа и вывод в `docs/real-kit-protocol.md`.

Результат:

- `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 make test` зелёный;
- `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 build/release/tests/beat_tests "[.real-kit-source]" -s`
  зелёный: 124 hits, 91 snare-owned, 608 obs, 7 calibrated delays;
- source table: snare bottom 0.281 мс, room 14.667 мс, `return(room)=1`
  сохраняет 14.667 мс;
- `make test-gui` зелёный;
- `BEAT_REAL_KIT_DIR=/Users/maximeliseyev/Sound/YAN9 build/release/tests/beat_gui_tests "[.real-kit-export]" -s`
  зелёный: Analyze 79 % → 92 %, Detect 59 hits / 239 obs / 4 delays, Glide
  50 % 81 % → 81 % с 13 limited, Glide 100 % 81 % → 81 % с 30 limited.

## Why

После PR #43 у инженера появилась таблица, которая показывает не среднюю
когерентность всей суммы, а источник: close-пару, room/OH return, residual и
наблюдения по каналам. Gate нужен до шага 7: если glide на реальном snare
source не даёт выигрыша или упирается в slew-limit, переходить к нарезке без
ясной карты событий рано.

## Status

status.md обновлён: да. Ветка: `test/yan9-advanced-gate`.
PR: none.
Проверено: `make test` с `BEAT_REAL_KIT_DIR`, hidden `[.real-kit-source]`,
`make test-gui`, hidden `[.real-kit-export]`.

## Next

Открыть draft PR с YAN9 gate-протоколом.

## Open

- Агент не оценивает звук на слух; вывод по gate будет по числам и доступным
  тестовым артефактам.

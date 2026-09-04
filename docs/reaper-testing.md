# Как проверить в Reaper

Плагин проверяется как **один N-in / N-out insert на одной многоканальной
дорожке**, а не как инстанс на каждый микрофон.

## Подготовка

1. Собрать и установить локальный VST3/AU:

   ```bash
   make
   ```

2. Перезапустить Reaper или сделать rescan плагинов.
3. Создать один track, `Track channels` = число стемов, например 8.
4. Завести каждый моно-стем в свой канал этого track через routing/pin:
   1 → 1, 2 → 2, 3 → 3 и так далее.
5. Вставить **Beat Equalizer** на этот track.

## Static Alignment Smoke

1. В header виден текущий `Beat Equalizer <VERSION>`.
2. Playback рисует waveform по каналам; без playback допустима пустая scope
   строка.
3. В Advanced видны ручные delay/polarity/rotator controls и diagnostics.
4. `Analyze` не должен блокировать playback; результат меняет delay/polarity
   только после завершения worker.
5. Раньше пришедшие close-микрофоны задерживаются, чтобы совпасть с поздними.
   Нижний снейр обычно требует invert.
6. `A/B` сравнивает aligned и исходный сигнал при одной PDC: кит не должен
   прыгать по таймлайну.
7. `PDC smp / ms` в header должен совпадать с latency, которую видит Reaper.

## Standalone Editor Smoke

Reaper smoke не заменяет Standalone. Для задач этапа 2 отдельно пройти:

```bash
make test-gui
make run
```

Минимальный сценарий в Standalone: Load stems → Analyze → Detect → проверить
маркеры событий → `Export static...` или текущий export режима разработки.
Монитор-микс, Solo/Mute и Mono Sum не должны менять exported stems.

## Реальный Кит

Если доступен YAN9 или другой локальный кит:

```bash
BEAT_REAL_KIT_DIR=/path/to/kit make test
```

Без переменной real-kit tests пропускаются. Аудио в git не попадает никогда;
результаты фиксируются только числами в `docs/real-kit-protocol.md`.

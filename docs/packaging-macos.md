# Сборка и раздача под macOS

`.app` собирается всегда — JUCE делает его сам вместе с плагинами. Отдельной
сборки не нужно, нужна подпись: без неё приложение запустится только на той
машине, где собрано.

```sh
make app          # собрать, подписать, сделать dist/Beat Equalizer <версия>.dmg
make release-dmg  # то же плюс нотаризация и степлинг — для раздачи наружу
```

Всё делает `scripts/package-macos.sh`; `make` — только обёртка.

---

## Что кладётся в образ

```
Beat Equalizer <версия>.dmg
├── Beat Equalizer.app
├── Applications ->  /Applications     (перетащить приложение)
└── Plug-Ins/
    ├── Beat Equalizer.component        → ~/Library/Audio/Plug-Ins/Components/
    └── Beat Equalizer.vst3             → ~/Library/Audio/Plug-Ins/VST3/
```

---

## Сертификаты

Нужен **Developer ID Application** — не «Apple Development». Последний
подписывает для отладки на своей машине, Gatekeeper на чужой его не примет.

Сертификаты в `~/Documents/certs/` — это `.cer`, то есть только открытая
часть. Подписывать ими нельзя, пока они не в связке ключей рядом со своим
приватным ключом:

```sh
security import ~/Documents/certs/developerID_application.cer -k ~/Library/Keychains/login.keychain-db
security import ~/Documents/certs/developerID_installer.cer  -k ~/Library/Keychains/login.keychain-db
security find-identity -v -p codesigning
```

Последняя команда обязана показать `Developer ID Application: … (PK2473XF3P)`.
Если после импорта её нет — на этой машине нет приватного ключа: сертификат
выпущен по запросу (CSR), созданному на другом компьютере. Тогда либо экспорт
`.p12` оттуда (Keychain Access → сертификат → Export, с паролем) и импорт
здесь, либо новый сертификат по CSR, сделанному тут.

`SecKeychainItemImport: The specified item already exists` — не ошибка:
сертификат уже в связке, импорт не нужен. Проверять всё равно через
`find-identity`.

Один и тот же сертификат нередко оказывается сразу в двух связках, `login` и
`System`, и тогда `find-identity` показывает его дважды с одинаковым
отпечатком. Это одна личность, а не две, но `codesign` по имени отвечает
`ambiguous` — поэтому скрипт подписывает по отпечатку, а не по имени.

`Developer ID Installer` нужен только для `.pkg`; для `.dmg` он не участвует.
Пусть лежит.

### Нотаризация

Один раз сохранить учётные данные:

```sh
xcrun notarytool store-credentials beat-equalizer \
    --apple-id <Apple ID> --team-id PK2473XF3P \
    --password <app-specific password с appleid.apple.com>
```

Пароль именно **app-specific**, обычный пароль от Apple ID не подойдёт.
Дальше `make release-dmg` работает сам.

Порядок в скрипте не случайный: сначала нотаризуется приложение и степлится,
и только потом собирается образ. Талон живёт внутри бандла — если степлить
только образ, то перетащенное из него приложение на машине без интернета
проверку не пройдёт.

### Права

`packaging/macos/BeatEqualizer.entitlements` — одно право,
`com.apple.security.device.audio-input`. Под hardened runtime без него macOS
убивает процесс в момент открытия входа звуковой карты, и падение выглядит как
баг в аудио, а не как незаполненный список прав.

Парный к нему `NSMicrophoneUsageDescription` прописан в `CMakeLists.txt`
плагина: это текст, который пользователь увидит в запросе доступа.

---

## Иконка

```sh
make icon FILE=~/Desktop/beat-equalizer-1024.png
```

Кладёт картинку в `packaging/macos/icon.png`, откуда её забирает сборка: JUCE
сам делает из неё весь набор размеров и прописывает `CFBundleIconFile`. Нет
файла — сборка идёт без иконки, а не падает.

### Как её нарисовать

Иконка macOS — **не квадратная картинка**. Это скруглённый квадрат с полями и
тенью, и подсунуть вместо него полноразмерный квадрат нельзя: в доке он будет
торчать выше и шире всех соседей.

Геометрия на холсте 1024×1024:

| что | сколько |
|---|---|
| холст | 1024 × 1024, с альфа-каналом |
| тело иконки | 824 × 824 по центру, то есть по 100 пикселей полей |
| скругление | радиус ≈ 185, форма непрерывная (squircle), а не дуга окружности |
| за пределами формы | прозрачность, не белое |

Проще всего не считать это руками. В Xcode 26 лежит **Icon Composer**
(`/Applications/Xcode.app/Contents/Applications/Icon Composer.app`): туда
кладётся плоская картинка, форму и тень он ставит сам, по текущим правилам
системы. Оттуда — экспорт PNG 1024×1024, и дальше `make icon`.

### Мелкие размеры

`make icon` масштабирует одну картинку во все размеры: от 16×16 до 1024×1024.
Для 128 и выше этого достаточно, для 16 и 32 — нет. В списке файлов иконка
занимает шестнадцать точек, и уменьшенный крупный рисунок превращается там в
кашу.

Если мелкие размеры важны, их рисуют отдельно — крупнее штрих, меньше
деталей, часто вообще другой силуэт. Тогда `iconutil` собирается из готового
`.iconset` руками:

```sh
mkdir icon.iconset
# положить icon_16x16.png, icon_16x16@2x.png, … icon_512x512@2x.png
iconutil --convert icns icon.iconset --output icon.icns
```

Посмотреть, что вышло: `open build/icon.icns`.

---

## Info.plist подписывать отдельно не нужно

Info.plist собирает JUCE, и `codesign` запечатывает его сам вместе со всем
бандлом. Разница видна в выводе `codesign -dv`:

```
Info.plist=not bound        до подписи (ad-hoc от линковщика)
Info.plist entries=12       после
```

Отдельного действия для него нет. Менять plist после подписи нельзя — подпись
сломается, придётся подписывать заново.

Что должно быть в выводе `codesign -dv --verbose=4` на подписанном бандле:

| строка | зачем |
|---|---|
| `flags=0x10000(runtime)` | hardened runtime включён — без него нотаризация откажет |
| `Authority=Developer ID Application: …` | не «Apple Development» |
| `Authority=Apple Root CA` | цепочка целая |
| `Timestamp=…` | защищённая метка времени, тоже обязательна для нотаризации |
| `TeamIdentifier=PK2473XF3P` | не `not set` |
| `Info.plist entries=…` | plist запечатан |

## Проверка

Скрипт в конце сам прогоняет Gatekeeper и печатает вердикт. Вручную:

```sh
codesign --verify --strict --verbose=2 "dist/…/Beat Equalizer.app"
spctl --assess --type exec --verbose=4 "dist/…/Beat Equalizer.app"
xcrun stapler validate "dist/Beat Equalizer <версия>.dmg"
```

`spctl … rejected` c припиской `source=Unnotarized Developer ID` — норма до
нотаризации: подпись правильная, талона ещё нет. `rejected` **после**
`make release-dmg` — уже не норма.

Честная проверка — на другой машине, а не на своей: на машине сборщика
Gatekeeper мягче, и подпись, которая проходит здесь, может не пройти там.

#!/usr/bin/env bash
#
# Сборка, подпись и упаковка для macOS.
#
#   scripts/package-macos.sh                 собрать, подписать, сделать DMG
#   scripts/package-macos.sh --no-build      не пересобирать, только подписать
#   scripts/package-macos.sh --notarize      плюс нотаризация и степлинг
#
# Что получается в dist/:
#   Beat Equalizer <версия>.dmg   приложение и папка Plug-Ins внутри
#
# Подпись берётся из связки ключей: ищется Developer ID Application. Ad-hoc
# подписи, которую ставит линковщик, достаточно только на своей машине —
# на чужой Gatekeeper её не пропустит.

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

VERSION="$(tr -d ' \n' < VERSION)"
PRODUCT="Beat Equalizer"
BUNDLE_ID="com.beatequalizer.align"
CONFIG="${CONFIG:-release}"
BUILD_DIR="build/${CONFIG}"
ARTEFACT_CONFIG="Release"
[ "$CONFIG" = "debug" ] && ARTEFACT_CONFIG="Debug"
ARTEFACTS="${BUILD_DIR}/src/plugin/BeatEqualizer_artefacts/${ARTEFACT_CONFIG}"
ENTITLEMENTS="packaging/macos/BeatEqualizer.entitlements"
STAGING="${BUILD_DIR}/dmg-staging"
DIST="dist"
DMG="${DIST}/${PRODUCT} ${VERSION}.dmg"

BUILD=1
NOTARIZE=0
IDENTITY="${CODESIGN_IDENTITY:-}"
PROFILE="${NOTARY_PROFILE:-beat-equalizer}"

while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) BUILD=0 ;;
        --notarize) NOTARIZE=1 ;;
        --identity) IDENTITY="$2"; shift ;;
        --keychain-profile) PROFILE="$2"; shift ;;
        -h|--help) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "неизвестный аргумент: $1" >&2; exit 2 ;;
    esac
    shift
done

[ "$(uname -s)" = "Darwin" ] || { echo "только для macOS" >&2; exit 1; }

say() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }

# --- личность для подписи ---------------------------------------------------

# Личность берётся по SHA-1, а не по имени. Один и тот же сертификат нередко
# лежит сразу в двух связках — login и System, — и тогда find-identity
# показывает его дважды, а codesign по имени отвечает «ambiguous».
if [ -z "$IDENTITY" ]; then
    IDENTITY="$(security find-identity -v -p codesigning \
                | awk '/Developer ID Application/{print $2; exit}')"
    IDENTITY_NAME="$(security find-identity -v -p codesigning \
                     | sed -n 's/.*"\(Developer ID Application: [^"]*\)".*/\1/p' | head -1)"
else
    IDENTITY_NAME="$IDENTITY"
fi

if [ -z "$IDENTITY" ]; then
    cat >&2 <<'MSG'
В связке ключей нет Developer ID Application.

Если сертификат скачан, но не импортирован:

    security import ~/Documents/certs/developerID_application.cer -k ~/Library/Keychains/login.keychain-db
    security import ~/Documents/certs/developerID_installer.cer  -k ~/Library/Keychains/login.keychain-db
    security find-identity -v -p codesigning

Если после импорта личности всё равно нет, значит на этой машине нет
приватного ключа: сертификат выпущен по запросу (CSR) с другого компьютера.
Тогда нужен экспорт .p12 с той машины (Keychain Access → сертификат →
Export) и импорт здесь, либо новый сертификат по CSR, созданному тут.
MSG
    exit 1
fi

TEAM_ID="$(printf '%s' "$IDENTITY_NAME" | sed -n 's/.*(\([A-Z0-9]*\))$/\1/p')"
say "подпись: $IDENTITY_NAME"
echo "    отпечаток: $IDENTITY   team: $TEAM_ID"

# --- сборка -----------------------------------------------------------------

if [ "$BUILD" -eq 1 ]; then
    say "сборка $CONFIG"
    cmake --preset "$CONFIG" > /dev/null
    cmake --build --preset "$CONFIG" --target \
        BeatEqualizer_VST3 BeatEqualizer_AU BeatEqualizer_Standalone
fi

APP="${ARTEFACTS}/Standalone/${PRODUCT}.app"
AU="${ARTEFACTS}/AU/${PRODUCT}.component"
VST3="${ARTEFACTS}/VST3/${PRODUCT}.vst3"

for bundle in "$APP" "$AU" "$VST3"; do
    [ -d "$bundle" ] || { echo "нет сборки: $bundle" >&2; exit 1; }
done

# --- подпись ----------------------------------------------------------------

sign_bundle() {
    local bundle="$1"

    # Вложенный код подписывается раньше контейнера, иначе подпись контейнера
    # ломается при подписи внутренностей. --deep Apple не рекомендует: он
    # ставит одни и те же права всему подряд.
    while IFS= read -r -d '' nested; do
        codesign --force --options runtime --timestamp \
                 --sign "$IDENTITY" "$nested"
    done < <(find "$bundle" \( -name '*.dylib' -o -name '*.framework' \) -print0 2>/dev/null)

    codesign --force --options runtime --timestamp \
             --entitlements "$ENTITLEMENTS" \
             --identifier "$BUNDLE_ID" \
             --sign "$IDENTITY" "$bundle"

    codesign --verify --strict --verbose=2 "$bundle" 2>&1 | sed 's/^/    /'
}

say "подпись бандлов"
for bundle in "$AU" "$VST3" "$APP"; do
    echo "  $(basename "$bundle")"
    sign_bundle "$bundle"
done

# --- нотаризация приложения и плагинов --------------------------------------

notarize() {
    local what="$1"
    say "нотаризация: $(basename "$what")"
    if ! xcrun notarytool submit "$what" --keychain-profile "$PROFILE" --wait; then
        cat >&2 <<MSG

Нотаризация не прошла. Если дело в учётных данных, их надо сохранить один раз:

    xcrun notarytool store-credentials "$PROFILE" \\
        --apple-id <ваш Apple ID> --team-id $TEAM_ID \\
        --password <app-specific password с appleid.apple.com>

MSG
        exit 1
    fi
}

if [ "$NOTARIZE" -eq 1 ]; then
    # Нотаризуется один архив со всеми тремя бандлами, а талон степлится в
    # каждый по отдельности: он живёт внутри бандла, а не в архиве, и без
    # степлинга первая проверка на чужой машине пойдёт в сеть.
    #
    # Архив делает ditto, а не zip: у бандлов есть симлинки и ресурсные вилки,
    # и обычный zip складывает их так, что подпись после распаковки ломается.
    BATCH_DIR="${BUILD_DIR}/notarize"
    BATCH="${BUILD_DIR}/notarize.zip"
    rm -rf "$BATCH_DIR" "$BATCH"
    mkdir -p "$BATCH_DIR"
    cp -R "$APP" "$AU" "$VST3" "$BATCH_DIR/"
    ditto -c -k --keepParent --sequesterRsrc "$BATCH_DIR" "$BATCH"

    notarize "$BATCH"

    xcrun stapler staple "$APP"
    xcrun stapler staple "$AU"
    xcrun stapler staple "$VST3"
    rm -rf "$BATCH_DIR" "$BATCH"
fi

# --- DMG --------------------------------------------------------------------

say "образ диска"
rm -rf "$STAGING" && mkdir -p "$STAGING/Plug-Ins"
cp -R "$APP" "$STAGING/"
cp -R "$AU" "$STAGING/Plug-Ins/"
cp -R "$VST3" "$STAGING/Plug-Ins/"
ln -s /Applications "$STAGING/Applications"

mkdir -p "$DIST"
rm -f "$DMG"
hdiutil create -volname "$PRODUCT $VERSION" -srcfolder "$STAGING" \
    -ov -format UDZO "$DMG" > /dev/null

codesign --force --timestamp --sign "$IDENTITY" "$DMG"

if [ "$NOTARIZE" -eq 1 ]; then
    notarize "$DMG"
    xcrun stapler staple "$DMG"
fi

# --- проверка ---------------------------------------------------------------

say "проверка"
echo "  Gatekeeper, приложение:"
spctl --assess --type exec --verbose=4 "$APP" 2>&1 | sed 's/^/    /' || true
echo "  Gatekeeper, образ:"
spctl --assess --type open --context context:primary-signature --verbose=2 "$DMG" 2>&1 \
    | sed 's/^/    /' || true

if [ "$NOTARIZE" -eq 0 ]; then
    cat <<'MSG'

  Нотаризации не было: на чужой машине Gatekeeper покажет предупреждение.
  Для раздачи наружу нужен прогон с --notarize.
MSG
fi

say "готово: $DMG"
ls -lh "$DMG" | awk '{printf "    %s\n", $5}'

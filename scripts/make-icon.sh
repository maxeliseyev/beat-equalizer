#!/usr/bin/env bash
#
# Иконка приложения из готовой картинки 1024×1024.
#
#   scripts/make-icon.sh path/to/artwork.png
#
# Кладёт packaging/macos/icon.png (её забирает CMake) и собирает рядом
# icon.icns — посмотреть глазами, что все размеры вышли как надо.
#
# Картинка должна быть УЖЕ в форме иконки macOS: скруглённый квадрат с полями,
# прозрачность за его пределами. Скрипт ничего не обрезает и не скругляет —
# форма это дизайн, а не постобработка. Геометрия — в docs/packaging-macos.md.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

SOURCE="${1:-}"
[ -n "$SOURCE" ] && [ -f "$SOURCE" ] || { sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }

WIDTH="$(sips -g pixelWidth "$SOURCE" | awk '/pixelWidth/{print $2}')"
HEIGHT="$(sips -g pixelHeight "$SOURCE" | awk '/pixelHeight/{print $2}')"

if [ "$WIDTH" != "1024" ] || [ "$HEIGHT" != "1024" ]; then
    echo "нужен квадрат 1024×1024, а это ${WIDTH}×${HEIGHT}" >&2
    exit 1
fi

if ! sips -g hasAlpha "$SOURCE" | grep -q "hasAlpha: yes"; then
    echo "предупреждение: в картинке нет альфа-канала." >&2
    echo "Иконка macOS не квадратная: за скруглением обязана быть прозрачность." >&2
fi

mkdir -p packaging/macos
cp "$SOURCE" packaging/macos/icon.png

SET="build/icon.iconset"
rm -rf "$SET" && mkdir -p "$SET"

# Набор, который ждёт iconutil. 16 и 32 рисуются отдельно не от хорошей жизни:
# в доке иконку видно крупной, а в списке файлов — в шестнадцать точек, и
# уменьшенная крупная там превращается в кашу. Если мелкие размеры важны,
# рисовать их надо руками, а не масштабированием.
for size in 16 32 128 256 512; do
    sips -z $size $size "$SOURCE" --out "$SET/icon_${size}x${size}.png" > /dev/null
    sips -z $((size * 2)) $((size * 2)) "$SOURCE" \
        --out "$SET/icon_${size}x${size}@2x.png" > /dev/null
done

iconutil --convert icns "$SET" --output build/icon.icns
rm -rf "$SET"

echo "packaging/macos/icon.png   — её заберёт сборка"
echo "build/icon.icns            — посмотреть: open build/icon.icns"
echo
echo "Дальше: make app"


#!/usr/bin/env bash

set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="$PROJECT_DIR/assets/logo.png"
OUTPUT="$PROJECT_DIR/assets/icons"

SIZES=(16 22 24 32 48 64 128 256 512)

if [[ ! -f "$SOURCE" ]]; then
    echo "HATA: assets/logo.png bulunamadı!"
    exit 1
fi

mkdir -p "$OUTPUT"

for SIZE in "${SIZES[@]}"; do
    mkdir -p "$OUTPUT/${SIZE}x${SIZE}"

    magick "$SOURCE" \
        -resize "${SIZE}x${SIZE}" \
        -gravity center \
        -background none \
        -extent "${SIZE}x${SIZE}" \
        "$OUTPUT/${SIZE}x${SIZE}/deezer-desktop.png"

    echo "Oluşturuldu: ${SIZE}x${SIZE}"
done

echo "Tüm ikonlar başarıyla oluşturuldu!"

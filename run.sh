
#!/usr/bin/env bash

APP_NAME="Deezer Desktop"
APP_URL="https://www.deezer.com/"

PROFILE_DIR="$HOME/.config/deezer-desktop"

BROWSER="$(command -v chromium)"

if [ -z "$BROWSER" ]; then
    echo "Chromium bulunamadı!"
    exit 1
fi

mkdir -p "$PROFILE_DIR"

exec "$BROWSER" \
    --app="$APP_URL" \
    --user-data-dir="$PROFILE_DIR" \
    --class=DeezerDesktop

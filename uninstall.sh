
#!/usr/bin/env bash
set -euo pipefail

APP_NAME="deezer-desktop"

INSTALL_DIR="$HOME/.local/opt/$APP_NAME"
LAUNCHER="$HOME/.local/bin/$APP_NAME"
DESKTOP_FILE="$HOME/.local/share/applications/$APP_NAME.desktop"
ICON_DIR="$HOME/.local/share/icons/hicolor"

SIZES=(16 22 24 32 48 64 128 256 512)

if pgrep -x "$APP_NAME" >/dev/null 2>&1; then
    echo "Deezer Desktop is currently running."
    echo "Please close it before uninstalling."
    exit 1
fi

echo "Uninstalling Deezer Desktop..."

rm -rf "$INSTALL_DIR"
rm -f "$LAUNCHER"
rm -f "$DESKTOP_FILE"

# Remove application icons
for SIZE in "${SIZES[@]}"; do
    rm -f "$ICON_DIR/${SIZE}x${SIZE}/apps/deezer-desktop.png"
done

echo
echo "Deezer Desktop has been uninstalled."
echo "Application settings have been preserved."

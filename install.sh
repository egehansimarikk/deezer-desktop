
#!/usr/bin/env bash
set -euo pipefail

APP_NAME="deezer-desktop"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

INSTALL_DIR="$HOME/.local/opt/$APP_NAME"
BIN_DIR="$HOME/.local/bin"
DESKTOP_DIR="$HOME/.local/share/applications"
ICON_DIR="$HOME/.local/share/icons/hicolor"

SIZES=(16 22 24 32 48 64 128 256 512)

if [ ! -x "$SCRIPT_DIR/$APP_NAME" ]; then
    echo "Error: Deezer Desktop executable not found."
    exit 1
fi

if [ ! -d "$SCRIPT_DIR/assets/icons" ]; then
    echo "Error: Application icons not found."
    exit 1
fi

if pgrep -x "$APP_NAME" >/dev/null 2>&1; then
    echo "Deezer Desktop is currently running."
    echo "Please close it before installing or updating."
    exit 1
fi

echo "Installing Deezer Desktop..."

mkdir -p "$HOME/.local/opt" "$BIN_DIR" "$DESKTOP_DIR"

rm -rf "$INSTALL_DIR"

mkdir -p "$INSTALL_DIR"
cp -a "$SCRIPT_DIR/." "$INSTALL_DIR/"

# Install application icons
for SIZE in "${SIZES[@]}"; do
    ICON_SOURCE="$INSTALL_DIR/assets/icons/${SIZE}x${SIZE}/deezer-desktop.png"
    ICON_TARGET="$ICON_DIR/${SIZE}x${SIZE}/apps"

    if [ -f "$ICON_SOURCE" ]; then
        mkdir -p "$ICON_TARGET"
        cp "$ICON_SOURCE" "$ICON_TARGET/deezer-desktop.png"
        echo "Installed icon: ${SIZE}x${SIZE}"
    fi
done

# Create launcher
cat > "$BIN_DIR/$APP_NAME" <<'LAUNCHER'
#!/usr/bin/env bash
APP_DIR="$HOME/.local/opt/deezer-desktop"
exec "$APP_DIR/deezer-desktop" --ozone-platform=x11 "$@"
LAUNCHER

chmod +x "$BIN_DIR/$APP_NAME"

# Create application menu entry
cat > "$DESKTOP_DIR/$APP_NAME.desktop" <<EOF_DESKTOP
[Desktop Entry]
Name=Deezer Desktop
Comment=Unofficial Deezer desktop client
Exec=$BIN_DIR/$APP_NAME
Icon=$HOME/.local/share/icons/hicolor/512x512/apps/deezer-desktop.png
Terminal=false
Type=Application
Categories=AudioVideo;Audio;Player;
StartupNotify=true
EOF_DESKTOP

chmod +x "$DESKTOP_DIR/$APP_NAME.desktop"

echo
echo "Deezer Desktop installed successfully!"
echo "Launch it from your application menu or run:"
echo "  deezer-desktop"


pkgname=deezer-desktop
pkgver=0.1.2
pkgrel=1
pkgdesc="Unofficial Deezer desktop client built with Qt6 and CEF"
arch=('x86_64')
url="https://github.com/egehansimarikk/deezer-desktop"
license=('MIT' 'BSD-3-Clause')

depends=(
    'qt6-base'
    'libx11'
    'libpulse'
    'libglvnd'
    'mesa'
    'nss'
    'nspr'
    'glib2'
    'dbus'
    'gtk3'
    'atk'
    'at-spi2-core'
    'cups'
    'alsa-lib'
    'libdrm'
    'libxcb'
    'libxkbcommon'
    'libxcomposite'
    'libxdamage'
    'libxext'
    'libxfixes'
    'libxrandr'
    'libxrender'
    'libxss'
    'libxtst'
    'libxi'
    'libxinerama'
    'libxcursor'
    'cairo'
    'pango'
    'fontconfig'
    'freetype2'
    'expat'
    'systemd-libs'
)

_archive="deezer-desktop-v${pkgver}-linux-x86_64.tar.gz"

source=(
    "https://github.com/egehansimarikk/deezer-desktop/releases/download/v${pkgver}/${_archive}"
)

noextract=("${_archive}")

sha256sums=(
    '8ce8a0b81f05ad95b76a929f7a5ab29e5439deda505cc959fa9f4d75c9bcc8ba'
)

package() {
    local archive="$srcdir/$_archive"
    local appdir="$pkgdir/opt/deezer-desktop"

    install -d "$appdir"
    bsdtar -xf "$archive" -C "$appdir"

    install -Dm755 /dev/stdin "$pkgdir/usr/bin/deezer-desktop" <<'LAUNCHER'
#!/usr/bin/env bash
exec /opt/deezer-desktop/deezer-desktop --ozone-platform=x11 --disable-vulkan "$@"
LAUNCHER

    install -Dm644 /dev/stdin \
        "$pkgdir/usr/share/applications/deezer-desktop.desktop" <<'DESKTOP'
[Desktop Entry]
Name=Deezer Desktop
Comment=Unofficial Deezer desktop client
Exec=/usr/bin/deezer-desktop
Icon=deezer-desktop
Terminal=false
Type=Application
Categories=AudioVideo;Audio;Player;
StartupNotify=true
DESKTOP

    for size in 16 22 24 32 48 64 128 256 512; do
        install -Dm644 \
            "$appdir/assets/icons/${size}x${size}/deezer-desktop.png" \
            "$pkgdir/usr/share/icons/hicolor/${size}x${size}/apps/deezer-desktop.png"
    done

    install -Dm644 "$appdir/LICENSE" \
        "$pkgdir/usr/share/licenses/$pkgname/LICENSE"

    install -Dm644 "$appdir/LICENSE.txt" \
        "$pkgdir/usr/share/licenses/$pkgname/LICENSE-CEF"
}

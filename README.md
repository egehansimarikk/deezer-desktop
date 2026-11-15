# Deezer Desktop

**An unofficial Deezer desktop client for Linux, built with C++, Qt6 and Chromium Embedded Framework (CEF).**

Deezer Desktop brings the Deezer web player into a standalone Linux application, featuring desktop media integration, MPRIS support, application icons, and configurable performance settings.

> **Version:** v0.1.2  
> **Status:** Early development  
> **Tested on:** CachyOS Linux / Hyprland (XWayland)  
> **Architecture:** Linux x86_64  
> **CEF:** 144.0.6  
> **Chromium:** 144.0.7559.59

## Download

**Latest release: [Deezer Desktop v0.1.2](https://github.com/egehansimarikk/deezer-desktop/releases/tag/v0.1.2)**

Two prebuilt Linux x86_64 packages are available:

- `deezer-desktop-v0.1.2-linux-x86_64.tar.gz` — portable archive with installation scripts.
- `deezer-desktop-0.1.2-1-x86_64.pkg.tar.zst` — Arch Linux package.

## Installation

### Arch Linux / CachyOS package

Download the `.pkg.tar.zst` package from the [Releases page](https://github.com/egehansimarikk/deezer-desktop/releases).

Open a terminal in the directory containing the downloaded package and run:

```bash
sudo pacman -U ./deezer-desktop-0.1.2-1-x86_64.pkg.tar.zst
```

Pacman will install the application and handle dependencies available in your configured repositories.

After installation, Deezer Desktop can be launched from your application menu.

**Uninstallation**

To remove the application, run:

```bash
sudo pacman -Rns deezer-desktop
```

Your application settings are preserved after uninstallation.

### Manual installation (tar.gz)

1. Download the `.tar.gz` archive from the [Releases page](https://github.com/egehansimarikk/deezer-desktop/releases).
2. Extract the archive.
3. Open a terminal in the extracted directory.
4. Run the installation script:

```bash
./install.sh
```

The installer automatically installs the application, runtime files, application icons, and desktop launcher.

After installation, Deezer Desktop can be launched from your application menu.

**Uninstallation**

Open the extracted package directory and run:

```bash
./uninstall.sh
```

Application settings are preserved after uninstallation.

**Note:** Deezer Desktop currently uses X11 window integration. On Wayland compositors such as Hyprland, it runs through XWayland. Additional system dependencies may be required on distributions other than CachyOS.

## Features

- Standalone Deezer desktop window
- Embedded Deezer web player powered by CEF
- MPRIS desktop media integration
- System media key support
- Media metadata and playback status integration
- Play, pause, resume, stop, next, and previous controls
- Light, dark, and system theme options
- Optional window size persistence
- Configurable hardware acceleration
- Chromium GPU acceleration settings
- Custom application logo and Linux application icons
- Installation and uninstallation scripts

## Screenshots

*Screenshots will be added soon.*

## Requirements

### Running the prebuilt release

- Linux x86_64
- X11 or XWayland support
- Required system runtime libraries
- Internet connection

The release packages include the required CEF and Chromium runtime files. You do not need to download the CEF SDK separately.

### Building from source

- Linux
- C++17 compatible compiler
- CMake 3.20 or newer
- Ninja
- Qt6 (Widgets and DBus)
- X11 development libraries
- CEF Binary Distribution 144.0.6

## Building from source

### 1. Install dependencies

On Arch Linux-based distributions, install the required build tools, Qt6 packages, and X11 development libraries.

### 2. Download CEF

Download the Linux 64-bit CEF binary distribution matching the following versions:

`CEF 144.0.6 + Chromium 144.0.7559.59`

Extract the archive somewhere on your system.

### 3. Clone the repository

```bash
git clone https://github.com/egehansimarikk/deezer-desktop.git
cd deezer-desktop
```

### 4. Configure the project

Replace `/path/to/cef` with the path to your extracted CEF directory.

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCEF_ROOT="/path/to/cef"
```

### 5. Build

```bash
cmake --build build -j2
```

### 6. Run

```bash
./build/deezer-desktop --ozone-platform=x11 --disable-vulkan
```

## Project status

Deezer Desktop is currently in early development. It is primarily developed and tested on CachyOS Linux with Hyprland using XWayland.

Compatibility with other Linux distributions and desktop environments has not yet been fully tested.

Features and behavior may change in future releases.

## Disclaimer

Deezer Desktop is an unofficial, community-developed project. It is not affiliated with, endorsed by, or officially connected to Deezer.

Deezer is a trademark of its respective owner.

---

# Türkçe

**C++, Qt6 ve Chromium Embedded Framework (CEF) kullanılarak geliştirilmiş, Linux için resmi olmayan bir Deezer masaüstü istemcisi.**

Deezer Desktop, Deezer web oynatıcısını bağımsız bir Linux uygulamasına dönüştürür. Masaüstü medya entegrasyonu, MPRIS desteği, uygulama ikonları ve yapılandırılabilir performans ayarları sunar.

> **Sürüm:** v0.1.2  
> **Durum:** Erken geliştirme aşaması  
> **Test edilen sistem:** CachyOS Linux / Hyprland (XWayland)  
> **Mimari:** Linux x86_64  
> **CEF:** 144.0.6  
> **Chromium:** 144.0.7559.59

## İndir

**Son sürüm: [Deezer Desktop v0.1.2](https://github.com/egehansimarikk/deezer-desktop/releases/tag/v0.1.2)**

Linux x86_64 için iki hazır paket sunulmaktadır:

- `deezer-desktop-v0.1.2-linux-x86_64.tar.gz` — kurulum betiklerini içeren arşiv.
- `deezer-desktop-0.1.2-1-x86_64.pkg.tar.zst` — Arch Linux paketi.

## Kurulum

### Arch Linux / CachyOS paketi

`.pkg.tar.zst` paketini [Releases sayfasından](https://github.com/egehansimarikk/deezer-desktop/releases) indirin.

İndirdiğiniz paketin bulunduğu klasörde terminal açın ve şu komutu çalıştırın:

```bash
sudo pacman -U ./deezer-desktop-0.1.2-1-x86_64.pkg.tar.zst
```

Pacman uygulamayı kurar ve yapılandırılmış depolarınızda bulunan bağımlılıkları yönetir.

Kurulumdan sonra Deezer Desktop uygulama menüsünden başlatılabilir.

**Kaldırma**

Uygulamayı kaldırmak için:

```bash
sudo pacman -Rns deezer-desktop
```

Uygulama kaldırıldığında ayarlarınız korunur.

### Manuel kurulum (tar.gz)

1. `.tar.gz` arşivini [Releases sayfasından](https://github.com/egehansimarikk/deezer-desktop/releases) indirin.
2. Arşivi çıkarın.
3. Çıkarılan klasörü terminalde açın.
4. Kurulum betiğini çalıştırın:

```bash
./install.sh
```

Kurulum betiği uygulamayı, gerekli çalışma zamanı dosyalarını, uygulama ikonlarını ve masaüstü başlatıcısını otomatik olarak yükler.

Kurulumdan sonra Deezer Desktop uygulama menüsünden başlatılabilir.

**Kaldırma**

Çıkardığınız paket klasörünü terminalde açıp şu komutu çalıştırın:

```bash
./uninstall.sh
```

Uygulama kaldırıldığında ayarlarınız korunur.

**Not:** Deezer Desktop şu anda X11 pencere entegrasyonunu kullanmaktadır. Hyprland gibi Wayland tabanlı masaüstü ortamlarında XWayland üzerinden çalışır. CachyOS dışındaki dağıtımlarda ek sistem bağımlılıkları gerekebilir.

## Özellikler

- Bağımsız Deezer masaüstü penceresi
- CEF tabanlı gömülü Deezer web oynatıcısı
- MPRIS üzerinden masaüstü medya entegrasyonu
- Sistem medya tuşu desteği
- Medya bilgisi ve oynatma durumu entegrasyonu
- Oynat, duraklat, devam ettir, durdur, sonraki ve önceki parça kontrolleri
- Açık, koyu ve sistem teması seçenekleri
- Pencere boyutunu hatırlama seçeneği
- Yapılandırılabilir donanım hızlandırması
- Chromium GPU hızlandırma ayarları
- Özel uygulama logosu ve Linux uygulama ikonları
- Kurulum ve kaldırma betikleri

## Ekran görüntüleri

*Ekran görüntüleri yakında eklenecek.*

## Gereksinimler

### Hazır sürümü çalıştırmak için

- Linux x86_64
- X11 veya XWayland desteği
- Gerekli sistem çalışma zamanı kütüphaneleri
- İnternet bağlantısı

Release paketleri gerekli CEF ve Chromium çalışma zamanı dosyalarını içerir. CEF SDK'yı ayrıca indirmeniz gerekmez.

### Kaynak koddan derlemek için

- Linux
- C++17 destekleyen derleyici
- CMake 3.20 veya üzeri
- Ninja
- Qt6 (Widgets ve DBus)
- X11 geliştirme kütüphaneleri
- CEF Binary Distribution 144.0.6

## Kaynak koddan derleme

### 1. Bağımlılıkları yükleyin

Arch Linux tabanlı dağıtımlarda gerekli derleme araçlarını, Qt6 paketlerini ve X11 geliştirme kütüphanelerini yükleyin.

### 2. CEF'i indirin

Aşağıdaki sürümlerle eşleşen Linux 64-bit CEF paketini indirin:

`CEF 144.0.6 + Chromium 144.0.7559.59`

Arşivi sisteminizde uygun bir konuma çıkarın.

### 3. Depoyu klonlayın

```bash
git clone https://github.com/egehansimarikk/deezer-desktop.git
cd deezer-desktop
```

### 4. Projeyi yapılandırın

`/cef/klasorunun/yolu` bölümünü kendi sisteminizdeki CEF klasörünün yoluyla değiştirin.

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCEF_ROOT="/cef/klasorunun/yolu"
```

### 5. Derleyin

```bash
cmake --build build -j2
```

### 6. Çalıştırın

```bash
./build/deezer-desktop --ozone-platform=x11 --disable-vulkan
```

## Proje durumu

Deezer Desktop şu anda erken geliştirme aşamasındadır. Öncelikli olarak Hyprland masaüstü ortamıyla birlikte CachyOS üzerinde geliştirilmekte ve XWayland üzerinden test edilmektedir.

Diğer Linux dağıtımları ve masaüstü ortamlarıyla uyumluluğu henüz tamamen test edilmemiştir.

Gelecek sürümlerde özellikler ve uygulamanın çalışma biçimi değişebilir.

## Sorumluluk reddi

Deezer Desktop, topluluk tarafından geliştirilmiş resmi olmayan bir projedir. Deezer ile herhangi bir resmî bağlantısı, ortaklığı veya onayı bulunmamaktadır.

Deezer, ilgili sahibinin tescilli markasıdır.
# Deezer Desktop

**An unofficial Deezer desktop client for Linux, built with C++, Qt6 and Chromium Embedded Framework (CEF).**

Deezer Desktop brings the Deezer web player into a standalone Linux application, with desktop integration and configurable performance settings.

> **Version:** v0.1.0  
> **Status:** Early development  
> **Tested on:** CachyOS Linux / Hyprland (XWayland)  
> **Architecture:** Linux x86_64  
> **CEF:** 144.0.6  
> **Chromium:** 144.0.7559.59

## Download

**Latest release: [Deezer Desktop v0.1.0](https://github.com/egehansimarikk/deezer-desktop/releases/tag/v0.1.0)**

Download the Linux x86_64 archive:

`deezer-desktop-v0.1.0-linux-x86_64.tar.gz`

### Installation

1. Download the `.tar.gz` archive from the [Releases page](https://github.com/egehansimarikk/deezer-desktop/releases).
2. Extract the archive.
3. Open the extracted directory.
4. Run the application:

```bash
./deezer-desktop --ozone-platform=x11
```

The release package includes the required CEF and Chromium runtime files. You do not need to download the CEF SDK separately.

**Note:** The application currently uses X11 window integration. On Wayland compositors such as Hyprland, it runs through XWayland. Additional system dependencies may be required on distributions other than CachyOS.

## Features

- Standalone Deezer desktop window
- Embedded Deezer web player powered by CEF
- Desktop media controls through MPRIS
- Media metadata integration
- Light, dark and system theme options
- Optional window size persistence
- Hardware acceleration toggle
- Configurable Chromium GPU acceleration

## Screenshots

*Screenshots will be added soon.*

## Requirements

### Running the prebuilt release

- Linux x86_64
- X11 or XWayland support
- Required system runtime libraries
- Internet connection

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

On Arch Linux-based distributions, install the required build tools and Qt/X11 development packages.

### 2. Download CEF

Download the CEF Linux 64-bit binary distribution matching this version:

`CEF 144.0.6 + Chromium 144.0.7559.59`

Extract the archive somewhere on your system.

### 3. Clone the repository

```bash
git clone https://github.com/egehansimarikk/deezer-desktop.git
cd deezer-desktop
```

### 4. Configure the project

Replace the path below with your extracted CEF directory:

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

## Disclaimer

Deezer Desktop is an unofficial, community-developed project. It is not affiliated with, endorsed by, or officially connected to Deezer.

Deezer is a trademark of its respective owner.

---

# Türkçe

**C++, Qt6 ve Chromium Embedded Framework (CEF) kullanılarak geliştirilmiş, Linux için resmi olmayan bir Deezer masaüstü istemcisi.**

Deezer Desktop, Deezer web oynatıcısını masaüstü ortamına taşıyarak bağımsız bir Linux uygulaması olarak kullanılmasını sağlar.

> **Sürüm:** v0.1.0  
> **Durum:** Erken geliştirme aşaması  
> **Test edilen sistem:** CachyOS Linux / Hyprland (XWayland)  
> **Mimari:** Linux x86_64  
> **CEF:** 144.0.6  
> **Chromium:** 144.0.7559.59

## İndir

**Son sürüm: [Deezer Desktop v0.1.0](https://github.com/egehansimarikk/deezer-desktop/releases/tag/v0.1.0)**

Linux x86_64 arşivi:

`deezer-desktop-v0.1.0-linux-x86_64.tar.gz`

### Kurulum

1. `.tar.gz` arşivini [Releases sayfasından](https://github.com/egehansimarikk/deezer-desktop/releases) indirin.
2. Arşivi çıkarın.
3. Çıkarılan klasörü açın.
4. Uygulamayı çalıştırın:

```bash
./deezer-desktop --ozone-platform=x11
```

Release paketi gerekli CEF ve Chromium çalışma zamanı dosyalarını içerir. CEF SDK'yı ayrıca indirmeniz gerekmez.

**Not:** Uygulama şu anda X11 pencere entegrasyonunu kullanmaktadır. Hyprland gibi Wayland tabanlı masaüstlerinde XWayland üzerinden çalışır. CachyOS dışındaki dağıtımlarda ek sistem bağımlılıkları gerekebilir.

## Özellikler

- Bağımsız Deezer masaüstü penceresi
- CEF tabanlı gömülü Deezer web oynatıcısı
- MPRIS üzerinden masaüstü medya kontrolleri
- Medya bilgisi entegrasyonu
- Açık, koyu ve sistem teması seçenekleri
- Pencere boyutunu hatırlama seçeneği
- Donanım hızlandırması ayarı
- Chromium GPU hızlandırmasını açıp kapatma

## Ekran görüntüleri

*Ekran görüntüleri yakında eklenecek.*

## Gereksinimler

### Hazır sürümü çalıştırmak için

- Linux x86_64
- X11 veya XWayland desteği
- Gerekli sistem çalışma zamanı kütüphaneleri
- İnternet bağlantısı

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

Arch Linux tabanlı dağıtımlarda gerekli derleme araçlarını ve Qt/X11 geliştirme paketlerini yükleyin.

### 2. CEF'i indirin

Aşağıdaki sürümle eşleşen Linux 64-bit CEF paketini indirin:

`CEF 144.0.6 + Chromium 144.0.7559.59`

Arşivi sisteminizde uygun bir konuma çıkarın.

### 3. Depoyu klonlayın

```bash
git clone https://github.com/egehansimarikk/deezer-desktop.git
cd deezer-desktop
```

### 4. Projeyi yapılandırın

Aşağıdaki CEF yolunu kendi sisteminizdeki çıkarılmış CEF klasörünün yoluyla değiştirin:

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

## Sorumluluk reddi

Deezer Desktop, topluluk tarafından geliştirilmiş resmi olmayan bir projedir. Deezer ile herhangi bir resmî bağlantısı, ortaklığı veya onayı bulunmamaktadır.

Deezer, ilgili sahibinin tescilli markasıdır.
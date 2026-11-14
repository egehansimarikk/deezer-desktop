# Deezer Desktop

**An unofficial Deezer desktop client for Linux, built with C++, Qt6 and Chromium Embedded Framework (CEF).**

Deezer Desktop brings the Deezer web player into a standalone Linux application, with desktop integration and configurable performance settings.

> **Status:** Early development (v0.1.0)
> **Tested on:** CachyOS Linux
> **CEF:** 144.0.6
> **Chromium:** 144.0.7559.59

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

This project is currently in early development. It is primarily developed and tested on CachyOS. Compatibility with other Linux distributions has not yet been fully tested.

## Disclaimer

Deezer Desktop is an unofficial, community-developed project. It is not affiliated with, endorsed by, or officially connected to Deezer.

Deezer is a trademark of its respective owner.

---

# Türkçe

**C++, Qt6 ve Chromium Embedded Framework (CEF) kullanılarak geliştirilmiş, Linux için resmi olmayan bir Deezer masaüstü istemcisi.**

Deezer Desktop, Deezer web oynatıcısını masaüstü ortamına taşıyarak bağımsız bir Linux uygulaması olarak kullanılmasını sağlar.

> **Durum:** Erken geliştirme aşaması (v0.1.0)
> **Test edilen sistem:** CachyOS Linux
> **CEF:** 144.0.6
> **Chromium:** 144.0.7559.59

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

Proje şu anda erken geliştirme aşamasındadır. Öncelikli olarak CachyOS üzerinde geliştirilmekte ve test edilmektedir. Diğer Linux dağıtımlarıyla uyumluluğu henüz tamamen test edilmemiştir.

## Sorumluluk reddi

Deezer Desktop, topluluk tarafından geliştirilmiş resmi olmayan bir projedir. Deezer ile herhangi bir resmî bağlantısı, ortaklığı veya onayı bulunmamaktadır.

Deezer, ilgili sahibinin tescilli markasıdır.

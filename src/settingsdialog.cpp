#include "settingsdialog.h"
#include "audiooutputcontroller.h"

#include <QSettings>
#include <QComboBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVariantMap>

#define CEF_VERSION "144.0.6"
#define CHROMIUM_VERSION "144.0.7559.59"
#define APP_VERSION "0.1.2"

SettingsDialog::SettingsDialog(
    AudioOutputController *controller,
    QWidget *parent
)
    : QDialog(parent),
      audioController(controller)
{
    setWindowTitle("Deezer Desktop - Ayarlar");
    setMinimumWidth(440);

    QSettings settings;

    auto *mainLayout = new QVBoxLayout(this);

    // General settings
    auto *generalGroup = new QGroupBox("Genel", this);
    auto *formLayout = new QFormLayout(generalGroup);

    themeCombo = new QComboBox(this);

    themeCombo->addItem("Sistem varsayılanı", "system");
    themeCombo->addItem("Açık", "light");
    themeCombo->addItem("Koyu", "dark");

    QString savedTheme =
        settings.value("appearance/theme", "system").toString();

    int themeIndex = themeCombo->findData(savedTheme);

    if (themeIndex >= 0)
        themeCombo->setCurrentIndex(themeIndex);

    formLayout->addRow("Uygulama teması:", themeCombo);

    rememberWindowSizeCheck = new QCheckBox(
        "Pencere boyutunu hatırla",
        this
    );

    rememberWindowSizeCheck->setChecked(
        settings.value("window/rememberSize", true).toBool()
    );

    formLayout->addRow("", rememberWindowSizeCheck);

    mainLayout->addWidget(generalGroup);

    auto *infoLabel = new QLabel(
        "Tema değişikliği uygulamanın Qt arayüzünü etkiler. "
        "Deezer web sayfasının görünümü ayrıca kontrol edilir.",
        this
    );

    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    // Audio settings
    auto *audioGroup = new QGroupBox("Ses", this);
    auto *audioLayout = new QFormLayout(audioGroup);

    audioOutputCombo = new QComboBox(this);
    audioOutputCombo->setMinimumContentsLength(25);
    audioOutputCombo->setSizeAdjustPolicy(
        QComboBox::AdjustToMinimumContentsLengthWithIcon
    );

    audioOutputCombo->addItem(
        "Sistem varsayılanını kullan",
        QString()
    );

    audioLayout->addRow(
        "Ses çıkış cihazı:",
        audioOutputCombo
    );

    auto *audioButtonLayout = new QHBoxLayout();

    auto *refreshAudioButton = new QPushButton(
        "Cihazları yenile",
        this
    );

    audioButtonLayout->addWidget(refreshAudioButton);
    audioButtonLayout->addStretch();

    audioLayout->addRow("", audioButtonLayout);

        auto *audioInfo = new QLabel(
        "Seçilen ses çıkış cihazı yalnızca Deezer Desktop için "
        "kullanılır. Cihazın bağlantısı kesildiğinde ses otomatik "
        "olarak sessize alınır. Cihaz yeniden bağlandığında ses "
        "akışı otomatik olarak seçilen cihaza yönlendirilir ve "
        "ses yeniden etkinleştirilir. Oynatma otomatik olarak "
        "yeniden başlatılmaz.",
        this
    );

    audioInfo->setWordWrap(true);
    audioLayout->addRow("", audioInfo);

    mainLayout->addWidget(audioGroup);

    // Receive audio devices
    if (audioController) {
        connect(
            audioController,
            &AudioOutputController::devicesUpdated,
            this,
            [this](
                const QVariantList &devices,
                const QString &defaultSink
            ) {
                populateAudioDevices(devices, defaultSink);
            }
        );

        connect(
            refreshAudioButton,
            &QPushButton::clicked,
            audioController,
            &AudioOutputController::refreshDevices
        );

        audioController->refreshDevices();
    }
    else {
        audioOutputCombo->setEnabled(false);
        refreshAudioButton->setEnabled(false);
    }

    // Performance settings
    auto *performanceGroup = new QGroupBox("Performans", this);
    auto *performanceLayout = new QVBoxLayout(performanceGroup);

    hardwareAccelerationCheck = new QCheckBox(
        "Donanım hızlandırmasını kullan",
        this
    );

    hardwareAccelerationCheck->setChecked(
        settings.value(
            "performance/hardwareAcceleration",
            true
        ).toBool()
    );

    performanceLayout->addWidget(hardwareAccelerationCheck);

    auto *restartInfo = new QLabel(
        "Bu ayarın değişmesi için uygulamayı yeniden başlatmanız gerekir.",
        this
    );

    restartInfo->setWordWrap(true);
    performanceLayout->addWidget(restartInfo);

    mainLayout->addWidget(performanceGroup);

    // About section
    auto *aboutGroup = new QGroupBox("Hakkında", this);
    auto *aboutLayout = new QFormLayout(aboutGroup);

    aboutLayout->addRow(
        "Uygulama:",
        new QLabel("Deezer Desktop", this)
    );

    aboutLayout->addRow(
        "Sürüm:",
        new QLabel(APP_VERSION, this)
    );

    aboutLayout->addRow(
        "CEF:",
        new QLabel(CEF_VERSION, this)
    );

    aboutLayout->addRow(
        "Chromium:",
        new QLabel(CHROMIUM_VERSION, this)
    );

    mainLayout->addWidget(aboutGroup);

    // Buttons
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel,
        this
    );

    connect(
        buttonBox,
        &QDialogButtonBox::accepted,
        this,
        [this]() {
            saveSettings();
            accept();
        }
    );

    connect(
        buttonBox,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject
    );

    mainLayout->addWidget(buttonBox);
}

void SettingsDialog::populateAudioDevices(
    const QVariantList &devices,
    const QString &defaultSink
)
{
    Q_UNUSED(defaultSink);

    QSettings settings;

    QString savedDevice =
        settings.value("audio/outputDevice", "").toString();

    QString currentDevice =
        audioOutputCombo->currentData().toString();

    QString preferredDevice =
        currentDevice.isEmpty() ? savedDevice : currentDevice;

    audioOutputCombo->clear();

    audioOutputCombo->addItem(
        "Sistem varsayılanını kullan",
        QString()
    );

    bool savedDeviceFound = false;

    for (const QVariant &item : devices) {
        QVariantMap device = item.toMap();

        QString name = device["name"].toString();
        QString description = device["description"].toString();

        if (name.isEmpty())
            continue;

        QString displayName = description;

        if (displayName.isEmpty())
            displayName = name;

        if (device["isDefault"].toBool())
            displayName += " (Varsayılan)";

        audioOutputCombo->addItem(displayName, name);

        if (name == preferredDevice)
            savedDeviceFound = true;
    }

    if (!preferredDevice.isEmpty() && !savedDeviceFound) {
        audioOutputCombo->addItem(
            "Kayıtlı cihaz şu anda kullanılamıyor",
            preferredDevice
        );
    }

    int selectedIndex =
        audioOutputCombo->findData(preferredDevice);

    if (selectedIndex >= 0)
        audioOutputCombo->setCurrentIndex(selectedIndex);
    else
        audioOutputCombo->setCurrentIndex(0);

    audioDeviceListReceived = true;
}

void SettingsDialog::saveSettings()
{
    QSettings settings;

    settings.setValue(
        "appearance/theme",
        themeCombo->currentData().toString()
    );

    settings.setValue(
        "window/rememberSize",
        rememberWindowSizeCheck->isChecked()
    );

    settings.setValue(
        "performance/hardwareAcceleration",
        hardwareAccelerationCheck->isChecked()
    );

    if (audioDeviceListReceived) {
    QString selectedDevice =
        audioOutputCombo->currentData().toString();

    settings.setValue(
        "audio/outputDevice",
        selectedDevice
    );

    if (audioController)
        audioController->setOutputDevice(selectedDevice);
}
    settings.sync();
}
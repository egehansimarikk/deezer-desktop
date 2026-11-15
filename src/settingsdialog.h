#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QComboBox;
class QCheckBox;
class AudioOutputController;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(
        AudioOutputController *audioController,
        QWidget *parent = nullptr
    );

private:
    QComboBox *themeCombo = nullptr;
    QComboBox *audioOutputCombo = nullptr;

    QCheckBox *rememberWindowSizeCheck = nullptr;
    QCheckBox *hardwareAccelerationCheck = nullptr;

    AudioOutputController *audioController = nullptr;

    bool audioDeviceListReceived = false;

    void populateAudioDevices(
        const QVariantList &devices,
        const QString &defaultSink
    );

    void saveSettings();
};

#endif
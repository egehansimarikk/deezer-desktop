
#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QComboBox;
class QCheckBox;

class SettingsDialog : public QDialog
{
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private:
    QComboBox *themeCombo = nullptr;
    QCheckBox *rememberWindowSizeCheck = nullptr;
    QCheckBox *hardwareAccelerationCheck = nullptr;

    void saveSettings();
};

#endif

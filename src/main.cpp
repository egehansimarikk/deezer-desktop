
#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("Deezer Desktop");
    QApplication::setOrganizationName("DeezerDesktop");

    QMainWindow window;
    window.setWindowTitle("Deezer Desktop");
    window.resize(500, 300);

    auto *central = new QWidget(&window);
    auto *layout = new QVBoxLayout(central);

    auto *title = new QLabel("Deezer Desktop");
    title->setAlignment(Qt::AlignCenter);

    QFont titleFont = title->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    title->setFont(titleFont);

    auto *description = new QLabel(
        "Linux icin Deezer masaustu uygulamasi"
    );
    description->setAlignment(Qt::AlignCenter);

    auto *status = new QLabel("Chromium hazir.");
    status->setAlignment(Qt::AlignCenter);

    auto *launchButton = new QPushButton("Deezer'i Ac");

    layout->addStretch();
    layout->addWidget(title);
    layout->addWidget(description);
    layout->addSpacing(15);
    layout->addWidget(status);
    layout->addWidget(launchButton);
    layout->addStretch();

    window.setCentralWidget(central);

    QObject::connect(
        launchButton,
        &QPushButton::clicked,
        [&window, status]()
        {
            QString chromium =
                QStandardPaths::findExecutable("chromium");

            if (chromium.isEmpty()) {
                QMessageBox::critical(
                    &window,
                    "Hata",
                    "Chromium bulunamadi!"
                );
                return;
            }

            QString profile =
                QStandardPaths::writableLocation(
                    QStandardPaths::AppConfigLocation
                ) + "/chromium-profile";

            QDir().mkpath(profile);

            QStringList arguments;
            arguments << "--app=https://www.deezer.com/";
            arguments << "--user-data-dir=" + profile;
            arguments << "--class=DeezerDesktop";

            bool started = QProcess::startDetached(
                chromium,
                arguments
            );

            if (started) {
                status->setText("Deezer baslatildi!");
            } else {
                status->setText("Chromium baslatilamadi!");
            }
        }
    );

    window.show();

    return app.exec();
}

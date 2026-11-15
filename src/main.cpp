#include "settingsdialog.h"
#include <QSettings>
#include "include/cef_command_line.h"

#include <QSettings>
#include <QPalette>
#include <QColor>
#include <QStyle>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include "mpriscontroller.h"
#include "audiooutputcontroller.h"
#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QTimer>
#include <QMetaObject>
#include <QPointer>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QDebug>
#include <QCoreApplication>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

#include <X11/Xlib.h>

// Prevent X11 macro from conflicting with CEF callback methods.
#undef Success

#include <algorithm>
#include <cstdint>
#include <climits>

#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_frame.h"
#include "include/cef_load_handler.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_browser_process_handler.h"
#include "include/internal/cef_linux.h"
#include "include/cef_render_process_handler.h"
#include "include/cef_request_handler.h"
#include "include/cef_display_handler.h"
#include "include/cef_v8.h"
#include "include/wrapper/cef_message_router.h"

// --------------------------------------------------
// CEF message pump - Qt event loop integration
// --------------------------------------------------




class CefMessagePump : public QObject
{
public:
    explicit CefMessagePump(QObject *parent = nullptr)
        : QObject(parent)
    {
        timer.setSingleShot(true);

        QObject::connect(
            &timer,
            &QTimer::timeout,
            this,
            [this]() {
                doWork();
            }
        );
    }

    void schedule(int64_t delayMs)
    {
        // CEF may request a maximum-delay timer.
        // Do not replace an already pending timer.
        if (delayMs == INT_MAX && timer.isActive())
            return;

        timer.stop();

        // CEF should never wait too long between message pump calls.
        constexpr int64_t maxDelay = 1000 / 30;

        int64_t delay = delayMs;

        if (delay <= 0)
            delay = 0;
        else if (delay > maxDelay)
            delay = maxDelay;

        qDebug() << "CEF TIMER SCHEDULED:" << delay;

        timer.start(static_cast<int>(delay));
    }

private:
    void doWork()
    {
        if (isActive) {
            reentrancyDetected = true;
            return;
        }

        reentrancyDetected = false;
        isActive = true;

        qDebug() << "CEF MESSAGE PUMP TICK";

        CefDoMessageLoopWork();

        qDebug() << "CEF MESSAGE PUMP WORK FINISHED";

        isActive = false;

        if (reentrancyDetected) {
            schedule(0);
        }
        else if (!timer.isActive()) {
            // Keep the message pump alive even when CEF
            // does not explicitly request another timer.
            schedule(1000 / 30);
        }
    }

    QTimer timer;

    bool isActive = false;
    bool reentrancyDetected = false;
};




// --------------------------------------------------
// CEF application
// --------------------------------------------------



class DeezerCefApp :
    public CefApp,
    public CefBrowserProcessHandler,
    public CefRenderProcessHandler
{
public:
    void setMessagePump(CefMessagePump *pump)
    {
        messagePump = pump;
    }

    void setHardwareAccelerationEnabled(bool enabled)
    {
        hardwareAccelerationEnabled = enabled;
    }

    CefRefPtr<CefBrowserProcessHandler>
    GetBrowserProcessHandler() override
    {
        return this;
    }

    CefRefPtr<CefRenderProcessHandler>
    GetRenderProcessHandler() override
    {
        return this;
    }

    void OnBeforeCommandLineProcessing(
        const CefString& processType,
        CefRefPtr<CefCommandLine> commandLine) override
    {
        if (!hardwareAccelerationEnabled) {
            commandLine->AppendSwitch("disable-gpu");

            qDebug() << "CEF HARDWARE ACCELERATION: DISABLED";
        }
        else {
            qDebug() << "CEF HARDWARE ACCELERATION: ENABLED";
        }
    }

    void OnScheduleMessagePumpWork(int64_t delayMs) override
    {
        qDebug() << "CEF SCHEDULE REQUEST:" << delayMs;

        QPointer<CefMessagePump> pump = messagePump;

        if (!pump)
            return;

        QMetaObject::invokeMethod(
            pump.data(),
            [pump, delayMs]() {
                if (pump)
                    pump->schedule(delayMs);
            },
            Qt::QueuedConnection
        );
    }

    // Renderer process: initialize JavaScript query router.
    void OnWebKitInitialized() override
    {
        CefMessageRouterConfig config;

        rendererMessageRouter =
            CefMessageRouterRendererSide::Create(config);

        qDebug() << "CEF RENDERER MESSAGE ROUTER CREATED";
    }

    void OnContextCreated(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefV8Context> context) override
    {
        if (rendererMessageRouter) {
            rendererMessageRouter->OnContextCreated(
                browser,
                frame,
                context
            );
        }
    }

    void OnContextReleased(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefV8Context> context) override
    {
        if (rendererMessageRouter) {
            rendererMessageRouter->OnContextReleased(
                browser,
                frame,
                context
            );
        }
    }

    bool OnProcessMessageReceived(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefProcessId sourceProcess,
        CefRefPtr<CefProcessMessage> message) override
    {
        if (rendererMessageRouter) {
            return rendererMessageRouter->OnProcessMessageReceived(
                browser,
                frame,
                sourceProcess,
                message
            );
        }

        return false;
    }

private:
    QPointer<CefMessagePump> messagePump;

    CefRefPtr<CefMessageRouterRendererSide>
        rendererMessageRouter;

    bool hardwareAccelerationEnabled = true;

    IMPLEMENT_REFCOUNTING(DeezerCefApp);
};

// --------------------------------------------------
// Forward declaration
// --------------------------------------------------

class MainWindow;

// --------------------------------------------------
// CEF browser client
// --------------------------------------------------


class DeezerClient :
    public CefClient,
    public CefLifeSpanHandler,
    public CefLoadHandler,
    public CefRequestHandler,
    public CefDisplayHandler,
    public CefMessageRouterBrowserSide::Handler
{
public:
   explicit DeezerClient(
    MainWindow *window,
    MprisController *mpris)
    : mainWindow(window),
      mprisController(mpris)
{
}

    void executeMediaCommand(const QString &command)
{
    if (!mediaFrame || !mediaFrame->IsValid()) {
        qWarning() << "MPRIS: No valid media frame for command:"
                   << command;
        return;
    }

    QString action;

    if (command == "play") {
        action = R"JS(
            if (media.paused) {
                media.play().catch(function(error) {
                    console.error("MPRIS PLAY ERROR:", error);
                });
            }
        )JS";
    }
    else if (command == "pause") {
        action = "media.pause();";
    }
    else if (command == "playpause") {
        action = R"JS(
            if (media.paused) {
                media.play().catch(function(error) {
                    console.error("MPRIS PLAY ERROR:", error);
                });
            } else {
                media.pause();
            }
        )JS";
    }
    else if (command == "stop") {
        action = R"JS(
            media.pause();
            try {
                media.currentTime = 0;
            } catch (error) {
                console.error("MPRIS STOP ERROR:", error);
            }
        )JS";
    }
    else {
        qWarning() << "MPRIS: Unknown media command:" << command;
        return;
    }

    QString script =
        QStringLiteral(
            "(function(){"
            "const media = document.querySelector('audio, video');"
            "if (!media) {"
            "console.warn('MPRIS: Media element not found');"
            "return;"
            "}"
            "%1"
            "})();"
        ).arg(action);

    mediaFrame->ExecuteJavaScript(
        script.toStdString(),
        mediaFrame->GetURL(),
        0
    );

    qDebug() << "MPRIS COMMAND SENT:" << command;
}


    CefRefPtr<CefLifeSpanHandler>
    GetLifeSpanHandler() override
    {
        return this;
    }

    CefRefPtr<CefLoadHandler>
    GetLoadHandler() override
    {
        return this;
    }

    CefRefPtr<CefRequestHandler>
    GetRequestHandler() override
    {
        return this;
    }

    CefRefPtr<CefDisplayHandler>
    GetDisplayHandler() override
    {
        return this;
    }

    bool OnConsoleMessage(
        CefRefPtr<CefBrowser> browser,
        cef_log_severity_t level,
        const CefString &message,
        const CefString &source,
        int line) override
    {
        Q_UNUSED(browser);

        qDebug().noquote()
            << "[CEF JS]"
            << QString::fromStdString(message.ToString())
            << "|"
            << QString::fromStdString(source.ToString())
            << ":"
            << line;

        return false;
    }

    void OnAfterCreated(
        CefRefPtr<CefBrowser> browser) override;

    void OnBeforeClose(
        CefRefPtr<CefBrowser> browser) override;

    bool OnBeforeBrowse(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool userGesture,
        bool isRedirect) override
    {
        if (messageRouter) {
            messageRouter->OnBeforeBrowse(browser, frame);
        }

        return false;
    }

    void OnRenderProcessTerminated(
        CefRefPtr<CefBrowser> browser,
        TerminationStatus status,
        int errorCode,
        const CefString &errorString) override
    {
        qWarning() << "CEF RENDER PROCESS TERMINATED:"
                   << errorCode
                   << QString::fromStdString(
                          errorString.ToString());

        if (messageRouter) {
            messageRouter->OnRenderProcessTerminated(browser);
        }
    }

    bool OnProcessMessageReceived(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefProcessId sourceProcess,
        CefRefPtr<CefProcessMessage> message) override
    {
        if (messageRouter) {
            return messageRouter->OnProcessMessageReceived(
                browser,
                frame,
                sourceProcess,
                message
            );
        }

        return false;
    }

    // JavaScript -> C++ query handler.
    bool OnQuery(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    int64_t queryId,
    const CefString &request,
    bool persistent,
    CefRefPtr<Callback> callback) override
{
    Q_UNUSED(browser);
    qDebug() << "MPRIS FRAME:"
         << frame->GetIdentifier().ToString()
         << QString::fromStdString(frame->GetURL().ToString());
    Q_UNUSED(queryId);
    Q_UNUSED(persistent);

    QString requestText =
        QString::fromStdString(request.ToString());

    qDebug().noquote()
        << "CEF JS QUERY RECEIVED:"
        << requestText.left(500);

    if (requestText == "ping") {
        qDebug() << "CEF JS QUERY RECEIVED: ping";
        callback->Success("pong");
        return true;
    }

    QJsonParseError parseError;

    QJsonDocument document =
        QJsonDocument::fromJson(
            requestText.toUtf8(),
            &parseError
        );

    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        qWarning() << "MPRIS: Invalid JSON:"
                   << parseError.errorString();

        callback->Failure(400, "Invalid JSON");
        return true;
    }

    QJsonObject data = document.object();

    mediaFrame = frame;

    QString title = data.value("title").toString();
    QString artist = data.value("artist").toString();
    QString artUrl = data.value("artUrl").toString();

    QString status = data.value("status").toString();

    qDebug().noquote()
        << "MPRIS MEDIA DATA:"
        << "title =" << title
        << "| artist =" << artist
        << "| status =" << status;

    double currentTime =
        data.value("currentTime").toDouble();

    double duration =
        data.value("duration").toDouble();

    if (!mprisController) {
        callback->Failure(500, "MPRIS controller unavailable");
        return true;
    }

    QVariantMap metadata;

    metadata.insert(
        "mpris:trackid",
        QVariant::fromValue(
            QDBusObjectPath("/org/mpris/MediaPlayer2/track/current")
        )
    );

    if (!title.isEmpty())
        metadata.insert("xesam:title", title);

    if (!artist.isEmpty())
        metadata.insert("xesam:artist", QStringList{artist});

    if (!artUrl.isEmpty())
        metadata.insert("mpris:artUrl", artUrl);

    if (duration > 0) {
        metadata.insert(
            "mpris:length",
            QVariant::fromValue(
                static_cast<qlonglong>(duration * 1000000.0)
            )
        );
    }

    mprisController->setMetadata(metadata);

    if (status == "Playing" ||
        status == "Paused" ||
        status == "Stopped") {
        mprisController->setPlaybackStatus(status);
    }

    mprisController->setPosition(
        static_cast<qlonglong>(currentTime * 1000000.0)
    );

    callback->Success("metadata updated");

    return true;
}

    void OnLoadStart(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        TransitionType transitionType) override
    {
        if (frame->IsMain()) {
            qDebug() << "MAIN PAGE LOAD STARTED:"
                     << QString::fromStdString(
                            frame->GetURL().ToString());
        }
    }



void OnLoadEnd(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    int httpStatusCode) override
{
    Q_UNUSED(browser);

    if (frame->IsMain()) {
        qDebug() << "MAIN PAGE LOADED:"
                 << QString::fromStdString(
                        frame->GetURL().ToString())
                 << "HTTP:"
                 << httpStatusCode;
    }

    const char *monitorScript = R"JS(
(function() {

    if (window.__deezerDesktopMonitorStarted)
        return;

    window.__deezerDesktopMonitorStarted = true;

    const frameUrl = location.href;

    console.log(
        "DEEZER MONITOR STARTED IN FRAME:",
        frameUrl
    );

    let currentMedia = null;

    function getMeta(property) {
        const element = document.querySelector(
            'meta[property="' + property + '"]'
        );

        return element ? element.content : "";
    }

    function sendMediaData(media) {

        const title =
            getMeta("og:title") ||
            document.title ||
            "";

        const artist = getMeta("music:musician");
        const artUrl = getMeta("og:image");

        let status = "Stopped";

        if (!media.paused && !media.ended)
            status = "Playing";
        else if (media.paused && !media.ended)
            status = "Paused";

        const data = {
            title: title,
            artist: artist,
            artUrl: artUrl,
            status: status,
            currentTime: media.currentTime || 0,
            duration: Number.isFinite(media.duration)
                ? media.duration
                : 0,
            frameUrl: frameUrl
        };

        if (typeof window.cefQuery !== "function")
            return;

        window.cefQuery({
            request: JSON.stringify(data),
            onSuccess: function(response) {},
            onFailure: function(code, message) {
                console.error(
                    "DEEZER QUERY ERROR:",
                    code,
                    message
                );
            }
        });
    }

    function monitor() {

        const mediaElements =
            document.querySelectorAll("audio, video");

        if (mediaElements.length === 0) {
            if (!window.__deezerNoMediaLogged) {
                console.log(
                    "DEEZER: No media in frame:",
                    frameUrl
                );

                window.__deezerNoMediaLogged = true;
            }

            return;
        }

        const media = mediaElements[0];

        if (media !== currentMedia) {

            currentMedia = media;

            console.log(
                "DEEZER: MEDIA ELEMENT FOUND!",
                frameUrl,
                media.tagName
            );

            [
                "play",
                "pause",
                "ended",
                "loadedmetadata",
                "durationchange",
                "timeupdate"
            ].forEach(function(eventName) {

                media.addEventListener(
                    eventName,
                    function() {
                        sendMediaData(media);
                    }
                );

            });
        }

        sendMediaData(media);
    }

    monitor();

    setInterval(monitor, 1000);

})();
)JS";

    frame->ExecuteJavaScript(
        monitorScript,
        frame->GetURL(),
        0
    );

    if (frame->IsMain()) {
        qDebug() << "CEF MEDIA MONITOR INJECTED INTO MAIN FRAME";
    }
    else {
        qDebug() << "CEF MEDIA MONITOR INJECTED INTO SUBFRAME:"
                 << QString::fromStdString(
                        frame->GetURL().ToString());
    }
}


    void OnLoadError(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        ErrorCode errorCode,
        const CefString &errorText,
        const CefString &failedUrl) override
    {
        if (frame->IsMain()) {
            qWarning() << "MAIN PAGE LOAD FAILED:"
                       << QString::fromStdString(
                              failedUrl.ToString())
                       << "ERROR CODE:"
                       << errorCode
                       << "ERROR TEXT:"
                       << QString::fromStdString(
                              errorText.ToString());
        }
    }

private:
    QPointer<MainWindow> mainWindow;
    MprisController *mprisController = nullptr;

    CefRefPtr<CefFrame> mediaFrame;

    CefRefPtr<CefMessageRouterBrowserSide>
        messageRouter;

    IMPLEMENT_REFCOUNTING(DeezerClient);
};

// --------------------------------------------------
// Native X11 browser container
// --------------------------------------------------

class BrowserWidget : public QWidget
{
public:
    explicit BrowserWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_DontCreateNativeAncestors);

        display = XOpenDisplay(nullptr);

        if (!display) {
            qCritical() << "Failed to open X11 display.";
        }

        setMinimumSize(600, 400);
    }

    ~BrowserWidget() override
    {
        if (display) {
            XCloseDisplay(display);
        }
    }

    void setBrowser(CefRefPtr<CefBrowser> browser)
    {
        cefBrowser = browser;
        resizeBrowser();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        resizeBrowser();
    }

private:
    void resizeBrowser()
    {
        if (!display || !cefBrowser || !cefBrowser->IsValid())
            return;

        CefWindowHandle handle =
            cefBrowser->GetHost()->GetWindowHandle();

        if (!handle)
            return;

        XMoveResizeWindow(
            display,
            static_cast<Window>(handle),
            0,
            0,
            static_cast<unsigned int>(width()),
            static_cast<unsigned int>(height())
        );

        XFlush(display);
    }

    Display *display = nullptr;
    CefRefPtr<CefBrowser> cefBrowser;
};

// --------------------------------------------------
// Main application window
// --------------------------------------------------

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(
    AudioOutputController *audioController,
    QWidget *parent = nullptr
)
    : QMainWindow(parent),
      audioOutputController(audioController)
    {
        setWindowTitle("Deezer Desktop");

        QSettings settings;

        bool rememberSize =
            settings.value("window/rememberSize", true).toBool();

        QByteArray savedGeometry =
            settings.value("window/geometry").toByteArray();

        if (rememberSize && !savedGeometry.isEmpty()) {
            restoreGeometry(savedGeometry);
        }
        else {
            resize(1100, 750);
        }

        applyTheme();

        browserWidget = new BrowserWidget(this);
        setCentralWidget(browserWidget);

        // Settings menu
        QMenu *settingsMenu = menuBar()->addMenu("Ayarlar");

        QAction *settingsAction =
            settingsMenu->addAction("Tercihler...");

        connect(
            settingsAction,
            &QAction::triggered,
            this,
            [this]() {
                SettingsDialog dialog(audioOutputController, this);

                if (dialog.exec() == QDialog::Accepted)
                    applyTheme();
            }
        );
    }

    void createBrowser(CefRefPtr<CefClient> client)
    {
        qDebug() << "CEF CREATE BROWSER STARTED";

        if (browserCreated) {
            qDebug() << "CEF BROWSER ALREADY CREATED";
            return;
        }

        qDebug() << "QT BROWSER WIDGET SIZE:"
                 << browserWidget->width()
                 << "x"
                 << browserWidget->height();

        qDebug() << "QT BROWSER WIDGET ID:"
                 << browserWidget->winId();

        browserCreated = true;

        CefWindowInfo windowInfo;

        windowInfo.SetAsChild(
            static_cast<CefWindowHandle>(browserWidget->winId()),
            CefRect(
                0,
                0,
                browserWidget->width(),
                browserWidget->height()
            )
        );

        CefBrowserSettings browserSettings;

        qDebug() << "CEF CREATE BROWSER REQUEST";

        bool success = CefBrowserHost::CreateBrowser(
            windowInfo,
            client,
            "https://www.deezer.com/",
            browserSettings,
            nullptr,
            nullptr
        );

        qDebug() << "CEF CREATE BROWSER RESULT:" << success;

        if (!success) {
            qCritical() << "CEF browser creation failed.";
            browserCreated = false;
        }
    }

    void setBrowser(CefRefPtr<CefBrowser> browser)
    {
        cefBrowser = browser;
        browserWidget->setBrowser(browser);

        qDebug() << "Deezer browser created.";
    }

    void browserClosed()
    {
        cefBrowser = nullptr;
        browserWidget->setBrowser(nullptr);

        allowClose = true;
        close();
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
        if (allowClose) {
            saveWindowGeometry();
            event->accept();
            return;
        }

        if (cefBrowser && cefBrowser->IsValid()) {
            event->ignore();

            if (!closeRequested) {
                closeRequested = true;
                cefBrowser->GetHost()->CloseBrowser(true);
            }

            return;
        }

        saveWindowGeometry();
        event->accept();
    }

private:
    AudioOutputController *audioOutputController = nullptr;

    BrowserWidget *browserWidget = nullptr;
    CefRefPtr<CefBrowser> cefBrowser;

    bool browserCreated = false;
    bool closeRequested = false;
    bool allowClose = false;

    void saveWindowGeometry()
    {
        QSettings settings;

        if (settings.value("window/rememberSize", true).toBool()) {
            settings.setValue("window/geometry", saveGeometry());
        }
        else {
            settings.remove("window/geometry");
        }

        settings.sync();
    }

    void applyTheme()
    {
        QSettings settings;

        QString theme =
            settings.value("appearance/theme", "system").toString();

        if (theme == "system") {
            QApplication::setPalette(
                QApplication::style()->standardPalette()
            );

            return;
        }

        QPalette palette;

        if (theme == "dark") {
            palette.setColor(QPalette::Window, QColor(35, 35, 35));
            palette.setColor(QPalette::WindowText, QColor(235, 235, 235));
            palette.setColor(QPalette::Base, QColor(25, 25, 25));
            palette.setColor(QPalette::AlternateBase, QColor(45, 45, 45));
            palette.setColor(QPalette::Text, QColor(235, 235, 235));
            palette.setColor(QPalette::Button, QColor(45, 45, 45));
            palette.setColor(QPalette::ButtonText, QColor(235, 235, 235));
            palette.setColor(QPalette::Highlight, QColor(110, 70, 190));
            palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
            palette.setColor(QPalette::PlaceholderText, QColor(150, 150, 150));
        }
        else {
            palette.setColor(QPalette::Window, QColor(245, 245, 245));
            palette.setColor(QPalette::WindowText, QColor(25, 25, 25));
            palette.setColor(QPalette::Base, QColor(255, 255, 255));
            palette.setColor(QPalette::AlternateBase, QColor(240, 240, 240));
            palette.setColor(QPalette::Text, QColor(25, 25, 25));
            palette.setColor(QPalette::Button, QColor(235, 235, 235));
            palette.setColor(QPalette::ButtonText, QColor(25, 25, 25));
            palette.setColor(QPalette::Highlight, QColor(110, 70, 190));
            palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
            palette.setColor(QPalette::PlaceholderText, QColor(120, 120, 120));
        }

        QApplication::setPalette(palette);
    }
};
// --------------------------------------------------
// Browser lifecycle callbacks
// --------------------------------------------------




void DeezerClient::OnAfterCreated(
    CefRefPtr<CefBrowser> browser)
{
    qDebug() << "========== CEF BROWSER DIAGNOSTICS ==========";

    qDebug() << "BROWSER VALID:" << browser->IsValid();
    qDebug() << "BROWSER ID:" << browser->GetIdentifier();
    qDebug() << "BROWSER LOADING:" << browser->IsLoading();

    // Create browser-side message router.
    CefMessageRouterConfig config;

    messageRouter =
        CefMessageRouterBrowserSide::Create(config);

    if (messageRouter) {
        bool added = messageRouter->AddHandler(this, false);

        qDebug() << "CEF BROWSER MESSAGE ROUTER CREATED:"
                 << added;
    }
    else {
        qCritical() << "CEF BROWSER MESSAGE ROUTER CREATION FAILED";
    }

    CefRefPtr<CefFrame> mainFrame = browser->GetMainFrame();

    qDebug() << "MAIN FRAME EXISTS:" << (mainFrame != nullptr);

    if (mainFrame) {
        qDebug() << "MAIN FRAME VALID:" << mainFrame->IsValid();

        qDebug() << "MAIN FRAME URL BEFORE:"
                 << QString::fromStdString(
                        mainFrame->GetURL().ToString());

        qDebug() << "MANUALLY LOADING DEEZER";

        mainFrame->LoadURL("https://www.deezer.com/");

        qDebug() << "MAIN FRAME URL AFTER:"
                 << QString::fromStdString(
                        mainFrame->GetURL().ToString());
    }

    qDebug() << "==============================================";

    QPointer<MainWindow> window = mainWindow;

    if (!window)
        return;

    QMetaObject::invokeMethod(
        window.data(),
        [window, browser]() {
            if (window)
                window->setBrowser(browser);
        },
        Qt::QueuedConnection
    );
}


void DeezerClient::OnBeforeClose(
    CefRefPtr<CefBrowser> browser)
{
    qDebug() << "CEF BROWSER CLOSING:"
             << browser->GetIdentifier();

    if (messageRouter) {
        messageRouter->OnBeforeClose(browser);
        messageRouter->RemoveHandler(this);
        messageRouter = nullptr;

        qDebug() << "CEF BROWSER MESSAGE ROUTER DESTROYED";
    }

    QPointer<MainWindow> window = mainWindow;

    if (!window)
        return;

    QMetaObject::invokeMethod(
        window.data(),
        [window]() {
            if (window)
                window->browserClosed();
        },
        Qt::QueuedConnection
    );
}



// --------------------------------------------------
// Main
// --------------------------------------------------



int main(int argc, char *argv[])
{
    // This implementation uses X11 child windows.
    qputenv("QT_QPA_PLATFORM", "xcb");

    // Application identity must be set before QSettings is used.
    QApplication::setApplicationName("Deezer Desktop");
    QApplication::setOrganizationName("DeezerDesktop");

    // Read hardware acceleration preference before CEF processes start.
    QSettings appSettings;

    bool hardwareAccelerationEnabled =
        appSettings.value(
            "performance/hardwareAcceleration",
            true
        ).toBool();

    qDebug() << "HARDWARE ACCELERATION SETTING:"
             << hardwareAccelerationEnabled;

    CefMainArgs mainArgs(argc, argv);

    CefRefPtr<DeezerCefApp> cefApp =
        new DeezerCefApp();

    cefApp->setHardwareAccelerationEnabled(
        hardwareAccelerationEnabled
    );

    int exitCode = CefExecuteProcess(
        mainArgs,
        cefApp,
        nullptr
    );

    if (exitCode >= 0)
        return exitCode;

    QApplication app(argc, argv);

MprisController mprisController;
AudioOutputController audioOutputController;

// Audio output device discovery
QObject::connect(
    &audioOutputController,
    &AudioOutputController::devicesUpdated,
    &app,
    [](const QVariantList &devices, const QString &defaultSink) {
        qDebug() << "=== AUDIO OUTPUT DEVICES ===";
        qDebug() << "Default sink:" << defaultSink;

        for (const QVariant &item : devices) {
            QVariantMap device = item.toMap();

            qDebug() << "Device:"
                     << device["description"].toString();

            qDebug() << "Sink name:"
                     << device["name"].toString();

            qDebug() << "Sample rate:"
                     << device["sampleRate"].toInt();

            qDebug() << "Channels:"
                     << device["channels"].toInt();

            qDebug() << "Default:"
                     << device["isDefault"].toBool();
        }

        qDebug() << "============================";
    }
);

QObject::connect(
    &audioOutputController,
    &AudioOutputController::errorOccurred,
    &app,
    [](const QString &message) {
        qWarning() << "Audio output error:" << message;
    }
);

    // Create the Qt timer only after QApplication.
    CefMessagePump messagePump;
    cefApp->setMessagePump(&messagePump);

    CefSettings settings;

    settings.no_sandbox = 1;
    settings.external_message_pump = 1;
    settings.multi_threaded_message_loop = 0;
    settings.windowless_rendering_enabled = 0;

    QString applicationDirectory =
        QCoreApplication::applicationDirPath();

    CefString(&settings.resources_dir_path) =
        applicationDirectory.toStdString();

    CefString(&settings.locales_dir_path) =
        (applicationDirectory + "/locales").toStdString();

    if (!CefInitialize(
            mainArgs,
            settings,
            cefApp,
            nullptr))
    {
        qCritical() << "CEF initialization failed.";
        return 1;
    }

    MainWindow window(&audioOutputController);

CefRefPtr<DeezerClient> client =
    new DeezerClient(&window, &mprisController);

// MPRIS media commands
QObject::connect(
    &mprisController,
    &MprisController::playRequested,
    [&client]() {
        client->executeMediaCommand("play");
    }
);

QObject::connect(
    &mprisController,
    &MprisController::pauseRequested,
    [&client]() {
        client->executeMediaCommand("pause");
    }
);

QObject::connect(
    &mprisController,
    &MprisController::playPauseRequested,
    [&client]() {
        client->executeMediaCommand("playpause");
    }
);

QObject::connect(
    &mprisController,
    &MprisController::stopRequested,
    [&client]() {
        client->executeMediaCommand("stop");
    }
);

window.show();

    QTimer::singleShot(
        0,
        &window,
        [&window, client]() {
            window.createBrowser(client);
        }
    );

    int result = app.exec();

    // All browser windows must be closed before shutdown.
    CefShutdown();

    return result;
}

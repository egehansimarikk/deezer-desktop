
#include "settingsdialog.h"
#include <QSettings>
#include "include/cef_command_line.h"

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
#include <QDateTime>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QCryptographicHash>

#include <X11/Xlib.h>

// Prevent X11 macro from conflicting with CEF callback methods.
#undef Success

#include <algorithm>
#include <cstdint>
#include <climits>
#include <vector>

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
        if (delayMs == INT_MAX && timer.isActive())
            return;

        timer.stop();

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
        Q_UNUSED(processType);

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
        Q_UNUSED(queryId);
        Q_UNUSED(persistent);

        const QString actualFrameId = QString::fromStdString(
            frame->GetIdentifier().ToString()
        );
        const QString actualFrameUrl = QString::fromStdString(
            frame->GetURL().ToString()
        );

        QString requestText =
            QString::fromStdString(request.ToString());

        const bool isDiagnosticRequest =
            requestText.contains("\"type\":\"media-diagnostic\"");

        if (!isDiagnosticRequest) {
            qDebug() << "CEF QUERY FRAME:"
                     << actualFrameId
                     << actualFrameUrl;

            qDebug().noquote()
                << "CEF JS QUERY RECEIVED:"
                << requestText.left(500);
        }

        if (requestText == "ping") {
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

        const QString reportedFrameUrl =
            data.value("frameUrl").toString();

        if (!reportedFrameUrl.isEmpty() &&
            reportedFrameUrl != actualFrameUrl) {
            const QString mismatchKey =
                actualFrameId + QChar('\n') +
                reportedFrameUrl + QChar('\n') +
                actualFrameUrl;

            if (!warnedFrameUrlMismatches.contains(mismatchKey)) {
                warnedFrameUrlMismatches.insert(mismatchKey);
                qWarning().noquote()
                    << "CEF FRAME URL MISMATCH:"
                    << "frameId =" << actualFrameId
                    << "| JSON frameUrl =" << reportedFrameUrl
                    << "| actual frameUrl =" << actualFrameUrl;
            }
        }

        if (data.value("type").toString() == "media-diagnostic") {
            const CefRefPtr<CefFrame> parentFrame = frame->GetParent();
            const QString parentFrameId = parentFrame
                ? QString::fromStdString(
                      parentFrame->GetIdentifier().ToString()
                  )
                : QStringLiteral("<none>");

            if (!diagnosticFrameIdsSeen.contains(actualFrameId)) {
                diagnosticFrameIdsSeen.insert(actualFrameId);
                qDebug().noquote()
                    << "[CEF DIAGNOSTIC FRAME QUERY SEEN]"
                    << "frameId =" << actualFrameId
                    << "| role =" << (frame->IsMain() ? "main" : "subframe")
                    << "| parentFrameId =" << parentFrameId
                    << "| frameName ="
                    << QString::fromStdString(frame->GetName().ToString())
                    << "| actual frameUrl =" << actualFrameUrl;
            }

            logFrameInventory(
                browser,
                !lastFrameInventoryIds.contains(actualFrameId)
            );

            qDebug().noquote()
                << "[CEF MEDIA DIAGNOSTIC]"
                << "frameId =" << actualFrameId
                << "| actual frameUrl =" << actualFrameUrl
                << "| snapshot ="
                << QString::fromUtf8(
                       QJsonDocument(data).toJson(
                           QJsonDocument::Compact
                       )
                   );

            callback->Success("diagnostic recorded");
            return true;
        }

        mediaFrame = frame;

        QString title = data.value("title").toString();
        QString artist = data.value("artist").toString();
        QString album = data.value("album").toString();
        QString artUrl = data.value("artUrl").toString();
        qDebug().noquote()
            << "MPRIS MEDIA DATA:"
            << "title =" << title
            << "| artist =" << artist
            << "| album =" << album
            << "| artUrl =" << artUrl;

        if (!mprisController) {
            callback->Failure(500, "MPRIS controller unavailable");
            return true;
        }

        QVariantMap metadata;

        // Give every track a unique MPRIS track ID.
        // This prevents desktop clients from reusing cached artwork.
        QString trackIdentity =
            title + "\n" + artist + "\n" + album;

        QByteArray trackHash =
            QCryptographicHash::hash(
                trackIdentity.toUtf8(),
                QCryptographicHash::Sha1
            ).toHex();

        QString trackPath =
            "/org/mpris/MediaPlayer2/track/" +
            QString::fromLatin1(trackHash);

        metadata.insert(
            "mpris:trackid",
            QVariant::fromValue(
                QDBusObjectPath(trackPath)
            )
        );

        if (!title.isEmpty())
            metadata.insert("xesam:title", title);

        if (!artist.isEmpty())
            metadata.insert("xesam:artist", QStringList{artist});

        if (!album.isEmpty())
            metadata.insert("xesam:album", album);

        // Always update the artwork property.
        // An empty value prevents an old cover from being retained.
        metadata.insert("mpris:artUrl", artUrl);

        mprisController->setMetadata(metadata);

        callback->Success("metadata updated");

        return true;
    }

    void OnLoadStart(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        TransitionType transitionType) override
    {
        Q_UNUSED(browser);
        Q_UNUSED(transitionType);

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

    console.log(
        "DEEZER MONITOR STARTED IN FRAME:",
        location.href
    );

    let lastMetadataDebug = "";
    let lastSentTrack = "";
    let currentMedia = null;

    let lastSentMetadata = "";
    let trackChangeDetectedAt = 0;
    let fastPollUntil = 0;
    let fastPollTimer = null;
    let lastDiagnosticSignature = "";
    let lastDiagnosticSentAt = 0;

    function scheduleFastMetadataChecks() {

        fastPollUntil = Date.now() + 2000;

        if (fastPollTimer !== null)
            return;

        function checkForMetadataUpdate() {

            fastPollTimer = null;

            if (Date.now() >= fastPollUntil)
                return;

            sendMediaData(false);

            if (Date.now() < fastPollUntil) {
                fastPollTimer = setTimeout(
                    checkForMetadataUpdate,
                    100
                );
            }
        }

        fastPollTimer = setTimeout(
            checkForMetadataUpdate,
            100
        );
    }

    function getMediaSessionMetadata() {

        try {
            if (!navigator.mediaSession)
                return null;

            return navigator.mediaSession.metadata || null;
        }
        catch (error) {
            console.error(
                "MEDIA SESSION METADATA ERROR:",
                error
            );

            return null;
        }
    }

    function getArtworkUrl(metadata) {

        if (!metadata || !metadata.artwork)
            return "";

        const artwork = Array.from(metadata.artwork);

        if (artwork.length === 0)
            return "";

        // Prefer the largest available artwork image.
        artwork.sort(function(a, b) {

            function getSize(item) {
                if (!item.sizes)
                    return 0;

                const match = item.sizes.match(
                    /(\d+)x(\d+)/
                );

                if (!match)
                    return 0;

                return Number(match[1]) * Number(match[2]);
            }

            return getSize(b) - getSize(a);
        });

        for (const item of artwork) {

            if (!item.src)
                continue;

            try {
                const url = new URL(item.src, location.href);

                if (
                    url.protocol === "https:" ||
                    url.protocol === "http:"
                ) {
                    return url.href;
                }
            }
            catch (error) {
                console.warn(
                    "INVALID ARTWORK URL:",
                    item.src
                );
            }
        }

        return "";
    }

    function getPlaybackStatus(media) {

        try {
            if (navigator.mediaSession) {

                const playbackState =
                    navigator.mediaSession.playbackState;

                if (playbackState === "playing")
                    return "Playing";

                if (playbackState === "paused")
                    return "Paused";

                if (playbackState === "none")
                    return "Stopped";
            }
        }
        catch (error) {
            console.warn(
                "MEDIA SESSION STATE ERROR:",
                error
            );
        }

        if (media) {
            if (!media.paused && !media.ended)
                return "Playing";

            if (media.paused && !media.ended)
                return "Paused";
        }

        return "Stopped";
    }

    function getAccessibleMediaElements() {

        const mediaElements = new Set();
        const visitedRoots = new Set();

        function inspectRoot(root) {

            if (!root || visitedRoots.has(root))
                return;

            visitedRoots.add(root);

            if (typeof root.querySelectorAll !== "function")
                return;

            root.querySelectorAll("audio, video").forEach(function(element) {
                mediaElements.add(element);
            });

            root.querySelectorAll("*").forEach(function(element) {
                if (element.shadowRoot)
                    inspectRoot(element.shadowRoot);
            });
        }

        inspectRoot(document);
        return Array.from(mediaElements);
    }

    function sendFrameDiagnostics(mediaElements, sessionMetadata) {

        let mediaSessionAvailable = false;
        let playbackState = "unavailable";

        try {
            mediaSessionAvailable =
                typeof navigator.mediaSession !== "undefined" &&
                navigator.mediaSession !== null;

            if (mediaSessionAvailable) {
                playbackState =
                    navigator.mediaSession.playbackState || "none";
            }
        }
        catch (error) {
            playbackState = "error";
        }

        const media = Array.from(mediaElements).map(function(element) {
            return {
                tagName: element.tagName.toLowerCase(),
                currentSrc: element.currentSrc || "",
                paused: !!element.paused,
                ended: !!element.ended,
                currentTime: Number.isFinite(element.currentTime)
                    ? element.currentTime
                    : null,
                duration: Number.isFinite(element.duration)
                    ? element.duration
                    : null
            };
        });

        const snapshot = {
            type: "media-diagnostic",
            frameUrl: location.href,
            audioCount: media.filter(function(element) {
                return element.tagName === "audio";
            }).length,
            videoCount: media.filter(function(element) {
                return element.tagName === "video";
            }).length,
            mediaSessionAvailable: mediaSessionAvailable,
            playbackState: playbackState,
            title: sessionMetadata
                ? sessionMetadata.title || ""
                : "",
            artist: sessionMetadata
                ? sessionMetadata.artist || ""
                : "",
            media: media
        };

        // Time advances continuously, so exclude it from the change key.
        // Send a fresh time sample every five seconds for frames with media.
        const signature = JSON.stringify({
            frameUrl: snapshot.frameUrl,
            audioCount: snapshot.audioCount,
            videoCount: snapshot.videoCount,
            mediaSessionAvailable: snapshot.mediaSessionAvailable,
            playbackState: snapshot.playbackState,
            title: snapshot.title,
            artist: snapshot.artist,
            media: media.map(function(element) {
                return {
                    tagName: element.tagName,
                    currentSrc: element.currentSrc,
                    paused: element.paused,
                    ended: element.ended,
                    duration: element.duration
                };
            })
        });

        const now = Date.now();
        const changed = signature !== lastDiagnosticSignature;
        const mediaSampleDue =
            media.length > 0 && now - lastDiagnosticSentAt >= 5000;

        if (!changed && !mediaSampleDue)
            return;

        if (typeof window.cefQuery !== "function")
            return;

        lastDiagnosticSignature = signature;
        lastDiagnosticSentAt = now;

        window.cefQuery({
            request: JSON.stringify(snapshot),
            onSuccess: function(response) {},
            onFailure: function(code, message) {
                console.warn(
                    "[CEF MEDIA DIAGNOSTIC QUERY ERROR]",
                    code,
                    message,
                    location.href
                );
            }
        });
    }

    function sendMediaData(periodic) {

        const sessionMetadata =
            getMediaSessionMetadata();

        const mediaElements =
            document.querySelectorAll("audio, video");

        const media =
            mediaElements.length > 0
                ? mediaElements[0]
                : null;

        if (media && media !== currentMedia) {

            currentMedia = media;

            console.log(
                "DEEZER: MEDIA ELEMENT FOUND!",
                location.href,
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
                        if (eventName !== "timeupdate")
                            scheduleFastMetadataChecks();

                        sendMediaData(false);
                    }
                );

            });
        }

        // MediaSession metadata is the primary source.
        // Do not use page-level og:image as track artwork.
        const title = sessionMetadata
            ? sessionMetadata.title || ""
            : "";

        const artist = sessionMetadata
            ? sessionMetadata.artist || ""
            : "";

        const album = sessionMetadata
            ? sessionMetadata.album || ""
            : "";

        const artUrl =
            getArtworkUrl(sessionMetadata);

        sendFrameDiagnostics(
            getAccessibleMediaElements(),
            sessionMetadata
        );

        const status =
            getPlaybackStatus(media);

        const currentTime =
            media && Number.isFinite(media.currentTime)
                ? media.currentTime
                : 0;

        const duration =
            media && Number.isFinite(media.duration)
                ? media.duration
                : 0;

        const debugInfo = JSON.stringify({
            title: title,
            artist: artist,
            album: album,
            artUrl: artUrl,
            playbackState:
                navigator.mediaSession
                    ? navigator.mediaSession.playbackState
                    : "unavailable",
            mediaElementFound: !!media
        });

        if (debugInfo !== lastMetadataDebug) {
            console.log(
                "[ARTWORK DEBUG]",
                debugInfo
            );

            lastMetadataDebug = debugInfo;
        }

        // There is nothing useful to send until Deezer
        // provides actual track metadata.
        if (!title && !artist)
            return;

        const trackKey =
            title + "\n" + artist + "\n" + album;

        if (trackKey !== lastSentTrack) {

            console.log(
                "DEEZER TRACK CHANGED:",
                trackKey
            );

            lastSentTrack = trackKey;
            trackChangeDetectedAt = performance.now();
            scheduleFastMetadataChecks();
        }

        const metadataKey = JSON.stringify({
            title: title,
            artist: artist,
            album: album,
            artUrl: artUrl,
            status: status,
            duration: duration
        });

        const metadataChanged =
            metadataKey !== lastSentMetadata;

        if (!periodic && !metadataChanged)
            return;

        if (metadataChanged) {
            console.log(
                "[MEDIA METADATA FORWARD]",
                "trackChangeDelayMs =",
                trackChangeDetectedAt
                    ? Math.round(
                          performance.now() - trackChangeDetectedAt
                      )
                    : "unavailable",
                "| artUrl =",
                artUrl
            );
        }

        const data = {
            title: title,
            artist: artist,
            album: album,
            artUrl: artUrl,
            status: status,
            currentTime: currentTime,
            duration: duration,
            frameUrl: location.href
        };

        if (typeof window.cefQuery !== "function") {
            console.warn(
                "CEF QUERY IS NOT AVAILABLE:",
                location.href
            );

            return;
        }

        lastSentMetadata = metadataKey;

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

    // Run immediately and keep checking because Deezer
    // may update MediaSession metadata asynchronously.
    sendMediaData(true);

    setInterval(function() {
        sendMediaData(true);
    }, 1000);

})();
)JS";

        // Inject the media monitor.
        frame->ExecuteJavaScript(
            monitorScript,
            frame->GetURL(),
            0
        );

        // Test the CEF JavaScript query connection.
        frame->ExecuteJavaScript(
            R"JS(
            console.log(
                "[CEF TEST] Frame:",
                location.href,
                "| cefQuery:",
                typeof window.cefQuery
            );

            if (typeof window.cefQuery === "function") {
                window.cefQuery({
                    request: "ping",
                    onSuccess: function(response) {
                        console.log("[CEF TEST] Ping success:", response);
                    },
                    onFailure: function(code, message) {
                        console.error("[CEF TEST] Ping failed:", code, message);
                    }
                });
            }
            )JS",
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
        Q_UNUSED(browser);

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
    void logFrameInventory(
        CefRefPtr<CefBrowser> browser,
        bool forceRefresh)
    {
        if (!browser || !browser->IsValid())
            return;

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

        if (!forceRefresh &&
            lastFrameInventoryScanAtMs != 0 &&
            nowMs - lastFrameInventoryScanAtMs < 5000) {
            return;
        }

        lastFrameInventoryScanAtMs = nowMs;

        std::vector<CefString> frameIdentifiers;
        browser->GetFrameIdentifiers(frameIdentifiers);

        QSet<QString> currentFrameIds;
        QStringList frameEntries;

        for (const CefString &identifier : frameIdentifiers) {
            const QString frameId =
                QString::fromStdString(identifier.ToString());
            const CefRefPtr<CefFrame> listedFrame =
                browser->GetFrameByIdentifier(identifier);

            if (!listedFrame || !listedFrame->IsValid())
                continue;

            currentFrameIds.insert(frameId);

            const CefRefPtr<CefFrame> parentFrame =
                listedFrame->GetParent();
            const QString parentFrameId = parentFrame
                ? QString::fromStdString(
                      parentFrame->GetIdentifier().ToString()
                  )
                : QStringLiteral("<none>");
            const QString role = listedFrame->IsMain()
                ? QStringLiteral("main")
                : QStringLiteral("subframe");
            const QString frameName = QString::fromStdString(
                listedFrame->GetName().ToString()
            );
            const QString frameUrl = QString::fromStdString(
                listedFrame->GetURL().ToString()
            );

            frameEntries.append(
                QStringLiteral(
                    "frameId=%1 | role=%2 | name=%3 | parentFrameId=%4 | url=%5"
                ).arg(frameId, role, frameName, parentFrameId, frameUrl)
            );
        }

        frameEntries.sort();
        const QString inventorySignature = frameEntries.join(QChar('\n'));

        if (inventorySignature != lastFrameInventorySignature) {
            qDebug().noquote()
                << "[CEF FRAME INVENTORY] count ="
                << frameEntries.size();

            for (const QString &entry : frameEntries)
                qDebug().noquote() << "[CEF FRAME]" << entry;

            lastFrameInventorySignature = inventorySignature;
        }

        lastFrameInventoryIds = currentFrameIds;

        QStringList framesWithoutDiagnostic;
        for (const QString &frameId : currentFrameIds) {
            if (!diagnosticFrameIdsSeen.contains(frameId))
                framesWithoutDiagnostic.append(frameId);
        }
        framesWithoutDiagnostic.sort();

        const QString coverageState =
            QStringLiteral("%1/%2|%3")
                .arg(currentFrameIds.size() - framesWithoutDiagnostic.size())
                .arg(currentFrameIds.size())
                .arg(framesWithoutDiagnostic.join(","));

        if (coverageState != lastFrameDiagnosticCoverageState) {
            qDebug().noquote()
                << "[CEF FRAME DIAGNOSTIC COVERAGE] received ="
                << currentFrameIds.size() - framesWithoutDiagnostic.size()
                << "/" << currentFrameIds.size()
                << "| waitingFrameIds ="
                << (framesWithoutDiagnostic.isEmpty()
                        ? QStringLiteral("<none>")
                        : framesWithoutDiagnostic.join(","));

            lastFrameDiagnosticCoverageState = coverageState;
        }
    }

    QPointer<MainWindow> mainWindow;
    MprisController *mprisController = nullptr;

    CefRefPtr<CefFrame> mediaFrame;
    QSet<QString> warnedFrameUrlMismatches;
    QSet<QString> diagnosticFrameIdsSeen;
    QSet<QString> lastFrameInventoryIds;
    qint64 lastFrameInventoryScanAtMs = 0;
    QString lastFrameInventorySignature;
    QString lastFrameDiagnosticCoverageState;

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
    qputenv("QT_QPA_PLATFORM", "xcb");

    QApplication::setApplicationName("Deezer Desktop");
    QApplication::setOrganizationName("DeezerDesktop");

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

    CefShutdown();

    return result;
}

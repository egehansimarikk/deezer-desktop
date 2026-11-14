#include "mpriscontroller.h"

#include <QDebug>
#include <QtGlobal>
#include <QDBusError>

// --------------------------------------------------
// MPRIS root adaptor
// --------------------------------------------------

MprisRootAdaptor::MprisRootAdaptor(MprisController *controller)
    : QDBusAbstractAdaptor(controller),
      controller(controller)
{
}

bool MprisRootAdaptor::canQuit() const
{
    return true;
}

bool MprisRootAdaptor::canRaise() const
{
    return true;
}

bool MprisRootAdaptor::hasTrackList() const
{
    return false;
}

QString MprisRootAdaptor::identity() const
{
    return "Deezer Desktop";
}

QString MprisRootAdaptor::desktopEntry() const
{
    return "deezer-desktop";
}

QStringList MprisRootAdaptor::supportedUriSchemes() const
{
    return {"http", "https"};
}

QStringList MprisRootAdaptor::supportedMimeTypes() const
{
    return {"audio/mpeg", "audio/flac", "audio/ogg"};
}

void MprisRootAdaptor::Raise()
{
    controller->requestRaise();
}

void MprisRootAdaptor::Quit()
{
    controller->requestQuit();
}

// --------------------------------------------------
// MPRIS player adaptor
// --------------------------------------------------

MprisPlayerAdaptor::MprisPlayerAdaptor(MprisController *controller)
    : QDBusAbstractAdaptor(controller),
      controller(controller)
{
    connect(
        controller, &MprisController::playbackStatusChanged,
        this, &MprisPlayerAdaptor::playbackStatusChanged
    );

    connect(
        controller, &MprisController::loopStatusChanged,
        this, &MprisPlayerAdaptor::loopStatusChanged
    );

    connect(
        controller, &MprisController::shuffleChanged,
        this, &MprisPlayerAdaptor::shuffleChanged
    );

    connect(
        controller, &MprisController::metadataChanged,
        this, &MprisPlayerAdaptor::metadataChanged
    );

    connect(
        controller, &MprisController::volumeChanged,
        this, &MprisPlayerAdaptor::volumeChanged
    );
}

QString MprisPlayerAdaptor::playbackStatus() const
{
    return controller->playbackStatus();
}

QString MprisPlayerAdaptor::loopStatus() const
{
    return controller->loopStatus();
}

void MprisPlayerAdaptor::setLoopStatus(const QString &value)
{
    controller->setLoopStatus(value);
}

double MprisPlayerAdaptor::rate() const
{
    return 1.0;
}

bool MprisPlayerAdaptor::shuffle() const
{
    return controller->shuffle();
}

void MprisPlayerAdaptor::setShuffle(bool value)
{
    controller->setShuffle(value);
}

QVariantMap MprisPlayerAdaptor::metadata() const
{
    return controller->metadata();
}

double MprisPlayerAdaptor::volume() const
{
    return controller->volume();
}

void MprisPlayerAdaptor::setVolume(double value)
{
    controller->setVolume(value);
}

qlonglong MprisPlayerAdaptor::position() const
{
    return controller->position();
}

double MprisPlayerAdaptor::minimumRate() const
{
    return 1.0;
}

double MprisPlayerAdaptor::maximumRate() const
{
    return 1.0;
}

bool MprisPlayerAdaptor::canGoNext() const
{
    return true;
}

bool MprisPlayerAdaptor::canGoPrevious() const
{
    return true;
}

bool MprisPlayerAdaptor::canPlay() const
{
    return true;
}

bool MprisPlayerAdaptor::canPause() const
{
    return true;
}

bool MprisPlayerAdaptor::canSeek() const
{
    return true;
}

bool MprisPlayerAdaptor::canControl() const
{
    return true;
}

void MprisPlayerAdaptor::Next()
{
    emit controller->nextRequested();
}

void MprisPlayerAdaptor::Previous()
{
    emit controller->previousRequested();
}

void MprisPlayerAdaptor::Pause()
{
    emit controller->pauseRequested();
}

void MprisPlayerAdaptor::PlayPause()
{
    emit controller->playPauseRequested();
}

void MprisPlayerAdaptor::Stop()
{
    emit controller->stopRequested();
}

void MprisPlayerAdaptor::Play()
{
    emit controller->playRequested();
}

void MprisPlayerAdaptor::Seek(qlonglong offset)
{
    emit controller->seekRequested(offset);
}

void MprisPlayerAdaptor::SetPosition(
    const QDBusObjectPath &trackId,
    qlonglong position)
{
    Q_UNUSED(trackId);
    emit controller->setPositionRequested(position);
}

void MprisPlayerAdaptor::OpenUri(const QString &uri)
{
    emit controller->openUriRequested(uri);
}

// --------------------------------------------------
// MPRIS controller
// --------------------------------------------------

MprisController::MprisController(QObject *parent)
    : QObject(parent),
      connection(QDBusConnection::sessionBus())
{
    new MprisRootAdaptor(this);
    new MprisPlayerAdaptor(this);

    if (!connection.isConnected()) {
        qWarning() << "MPRIS: Session D-Bus connection failed.";
        return;
    }

    const QString serviceName =
        "org.mpris.MediaPlayer2.deezer_desktop";

    if (!connection.registerService(serviceName)) {
        qWarning() << "MPRIS: Service registration failed:"
                   << connection.lastError().message();
        return;
    }

    if (!connection.registerObject(
            "/org/mpris/MediaPlayer2",
            this,
            QDBusConnection::ExportAdaptors))
    {
        qWarning() << "MPRIS: Object registration failed:"
                   << connection.lastError().message();

        connection.unregisterService(serviceName);
        return;
    }

    qDebug() << "MPRIS: Service registered successfully.";
}

MprisController::~MprisController()
{
    connection.unregisterObject("/org/mpris/MediaPlayer2");
    connection.unregisterService(
        "org.mpris.MediaPlayer2.deezer_desktop"
    );
}

QString MprisController::playbackStatus() const
{
    return currentPlaybackStatus;
}

QString MprisController::loopStatus() const
{
    return currentLoopStatus;
}

void MprisController::setLoopStatus(const QString &value)
{
    if (value != "None" &&
        value != "Track" &&
        value != "Playlist")
        return;

    if (currentLoopStatus == value)
        return;

    currentLoopStatus = value;
    emit loopStatusChanged();
}

bool MprisController::shuffle() const
{
    return currentShuffle;
}

void MprisController::setShuffle(bool value)
{
    if (currentShuffle == value)
        return;

    currentShuffle = value;
    emit shuffleChanged();
}

QVariantMap MprisController::metadata() const
{
    return currentMetadata;
}

double MprisController::volume() const
{
    return currentVolume;
}

void MprisController::setVolume(double value)
{
    value = qBound(0.0, value, 1.0);

    if (qFuzzyCompare(currentVolume, value))
        return;

    currentVolume = value;
    emit volumeChanged();
}

qlonglong MprisController::position() const
{
    return currentPosition;
}

void MprisController::setPlaybackStatus(const QString &status)
{
    if (status != "Playing" &&
        status != "Paused" &&
        status != "Stopped")
        return;

    if (currentPlaybackStatus == status)
        return;

    currentPlaybackStatus = status;
    emit playbackStatusChanged();
}

void MprisController::setMetadata(const QVariantMap &metadata)
{
    if (currentMetadata == metadata)
        return;

    currentMetadata = metadata;
    emit metadataChanged();
}

void MprisController::setPosition(qlonglong position)
{
    currentPosition = qMax(0LL, position);
}

void MprisController::requestRaise()
{
    emit raiseRequested();
}

void MprisController::requestQuit()
{
    emit quitRequested();
}
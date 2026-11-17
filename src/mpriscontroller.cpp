#include "mpriscontroller.h"

#include <QDebug>
#include <QtGlobal>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QCoreApplication>
#include <QDateTime>
#include <QDBusReply>

namespace {

QVariant unwrapDBusVariant(QVariant value)
{
    if (value.metaType() == QMetaType::fromType<QDBusVariant>())
        value = value.value<QDBusVariant>().variant();
    return value;
}

bool decodeMetadataMap(
    const QVariant &input,
    QVariantMap *metadata,
    QString *error)
{
    if (!metadata) {
        if (error)
            *error = QStringLiteral("output map pointer is null");
        return false;
    }

    const QVariant value = unwrapDBusVariant(input);
    if (value.metaType() == QMetaType::fromType<QVariantMap>()) {
        *metadata = value.toMap();
        return true;
    }

    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
        const QDBusArgument argument = value.value<QDBusArgument>();
        if (argument.currentType() != QDBusArgument::MapType ||
            argument.currentSignature() != QStringLiteral("a{sv}")) {
            if (error) {
                *error = QStringLiteral("unexpected D-Bus type/signature: %1 / %2")
                             .arg(static_cast<int>(argument.currentType()))
                             .arg(argument.currentSignature());
            }
            return false;
        }

        // Qt 6.12 provides QDBusArgument's associative-container extraction
        // for QString -> QVariant maps; the QVariant reader unwraps each D-Bus
        // variant while preserving each contained value's Qt type.
        *metadata = qdbus_cast<QVariantMap>(argument);
        return true;
    }

    if (value.canConvert<QVariantMap>()) {
        *metadata = value.toMap();
        return true;
    }

    if (error) {
        *error = QStringLiteral("cannot convert QVariant type %1 to QVariantMap")
                     .arg(value.isValid() ? QString::fromLatin1(value.metaType().name())
                                          : QStringLiteral("<invalid>"));
    }
    return false;
}

} // namespace

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
        controller, &MprisController::positionSeeked,
        this, &MprisPlayerAdaptor::Seeked
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

    if (connection.interface()) {
        connect(
            connection.interface(),
            &QDBusConnectionInterface::serviceOwnerChanged,
            this,
            &MprisController::onBusServiceOwnerChanged
        );
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

    mprisObjectRegistered = true;

    qDebug() << "MPRIS: Service registered successfully.";
    discoverChromiumService();

    positionRefreshTimer.setInterval(1000);
    connect(&positionRefreshTimer, &QTimer::timeout, this, [this]() {
        requestChromiumPosition();
    });
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
    emitPlayerPropertiesChanged({
        {QStringLiteral("PlaybackStatus"), status}
    });
}

void MprisController::setMetadata(const QVariantMap &metadata)
{
    const QVariant oldTrackId = deezerMetadata.value(
        QStringLiteral("mpris:trackid")
    );
    deezerMetadata = metadata;

    if (oldTrackId != deezerMetadata.value(QStringLiteral("mpris:trackid"))) {
        hasChromiumPosition = false;
        chromiumPosition = 0;
        lastPositionSample = 0;
        lastPositionSampleAtMs = 0;
        setPosition(0);
        chromiumTrackLength = 0;
        updateCombinedMetadata();
    } else {
        updateCombinedMetadata();
    }
}

void MprisController::setChromiumMetadata(const QVariantMap &metadata)
{
    chromiumMetadata = metadata;
    QVariant length = chromiumMetadata.value(QStringLiteral("mpris:length"));
    if (length.metaType() == QMetaType::fromType<QDBusVariant>())
        length = length.value<QDBusVariant>().variant();
    if (length.isValid()) {
        bool ok = false;
        const qlonglong parsedLength = length.toLongLong(&ok);
        chromiumTrackLength = ok && parsedLength > 0 ? parsedLength : 0;
    } else {
        chromiumTrackLength = 0;
    }
    updateCombinedMetadata();
    if (chromiumMetadataMatchesDeezerTrack() &&
        chromiumPlaybackStatus != QStringLiteral("Stopped"))
        requestChromiumPosition();
}

bool MprisController::chromiumMetadataMatchesDeezerTrack() const
{
    auto rawStringValue = [](const QVariantMap &map, const QString &key) {
        QVariant value = map.value(key);
        if (value.metaType() == QMetaType::fromType<QDBusVariant>())
            value = value.value<QDBusVariant>().variant();
        return value.toString();
    };
    auto normalize = [](const QString &value) {
        // Current matching normalizes whitespace and case only; it does not
        // perform Unicode NFC/NFKC normalization or remove provider suffixes.
        return value.simplified().toCaseFolded();
    };
    const QString rawDeezerTitle = rawStringValue(deezerMetadata, QStringLiteral("xesam:title"));
    const QString rawChromiumTitle = rawStringValue(chromiumMetadata, QStringLiteral("xesam:title"));
    const QString rawDeezerAlbum = rawStringValue(deezerMetadata, QStringLiteral("xesam:album"));
    const QString rawChromiumAlbum = rawStringValue(chromiumMetadata, QStringLiteral("xesam:album"));
    const QString deezerTitle = normalize(rawDeezerTitle);
    const QString chromiumTitle = normalize(rawChromiumTitle);
    const QString deezerAlbum = normalize(rawDeezerAlbum);
    const QString chromiumAlbum = normalize(rawChromiumAlbum);

    auto rawArtistsValue = [](const QVariantMap &map) {
        QVariant value = map.value(QStringLiteral("xesam:artist"));
        if (value.metaType() == QMetaType::fromType<QDBusVariant>())
            value = value.value<QDBusVariant>().variant();
        QStringList rawArtists = value.toStringList();
        if (rawArtists.isEmpty() && value.canConvert<QString>())
            rawArtists = {value.toString()};
        return rawArtists;
    };
    auto artistsValue = [&normalize](const QStringList &rawArtists) {
        QStringList artists;
        for (const QString &rawArtist : rawArtists) {
            const QStringList parts = rawArtist.split(
                QChar(','), Qt::SkipEmptyParts
            );
            for (const QString &part : parts) {
                const QString normalized = normalize(part);
                if (!normalized.isEmpty())
                    artists.append(normalized);
            }
        }
        return artists;
    };

    const QStringList deezerArtists = artistsValue(rawArtistsValue(deezerMetadata));
    const QStringList chromiumArtists = artistsValue(rawArtistsValue(chromiumMetadata));

    if (deezerTitle.isEmpty() || chromiumTitle.isEmpty())
        return false;
    if (deezerArtists.isEmpty())
        return false;
    if (!deezerAlbum.isEmpty() && !chromiumAlbum.isEmpty() &&
        deezerAlbum != chromiumAlbum)
        return false;
    if (!chromiumArtists.isEmpty() && chromiumArtists != deezerArtists)
        return false;

    QStringList chromiumTitleForms{deezerTitle};
    const QString deezerArtistText = deezerArtists.join(QStringLiteral(", "));
    const QString combinedTitle =
        deezerTitle + QStringLiteral(" - ") + deezerArtistText;
    chromiumTitleForms.append(combinedTitle);
    chromiumTitleForms.append(combinedTitle + QStringLiteral(" - deezer"));

    if (!chromiumTitleForms.contains(chromiumTitle))
        return false;
    if (chromiumArtists.isEmpty() && chromiumTitle == deezerTitle)
        return false;
    return true;
}

void MprisController::updateCombinedMetadata()
{
    QVariantMap combined = deezerMetadata;
    if (chromiumMetadataMatchesDeezerTrack() && chromiumTrackLength > 0) {
        combined.insert(
            QStringLiteral("mpris:length"),
            QVariant::fromValue(chromiumTrackLength)
        );
    } else {
        combined.remove(QStringLiteral("mpris:length"));
    }

    if (currentMetadata == combined)
        return;

    currentMetadata = combined;
    emit metadataChanged();

    if (!mprisObjectRegistered)
        return;

    emitPlayerPropertiesChanged({
        {QStringLiteral("Metadata"), QVariant::fromValue(currentMetadata)}
    });
}

void MprisController::emitPlayerPropertiesChanged(
    const QVariantMap &changedProperties)
{
    if (!mprisObjectRegistered || changedProperties.isEmpty())
        return;

    QDBusMessage signal = QDBusMessage::createSignal(
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged")
    );
    signal << QStringLiteral("org.mpris.MediaPlayer2.Player")
           << changedProperties
           << QStringList{};

    if (!connection.send(signal)) {
        qWarning() << "MPRIS: PropertiesChanged signal failed:"
                   << connection.lastError().message();
    }
}

void MprisController::discoverChromiumService()
{
    if (!connection.interface())
        return;

    const QDBusReply<QStringList> namesReply =
        connection.interface()->registeredServiceNames();
    if (!namesReply.isValid()) {
        qWarning() << "MPRIS: Could not enumerate services:"
                   << namesReply.error().message();
        return;
    }

    const QString prefix = QStringLiteral("org.mpris.MediaPlayer2.chromium.instance");
    const uint appPid = static_cast<uint>(QCoreApplication::applicationPid());
    QStringList candidates;
    for (const QString &name : namesReply.value()) {
        if (!name.startsWith(prefix))
            continue;
        const QDBusReply<uint> pidReply = connection.interface()->servicePid(name);
        if (pidReply.isValid() && pidReply.value() == appPid)
            candidates.append(name);
    }
    candidates.sort();

    if (candidates.isEmpty()) {
        if (!chromiumServiceName.isEmpty())
            detachChromiumService();
        return;
    }

    const QString candidate = candidates.first();
    if (candidate != chromiumServiceName)
        attachChromiumService(candidate);
}

void MprisController::onBusServiceOwnerChanged(
    const QString &serviceName,
    const QString &oldOwner,
    const QString &newOwner)
{
    Q_UNUSED(oldOwner);
    Q_UNUSED(newOwner);
    if (!serviceName.startsWith(
            QStringLiteral("org.mpris.MediaPlayer2.chromium.instance")))
        return;

    if (serviceName == chromiumServiceName)
        detachChromiumService();
    discoverChromiumService();
}

void MprisController::attachChromiumService(const QString &serviceName)
{
    detachChromiumService();
    chromiumServiceName = serviceName;
    ++chromiumConnectionGeneration;
    qDebug() << "MPRIS: Attached Chromium service" << serviceName;

    const QString path = QStringLiteral("/org/mpris/MediaPlayer2");
    if (!connection.connect(
            serviceName, path,
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"),
            this,
            SLOT(onChromiumPropertiesChanged(QString,QVariantMap,QStringList)))) {
        qWarning() << "MPRIS: Could not subscribe to Chromium properties:" << connection.lastError().message();
    }
    if (!connection.connect(
            serviceName, path,
            QStringLiteral("org.mpris.MediaPlayer2.Player"),
            QStringLiteral("Seeked"),
            this,
            SLOT(onChromiumSeeked(qlonglong)))) {
        qWarning() << "MPRIS: Could not subscribe to Chromium Seeked:" << connection.lastError().message();
    }

    requestChromiumProperties();
}

void MprisController::detachChromiumService()
{
    ++chromiumConnectionGeneration;
    positionRefreshTimer.stop();
    positionRequestInFlight = false;
    if (!chromiumServiceName.isEmpty()) {
        connection.disconnect(
            chromiumServiceName,
            QStringLiteral("/org/mpris/MediaPlayer2"),
            QStringLiteral("org.freedesktop.DBus.Properties"),
            QStringLiteral("PropertiesChanged"),
            this,
            SLOT(onChromiumPropertiesChanged(QString,QVariantMap,QStringList)));
        connection.disconnect(
            chromiumServiceName,
            QStringLiteral("/org/mpris/MediaPlayer2"),
            QStringLiteral("org.mpris.MediaPlayer2.Player"),
            QStringLiteral("Seeked"),
            this,
            SLOT(onChromiumSeeked(qlonglong)));
    }
    chromiumServiceName.clear();
    chromiumMetadata.clear();
    chromiumTrackLength = 0;
    hasChromiumPosition = false;
    chromiumPosition = 0;
    lastPositionSample = 0;
    lastPositionSampleAtMs = 0;
    applyChromiumPlaybackStatus(QStringLiteral("Stopped"));
    setPosition(0);
    updateCombinedMetadata();
}

void MprisController::requestChromiumProperties()
{
    if (chromiumServiceName.isEmpty())
        return;
    const QString requestedService = chromiumServiceName;
    const quint64 generation = chromiumConnectionGeneration;
    QDBusMessage message = QDBusMessage::createMethodCall(
        requestedService,
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("GetAll"));
    message << QStringLiteral("org.mpris.MediaPlayer2.Player");
    auto *watcher = new QDBusPendingCallWatcher(
        connection.asyncCall(message, 1500), this
    );
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, requestedService, generation]() {
        QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();
        if (generation != chromiumConnectionGeneration ||
            requestedService != chromiumServiceName)
            return;
        if (reply.isError()) {
            qWarning() << "MPRIS: Chromium property read failed:"
                       << reply.error().message();
            return;
        }
        const QVariantMap properties = reply.value();
        QVariant status = properties.value(QStringLiteral("PlaybackStatus"));
        if (status.metaType() == QMetaType::fromType<QDBusVariant>())
            status = status.value<QDBusVariant>().variant();
        if (status.isValid())
            applyChromiumPlaybackStatus(status.toString());

        const bool metadataPresent = properties.contains(QStringLiteral("Metadata"));
        const QVariant rawMetadata = properties.value(QStringLiteral("Metadata"));
        QVariantMap metadataMap;
        QString metadataDecodeError;
        const bool metadataConvertible = metadataPresent &&
            decodeMetadataMap(rawMetadata, &metadataMap, &metadataDecodeError);
        if (metadataConvertible) {
            setChromiumMetadata(metadataMap);
        } else if (metadataPresent) {
            qWarning() << "MPRIS: Could not decode Chromium Metadata from GetAll:"
                       << metadataDecodeError;
        }

        QVariant position = properties.value(QStringLiteral("Position"));
        if (position.metaType() == QMetaType::fromType<QDBusVariant>())
            position = position.value<QDBusVariant>().variant();
        bool ok = false;
        const qlonglong value = position.toLongLong(&ok);
        if (ok)
            applyChromiumPosition(value, false);
    });
}

void MprisController::requestChromiumPosition()
{
    if (chromiumServiceName.isEmpty() || positionRequestInFlight)
        return;
    positionRequestInFlight = true;
    const QString requestedService = chromiumServiceName;
    const quint64 generation = chromiumConnectionGeneration;
    const QVariantMap requestedChromiumMetadata = chromiumMetadata;
    const QVariant requestedDeezerTrackId = deezerMetadata.value(
        QStringLiteral("mpris:trackid")
    );
    QDBusMessage message = QDBusMessage::createMethodCall(
        requestedService,
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Get"));
    message << QStringLiteral("org.mpris.MediaPlayer2.Player")
            << QStringLiteral("Position");
    auto *watcher = new QDBusPendingCallWatcher(
        connection.asyncCall(message, 1500), this
    );
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, requestedService, generation,
             requestedChromiumMetadata, requestedDeezerTrackId]() {
        QDBusPendingReply<QVariant> reply = *watcher;
        watcher->deleteLater();
        if (generation != chromiumConnectionGeneration ||
            requestedService != chromiumServiceName)
            return;
        positionRequestInFlight = false;
        if (requestedChromiumMetadata != chromiumMetadata ||
            requestedDeezerTrackId != deezerMetadata.value(
                QStringLiteral("mpris:trackid")
            )) {
            if (chromiumPlaybackStatus != QStringLiteral("Stopped"))
                requestChromiumPosition();
            return;
        }
        if (reply.isError()) {
            qWarning() << "MPRIS: Chromium Position read failed from"
                       << requestedService << ":" << reply.error().message();
            return;
        }
        QVariant value = reply.value();
        if (value.metaType() == QMetaType::fromType<QDBusVariant>())
            value = value.value<QDBusVariant>().variant();
        bool ok = false;
        const qlonglong position = value.toLongLong(&ok);
        if (ok) {
            applyChromiumPosition(position, true);
        } else {
            qWarning() << "MPRIS: Chromium Position reply was not an integer; type:"
                       << value.metaType().name();
        }
    });
}

void MprisController::onChromiumPropertiesChanged(
    const QString &interfaceName,
    const QVariantMap &changedProperties,
    const QStringList &invalidatedProperties)
{
    const bool metadataPresent = changedProperties.contains(QStringLiteral("Metadata"));
    const QVariant rawMetadata = changedProperties.value(QStringLiteral("Metadata"));
    if (interfaceName != QStringLiteral("org.mpris.MediaPlayer2.Player"))
        return;

    if (invalidatedProperties.contains(QStringLiteral("Metadata"))) {
        chromiumMetadata.clear();
        chromiumTrackLength = 0;
        updateCombinedMetadata();
    }

    QVariant status = changedProperties.value(QStringLiteral("PlaybackStatus"));
    if (status.metaType() == QMetaType::fromType<QDBusVariant>())
        status = status.value<QDBusVariant>().variant();
    if (status.isValid())
        applyChromiumPlaybackStatus(status.toString());

    if (!metadataPresent)
        return;

    QVariantMap metadataMap;
    QString metadataDecodeError;
    const bool metadataConvertible = decodeMetadataMap(
        rawMetadata, &metadataMap, &metadataDecodeError
    );
    if (metadataConvertible) {
        setChromiumMetadata(metadataMap);
    } else {
        qWarning() << "MPRIS: Could not decode Chromium Metadata from PropertiesChanged:"
                   << metadataDecodeError;
    }
}

void MprisController::onChromiumSeeked(qlonglong position)
{
    if (chromiumServiceName.isEmpty() || !chromiumMetadataMatchesDeezerTrack())
        return;
    applyChromiumPosition(position, false);
    emit positionSeeked(qMax(0LL, position));
}

void MprisController::applyChromiumPlaybackStatus(const QString &status)
{
    if (status != QStringLiteral("Playing") &&
        status != QStringLiteral("Paused") &&
        status != QStringLiteral("Stopped"))
        return;
    chromiumPlaybackStatus = status;
    setPlaybackStatus(status);
    if (status == QStringLiteral("Playing")) {
        if (!positionRefreshTimer.isActive())
            positionRefreshTimer.start();
        requestChromiumPosition();
    } else {
        positionRefreshTimer.stop();
        requestChromiumPosition();
    }
}

void MprisController::applyChromiumPosition(qlonglong position, bool detectSeek)
{
    if (position < 0 || !chromiumMetadataMatchesDeezerTrack())
        return;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    bool unexpectedJump = false;
    if (detectSeek && hasChromiumPosition && lastPositionSampleAtMs > 0) {
        qlonglong expected = lastPositionSample;
        if (chromiumPlaybackStatus == QStringLiteral("Playing"))
            expected += qMax<qint64>(0, nowMs - lastPositionSampleAtMs) * 1000;
        unexpectedJump = qAbs(position - expected) > 3000000;
    }
    chromiumPosition = position;
    lastPositionSample = position;
    lastPositionSampleAtMs = nowMs;
    hasChromiumPosition = true;
    setPosition(position);
    if (unexpectedJump)
        emit positionSeeked(position);
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

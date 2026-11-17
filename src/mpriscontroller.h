
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QTimer>

class MprisController;

class MprisRootAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")

    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes CONSTANT)

public:
    explicit MprisRootAdaptor(MprisController *controller);

    bool canQuit() const;
    bool canRaise() const;
    bool hasTrackList() const;
    QString identity() const;
    QString desktopEntry() const;
    QStringList supportedUriSchemes() const;
    QStringList supportedMimeTypes() const;

public slots:
    void Raise();
    void Quit();

private:
    MprisController *controller;
};

class MprisPlayerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")

    Q_PROPERTY(QString PlaybackStatus READ playbackStatus NOTIFY playbackStatusChanged)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus NOTIFY loopStatusChanged)
    Q_PROPERTY(double Rate READ rate CONSTANT)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(QVariantMap Metadata READ metadata NOTIFY metadataChanged)
    Q_PROPERTY(double Volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ maximumRate CONSTANT)
    Q_PROPERTY(bool CanGoNext READ canGoNext CONSTANT)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious CONSTANT)
    Q_PROPERTY(bool CanPlay READ canPlay CONSTANT)
    Q_PROPERTY(bool CanPause READ canPause CONSTANT)
    Q_PROPERTY(bool CanSeek READ canSeek CONSTANT)
    Q_PROPERTY(bool CanControl READ canControl CONSTANT)

public:
    explicit MprisPlayerAdaptor(MprisController *controller);

    QString playbackStatus() const;
    QString loopStatus() const;
    void setLoopStatus(const QString &value);

    double rate() const;

    bool shuffle() const;
    void setShuffle(bool value);

    QVariantMap metadata() const;

    double volume() const;
    void setVolume(double value);

    qlonglong position() const;

    double minimumRate() const;
    double maximumRate() const;

    bool canGoNext() const;
    bool canGoPrevious() const;
    bool canPlay() const;
    bool canPause() const;
    bool canSeek() const;
    bool canControl() const;

public slots:
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong offset);
    void SetPosition(const QDBusObjectPath &trackId, qlonglong position);
    void OpenUri(const QString &uri);

signals:
    void Seeked(qlonglong position);

    void playbackStatusChanged();
    void loopStatusChanged();
    void shuffleChanged();
    void metadataChanged();
    void volumeChanged();

private:
    MprisController *controller;
};

class MprisController : public QObject
{
    Q_OBJECT

public:
    explicit MprisController(QObject *parent = nullptr);
    ~MprisController() override;

    QString playbackStatus() const;
    QString loopStatus() const;
    void setLoopStatus(const QString &value);

    bool shuffle() const;
    void setShuffle(bool value);

    QVariantMap metadata() const;

    double volume() const;
    void setVolume(double value);

    qlonglong position() const;

    void setPlaybackStatus(const QString &status);
    void setMetadata(const QVariantMap &metadata);
    void setPosition(qlonglong position);
    void setChromiumMetadata(const QVariantMap &metadata);

    void requestRaise();
    void requestQuit();

signals:
    void playbackStatusChanged();
    void positionSeeked(qlonglong position);
    void loopStatusChanged();
    void shuffleChanged();
    void metadataChanged();
    void volumeChanged();

    void nextRequested();
    void previousRequested();
    void pauseRequested();
    void playPauseRequested();
    void stopRequested();
    void playRequested();
    void seekRequested(qlonglong offset);
    void setPositionRequested(qlonglong position);
    void openUriRequested(const QString &uri);

    void raiseRequested();
    void quitRequested();

private:
    void discoverChromiumService();
    void attachChromiumService(const QString &serviceName);
    void detachChromiumService();
    void requestChromiumProperties();
    void requestChromiumPosition();
    void applyChromiumPlaybackStatus(const QString &status);
    void applyChromiumPosition(qlonglong position, bool detectSeek);
    void updateCombinedMetadata();
    bool chromiumMetadataMatchesDeezerTrack() const;
    void emitPlayerPropertiesChanged(const QVariantMap &changedProperties);

private slots:
    void onBusServiceOwnerChanged(
        const QString &serviceName,
        const QString &oldOwner,
        const QString &newOwner);
    void onChromiumPropertiesChanged(
        const QString &interfaceName,
        const QVariantMap &changedProperties,
        const QStringList &invalidatedProperties);
    void onChromiumSeeked(qlonglong position);

private:
    QDBusConnection connection;
    QTimer positionRefreshTimer;
    bool mprisObjectRegistered = false;
    QString chromiumServiceName;
    quint64 chromiumConnectionGeneration = 0;
    QVariantMap deezerMetadata;
    QVariantMap chromiumMetadata;
    qlonglong chromiumPosition = 0;
    qlonglong lastPositionSample = 0;
    qint64 lastPositionSampleAtMs = 0;
    bool hasChromiumPosition = false;
    bool positionRequestInFlight = false;
    QString chromiumPlaybackStatus = "Stopped";
    qlonglong chromiumTrackLength = 0;

    QString currentPlaybackStatus = "Stopped";
    QString currentLoopStatus = "None";

    bool currentShuffle = false;

    double currentVolume = 1.0;

    qlonglong currentPosition = 0;

    QVariantMap currentMetadata;
};

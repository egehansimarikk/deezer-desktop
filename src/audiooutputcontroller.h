
#ifndef AUDIOOUTPUTCONTROLLER_H
#define AUDIOOUTPUTCONTROLLER_H

#include <QObject>
#include <QVariantList>
#include <QString>

#include <pulse/pulseaudio.h>

class AudioOutputController : public QObject
{
    Q_OBJECT

public:
    explicit AudioOutputController(QObject *parent = nullptr);
    ~AudioOutputController() override;

    void refreshDevices();
    void setOutputDevice(const QString &sinkName);

signals:
    void devicesUpdated(
        const QVariantList &devices,
        const QString &defaultSink
    );

    void errorOccurred(const QString &message);

private:
    pa_threaded_mainloop *mainloop = nullptr;
    pa_context *context = nullptr;

    QVariantList devices;

    QString defaultSink;
    QString preferredSink;

    bool sinkListReady = false;
    bool serverInfoReady = false;
    bool sinkInputsQueryRunning = false;

    bool muteOperationRunning = false;
    bool unmuteOperationRunning = false;
    bool mutedByController = false;

    uint32_t mutedSinkInputIndex = PA_INVALID_INDEX;
    uint32_t pendingMuteSinkInputIndex = PA_INVALID_INDEX;
    uint32_t pendingMoveSinkInputIndex = PA_INVALID_INDEX;

    void requestRefreshLocked();
    void requestSinkInputsLocked();
    void emitDevicesIfReady();

    void applyOutputDeviceLocked(
        uint32_t sinkInputIndex,
        uint32_t currentSinkIndex,
        bool isCorked,
        bool isMuted
    );

    void unmuteStreamLocked(
        uint32_t sinkInputIndex
    );

    static void contextStateCallback(
        pa_context *context,
        void *userdata
    );

    static void sinkInfoCallback(
        pa_context *context,
        const pa_sink_info *info,
        int eol,
        void *userdata
    );

    static void serverInfoCallback(
        pa_context *context,
        const pa_server_info *info,
        void *userdata
    );

    static void sinkInputInfoCallback(
        pa_context *context,
        const pa_sink_input_info *info,
        int eol,
        void *userdata
    );

    static void subscriptionCallback(
        pa_context *context,
        pa_subscription_event_type_t type,
        uint32_t index,
        void *userdata
    );

    static void muteFinishedCallback(
        pa_context *context,
        int success,
        void *userdata
    );

    static void unmuteFinishedCallback(
        pa_context *context,
        int success,
        void *userdata
    );

    static void moveFinishedCallback(
        pa_context *context,
        int success,
        void *userdata
    );
};

#endif

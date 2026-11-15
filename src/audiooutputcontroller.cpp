
#include "audiooutputcontroller.h"

#include <QDebug>
#include <QSettings>
#include <QVariantMap>

#include <pulse/proplist.h>

AudioOutputController::AudioOutputController(QObject *parent)
    : QObject(parent)
{
    QSettings settings;

    preferredSink =
        settings.value("audio/outputDevice").toString();

    mainloop = pa_threaded_mainloop_new();

    if (!mainloop) {
        emit errorOccurred(
            "PulseAudio mainloop oluşturulamadı."
        );
        return;
    }

    pa_mainloop_api *api =
        pa_threaded_mainloop_get_api(mainloop);

    context = pa_context_new(api, "Deezer Desktop");

    if (!context) {
        emit errorOccurred(
            "PulseAudio context oluşturulamadı."
        );

        pa_threaded_mainloop_free(mainloop);
        mainloop = nullptr;
        return;
    }

    pa_context_set_state_callback(
        context,
        contextStateCallback,
        this
    );

    if (pa_threaded_mainloop_start(mainloop) < 0) {
        emit errorOccurred(
            "PulseAudio mainloop başlatılamadı."
        );

        pa_context_unref(context);
        context = nullptr;

        pa_threaded_mainloop_free(mainloop);
        mainloop = nullptr;
        return;
    }

    pa_threaded_mainloop_lock(mainloop);

    if (pa_context_connect(
            context,
            nullptr,
            PA_CONTEXT_NOFLAGS,
            nullptr
        ) < 0) {
        emit errorOccurred(
            QString("PulseAudio bağlantısı başarısız: %1")
                .arg(pa_strerror(pa_context_errno(context)))
        );
    }

    pa_threaded_mainloop_unlock(mainloop);
}

AudioOutputController::~AudioOutputController()
{
    if (mainloop) {
        pa_threaded_mainloop_lock(mainloop);

        if (context)
            pa_context_disconnect(context);

        pa_threaded_mainloop_unlock(mainloop);

        pa_threaded_mainloop_stop(mainloop);

        if (context) {
            pa_context_unref(context);
            context = nullptr;
        }

        pa_threaded_mainloop_free(mainloop);
        mainloop = nullptr;
    }
}

void AudioOutputController::refreshDevices()
{
    if (!mainloop || !context)
        return;

    pa_threaded_mainloop_lock(mainloop);

    if (pa_context_get_state(context) == PA_CONTEXT_READY)
        requestRefreshLocked();

    pa_threaded_mainloop_unlock(mainloop);
}

void AudioOutputController::setOutputDevice(
    const QString &sinkName
)
{
    if (!mainloop || !context)
        return;

    pa_threaded_mainloop_lock(mainloop);

    preferredSink = sinkName;

    qInfo() << "[Audio] Seçilen çıkış cihazı:"
            << (preferredSink.isEmpty()
                ? "Sistem varsayılanı"
                : preferredSink);

    if (pa_context_get_state(context) == PA_CONTEXT_READY) {
        requestSinkInputsLocked();
    } else {
        qWarning() << "[Audio] PulseAudio bağlantısı hazır değil.";
    }

    pa_threaded_mainloop_unlock(mainloop);
}

void AudioOutputController::requestRefreshLocked()
{
    devices.clear();

    sinkListReady = false;
    serverInfoReady = false;

    pa_operation *sinkOperation =
        pa_context_get_sink_info_list(
            context,
            sinkInfoCallback,
            this
        );

    if (sinkOperation) {
        pa_operation_unref(sinkOperation);
    } else {
        emit errorOccurred(
            "Ses çıkış cihazları listelenemedi."
        );
    }

    pa_operation *serverOperation =
        pa_context_get_server_info(
            context,
            serverInfoCallback,
            this
        );

    if (serverOperation) {
        pa_operation_unref(serverOperation);
    } else {
        emit errorOccurred(
            "Varsayılan ses cihazı bilgisi alınamadı."
        );
    }
}

void AudioOutputController::emitDevicesIfReady()
{
    if (!sinkListReady || !serverInfoReady)
        return;

    for (QVariant &item : devices) {
        QVariantMap device = item.toMap();

        device["isDefault"] =
            device["name"].toString() == defaultSink;

        item = device;
    }

    emit devicesUpdated(devices, defaultSink);

    requestSinkInputsLocked();
}

void AudioOutputController::requestSinkInputsLocked()
{
    if (!context)
        return;

    if (sinkInputsQueryRunning)
        return;

    if (!sinkListReady || !serverInfoReady)
        return;

    if (pa_context_get_state(context) != PA_CONTEXT_READY)
        return;

    sinkInputsQueryRunning = true;

    qInfo() << "[Audio] Ses akışları sorgulanıyor...";

    pa_operation *operation =
        pa_context_get_sink_input_info_list(
            context,
            sinkInputInfoCallback,
            this
        );

    if (operation) {
        pa_operation_unref(operation);
    } else {
        sinkInputsQueryRunning = false;

        qWarning() << "[Audio] Ses akışı sorgusu başlatılamadı.";

        emit errorOccurred(
            "Uygulama ses akışı sorgulanamadı."
        );
    }
}

void AudioOutputController::applyOutputDeviceLocked(
    uint32_t sinkInputIndex,
    uint32_t currentSinkIndex,
    bool isCorked,
    bool isMuted
)
{
    QString targetSink =
        preferredSink.isEmpty()
            ? defaultSink
            : preferredSink;

    if (targetSink.isEmpty()) {
        qWarning() << "[Audio] Hedef ses cihazı belirlenemedi.";
        return;
    }

    uint32_t targetSinkIndex = PA_INVALID_INDEX;
    bool targetFound = false;

    for (const QVariant &item : devices) {
        QVariantMap device = item.toMap();

        if (device["name"].toString() == targetSink) {
            targetSinkIndex =
                device["index"].toUInt();

            targetFound = true;
            break;
        }
    }

    // Seçilmiş cihaz bulunamıyorsa yalnızca Deezer akışını sustur.
    if (!targetFound) {
        if (preferredSink.isEmpty()) {
            qWarning() << "[Audio] Varsayılan cihaz bulunamadı.";
            return;
        }

        qWarning() << "[Audio] Seçilen cihaz kullanılamıyor:"
                   << targetSink;

        // Önceki akış kaldırılmış ve yeni bir akış oluşmuş olabilir.
        if (mutedByController &&
            mutedSinkInputIndex != sinkInputIndex) {
            mutedByController = false;
            mutedSinkInputIndex = PA_INVALID_INDEX;
        }

        // Bu akışı zaten biz susturduysak tekrar işlem yapma.
        if (mutedByController &&
            mutedSinkInputIndex == sinkInputIndex) {
            return;
        }

        // Kullanıcı zaten susturmuşsa veya akış duraklatılmışsa
        // mevcut duruma müdahale etme.
        if (isCorked || isMuted)
            return;

        if (muteOperationRunning)
            return;

        pendingMuteSinkInputIndex = sinkInputIndex;
        muteOperationRunning = true;

        qInfo() << "[Audio] Deezer ses akışı sessize alınıyor.";

        pa_operation *operation =
            pa_context_set_sink_input_mute(
                context,
                sinkInputIndex,
                1,
                muteFinishedCallback,
                this
            );

        if (operation) {
            pa_operation_unref(operation);
        } else {
            muteOperationRunning = false;
            pendingMuteSinkInputIndex = PA_INVALID_INDEX;

            qWarning() << "[Audio] Ses akışı sessize alınamadı.";
        }

        return;
    }

    // Akış zaten hedef cihazdaysa gerekiyorsa sessizden çıkar.
    if (targetSinkIndex == currentSinkIndex) {
        qInfo() << "[Audio] Akış zaten seçilen cihazda:"
                << targetSink;

        unmuteStreamLocked(sinkInputIndex);
        return;
    }

    QByteArray sinkName = targetSink.toUtf8();

    pendingMoveSinkInputIndex = sinkInputIndex;

    pa_operation *operation =
        pa_context_move_sink_input_by_name(
            context,
            sinkInputIndex,
            sinkName.constData(),
            moveFinishedCallback,
            this
        );

    if (operation) {
        pa_operation_unref(operation);

        qInfo() << "[Audio] Deezer ses akışı yönlendiriliyor:"
                << targetSink;
    } else {
        pendingMoveSinkInputIndex = PA_INVALID_INDEX;

        qWarning() << "[Audio] Ses akışı taşıma isteği başlatılamadı.";

        emit errorOccurred(
            "Deezer ses akışı seçilen cihaza taşınamadı."
        );
    }
}

void AudioOutputController::unmuteStreamLocked(
    uint32_t sinkInputIndex
)
{
    if (!mutedByController)
        return;

    if (mutedSinkInputIndex != sinkInputIndex)
        return;

    if (unmuteOperationRunning)
        return;

    unmuteOperationRunning = true;

    qInfo() << "[Audio] Deezer ses akışı yeniden etkinleştiriliyor.";

    pa_operation *operation =
        pa_context_set_sink_input_mute(
            context,
            sinkInputIndex,
            0,
            unmuteFinishedCallback,
            this
        );

    if (operation) {
        pa_operation_unref(operation);
    } else {
        unmuteOperationRunning = false;

        qWarning() << "[Audio] Ses akışı sessizden çıkarılamadı.";
    }
}

void AudioOutputController::contextStateCallback(
    pa_context *context,
    void *userdata
)
{
    auto *controller =
        static_cast<AudioOutputController *>(userdata);

    switch (pa_context_get_state(context)) {
    case PA_CONTEXT_READY: {
        qInfo() << "[Audio] PulseAudio bağlantısı hazır.";

        pa_context_set_subscribe_callback(
            context,
            subscriptionCallback,
            controller
        );

        pa_operation *operation =
            pa_context_subscribe(
                context,
                static_cast<pa_subscription_mask_t>(
                    PA_SUBSCRIPTION_MASK_SINK |
                    PA_SUBSCRIPTION_MASK_SINK_INPUT
                ),
                nullptr,
                nullptr
            );

        if (operation)
            pa_operation_unref(operation);

        controller->requestRefreshLocked();
        break;
    }

    case PA_CONTEXT_FAILED:
        emit controller->errorOccurred(
            QString("PulseAudio bağlantı hatası: %1")
                .arg(pa_strerror(pa_context_errno(context)))
        );
        break;

    case PA_CONTEXT_TERMINATED:
        emit controller->errorOccurred(
            "PulseAudio bağlantısı sonlandırıldı."
        );
        break;

    default:
        break;
    }
}

void AudioOutputController::sinkInfoCallback(
    pa_context *,
    const pa_sink_info *info,
    int eol,
    void *userdata
)
{
    auto *controller =
        static_cast<AudioOutputController *>(userdata);

    if (eol < 0) {
        controller->sinkListReady = true;

        emit controller->errorOccurred(
            "Ses çıkış cihazları alınırken hata oluştu."
        );

        controller->emitDevicesIfReady();
        return;
    }

    if (eol > 0) {
        controller->sinkListReady = true;
        controller->emitDevicesIfReady();
        return;
    }

    if (!info)
        return;

    QVariantMap device;

    device["name"] =
        QString::fromUtf8(info->name);

    device["description"] =
        QString::fromUtf8(info->description);

    device["sampleRate"] =
        static_cast<int>(info->sample_spec.rate);

    device["channels"] =
        static_cast<int>(info->sample_spec.channels);

    device["index"] =
        static_cast<uint>(info->index);

    device["isDefault"] = false;

    controller->devices.append(device);
}

void AudioOutputController::serverInfoCallback(
    pa_context *,
    const pa_server_info *info,
    void *userdata
)
{
    auto *controller =
        static_cast<AudioOutputController *>(userdata);

    if (!info) {
        controller->defaultSink.clear();
        controller->serverInfoReady = true;

        emit controller->errorOccurred(
            "Varsayılan ses cihazı bilgisi alınamadı."
        );

        controller->emitDevicesIfReady();
        return;
    }

    controller->defaultSink =
        info->default_sink_name
            ? QString::fromUtf8(info->default_sink_name)
            : QString();

    controller->serverInfoReady = true;
    controller->emitDevicesIfReady();
}

void AudioOutputController::sinkInputInfoCallback(
    pa_context *,
    const pa_sink_input_info *info,
    int eol,
    void *userdata
)
{
    auto *self =
        static_cast<AudioOutputController *>(userdata);

    if (eol < 0) {
        self->sinkInputsQueryRunning = false;

        qWarning() << "[Audio] Ses akışı sorgusu başarısız.";
        return;
    }

    if (eol > 0) {
        self->sinkInputsQueryRunning = false;

        qInfo() << "[Audio] Ses akışı sorgusu tamamlandı.";
        return;
    }

    if (!info)
        return;

    const char *binary =
        pa_proplist_gets(
            info->proplist,
            PA_PROP_APPLICATION_PROCESS_BINARY
        );

    const QString processBinary =
        binary ? QString::fromUtf8(binary) : QString();

    if (processBinary != QStringLiteral("deezer-desktop"))
        return;

    qInfo()
        << "[Audio] Deezer ses akışı bulundu:"
        << "Index:" << info->index
        << "Sink:" << info->sink
        << "Corked:" << info->corked
        << "Muted:" << info->mute;

    self->applyOutputDeviceLocked(
        info->index,
        info->sink,
        info->corked != 0,
        info->mute != 0
    );
}

void AudioOutputController::subscriptionCallback(
    pa_context *,
    pa_subscription_event_type_t type,
    uint32_t,
    void *userdata
)
{
    auto *controller =
        static_cast<AudioOutputController *>(userdata);

    pa_subscription_event_type_t facility =
        static_cast<pa_subscription_event_type_t>(
            type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK
        );

    if (facility == PA_SUBSCRIPTION_EVENT_SINK) {
        qInfo() << "[Audio] Ses cihazı değişikliği algılandı.";

        controller->requestRefreshLocked();
    }
    else if (facility == PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
        qInfo() << "[Audio] Ses akışı değişikliği algılandı.";

        controller->requestSinkInputsLocked();
    }
}

void AudioOutputController::muteFinishedCallback(
    pa_context *,
    int success,
    void *userdata
)
{
    auto *self =
        static_cast<AudioOutputController *>(userdata);

    self->muteOperationRunning = false;

    if (success) {
        self->mutedByController = true;

        self->mutedSinkInputIndex =
            self->pendingMuteSinkInputIndex;

        qInfo() << "[Audio] Deezer ses akışı sessize alındı."
                << "Index:" << self->mutedSinkInputIndex;
    } else {
        qWarning() << "[Audio] Deezer ses akışı sessize alınamadı.";
    }

    self->pendingMuteSinkInputIndex = PA_INVALID_INDEX;
}

void AudioOutputController::unmuteFinishedCallback(
    pa_context *,
    int success,
    void *userdata
)
{
    auto *self =
        static_cast<AudioOutputController *>(userdata);

    self->unmuteOperationRunning = false;

    if (success) {
        qInfo() << "[Audio] Deezer ses akışı sessizden çıkarıldı.";

        self->mutedByController = false;
        self->mutedSinkInputIndex = PA_INVALID_INDEX;
    } else {
        qWarning() << "[Audio] Deezer ses akışı sessizden çıkarılamadı.";
    }
}

void AudioOutputController::moveFinishedCallback(
    pa_context *,
    int success,
    void *userdata
)
{
    auto *self =
        static_cast<AudioOutputController *>(userdata);

    uint32_t sinkInputIndex =
        self->pendingMoveSinkInputIndex;

    self->pendingMoveSinkInputIndex = PA_INVALID_INDEX;

    if (!success) {
        qWarning() << "[Audio] Ses akışı taşıma işlemi başarısız.";
        return;
    }

    qInfo() << "[Audio] Ses akışı taşıma işlemi başarılı.";

    if (self->mutedByController &&
        self->mutedSinkInputIndex == sinkInputIndex) {
        self->unmuteStreamLocked(sinkInputIndex);
    }
}

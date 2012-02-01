/* alsa_mfld_cdk.cpp
 **
 ** Modified from hardware/alsa_sound/alsa_default.cpp
 */

/* alsa_default.cpp
 **
 ** Copyright 2009 Wind River Systems
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#define LOG_TAG "ALSAModuleCDK"
#define LOG_NDEBUG 0

#include <utils/Log.h>
#include <AudioHardwareALSACommon.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/AudioSystemLegacy.h>
#include <signal.h>

#undef DISABLE_HARWARE_RESAMPLING

#define ALSA_NAME_MAX (128)
#define PERIOD_TIME   (23220)  //microseconds
#define CAPTURE_PERIOD_TIME (20000) //microseconds

#define ALSA_STRCAT(x, y) \
    do { \
        assert((x) != NULL); \
        assert((y) != NULL); \
        if (strlen((x)) + strlen((y)) < sizeof((x))) { \
            strncat((x), (y), sizeof((x)) - strlen((x)) -1); \
        } else { \
            LOGE("Cannot catenate string for buf size limitation"); \
        } \
    } while (0)

#ifndef ALSA_DEFAULT_SAMPLE_RATE
#define ALSA_DEFAULT_SAMPLE_RATE 44100 // in Hz
#endif

#ifndef DEFAULT_CAPTURE_RATE
#define DEFAULT_CAPTURE_RATE 8000 // in Hz
#endif

#ifndef VOICE_MODEM_DEFAULT_SAMPLE_RATE
#define VOICE_MODEM_DEFAULT_SAMPLE_RATE 48000 // in Hz
#endif

#ifndef WIDI_DEFAULT_SAMPLE_RATE
#define WIDI_DEFAULT_SAMPLE_RATE 48000 // in Hz
#endif

#ifndef VOICE_CODEC_DEFAULT_SAMPLE_RATE
#define VOICE_CODEC_DEFAULT_SAMPLE_RATE 48000 // in Hz
#endif

#ifndef VOICE_BT_DEFAULT_SAMPLE_RATE
#define VOICE_BT_DEFAULT_SAMPLE_RATE 8000 // in Hz
#endif

#define DEVICE_OUT_BLUETOOTH_SCO_ALL (AudioSystem::DEVICE_OUT_BLUETOOTH_SCO | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET | AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT)

int voice_activated = 0;
namespace android_audio_legacy
{
//BridgeApp bapp;
static int s_device_open(const hw_module_t*, const char*, hw_device_t**);
static int s_device_close(hw_device_t*);
static status_t s_init(alsa_device_t *, ALSAHandleList &);
static status_t s_open(alsa_handle_t *, uint32_t, int);
static status_t s_init_stream(alsa_handle_t *handle, uint32_t devices, int mode);
static status_t s_standby(alsa_handle_t *);
static status_t s_close(alsa_handle_t *);
static status_t s_config(alsa_handle_t *, int);
static status_t s_volume(alsa_handle_t *, uint32_t, float);

static hw_module_methods_t s_module_methods = {
    open : s_device_open
};

extern "C" const hw_module_t HAL_MODULE_INFO_SYM = {
    tag           : HARDWARE_MODULE_TAG,
    version_major : 1,
    version_minor : 0,
    id            : ALSA_HARDWARE_MODULE_ID,
    name          : "mfld ALSA module",
    author        : "Intel Corporation",
    methods       : &s_module_methods,
    dso           : 0,
    reserved      : { 0, },
};

static int s_device_close(hw_device_t* device)
{
    free(device);
    return 0;
}

// ----------------------------------------------------------------------------

static const int DEFAULT_SAMPLE_RATE = ALSA_DEFAULT_SAMPLE_RATE;

static const char *devicePrefix[SND_PCM_STREAM_LAST + 1] = {
    /* SND_PCM_STREAM_PLAYBACK : */"AndroidPlayback",
    /* SND_PCM_STREAM_CAPTURE  : */"AndroidCapture",
};

static alsa_handle_t _defaultsOut = {
    module             : 0,
    devices            : AudioSystem::DEVICE_OUT_ALL,
    curDev             : 0,
    curMode            : 0,
    handle             : 0,
    format             : SND_PCM_FORMAT_S16_LE, // AudioSystem::PCM_16_BIT
    channels           : 2,
    sampleRate         : DEFAULT_SAMPLE_RATE,
    expectedSampleRate : DEFAULT_SAMPLE_RATE, //expected sample rate
    latency            : PERIOD_TIME * 4, // Desired Delay in usec
    bufferSize         : DEFAULT_SAMPLE_RATE / 5, // Desired Number of samples
    modPrivate         : 0,
    openFlag           : 0,
};

static alsa_handle_t _defaultsIn = {
    module             : 0,
    devices            : AudioSystem::DEVICE_IN_ALL,
    curDev             : 0,
    curMode            : 0,
    handle             : 0,
    format             : SND_PCM_FORMAT_S16_LE, // AudioSystem::PCM_16_BIT
    channels           : 2,
    sampleRate         : DEFAULT_SAMPLE_RATE,
    expectedSampleRate : DEFAULT_SAMPLE_RATE, //expected sample rate
    latency            : CAPTURE_PERIOD_TIME * 4, // Desired Delay in usec
    bufferSize         : 2048, // Desired Number of samples
    modPrivate         : 0,
    openFlag           : 0,
};

struct device_suffix_t {
    const AudioSystem::audio_devices device;
    const char *suffix;
};

/* The following table(s) need to match in order of the route bits
 */
static const device_suffix_t deviceSuffix[] = {
    { AudioSystem::DEVICE_OUT_EARPIECE,              "_Earpiece" },
    { AudioSystem::DEVICE_OUT_SPEAKER,               "_Speaker" },
    { AudioSystem::DEVICE_OUT_BLUETOOTH_SCO,         "_Bluetooth" },
    { AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET, "_Bluetooth" },
    { AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT,  "_Bluetooth" },
    { AudioSystem::DEVICE_OUT_WIRED_HEADPHONE,       "_Headphone" },
    { AudioSystem::DEVICE_OUT_WIRED_HEADSET,         "_Headset" },
    { AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP,        "_Bluetooth-A2DP" },
    { AudioSystem::DEVICE_OUT_AUX_DIGITAL,           "_HDMI" },
    { AudioSystem::DEVICE_OUT_WIDI_LOOPBACK,         "_Widi-Loopback" },
    { AudioSystem::DEVICE_OUT_DEFAULT,                "_Null" },
    { AudioSystem::DEVICE_IN_BUILTIN_MIC,            "_BuiltinMic" },
    { AudioSystem::DEVICE_IN_BACK_MIC,               "_BuiltinBackMic" },
    { AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET,  "_BluetoothScoHeadset" },
    { AudioSystem::DEVICE_IN_WIRED_HEADSET,          "_Headset" },
    { AudioSystem::DEVICE_IN_VOICE_CALL,             "_VoiceCall" },
    { AudioSystem::DEVICE_IN_DEFAULT,                "_Null" },
};

static const int deviceSuffixLen = (sizeof(deviceSuffix)
                                    / sizeof(device_suffix_t));

// ----------------------------------------------------------------------------

static snd_pcm_stream_t direction(alsa_handle_t *handle)
{
    return (handle->devices & AudioSystem::DEVICE_OUT_ALL) ?
            SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE;
}

static const char *deviceName(alsa_handle_t *handle, uint32_t device, int mode)
{
    static char devString[ALSA_NAME_MAX];
    int hasDevExt = 0;
    const char* prefix =  devicePrefix[direction(handle)];

    assert(sizeof(devString) > strlen(prefix));
    strncpy(devString, prefix, sizeof(devString) - 1);

    for (int dev = 0; device && dev < deviceSuffixLen; dev++) {
        if (device & deviceSuffix[dev].device) {
            ALSA_STRCAT(devString, deviceSuffix[dev].suffix);
            device &= ~deviceSuffix[dev].device;
            hasDevExt = 1;
        }
    }

    if (hasDevExt) switch (mode) {
        case AudioSystem::MODE_NORMAL:
            ALSA_STRCAT(devString, "_normal");
            break;
        case AudioSystem::MODE_RINGTONE:
            ALSA_STRCAT(devString, "_ringtone");
            break;
        case AudioSystem::MODE_IN_CALL:
            ALSA_STRCAT(devString, "_incall");
            break;
        case AudioSystem::MODE_IN_COMMUNICATION:
            ALSA_STRCAT(devString, "_incommunication");
            break;
        };

    LOGD("returning deviceName = %s", devString);
    return devString;
}

static const char *streamName(alsa_handle_t *handle)
{
    return snd_pcm_stream_name(direction(handle));
}

static status_t setHardwareParams(alsa_handle_t *handle)
{
    snd_pcm_hw_params_t *hardwareParams;
    status_t err = NO_ERROR;

    snd_pcm_uframes_t bufferSize = handle->bufferSize;
    unsigned int requestedRate = handle->expectedSampleRate;
    unsigned int latency = handle->latency;
    unsigned int channels = handle->channels;
    unsigned int periodTime = (direction(handle) == SND_PCM_STREAM_PLAYBACK) ?
                                 PERIOD_TIME : CAPTURE_PERIOD_TIME; //us

    // snd_pcm_format_description() and snd_pcm_format_name() do not perform
    // proper bounds checking.
    bool validFormat = (static_cast<int> (handle->format) > SND_PCM_FORMAT_UNKNOWN) &&
                       (static_cast<int> (handle->format) <= SND_PCM_FORMAT_LAST);
    const char *formatDesc = validFormat ? snd_pcm_format_description(handle->format)
                                         : "Invalid Format";
    const char *formatName = validFormat ? snd_pcm_format_name(handle->format)
                                         : "UNKNOWN";

    if (snd_pcm_hw_params_malloc(&hardwareParams) < 0) {
        LOG_ALWAYS_FATAL("Failed to allocate ALSA hardware parameters!");
        return NO_INIT;
    }

    err = snd_pcm_hw_params_any(handle->handle, hardwareParams);
    if (err < 0) {
        LOGE("Unable to configure hardware: %s", snd_strerror(err));
        snd_pcm_hw_params_free(hardwareParams);
        return err;
    }

    // Set the interleaved read and write format.
    err = snd_pcm_hw_params_set_access(handle->handle, hardwareParams,
                                       SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        LOGE("Unable to configure PCM read/write format: %s",
             snd_strerror(err));
        snd_pcm_hw_params_free(hardwareParams);
        return err;
    }

    err = snd_pcm_hw_params_set_format(handle->handle, hardwareParams,
                                       handle->format);
    if (err < 0) {
        LOGE("Unable to configure PCM format %s (%s): %s",
             formatName, formatDesc, snd_strerror(err));
        snd_pcm_hw_params_free(hardwareParams);
        return err;
    }

    LOGV("Set %s PCM format to %s (%s)", streamName(handle), formatName, formatDesc);

    err = snd_pcm_hw_params_set_channels(handle->handle, hardwareParams, channels);
    if (err < 0) {
        LOGE("Unable to set channel count to %i: %s", channels, snd_strerror(err));
        snd_pcm_hw_params_free(hardwareParams);
        return err;
    }

    LOGV("Using %i %s for %s.", channels,
         channels == 1 ? "channel" : "channels", streamName(handle));

    err = snd_pcm_hw_params_set_rate_near(handle->handle, hardwareParams,
                                          &requestedRate, 0);

    if (err < 0)
        LOGE("Unable to set %s sample rate to %u: %s",
             streamName(handle), handle->expectedSampleRate, snd_strerror(err));
    else if (requestedRate != handle->expectedSampleRate)
        // Some devices have a fixed sample rate, and can not be changed.
        // This may cause resampling problems; i.e. PCM playback will be too
        // slow or fast.
        LOGW("Requested rate (%u HZ) does not match actual rate (%u HZ)",
             handle->expectedSampleRate, requestedRate);
    else
        LOGV("Set %s sample rate to %u HZ", streamName(handle), requestedRate);

#ifdef DISABLE_HARWARE_RESAMPLING
    // Disable hardware re-sampling.
    err = snd_pcm_hw_params_set_rate_resample(handle->handle,
            hardwareParams, static_cast<int>(resample));
    if (err < 0) {
        LOGE("Unable to %s hardware resampling: %s",
             resample ? "enable" : "disable", snd_strerror(err));
        snd_pcm_hw_params_free(hardwareParams);
        return err;
    }
#endif

    // Setup buffers for latency
    err = snd_pcm_hw_params_set_buffer_time_near(handle->handle,
            hardwareParams, &latency, NULL);

    if (err < 0) {
        /* That didn't work, set the period instead */
        err = snd_pcm_hw_params_set_period_time_near(handle->handle,
                hardwareParams, &periodTime, NULL);
        if (err < 0) {
            LOGE("Unable to set the period time for latency: %s", snd_strerror(err));
            snd_pcm_hw_params_free(hardwareParams);
            return err;
        }
        snd_pcm_uframes_t periodSize;
        err = snd_pcm_hw_params_get_period_size(hardwareParams, &periodSize,
                                                NULL);
        if (err < 0) {
            LOGE("Unable to get the period size for latency: %s", snd_strerror(err));
            snd_pcm_hw_params_free(hardwareParams);
            return err;
        }
        bufferSize = periodSize * 4;
        if (bufferSize < handle->bufferSize) bufferSize = handle->bufferSize;
        err = snd_pcm_hw_params_set_buffer_size_near(handle->handle,
                hardwareParams, &bufferSize);
        if (err < 0) {
            LOGE("Unable to set the buffer size for latency: %s", snd_strerror(err));
            snd_pcm_hw_params_free(hardwareParams);
            return err;
        }
    } else {
        // OK, we got buffer time near what we expect. See what that did for bufferSize.
        err = snd_pcm_hw_params_get_buffer_size(hardwareParams, &bufferSize);
        if (err < 0) {
            LOGE("Unable to get the buffer size for latency: %s", snd_strerror(err));
            snd_pcm_hw_params_free(hardwareParams);
            return err;
        }
        // Does set_buffer_time_near change the passed value? It should.
        err = snd_pcm_hw_params_get_buffer_time(hardwareParams, &latency, NULL);
        if (err < 0) {
            LOGE("Unable to get the buffer time for latency: %s", snd_strerror(err));
            snd_pcm_hw_params_free(hardwareParams);
            return err;
        }
        periodTime = latency/4;
        err = snd_pcm_hw_params_set_period_time_near(handle->handle,
                hardwareParams, &periodTime, NULL);
        if (err < 0) {
            LOGE("Unable to set the period time for latency: %s", snd_strerror(err));
            snd_pcm_hw_params_free(hardwareParams);
            return err;
        }
    }

    LOGV("Buffer size: %d", (int)bufferSize);
    LOGV("Latency: %d", (int)latency);
    LOGV("periodTime: %d", (int)periodTime);

    handle->bufferSize = bufferSize;
    handle->latency = latency;

    // Commit the hardware parameters back to the device.
    err = snd_pcm_hw_params(handle->handle, hardwareParams);
    if (err < 0) LOGE("Unable to set hardware parameters: %s", snd_strerror(err));

    snd_pcm_hw_params_free(hardwareParams);
    return err;
}

static status_t setSoftwareParams(alsa_handle_t *handle)
{
    snd_pcm_sw_params_t * softwareParams;
    status_t err = NO_ERROR;

    snd_pcm_uframes_t bufferSize = 0;
    snd_pcm_uframes_t periodSize = 0;
    snd_pcm_uframes_t startThreshold, stopThreshold;

    if (snd_pcm_sw_params_malloc(&softwareParams) < 0) {
        LOG_ALWAYS_FATAL("Failed to allocate ALSA software parameters!");
        return NO_INIT;
    }

    // Get the current software parameters
    err = snd_pcm_sw_params_current(handle->handle, softwareParams);
    if (err < 0) {
        LOGE("Unable to get software parameters: %s", snd_strerror(err));
        snd_pcm_sw_params_free(softwareParams);
        return err;
    }

    // Configure ALSA to start the transfer when the buffer is almost full.
    snd_pcm_get_params(handle->handle, &bufferSize, &periodSize);

    if (handle->devices & AudioSystem::DEVICE_OUT_ALL) {
        // For playback, configure ALSA to start the transfer when the
        // buffer is full.
        startThreshold = periodSize - 1;
        stopThreshold = bufferSize;
    } else {
        // For recording, configure ALSA to start the transfer on the
        // first frame.
        startThreshold = 1;
        stopThreshold = bufferSize;
    }

    err = snd_pcm_sw_params_set_start_threshold(handle->handle, softwareParams,
            startThreshold);
    if (err < 0) {
        LOGE("Unable to set start threshold to %lu frames: %s",
             startThreshold, snd_strerror(err));
        snd_pcm_sw_params_free(softwareParams);
        return err;
    }

    err = snd_pcm_sw_params_set_stop_threshold(handle->handle, softwareParams,
            stopThreshold);
    if (err < 0) {
        LOGE("Unable to set stop threshold to %lu frames: %s",
             stopThreshold, snd_strerror(err));
        snd_pcm_sw_params_free(softwareParams);
        return err;
    }

    // Allow the transfer to start when at least periodSize samples can be
    // processed.
    err = snd_pcm_sw_params_set_avail_min(handle->handle, softwareParams,
                                          periodSize);
    if (err < 0) {
        LOGE("Unable to configure available minimum to %lu: %s",
             periodSize, snd_strerror(err));
        snd_pcm_sw_params_free(softwareParams);
        return err;
    }

    // Commit the software parameters back to the device.
    err = snd_pcm_sw_params(handle->handle, softwareParams);
    if (err < 0) LOGE("Unable to configure software parameters: %s",
                          snd_strerror(err));

    snd_pcm_sw_params_free(softwareParams);
    return err;
}

// ----------------------------------------------------------------------------

static status_t s_init(alsa_device_t *module, ALSAHandleList &list)
{
    list.clear();

    snd_pcm_uframes_t bufferSize = _defaultsOut.bufferSize;

    for (size_t i = 1; (bufferSize & ~i) != 0; i <<= 1)
        bufferSize &= ~i;

    _defaultsOut.module = module;
    _defaultsOut.bufferSize = bufferSize;

    list.push_back(_defaultsOut);

    bufferSize = _defaultsIn.bufferSize;

    for (size_t i = 1; (bufferSize & ~i) != 0; i <<= 1)
        bufferSize &= ~i;

    _defaultsIn.module = module;
    _defaultsIn.bufferSize = bufferSize;

    list.push_back(_defaultsIn);

    return NO_ERROR;
}

static status_t s_init_stream(alsa_handle_t *handle, uint32_t devices, int mode)
{
    // Close off previously opened device.
    // It would be nice to determine if the underlying device actually
    // changes, but we might be recovering from an error or manipulating
    // mixer settings (see asound.conf).
    //
    if(handle->openFlag)
        s_close(handle);

    LOGD("s_init_stream called for devices %08x in mode %d...", devices, mode);

    const char *stream = streamName(handle);
    const char *devName = deviceName(handle, devices, mode);

    LOGD("s_init_stream called for devices %s", devName);
    if (devices & AudioSystem::DEVICE_IN_ALL) {
        handle->bufferSize = _defaultsIn.bufferSize;
        handle->latency = _defaultsIn.latency;
        handle->sampleRate = _defaultsIn.sampleRate;
        handle->expectedSampleRate = _defaultsIn.expectedSampleRate;
        handle->channels = _defaultsIn.channels;
        handle->format = _defaultsIn.format;
    } else {
        handle->bufferSize = _defaultsOut.bufferSize;
        handle->latency = _defaultsOut.latency;
        handle->sampleRate = _defaultsOut.sampleRate;
        handle->expectedSampleRate = _defaultsOut.expectedSampleRate;
        handle->channels = _defaultsOut.channels;
        handle->format = _defaultsOut.format;
    }
    LOGI("Initialized ALSA %s device %s", stream, devName);

    handle->curDev = devices;
    handle->curMode = mode;
    handle->openFlag = 0;
    return NO_ERROR;
}

static int s_device_open(const hw_module_t* module, const char* name,
                         hw_device_t** device)
{
    alsa_device_t *dev;
    dev = (alsa_device_t *) malloc(sizeof(*dev));
    if (!dev) return -ENOMEM;

    memset(dev, 0, sizeof(*dev));

    /* initialize the procs */
    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (hw_module_t *) module;
    dev->common.close = s_device_close;
    dev->init = s_init;
    dev->open = s_open;
    dev->standby = s_standby;
    dev->close = s_close;
    dev->volume = s_volume;
    dev->initStream = s_init_stream;

    *device = &dev->common;
    return 0;
}

static status_t s_open(alsa_handle_t *handle, uint32_t devices, int mode)
{
    // Close off previously opened device.
    // It would be nice to determine if the underlying device actually
    // changes, but we might be recovering from an error or manipulating
    // mixer settings (see asound.conf).
    //
    LOGD("s_open: closing handle first %d, %d", devices, mode);
    s_close(handle);

    LOGD("open called for devices %08x in mode %d...", devices, mode);

    const char *stream = streamName(handle);
    const char *devName = deviceName(handle, devices, mode);

    LOGD("open called for devices %s", devName);

    int err;

    for (;;) {
        // The PCM stream is opened in blocking mode, per ALSA defaults.  The
        // AudioFlinger seems to assume blocking mode too, so asynchronous mode
        // should not be used.
        err = snd_pcm_open(&handle->handle, devName, direction(handle),
                           SND_PCM_ASYNC);
        if (err == 0) break;

        // See if there is a less specific name we can try.
        // Note: We are changing the contents of a const char * here.
        char *tail = strrchr(devName, '_');
        if (!tail) break;
        *tail = 0;
    }

    if (err < 0) {
        // None of the Android defined audio devices exist. Open a generic one.
        devName = "default";
        err = snd_pcm_open(&handle->handle, devName, direction(handle), 0);
    }

    if (err < 0) {
        LOGE("Failed to Initialize any ALSA %s device: %s",
             stream, strerror(err));
        return NO_INIT;
    }

    // reset the initial value; playback and caputure are the same value.
    handle->sampleRate = DEFAULT_SAMPLE_RATE;
    handle->expectedSampleRate = DEFAULT_SAMPLE_RATE;

    if (devices & AudioSystem::DEVICE_OUT_ALL) {
        if (mode == AudioSystem::MODE_IN_CALL) {
            LOGD("Setting expected sample rate to %d (IN_CALL)", VOICE_MODEM_DEFAULT_SAMPLE_RATE);
            handle->expectedSampleRate = VOICE_MODEM_DEFAULT_SAMPLE_RATE;
        }
        else if (devices & DEVICE_OUT_BLUETOOTH_SCO_ALL) {
            LOGD("Setting expected sample rate to %d (BT_SCO_ALL)", VOICE_BT_DEFAULT_SAMPLE_RATE);
            handle->expectedSampleRate = VOICE_BT_DEFAULT_SAMPLE_RATE;
            /* We use 4 buffers of period time, see set_hardware_params function */
            handle->latency = CAPTURE_PERIOD_TIME * 4;
        }
        else if (mode == AudioSystem::MODE_IN_COMMUNICATION) {
            LOGD("Setting expected sample rate to %d (IN_COMM)", VOICE_CODEC_DEFAULT_SAMPLE_RATE);
            handle->expectedSampleRate = VOICE_CODEC_DEFAULT_SAMPLE_RATE;
        }
        else if (devices & AudioSystem::DEVICE_OUT_WIDI_LOOPBACK) {
            LOGV("Setting expected sample rate to %d (WIDI)", WIDI_DEFAULT_SAMPLE_RATE);
            handle->expectedSampleRate = WIDI_DEFAULT_SAMPLE_RATE;
        }
    }

    //This is a tricky way to change the sample rate.
    //This is only effective for audio flinger reopen the HAL for each recording.
    //This SRC change will not take effect when device is changed during the recording.
    //This is not a formal solution and limitation, so maybe we will modify these codes
    //when we find a better way to resolve SRC.
    if (devices & AudioSystem::DEVICE_IN_ALL) {

        if (mode == AudioSystem::MODE_IN_CALL) {
            LOGD("Detected voice modem capture device, setting sample rate to %d instead of %d, SRC done by ALSA", 
								DEFAULT_SAMPLE_RATE, VOICE_MODEM_DEFAULT_SAMPLE_RATE);
            // SRC done by Alsa plug, later improvement: use intel optimized SRC
            handle->sampleRate = DEFAULT_SAMPLE_RATE;
        }
        else if (devices & AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
            LOGD("Detected voice Bluetooth capture device, setting sample rate to %d instead of %d, SRC done by ALSA", 
								DEFAULT_SAMPLE_RATE, VOICE_BT_DEFAULT_SAMPLE_RATE);
            // SRC done by Alsa plug, later improvement: use intel optimized SRC
            handle->sampleRate = DEFAULT_SAMPLE_RATE;
        }
        else if (mode == AudioSystem::MODE_IN_COMMUNICATION) {
            LOGD("Detected voice codec capture device, setting sample rate %d instead of %d, SRC done by ALSA", 
								DEFAULT_SAMPLE_RATE, VOICE_MODEM_DEFAULT_SAMPLE_RATE);
            // SRC done by Alsa plug, later improvement: use intel optimized SRC
            handle->sampleRate = DEFAULT_SAMPLE_RATE;
        }
    }

	err = setHardwareParams(handle);

    if (err == NO_ERROR) err = setSoftwareParams(handle);

    LOGI("Initialized ALSA %s device %s", stream, devName);
    handle->curDev = devices;
    handle->curMode = mode;
    handle->openFlag = 1;
    return err;
}

static status_t s_standby(alsa_handle_t *handle)
{
    LOGD("%s in \n", __func__);
    status_t err = NO_ERROR;
    snd_pcm_t *h = handle->handle;
    if (h) {
        if(handle->curMode == AudioSystem::MODE_IN_CALL || handle->curMode == AudioSystem::MODE_IN_COMMUNICATION) {
            snd_pcm_drop(h);
        } else {
            snd_pcm_drain(h);
            err = snd_pcm_close(h);
            handle->handle = NULL;
        }
    }
    LOGD("%s out \n", __func__);
    return err;
}

static status_t s_close(alsa_handle_t *handle)
{
    LOGD("%s in \n", __func__);
    status_t err = NO_ERROR;
    snd_pcm_t *h = handle->handle;
    handle->handle = 0;
    handle->curDev = 0;
    handle->curMode = 0;
    handle->openFlag = 0;
    if (h) {
        snd_pcm_drain(h);
        err = snd_pcm_close(h);
    }
    LOGD("%s out \n", __func__);
    return err;
}

static status_t s_volume(alsa_handle_t *handle, uint32_t devices, float volume)
{
    return NO_ERROR;
}

static status_t s_config(alsa_handle_t *handle, int mode)
{
    return NO_ERROR;
}
}


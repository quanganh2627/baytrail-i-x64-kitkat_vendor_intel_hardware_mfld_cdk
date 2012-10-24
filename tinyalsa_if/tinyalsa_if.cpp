/* tinyalsa_if.cpp
 **
 ** Copyright 2012 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **      http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#define LOG_TAG "TinyAlsaModule"
#define LOG_NDEBUG 0

#include <utils/Log.h>
#include <AudioHardwareALSACommon.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/AudioSystemLegacy.h>
#include <signal.h>

#include <tinyalsa/asoundlib.h>

//
// TODO
//
#define SAMPLE_RATE_48000               (48000)

#define MAX_RETRY (6)

#define NB_RING_BUFFER_NORMAL   2

#define USEC_PER_SEC        (1000000)

#define PLAYBACK_44100_PERIOD_SIZE   1024 //(23220*2 * 44100 / USEC_PER_SEC)
#define PLAYBACK_48000_PERIOD_SIZE   1152 //(24000*2 * 48000 / USEC_PER_SEC)
#define CAPTURE_48000_PERIOD_SIZE    1152 //(20000*2 * 48000 / USEC_PER_SEC)

#define NOT_SET -1


namespace android_audio_legacy
{

static int s_device_open(const hw_module_t*, const char*, hw_device_t**);
static int s_device_close(hw_device_t*);
static status_t s_init(alsa_device_t *, uint32_t, uint32_t);
static status_t s_open(alsa_handle_t *handle, int cardId, int deviceId, const pcm_config& pcmConfig);
static status_t s_init_stream(alsa_handle_t *handle, bool isOut, uint32_t rate, uint32_t channels, pcm_format format);
static status_t s_stop(alsa_handle_t *handle);
static status_t s_standby(alsa_handle_t *);
static status_t s_close(alsa_handle_t *);
static status_t s_config(alsa_handle_t *, int);
static status_t s_volume(alsa_handle_t *, uint32_t, float);

static hw_module_methods_t s_module_methods = {
    open : s_device_open
};

extern "C" hw_module_t HAL_MODULE_INFO_SYM;

hw_module_t HAL_MODULE_INFO_SYM =
{
    tag           : HARDWARE_MODULE_TAG,
    version_major : 1,
    version_minor : 0,
    id            : TINYALSA_HARDWARE_MODULE_ID,
    name          : "mrfld tiny alsa module",
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

static const pcm_config default_pcm_config_playback = {
    channels        : 2,
    rate            : SAMPLE_RATE_48000,
    period_size     : PLAYBACK_48000_PERIOD_SIZE,
    period_count    : NB_RING_BUFFER_NORMAL,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : PLAYBACK_48000_PERIOD_SIZE - 1,
    stop_threshold  : PLAYBACK_48000_PERIOD_SIZE * NB_RING_BUFFER_NORMAL,
    silence_threshold : 0,
    avail_min       : PLAYBACK_48000_PERIOD_SIZE,
};

static const pcm_config default_pcm_config_capture = {
    channels        : 2,
    rate            : SAMPLE_RATE_48000,
    period_size     : PLAYBACK_48000_PERIOD_SIZE / 2,
    period_count    : 4,
    format          : PCM_FORMAT_S16_LE,
    start_threshold : 0,
    stop_threshold  : 0,
    silence_threshold : 0,
    avail_min       : 0,
};

static alsa_handle_t _defaultsOut = {
    module             : 0,
    devices            : AudioSystem::DEVICE_OUT_ALL,
    curDev             : 0,
    curMode            : 0,
    handle             : NULL,
    config             : default_pcm_config_playback,
    flags              : PCM_OUT,
    modPrivate         : 0,
    openFlag           : 0,
};

static alsa_handle_t _defaultsIn = {
    module             : 0,
    devices            : AudioSystem::DEVICE_IN_ALL,
    curDev             : 0,
    curMode            : 0,
    handle             : NULL,
    config             : default_pcm_config_capture,
    flags              : PCM_IN,
    modPrivate         : 0,
    openFlag           : 0,
};

// ----------------------------------------------------------------------------

static status_t s_init(alsa_device_t *module)
{
    _defaultsOut.module = module;
    _defaultsIn.module = module;

    return NO_ERROR;
}

static status_t s_init_stream(alsa_handle_t *handle, bool isOut, uint32_t rate, uint32_t channels, pcm_format format)
{
    LOGD("%s called for %s stream", __FUNCTION__, isOut? "output" : "input");

    if (isOut) {

        *handle = _defaultsOut;

        // For output stream, use rate/channels/format requested by AF
        // upon information read into audio_policy.conf
        handle->config.rate = rate;
        handle->config.channels = channels;
        handle->config.format = format;
    } else {

        *handle = _defaultsIn;

        // For intput stream, DO NOT use rate/channels/format requested by AF
        // upon information read into audio_policy.conf
        // SRC might be used within HAL
        // Wait for open to give sample spec information from the AudioRoute borrowed
    }

    handle->handle = NULL;

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
    dev->stop = s_stop;

    *device = &dev->common;
    return 0;
}

static status_t s_open(alsa_handle_t *handle,
                       int cardId,
                       int deviceId,
                       const pcm_config& pcmConfig)
{
    handle->config = pcmConfig;

    ALOGD("%s called for card (%d,%d)",
                                __FUNCTION__,
                                cardId,
                                deviceId);
    ALOGD("\t\t config=rate(%d), format(%d), channels(%d))",
                                handle->config.rate,
                                handle->config.format,
                                handle->config.channels);
    ALOGD("\t\t period_size=%d, period_count=%d",
                                handle->config.period_size,
                                handle->config.period_count);
    ALOGD("\t\t startTh=%d, stop Th=%d silence Th=%d",
                                handle->config.start_threshold,
                                handle->config.stop_threshold,
                                handle->config.silence_threshold);

    int attempt = 0;
    for (;;) {
        //
        // Opens the device in NON_BLOCKING mode (default)
        // No need to check for NULL handle, tiny alsa
        // guarantee to return a pcm structure, even when failing to open
        // it will return a reference on a "bad pcm" structure
        //
        handle->handle = pcm_open(cardId, deviceId, handle->flags, &handle->config);
        if (handle->handle && !pcm_is_ready(handle->handle)) {

            ALOGE("cannot open pcm_in driver: %s", pcm_get_error(handle->handle));
            pcm_close(handle->handle);
            if (attempt < MAX_RETRY) {
                //The processing of the open request for HDMI is done at the interrupt
                //boundary in driver code, which takes max of 23ms. So any open request
                //would be responded by a -EAGAIN till this interrupt boundary.
                //The ALSA layer returns -EBUSY when driver returns -EAGAIN.
                //This -EBUSY is handled here by sending repeated requests for MAX 6 times,
                //without truncating the "devName", after a delay of 10ms each time.

                usleep(10 * 1000);
                attempt++;
                continue;
            }
            return NO_MEMORY;
        }
        break ;
    }

    handle->openFlag = 1;
    return NO_ERROR;
}

static status_t s_stop(alsa_handle_t *handle)
{
    LOGD("%s in \n", __func__);
    status_t err = NO_ERROR;
    pcm* h = handle->handle;
    if (h) {

        LOGD("%s stopping stream \n", __func__);
        err = pcm_stop(h);
    }
    LOGD("%s out \n", __func__);
    return err;
}

static status_t s_standby(alsa_handle_t *handle)
{
    LOGD("%s in \n", __func__);
    status_t err = NO_ERROR;
    pcm* h = handle->handle;
    if (h) {
        err = pcm_close(h);
        handle->handle = NULL;
    }
    LOGD("%s out \n", __func__);
    return err;
}

static status_t s_close(alsa_handle_t *handle)
{
    LOGD("%s in \n", __func__);
    status_t err = NO_ERROR;
    pcm* h = handle->handle;
    handle->handle = 0;

    handle->openFlag = 0;
    if (h) {

        err = pcm_close(h);
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

}; // namespace android_audio_legacy

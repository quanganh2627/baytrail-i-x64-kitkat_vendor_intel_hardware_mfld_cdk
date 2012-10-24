/*
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

#include <utils/Log.h>
#include <errno.h>

#include <hardware_legacy/AudioSystemLegacy.h>

#include <alsa/asoundlib.h>
#include <alsa/control_external.h>

#include <fm_rx_sm.h>

#include "fm_module.h"

#define LOG_TAG "FM_Module"

namespace android_audio_legacy
{

static snd_pcm_t *fm_handle;

static int fm_configure_codec(int mode);

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/
int fm_init()
{
    fm_handle = NULL;
    LOGD("FM HW module initialized.");
    return NO_ERROR;
}

/*---------------------------------------------------------------------------*/
/* Call FM chip and audio codec configuration                                */
/*---------------------------------------------------------------------------*/
int fm_set_state(int mode)
{
    int err = NO_ERROR;

    if ((err = fm_configure_codec(mode)) < 0) {
        LOGE("Cannot enable/disable Codec PCM1 (mode %s)",
            (mode == AudioSystem::MODE_FM_ON) ? "ON" : "OFF");
    }

    return err;
}

/*---------------------------------------------------------------------------*/
/* Configure Audio Codec                                                     */
/*---------------------------------------------------------------------------*/
static int fm_configure_codec(int mode) {

    int err = NO_ERROR;
    static const char *deviceFmPlaybackPCM1 = "FmPlayback";

    if (mode == AudioSystem::MODE_FM_ON) {
        LOGD("Opening codec PCM1 on device %s", deviceFmPlaybackPCM1);
        err = snd_pcm_open(&fm_handle, deviceFmPlaybackPCM1 , SND_PCM_STREAM_PLAYBACK, 0);
        if (err  < 0) {
            LOGE("Playback open error: %s\n", snd_strerror(err));
            fm_handle = NULL;
            return err;
        } else {
            err = snd_pcm_set_params(fm_handle,
                                     SND_PCM_FORMAT_S16_LE,
                                     SND_PCM_ACCESS_RW_INTERLEAVED,
                                     FM_CHANNEL_NB,
                                     FM_RATE_HZ,
                                     FM_SND_PCM_SOFT_RESAMPLE,
                                     FM_SND_PCM_LATENCY_US);
            if (err < 0) {
                LOGE("Set params error: %s\n", snd_strerror(err));
                snd_pcm_close(fm_handle);
                fm_handle = NULL;
                return err;
            }

        }
    } else {
        LOGD("Closing device PCM1");
        if (fm_handle != NULL) {
            snd_pcm_close(fm_handle);
            fm_handle = NULL;
        } else {
            LOGD("Device PCM1 already closed!");
        }
    }
    return err;
}

/*===========================================================================*/
/* HW module interface definition                                            */
/*===========================================================================*/

static int s_device_open(const hw_module_t*, const char*, hw_device_t**);
static int s_device_close(hw_device_t*);

static hw_module_methods_t s_module_methods =
{
    open : s_device_open
};

extern "C" hw_module_t HAL_MODULE_INFO_SYM =
{
    tag           : HARDWARE_MODULE_TAG,
    version_major : 1,
    version_minor : 0,
    id            : FM_HARDWARE_MODULE_ID,
    name          : "Intel FM Module",
    author        : "Intel Corporation",
    methods       : &s_module_methods,
    dso           : 0,
    reserved      : { 0, },
};

static int s_device_open(const hw_module_t* module, const char* name,
                         hw_device_t** device)
{
    fm_device_t *dev;
    dev = (fm_device_t *) malloc(sizeof(*dev));
    if (!dev) return -ENOMEM;

    memset(dev, 0, sizeof(*dev));

    dev->common.tag     = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module  = (hw_module_t *) module;
    dev->common.close   = s_device_close;
    dev->init           = fm_init;
    dev->set_state      = fm_set_state;
    *device = &dev->common;
    return 0;
}

static int s_device_close(hw_device_t* device)
{
    free(device);
    return 0;
}

}

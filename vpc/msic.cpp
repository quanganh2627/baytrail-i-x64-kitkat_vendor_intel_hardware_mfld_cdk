/*
 **
 ** Copyright 2011 Intel Corporation
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

#define LOG_TAG "VPC_MSIC"
#include <utils/Log.h>

#include "msic.h"

namespace android
{

#define MEDFIELDAUDIO "medfieldaudio"

snd_pcm_t *msic::handle_playback = NULL;
snd_pcm_t *msic::handle_capture  = NULL;

int msic::pcm_init()
{
    pcm_enable();
    pcm_disable();

    return 0;
}

int msic::pcm_enable()
{
    char device_v[128];
    int card = snd_card_get_index(MEDFIELDAUDIO);
    int err;

    LOGD("Enable MSIC voice path ~~~ Entry\n");

    if (handle_playback || handle_capture)
        pcm_disable();

    sprintf(device_v, "hw:%d,2", card);
    LOGD("  %s \n", device_v);

    if ((err = snd_pcm_open(&handle_playback, device_v, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        LOGE("  Playback open error: %s\n", snd_strerror(err));
        pcm_disable();
    }
    else if ((err = snd_pcm_open(&handle_capture, device_v, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        LOGE("  Capture open error: %s\n", snd_strerror(err));
        pcm_disable();
    }
    else if ((err = snd_pcm_set_params(handle_playback,
                                      SND_PCM_FORMAT_S16_LE,
                                      SND_PCM_ACCESS_RW_INTERLEAVED,
                                      2,
                                      48000,
                                      0,
                                      500000)) < 0) {    /* 0.5sec */
        LOGE("  [P]set params error: %s\n", snd_strerror(err));
        pcm_disable();
    }
    else if ((err = snd_pcm_set_params(handle_capture,
                                      SND_PCM_FORMAT_S16_LE,
                                      SND_PCM_ACCESS_RW_INTERLEAVED,
                                      1,
                                      48000,
                                      1,
                                      500000)) < 0) { /* 0.5sec */
        LOGE("  [C]set params error: %s\n", snd_strerror(err));
        pcm_disable();
    }

    LOGD("Enable MSIC voice path ~~~ Exit\n");

    return err;
}

int msic::pcm_disable()
{
    LOGD("Disable MSIC voice path ~~~ Entry\n");

    if (handle_playback) {
        snd_pcm_close(handle_playback);
        LOGD("  snd_pcm_close(handle_playback)\n");
    }
    if (handle_capture) {
        snd_pcm_close(handle_capture);
        LOGD("  snd_pcm_close(handle_capture)\n");
    }
    handle_playback = NULL;
    handle_capture  = NULL;

    LOGD("Disable MSIC voice path ~~~ Exit\n");

    return 0;
}

}


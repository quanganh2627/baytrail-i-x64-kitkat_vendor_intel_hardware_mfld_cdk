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

#ifndef __FM_MODULE_H__
#define __FM_MODULE_H__

#include <hardware/hardware.h>
namespace android_audio_legacy
{
#ifdef __cplusplus
extern "C"
{
#endif

#define FM_HARDWARE_MODULE_ID           "fm"
#define FM_HARDWARE_NAME                "fm"

#define HCI_CMD_TIMEOUT_MS              5000

#define FM_PCM_MODE_SET_CMD             0x1e
#define FM_PCM_MODE_SET_CMD_LEN         2
#define FM_I2S_MODE_CONFIG_SET_CMD      0x1f
#define FM_I2S_MODE_CONFIG_SET_CMD_LEN  2
#define FM_AUDIO_ENABLE_CMD             0x1d
#define FM_AUDIO_ENABLE_CMD_LEN         2

#define FM_SND_PCM_LATENCY_US           500000
#define FM_RATE_HZ                      48000
#define FM_CHANNEL_NB                   2
#define FM_SND_PCM_SOFT_RESAMPLE        0

static snd_pcm_t *fm_handle;

typedef struct fm_device_t {
    hw_device_t common;

    int (*init)(void);
    int (*set_state)(int mode);
} fm_device_t;

#ifdef __cplusplus
}
#endif
}
#endif /* __FM_MODULE_H__ */

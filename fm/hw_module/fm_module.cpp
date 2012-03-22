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

#include <bluetooth.h>
#include <hci.h>
#include <hci_lib.h>
#include <hci_vs_lib.h>


#include "fm_module.h"

#define LOG_TAG "FM_Module"

namespace android_audio_legacy
{

static int fm_configure_codec(int mode);
static int fm_configure_fm(int mode);
static int fm_enable_audio_path_cmd(int hci_sk, int bt_path, int fm_path);
static int fm_pcm_config_cmd(int hci_sk);
static int fm_i2s_config_cmd(int hci_sk);
static int fm_audio_enable_cmd(int hci_sk);

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
        LOGE("Cannot enable/diasble Codec PCM1 (mode %s)",
            (mode == AudioSystem::MODE_FM_ON) ? "ON" : "OFF");
    }

    if ((err = fm_configure_fm(mode)) < 0) {
        LOGE("Cannot set/unset FM audio path (mode %s)",
            (mode == AudioSystem::MODE_FM_ON) ? "ON" : "OFF");
        return err;
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

/*---------------------------------------------------------------------------*/
/* Configure BT/FM Chip                                                      */
/*---------------------------------------------------------------------------*/
static int fm_configure_fm(int mode) {
    int err = NO_ERROR;
    int dev_id;
    int hci_sk = -1;

    /* Get hci device id if up */
    if ((dev_id = hci_get_route(NULL)) < 0) {
        LOGD("  Can't get HCI device id: %s (%d)\n", strerror(errno), errno);
        LOGD("  -> Normal case if the BT chipset is disabled.\n");
        return dev_id;
    }
    /* Create HCI socket to send HCI cmd */
    else if ((hci_sk = hci_open_dev(dev_id)) < 0) {
        LOGE("  Can't open HCI socket: %s (%d)\n", strerror(errno), errno);
        return hci_sk;
    }
    /* Enable/disable FM PCM audio path */
    else {
        if (mode == AudioSystem::MODE_FM_ON) {
            err = fm_enable_audio_path_cmd(hci_sk, AUDIO_PATH_NONE, AUDIO_PATH_NONE);
            if(err  < 0) {
                LOGE("  Cannnot disable BT/FM audio path on sock: 0x%x %s(%d)\n", hci_sk,
                     strerror(errno), errno);
                goto end;
            }
            err = fm_pcm_config_cmd(hci_sk);
            if(err < 0) {
                LOGE("  Cannot config PCM on sock: 0x%x %s(%d)\n", hci_sk,
                     strerror(errno), errno);
                goto end;
            }
            err = fm_i2s_config_cmd(hci_sk);
            if(err  < 0) {
                LOGE("  Cannot config I2S on sock: 0x%x %s(%d)\n", hci_sk,
                     strerror(errno), errno);
                goto end;
            }
            err = fm_audio_enable_cmd(hci_sk);
            if(err  < 0) {
                LOGE("  Cannot enable FM audio on sock: 0x%x %s(%d)\n", hci_sk,
                     strerror(errno), errno);
                goto end;
            }
        }
        err = fm_enable_audio_path_cmd(hci_sk, AUDIO_PATH_NONE,
                                                   (mode == AudioSystem::MODE_FM_ON) ? AUDIO_PATH_I2S : AUDIO_PATH_NONE);
        if(err < 0) {
            LOGE("  Can't send HCI cmd to enable/disable FM audio path on sock: 0x%x %s(%d)\n", hci_sk,
                 strerror(errno), errno);
        }
    }
    end:
        /* Close HCI socket */
        if (hci_sk >= 0)
            hci_close_dev(hci_sk);

        return err;
}

/*---------------------------------------------------------------------------*/
/* Enable/disable BT Chip Audio Path                                         */
/*---------------------------------------------------------------------------*/
static int fm_enable_audio_path_cmd(int hci_sk, int bt_audio_path, int fm_audio_path) {
    int err = NO_ERROR;

    err = hci_vs_btip1_1_set_fm_audio_path(hci_sk, -1, -1,
                                           bt_audio_path,
                                           fm_audio_path, 1,
                                           HCI_CMD_TIMEOUT_MS);
    return err;
}

/*---------------------------------------------------------------------------*/
/* PCM Configuration                                                         */
/*---------------------------------------------------------------------------*/
static int fm_pcm_config_cmd(int hci_sk) {
    int err = NO_ERROR;

    /* I2S protocol - Left Swap - Fine Offset 0 - Slot offset 0
       16 bits per channels */
    uint8_t data[] = { 0x00, 0x00 };

    err = hci_vs_i2c_write_to_fm(hci_sk, FM_PCM_MODE_SET_CMD,
                                 FM_PCM_MODE_SET_CMD_LEN, data,
                                 HCI_CMD_TIMEOUT_MS);

    return err;
}

/*---------------------------------------------------------------------------*/
/* I2S Configuration                                                         */
/*---------------------------------------------------------------------------*/
static int fm_i2s_config_cmd(int hci_sk) {
    int err = NO_ERROR;

    /* Data width 32FS - I2S Standard format - master - SDO Trisate mode 0
        SDO Phase select 0 - WS Phase 11 - SDO_3st_alwz 0 - 44.1kHz*/
    uint8_t data[] = { 0x13, 0x00 };

    err = hci_vs_i2c_write_to_fm(hci_sk, FM_I2S_MODE_CONFIG_SET_CMD,
                                 FM_I2S_MODE_CONFIG_SET_CMD_LEN,
                                 data, HCI_CMD_TIMEOUT_MS);

    return err;
}

/*---------------------------------------------------------------------------*/
/* Audio Enable                                                              */
/*---------------------------------------------------------------------------*/
static int fm_audio_enable_cmd(int hci_sk) {
    int err = NO_ERROR;

    /* I2S enabled only (no analog) */
    uint8_t data[] = { 0x00, 0x01 };

    err = hci_vs_i2c_write_to_fm(hci_sk, FM_AUDIO_ENABLE_CMD,
                                 FM_AUDIO_ENABLE_CMD_LEN, data,
                                 HCI_CMD_TIMEOUT_MS);

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

extern "C" const hw_module_t HAL_MODULE_INFO_SYM =
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

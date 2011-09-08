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

#include <linux/ioctl.h>
#include <sound/intel_sst_ioctl.h>
#include "AudioHardwareALSA.h"
extern "C" {
#include "parameter_tuning_lib.h"
}
#define STEREO_MONO_VOLUME 0x02
#define STEREO_MONO_GAIN   0x01
#define MINVOL 0x0
#define MAXVOL 0x0 //The expected value of IHF and HS gain is 0.
#define MINGAIN 0x0
#define MAXGAIN 0xf

namespace android {
static int s_device_open(const hw_module_t*, const char*, hw_device_t**);
static int s_device_close(hw_device_t*);
static status_t lpe_control(int,uint32_t);
static status_t lpe_init(void);
static status_t lpe_set_master_volume(float volume);
static status_t lpe_set_master_gain(float gain);
static hw_module_methods_t s_module_methods = {
    open            : s_device_open
};

static status_t set_mono_in_firmware();
static status_t set_stereo_in_firmware();

static int prev_mode = 0x0;
static uint32_t prev_dev = 0x00;

typedef status_t (*controls_handler)();

typedef struct{
    controls_handler handler;
    char controls[128];
}controls_pair;

controls_pair lpe_controls[] = {
    {set_mono_in_firmware,"set mono in firmware"},
    {set_stereo_in_firmware,"set stereo in firmware"},
    {NULL,"end"}
};

static status_t set_mono_in_firmware()
{
    status_t result = NO_ERROR;
    int type = SST_STREAM_TYPE_NONE;
    int slot = 1;
    int str_id = SND_SST_DEVICE_CAPTURE;
    int size = sizeof(int);
    result = set_runtime_params(type, str_id, size, slot);
    return result;
}

static status_t set_stereo_in_firmware()
{
    status_t result = NO_ERROR;
    int type = SST_STREAM_TYPE_NONE;
    int slot = 3;
    int str_id = SND_SST_DEVICE_CAPTURE;
    int size = sizeof(int);
    result = set_runtime_params(type, str_id, size, slot);
    return result;
}

extern "C" const hw_module_t HAL_MODULE_INFO_SYM = {
    tag             : HARDWARE_MODULE_TAG,
    version_major   : 1,
    version_minor   : 0,
    id              : LPE_HARDWARE_MODULE_ID,
    name            : "mfld lpe module",
    author          : "Intel Corporation",
    methods         : &s_module_methods,
    dso             : 0,
    reserved        : { 0, },
};

static int s_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    lpe_device_t *dev;
    dev = (lpe_device_t *) malloc(sizeof(*dev));
    if (!dev) return -ENOMEM;

    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (hw_module_t *) module;
    dev->common.close = s_device_close;
    dev->init = lpe_init;
    dev->lpecontrol = lpe_control;
    dev->lpeSetMasterVolume = lpe_set_master_volume;
    dev->lpeSetMasterGain = lpe_set_master_gain;
    *device = &dev->common;
    return 0;
}

static int s_device_close(hw_device_t* device)
{
    free(device);
    return 0;
}

static status_t lpe_init()
{
    int status = 0;
    return status;
}

static status_t lpe_control(int Mode, uint32_t devices)
{
    status_t result = NO_ERROR;
    if((prev_mode != Mode || prev_dev!=devices) && ((devices & AudioSystem::DEVICE_IN_ALL)!=0x0)) {

        if(AudioSystem::DEVICE_IN_WIRED_HEADSET == devices) {
            result = (lpe_controls[0]).handler();
            LOGD("%s",(lpe_controls[0]).controls);
        }
        if(AudioSystem::DEVICE_IN_WIRED_HEADSET != devices) {
            result = (lpe_controls[1]).handler();
            LOGD("%s",(lpe_controls[1]).controls);
        }
    }

    return result;
}

static status_t set_volume(enum lpe_dev_types dev, char volume, char stereo_mono)
{
    snd_ppp_params_t *ppp_params = NULL;
    int retval = 0;
    char data1[1];
    char data2[1];
    int size = 0;

    size =sizeof(snd_ppp_params_t) + sizeof(ipc_ia_params_block_t) * 2 + sizeof(data1) + sizeof (data2);
    ppp_params = (snd_ppp_params_t *)malloc(size);
    if (!ppp_params) {
        LOGE("ppp_params malloc failed.\n");
        return NO_MEMORY;
    }
    data1[0] = stereo_mono;
    data2[0] = volume;

    retval = prepare_module_header(ppp_params, LPE_ALGO_TYPE_VOL_CTRL, dev, ENABLE);
    if (retval < 0) {
        LOGE("%d LPE_ALGO_TYPE_VOL_CTRL prepare_module_header failed.\n", dev);
        free(ppp_params);
        ppp_params = NULL;
        return BAD_VALUE;
    }

    retval = add_parameter_blocks(ppp_params, ALGO_PARAM_VOL_CTRL_STEREO_MONO, sizeof(data1), (char *)data1);
    if (retval < 0) {
        LOGE("%d ALGO_PARAM_VOL_CTRL_STEREO_MONO add_parameter_blocks failed.\n", dev);
        free(ppp_params);
        ppp_params = NULL;
        return BAD_VALUE;
    }

    retval = add_parameter_blocks(ppp_params, ALGO_PARAM_VOL_CTRL_GAIN, sizeof(data2), (char *)data2);
    if (retval < 0) {
        LOGE("%d_HS ALGO_PARAM_VOL_CTRL_GAIN add_parameter_blocks failed.\n", dev);
        free(ppp_params);
        ppp_params = NULL;
        return BAD_VALUE;
    }

    retval = set_parameters(ppp_params);
    if (retval < 0) {
        LOGE("%d set post processing parameters failed.\n", dev);
        free(ppp_params);
        ppp_params = NULL;
        return BAD_VALUE;
    }

    free(ppp_params);
    ppp_params = NULL;
    return NO_ERROR;
}

static status_t lpe_set_master_volume(float volume)
{
    status_t retval = 0;
    unsigned int minVol = MINVOL;
    unsigned int maxVol = MAXVOL;
    unsigned int vol = minVol + volume * (maxVol - minVol);

    if (vol > maxVol)
        vol = maxVol;
    if (vol < minVol)
        vol = minVol;

    retval = set_volume(LPE_DEV_HS, vol, STEREO_MONO_VOLUME);
    if (retval < 0) {
        LOGE("%d set post processing parameters failed.\n", LPE_DEV_HS);
        return BAD_VALUE;
    }

    retval = set_volume(LPE_DEV_IHF, vol, STEREO_MONO_VOLUME);
    if (retval < 0) {
        LOGE("%d set post processing parameters failed.\n", LPE_DEV_IHF);
        return BAD_VALUE;
    }
    return NO_ERROR;
}

static status_t lpe_set_master_gain(float gain)
{
    status_t retval = 0;
    unsigned int minGain = MINGAIN;
    unsigned int maxGain = MAXGAIN;
    unsigned int gain_value = minGain + gain * (maxGain - minGain);

    if (gain_value > maxGain)
        gain_value = maxGain;
    if (gain_value < minGain)
        gain_value = minGain;

    retval = set_volume(LPE_DEV_MIC1, gain_value, STEREO_MONO_GAIN);
    if (retval < 0) {
        LOGE("%d send_set_parameters failed.\n", LPE_DEV_MIC1);
        return BAD_VALUE;
    }

    retval = set_volume(LPE_DEV_MIC2, gain_value, STEREO_MONO_GAIN);
    if (retval < 0) {
        LOGE("%d send_set_parameters failed.\n", LPE_DEV_MIC2);
        return BAD_VALUE;
    }
    return NO_ERROR;
}

}

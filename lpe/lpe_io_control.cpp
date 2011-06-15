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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/ioctl.h>
#include <media/AudioRecord.h>
#include <signal.h>
#include <utils/Log.h>
#include <dlfcn.h>
#include "AudioHardwareALSA.h"
extern "C" {
#include "parameter_tuning_lib.h"
};

#define LOG_TAG "LPEModule"

namespace android {
static int s_device_open(const hw_module_t*, const char*, hw_device_t**);
static int s_device_close(hw_device_t*);
static status_t lpe_control(int,uint32_t);
static status_t lpe_init(void);
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
}

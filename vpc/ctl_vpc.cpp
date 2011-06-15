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

#define LOG_TAG "AudienceModule"
#include <linux/a1026.h>
#include "AudioHardwareALSA.h"
#include <media/AudioRecord.h>
#include <signal.h>
#include <utils/Log.h>
#include <utils/List.h>
#include <sys/stat.h>
#include <amc.h>
#include <properties.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <dlfcn.h>
#include <fcntl.h>

namespace android
{
#define ES305_FIRMWARE_FILE "/system/etc/vpimg.bin"
#define ES305_DEVICE_PATH "/dev/audience_es305"

static int fd_a1026 = -1;
static int support_a1026 = 1;
bool mA1026Init;
Mutex mA1026Lock;
static int enable1026 = 1;
static bool vr_mode_change = false;
static int new_pathid = -1;
static int prev_mode = 0x0;
static uint32_t prev_dev = 0x00;
static int at_thread_init = 0;
static int beg_call = 0;
#define A1026_PATH_INCALL_NO_NS_RECEIVER A1026_PATH_VR_NO_NS_RECEIVER

static int s_device_open(const hw_module_t*, const char*, hw_device_t**);
static int s_device_close(hw_device_t*);
static status_t amc(int,uint32_t);
static status_t volume(float);
static status_t doA1026_init(void);
static status_t disable_mixing(int);
static status_t enable_mixing(int,uint32_t);
static hw_module_methods_t s_module_methods = {
open            :
    s_device_open
};

extern "C" const hw_module_t HAL_MODULE_INFO_SYM = {
tag             :
    HARDWARE_MODULE_TAG,
    version_major   : 1,
    version_minor   : 0,
id              :
    VPC_HARDWARE_MODULE_ID,
name            : "mfld vpc module"
    ,
author          : "Intel Corporation"
    ,
methods         :
    &s_module_methods,
    dso             : 0,
reserved        : {
        0,
    },
};

static int s_device_open(const hw_module_t* module, const char* name,
                         hw_device_t** device)
{
    vpc_device_t *dev;
    dev = (vpc_device_t *) malloc(sizeof(*dev));
    if (!dev) return -ENOMEM;

    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (hw_module_t *) module;
    dev->common.close = s_device_close;
    dev->init = doA1026_init;
    dev->amcontrol = amc;
    dev->amcvolume = volume;
    dev->mix_disable = disable_mixing;
    dev->mix_enable = enable_mixing;
    *device = &dev->common;
    return 0;
}

static int s_device_close(hw_device_t* device)
{
    free(device);
    return 0;
}

static status_t doA1026_init()
{
    struct a1026img fwimg;
    char char_tmp = 0;
    unsigned char local_vpimg_buf[A1026_MAX_FW_SIZE];
    int rc = 0, fw_fd = -1;
    ssize_t nr;
    size_t remaining;
    struct stat fw_stat;
    char value[PROPERTY_VALUE_MAX];

    static const char *const fn = ES305_FIRMWARE_FILE;
    static const char *const path = ES305_DEVICE_PATH;

    fd_a1026 = open(path, O_RDWR | O_NONBLOCK, 0);

    if (fd_a1026 < 0) {
        LOGD("Cannot open %s %d\n", path, fd_a1026);
        support_a1026 = 0;
        rc = -1;
    } else {
        fw_fd = open(fn, O_RDONLY);
        if (fw_fd < 0) {
            LOGD("Fail to open %s\n", fn);
            rc = -1;
        } else {
            LOGD("Open %s success\n", fn);
            rc = fstat(fw_fd, &fw_stat);
            if (rc < 0) {
                LOGD("Cannot stat file %s: %s\n", fn, strerror(errno));
            } else {
                remaining = (int)fw_stat.st_size;
                LOGD("Firmware %s size %d\n", fn, remaining);

                if (remaining > sizeof(local_vpimg_buf)) {
                    LOGD("File %s size %d exceeds internal limit %d\n",
                         fn, remaining, sizeof(local_vpimg_buf));
                    rc = -1;
                } else {
                    nr = read(fw_fd, local_vpimg_buf, remaining);
                    if (nr != remaining) {
                        LOGD("Error reading firmware: %s\n", strerror(errno));
                        rc = -1;
                    } else {
                        fwimg.buf = local_vpimg_buf;
                        fwimg.img_size = nr;
                        LOGD("Total %d bytes put to user space buffer.\n", fwimg.img_size);
                        rc = ioctl(fd_a1026, A1026_BOOTUP_INIT, &fwimg);
                        if (!rc) {
                            LOGD("audience_a1026 init OK\n");
                            mA1026Init = 1;
                        } else {
                            LOGD("audience_a1026 init failed\n");
                        }
                    }
                }
            }
        }
        close (fw_fd);
    }
    close (fd_a1026);
    fd_a1026 = -1;
    return rc;
}

static status_t doAudience_A1026_Control(int path)
{
    int rc = 0;
    int retry = 4;

    if (!mA1026Init) {
        LOGD("Audience A1026 not initialized.\n");
        return NO_INIT;
    }

    mA1026Lock.lock();
    if (fd_a1026 < 0) {
        fd_a1026 = open(ES305_DEVICE_PATH, O_RDWR);
        if (fd_a1026 < 0) {
            LOGD("Cannot open audience_a1026 device (%d)\n", fd_a1026);
            mA1026Lock.unlock();
            return -1;
        }
    }

    do {
        rc = ioctl(fd_a1026, A1026_SET_CONFIG, &path);
        if (!rc) {
            break;
        }
    } while (--retry);

    if (rc < 0) {
        LOGD("A1026 do hard reset to recover from error!\n");
        rc = doA1026_init(); /* A1026 needs to do hard reset! */
        if (!rc) {
            /* after doA1026_init(), fd_a1026 is -1*/
            fd_a1026 = open(ES305_DEVICE_PATH, O_RDWR);
            if (fd_a1026 < 0) {
                LOGD("A1026 Fatal Error: unable to open A1026 after hard reset\n");
            } else {
                rc = ioctl(fd_a1026, A1026_SET_CONFIG, &path);
                if (!rc) {
                    LOGD("SET CONFIG OK after Hard Reset\n");
                } else {
                    LOGD("A1026 Fatal Error: unable to A1026_SET_CONFIG after hard reset\n");
                }
            }
        } else
            LOGD("A1026 Fatal Error: Re-init A1026 Failed\n");
    }

    if (fd_a1026 >= 0) {
        close(fd_a1026);
    }
    fd_a1026 = -1;
    mA1026Lock.unlock();

    return rc;

}
/* --------------------------------------- */
/* Control of the 2 I2S ports of the modem */
/* --------------------------------------- */
static status_t amc(int Mode, uint32_t devices)
{
    /* ------------------------------------------------------------- */
    /* Enter in this loop only if previous mode =! current mode ---- */
    /* or if previous device =! current dive and not capture device- */
    /* -------------------------------------------------------------- */
    if ((prev_mode != Mode || prev_dev!=devices) && ((devices & AudioSystem::DEVICE_OUT_ALL)!=0x0))
    {
        LOGD("mode = %d device = %d\n",Mode, devices);
        LOGD("previous mode = %d previous device = %d\n",prev_mode, prev_dev);

        /* Mode IN CALL */
        if (Mode == AudioSystem::MODE_IN_CALL) {
            LOGD("AMC CALL\n");
            /* start at thread only once */
            if (prev_mode != Mode && at_thread_init == 0) {
                amc_start(AUDIO_AT_CHANNEL_NAME, NULL);
                at_thread_init = 1;
                LOGD("AT thread start\n");
            }
            switch (devices) {
            case AudioSystem::DEVICE_OUT_EARPIECE:
            case AudioSystem::DEVICE_OUT_SPEAKER:
            case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
            case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:
                if (prev_mode!=AudioSystem::MODE_IN_CALL || prev_dev==AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET || beg_call == 0) {
                amc_disable(AMC_I2S1_RX);
                amc_disable(AMC_I2S2_RX);
                amc_configure_source(AMC_I2S1_RX, IFX_CLK1, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
                amc_configure_source(AMC_I2S2_RX, IFX_CLK0, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
                amc_configure_dest(AMC_I2S1_TX, IFX_CLK1, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
                amc_configure_dest(AMC_I2S2_TX, IFX_CLK0, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
                amc_route(AMC_RADIO_RX, AMC_I2S1_TX, AMC_ENDD);
                amc_route(AMC_I2S1_RX, AMC_RADIO_TX, AMC_ENDD);
                amc_route(AMC_I2S2_RX, AMC_I2S1_TX, AMC_ENDD);
                amc_route(AMC_SIMPLE_TONES, AMC_I2S1_TX, AMC_ENDD);
                amc_enable(AMC_I2S2_RX);
                amc_enable(AMC_I2S1_RX);
                beg_call = 1;
                }
                new_pathid = A1026_PATH_INCALL_RECEIVER;
                doAudience_A1026_Control(new_pathid);
                break;
            case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO:
            case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
            case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
                amc_disable(AMC_I2S1_RX);
                amc_disable(AMC_I2S2_RX);
                amc_configure_source(AMC_I2S1_RX, IFX_CLK1, IFX_MASTER,  IFX_SR_8KHZ, IFX_SW_16, IFX_PCM, I2S_SETTING_NORMAL, IFX_MONO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
                amc_configure_source(AMC_I2S2_RX, IFX_CLK0, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
                amc_configure_dest(AMC_I2S1_TX, IFX_CLK1, IFX_MASTER,  IFX_SR_8KHZ, IFX_SW_16, IFX_PCM, I2S_SETTING_NORMAL, IFX_MONO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
                amc_configure_dest(AMC_I2S2_TX, IFX_CLK0, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
                amc_route(AMC_RADIO_RX, AMC_I2S1_TX, AMC_ENDD);
                amc_route(AMC_I2S1_RX, AMC_RADIO_TX, AMC_ENDD);
                amc_route(AMC_I2S2_RX, AMC_I2S1_TX, AMC_ENDD);
                amc_route(AMC_SIMPLE_TONES, AMC_I2S1_TX, AMC_ENDD);
                amc_enable(AMC_I2S2_RX);
                amc_enable(AMC_I2S1_RX);
                new_pathid = A1026_PATH_INCALL_BT;
                doAudience_A1026_Control(new_pathid);
                break;
            default:
                break;
            }
            prev_mode = Mode;
            prev_dev = devices;
            return NO_ERROR;
        }
        /* Used to disable modem I2S at the end of the call */
        if (prev_mode == AudioSystem::MODE_IN_CALL && Mode == AudioSystem::MODE_NORMAL) {
            LOGD("AMC FROM IN CALL TO NORMAL\n");
            amc_disable(AMC_I2S1_RX);
            amc_disable(AMC_I2S2_RX);
            new_pathid = A1026_PATH_SUSPEND;
            doAudience_A1026_Control(new_pathid);
            beg_call = 0;
            prev_mode = Mode;
            prev_dev = devices;
            return NO_ERROR;
        }
        else {
            LOGD("NOTHING TO DO WITH AMC\n");
            return NO_ERROR;
        }
    }
    else {
        LOGD("CAPTURE DEVICE NOTHING TO DO\n");
        return NO_ERROR;
    }
}
/* ---------------- */
/* Volume managment */
/* ---------------- */
static status_t volume(float volume)
{
    int gain=0;
    if (at_thread_init == 1) {
        gain = volume * 100;
        gain = (gain >= 100) ? 100 : gain;
        gain = (gain <= 0) ? 0 : gain;
        amc_setGaindest(AMC_I2S1_TX, gain);
    }
    return NO_ERROR;
}

/*------------------*/
/*   IS21 disable   */
/*------------------*/

static status_t disable_mixing(int mode)
{
    return NO_ERROR;
}
/*------------------*/
/*   IS21 enable   */
/*------------------*/

static status_t enable_mixing(int mode,uint32_t device)
{
    if (mode==AudioSystem::MODE_IN_CALL)
        if (device == AudioSystem::DEVICE_IN_VOICE_CALL){
            amc_route(AMC_RADIO_RX, AMC_I2S1_TX, AMC_I2S2_TX, AMC_ENDD);
            amc_route(AMC_I2S1_RX, AMC_RADIO_TX, AMC_I2S2_TX, AMC_ENDD);
            amc_route(AMC_I2S2_RX, AMC_I2S1_TX, AMC_ENDD);}
    return NO_ERROR;
}

}

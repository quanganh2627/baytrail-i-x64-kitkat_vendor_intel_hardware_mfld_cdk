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

#define LOG_TAG "VPCModule"
#include <utils/Log.h>

#include "AudioHardwareALSA.h"
#include "amc.h"
#include "bt.h"
#include "msic.h"
#include <linux/a1026.h>
#include <sys/stat.h>
#include <properties.h>

namespace android
{
#define ES305_DEVICE_PATH "/dev/audience_es305"
#define MODEM_TTY_RETRY 60
#define MAX_LABEL_SIZE 100
static int fd_a1026 = -1;
static int support_a1026 = 1;
bool mA1026Init;
Mutex mA1026Lock;
static int prev_mode = 0x0;
static uint32_t prev_dev = 0x00;
static int at_thread_init = 0;
static int beg_call = 0;
static bool tty_call = false;
static bool bt_call = false;
static bool mixing_enable = false;
static bool voice_call_recording = false;

static int s_device_open(const hw_module_t*, const char*, hw_device_t**);
static int s_device_close(hw_device_t*);
static status_t vpc(int,uint32_t);
static status_t volume(float);
static status_t es305_init(void);
static status_t disable_mixing(int);
static status_t enable_mixing(int,uint32_t);
static status_t enable_tty(bool);
static status_t doAudience_A1026_suspend();
static hw_module_methods_t s_module_methods = {
open            :
    s_device_open
};

extern "C" const hw_module_t HAL_MODULE_INFO_SYM = {

    tag : HARDWARE_MODULE_TAG,
    version_major : 1,
    version_minor : 0,
    id : VPC_HARDWARE_MODULE_ID,
    name : "mfld vpc module",
    author : "Intel Corporation",
    methods : &s_module_methods,
    dso : 0,
    reserved : { 0, },
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
    dev->init = es305_init;
    dev->amcontrol = vpc;
    dev->amcvolume = volume;
    dev->mix_disable = disable_mixing;
    dev->mix_enable = enable_mixing;
    dev->tty_enable = enable_tty;
    *device = &dev->common;
    return 0;
}

static int s_device_close(hw_device_t* device)
{
    free(device);
    return 0;
}

static int get_label()
{
    unsigned char firstCharLabel[4] = {0x80, 0x20, 0x00, 0x00};
    unsigned char nextCharLabel[4] = {0x80, 0x21, 0x00, 0x00};
    unsigned char label[4] = {0x00, 0x00, 0x00, 0x01};
    unsigned char *label_name;
    int i = 1, size = 4, rc;

    label_name = (unsigned char*)malloc(sizeof(char)*MAX_LABEL_SIZE);

    rc = ioctl(fd_a1026, A1026_ENABLE_CLOCK);
    if (rc) {
        LOGE("Enable clock error\n");
        free(label_name);
        return -1;
    }

    // get first build label char
    rc = write(fd_a1026, firstCharLabel, size);
    if (rc != size)
    {
        LOGE("A1026_WRITE_MSG (0x%.2x%.2x%.2x%.2x) error, ret = %d\n", firstCharLabel[0], firstCharLabel[1], firstCharLabel[2], firstCharLabel[3], rc);
        free(label_name);
        return -1;
    }
    usleep(20000);

    rc = read(fd_a1026, label, size);
    if (rc != size)
    {
        LOGE("A1026_READ_DATA error, ret = %d\n", rc);
        free(label_name);
        return -1;
    }
    label_name[0] = label[3];

    // get next build label char
    while (label[3] && (i < MAX_LABEL_SIZE))
    {
        rc = write(fd_a1026, nextCharLabel, size);
        if (rc != 4)
        {
            LOGE("A1026_WRITE_MSG (0x%.2x%.2x%.2x%.2x) error, rc = %d\n", nextCharLabel[0], nextCharLabel[1], nextCharLabel[2], nextCharLabel[3], rc);
            free(label_name);
            return -1;
        }
        usleep(20000);

        rc = read(fd_a1026, label, size);
        if (rc != 4)
        {
            LOGE("A1026_READ_DATA error, ret = %d\n", rc);
            free(label_name);
            return -1;
        }
        label_name[i] = label[3];
        i++;
    }

    if (i < MAX_LABEL_SIZE)
        LOGD("FW name = %s\n",label_name);
    else
        LOGE("FW name not found\n");

    free(label_name);

    return 0;
}

static status_t es305_init()
{
    int rc = 0;
    ssize_t nr;
    AT_STATUS cmdStatus;
    int tries = 0;
    char value[PROPERTY_VALUE_MAX];
    static const char *const path = ES305_DEVICE_PATH;

    if (at_thread_init == 0) {
        cmdStatus = amc_start(AUDIO_AT_CHANNEL_NAME);
        while (cmdStatus != AT_OK && tries < MODEM_TTY_RETRY) {
            cmdStatus = amc_start(AUDIO_AT_CHANNEL_NAME);
            LOGD("AT thread retry\n");
            tries++;
            sleep(1);
        }
        if (cmdStatus == AT_OK) {
            LOGD("AT thread started\n");
            at_thread_init = 1;
        }
        else
            return NO_INIT;
    }

    msic::pcm_init();

#ifdef CUSTOM_BOARD_WITH_AUDIENCE

    fd_a1026 = open(path, O_RDWR | O_NONBLOCK, 0);

    if (fd_a1026 < 0) {
        LOGE("Cannot open %s %d\n", path, fd_a1026);
        support_a1026 = 0;
        rc = -1;
    }
    rc = ioctl(fd_a1026, A1026_ENABLE_CLOCK);
    if (rc) {
        LOGE("Enable clock error\n");
        return -1;
    }
    rc = ioctl(fd_a1026, A1026_BOOTUP_INIT);
    if (!rc) {
        LOGD("audience_a1026 init OK\n");
        mA1026Init = 1;
        rc = get_label();
        if (rc)
            LOGE("Unable to get FW Label");
    }
    else
        LOGE("audience_a1026 init failed\n");

    doAudience_A1026_suspend();

    close (fd_a1026);
    fd_a1026 = -1;
#endif
    return 0;
}

static status_t doAudience_A1026_suspend()
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
            LOGE("Cannot open audience_a1026 device (%d)\n", fd_a1026);
            mA1026Lock.unlock();
            return -1;
        }
    }

    do {
        rc = ioctl(fd_a1026, A1026_SUSPEND);
        if (!rc) {
            break;
        }
    } while (--retry);

    if (rc < 0) {
        LOGD("A1026 do hard reset to recover from error!\n");
        rc = es305_init(); /* A1026 needs to do hard reset! */
        if (!rc) {
            /* after es305_init(), fd_a1026 is -1*/
            fd_a1026 = open(ES305_DEVICE_PATH, O_RDWR);
            if (fd_a1026 < 0) {
                LOGE("A1026 Fatal Error: unable to open A1026 after hard reset\n");
            } else {
                rc = ioctl(fd_a1026, A1026_SUSPEND);
                if (!rc) {
                    LOGI("Set_config Ok after Hard Reset\n");
                } else {
                    LOGE("A1026 Fatal Error: unable to A1026_SET_CONFIG after hard reset\n");
                }
            }
        } else
            LOGE("A1026 Fatal Error: Re-init A1026 Failed\n");
    }

    if (fd_a1026 >= 0) {
        close(fd_a1026);
    }
    fd_a1026 = -1;
    mA1026Lock.unlock();

    return rc;

}

/* -------------------------------- */
/*     Audience configuration       */
/* -------------------------------- */

static int set_acoustic(uint32_t param)
{
    FILE *fd;
    unsigned char *i2c_cmd=NULL;
    int ret;
    int size,i,rc;

    char device_name[80] = "/system/etc/phonecall_";

    switch (param) {
        case AudioSystem::DEVICE_OUT_EARPIECE:
            strcat(device_name,"close_talk.bin");
            bt_call = false;
            break;
        case AudioSystem::DEVICE_OUT_SPEAKER:
            strcat(device_name,"speaker_far_talk.bin");
            bt_call = false;
            break;
        case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
            strcat(device_name,"headset_close_talk.bin");
            bt_call = false;
            break;
        case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:
            strcat(device_name,"headset_close_talk.bin");
            bt_call = false;
            break;
        case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO:
        case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
            if(bt_call == true)
                return 0;
            strcat(device_name,"bt_hsp.bin");
            bt_call = true;
            break;
        case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
            if(bt_call == true)
                return 0;
            strcat(device_name,"bt_carkit.bin");
            bt_call = true;
            break;
        case 5:
            strcat(device_name,"tty.bin");
        default:
            break;
    }

    mA1026Lock.lock();

    fd = fopen(device_name,"r");
    if (fd < 0){
            LOGD("Cannot open %s.bin\n",device_name);
            mA1026Lock.unlock();
            return -1;}

    fseek(fd,0,SEEK_END);
    size = ftell(fd);
    fseek(fd,0,SEEK_SET);
    i2c_cmd = (unsigned char*)malloc(sizeof(char)*size);
    if(i2c_cmd == NULL){
        LOGE("Could not allocate memory\n");
        mA1026Lock.unlock();
        return -1;}
    else
        memset(i2c_cmd,'\0',size);

    ret = fread(i2c_cmd,1,size,fd);
    if (ret<size){
        LOGE("Error while reading config file\n");
        mA1026Lock.unlock();
        return -1;}

    if (!mA1026Init) {
        LOGD("Audience A1026 not initialized.\n");
        mA1026Lock.unlock();
        return -1;
    }

    if (fd_a1026 < 0) {
        fd_a1026 = open(ES305_DEVICE_PATH, O_RDWR);
        if (fd_a1026 < 0) {
            LOGE("Cannot open audience_a1026 device (%d)\n", fd_a1026);
            mA1026Lock.unlock();
            return -1;
        }
    }
    if(!beg_call){
        rc = ioctl(fd_a1026, A1026_ENABLE_CLOCK);
        if (rc) {
            LOGE("Enable clock error\n");
            mA1026Lock.unlock();
            return -1;
        }
    }
    ret = write(fd_a1026,i2c_cmd,size);
    if(ret!=size){
        LOGE("Audience write error \n");
        mA1026Lock.unlock();
        return -1;
    }
    if (fd_a1026 >= 0) {
        close(fd_a1026);
    }
    fd_a1026 = -1;
    fclose(fd);
    free(i2c_cmd);
    mA1026Lock.unlock();

    return ret;

}
/* --------------------------------------- */
/* Control of the 2 I2S ports of the modem */
/* --------------------------------------- */
static status_t vpc(int Mode, uint32_t devices)
{
    AT_STATUS rts;
    int ret=0,tty_call_dev=5;

    /* Must be remove when gain will be integrated in the MODEM  (IMC) */
    int ModemGain = 100;

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
            LOGD("VPC Call\n");

            if(at_thread_init!=0)
            {
                rts = check_tty();
                if (rts == AT_WRITE_ERROR || rts == AT_ERROR)
                {
                    LOGE("Amc Error\n");
                    amc_stop();
                    amc_start(AUDIO_AT_CHANNEL_NAME);
                }
            }

#ifdef CUSTOM_BOARD_WITH_AUDIENCE
            /* Audience configurations for each devices */
            if(tty_call==true)
                ret=set_acoustic(tty_call_dev);
            else
                ret=set_acoustic(devices);

            if(ret == -1)
                return NO_INIT;
#endif
            /* Modem configuration for each devices */
            switch (devices) {
            case AudioSystem::DEVICE_OUT_EARPIECE:
            case AudioSystem::DEVICE_OUT_SPEAKER:
            case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
            case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:
                if (prev_mode!=AudioSystem::MODE_IN_CALL || prev_dev==AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET || beg_call == 0) {
                bt::pcm_disable();
                amc_disable(AMC_I2S1_RX);
                amc_disable(AMC_I2S2_RX);
                if (tty_call == false){
                    amc_configure_source(AMC_I2S1_RX, IFX_CLK1, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
                    amc_configure_source(AMC_I2S2_RX, IFX_CLK0, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
                    amc_configure_dest(AMC_I2S1_TX, IFX_CLK1, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
                    amc_configure_dest(AMC_I2S2_TX, IFX_CLK0, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);}
                else if(tty_call == true){
                    amc_configure_source(AMC_I2S1_RX, IFX_CLK1, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_TTY_S);
                    amc_configure_source(AMC_I2S2_RX, IFX_CLK0, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_TTY_S);
                    amc_configure_dest(AMC_I2S1_TX, IFX_CLK1, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_TTY_D);
                    amc_configure_dest(AMC_I2S2_TX, IFX_CLK0, IFX_MASTER,  IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_TTY_D);
                }
                amc_route(AMC_RADIO_RX, AMC_I2S1_TX, AMC_ENDD);
                amc_route(AMC_I2S1_RX, AMC_RADIO_TX, AMC_ENDD);
                amc_route(AMC_I2S2_RX, AMC_I2S1_TX, AMC_ENDD);
                amc_route(AMC_SIMPLE_TONES, AMC_I2S1_TX, AMC_ENDD);
                amc_setGaindest(AMC_RADIO_TX, ModemGain); /*must be removed when gain will be integrated in MODEM (IMC) */
                amc_enable(AMC_I2S2_RX);
                mixing_enable = true;
                amc_enable(AMC_I2S1_RX);
                msic::pcm_enable();
                }
                break;
            case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO:
            case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
            case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
                msic::pcm_disable();
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
                amc_setGaindest(AMC_RADIO_TX, ModemGain); /*must be removed when gain will be integrated in MODEM (IMC) */
                amc_enable(AMC_I2S2_RX);
                mixing_enable = true;
                amc_enable(AMC_I2S1_RX);
                bt::pcm_enable();
                break;
            default:
                break;
            }
            prev_mode = Mode;
            prev_dev = devices;
            beg_call = 1;
            return NO_ERROR;
        }
        /* Disable modem I2S at the end of the call */
        if (prev_mode == AudioSystem::MODE_IN_CALL && Mode == AudioSystem::MODE_NORMAL) {
            LOGV("VPC from in_call to normal\n");
            bt::pcm_disable();
            msic::pcm_disable();
            amc_disable(AMC_I2S1_RX);
            amc_disable(AMC_I2S2_RX);
            mixing_enable = false;
#ifdef CUSTOM_BOARD_WITH_AUDIENCE
            doAudience_A1026_suspend();
#endif
            beg_call = 0;
            bt_call = false;
            prev_mode = Mode;
            prev_dev = devices;
            return NO_ERROR;
        }
        else {
            LOGV("Nothing to do with vpc\n");
            return NO_ERROR;
        }
    }
    else {
        LOGV("Capture device nothing to do\n");
        return NO_ERROR;
    }
}

/* ---------------- */
/* Volume managment */
/* ---------------- */

static status_t volume(float volume)
{
    int gain=0;
    int range = 48; /* volume gain control must be remved when integrated in the MODEM */
    if (at_thread_init == 1) {
        gain = volume * range + 40;
        gain = (gain >= 88) ? 88 : gain;
        gain = (gain <= 40) ? 40 : gain;
        amc_setGaindest(AMC_I2S1_TX, gain);
    }
    return NO_ERROR;
}

/*------------------*/
/*   I2S1 disable   */
/*------------------*/

static status_t disable_mixing(int mode)
{
    if(mixing_enable) {
        LOGD("disable mixing");
        amc_disable(AMC_I2S2_RX);
        mixing_enable = false;
    }
    if(voice_call_recording) {
        voice_call_recording = false;
    }
    return NO_ERROR;
}

/*------------------*/
/*   I2S1 enable    */
/*------------------*/

static status_t enable_mixing(int mode, uint32_t device)
{
    if (mode==AudioSystem::MODE_IN_CALL) {
        if (device == AudioSystem::DEVICE_IN_VOICE_CALL){
            if(!voice_call_recording) {
                // Enable voice call record
                LOGD("voice in call recording");
                amc_route(AMC_RADIO_RX, AMC_I2S1_TX, AMC_I2S2_TX, AMC_ENDD);
                amc_route(AMC_I2S1_RX, AMC_RADIO_TX, AMC_I2S2_TX, AMC_ENDD);
                voice_call_recording = true;
            }
        } else {
            if(!mixing_enable) {
                // Enable alert mixing
                LOGD("enable mixing");
                amc_enable(AMC_I2S2_RX);
                mixing_enable = true;
            }
        }
    }
    return NO_ERROR;
}

/*-----------------*/
/*    Enable TTY   */
/*-----------------*/

static status_t enable_tty(bool tty)
{
    if (tty == true){
        tty_call = true;
        LOGD("TTY TRUE\n");}
    else
        tty_call = false;
    return NO_ERROR;
}
}

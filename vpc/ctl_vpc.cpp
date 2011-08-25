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

#define LOG_TAG "VPC_Module"
#include <utils/Log.h>

#include <utils/threads.h>
#include <media/AudioSystem.h>

#include "acoustic.h"
#include "amc.h"
#include "bt.h"
#include "msic.h"

#include "../../../alsa_sound/AudioHardwareALSA.h" // where is declared "vpc_device_t"

namespace android
{

/*===========================================================================*/
/* API                                                                       */
/*===========================================================================*/

static status_t vpc_init(void);
static status_t vpc(int, uint32_t);
static status_t volume(float);
static status_t disable_mixing(int);
static status_t enable_mixing(int, uint32_t);
static status_t enable_tty(bool);


/*===========================================================================*/
/* Definitions                                                               */
/*===========================================================================*/

#define ES305_DEVICE_PATH "/dev/audience_es305"
#define MODEM_TTY_RETRY 60

/*---------------------------------------------------------------------------*/
/* Global variables                                                          */
/*---------------------------------------------------------------------------*/

Mutex vpc_lock;

static int      prev_mode            = 0x0;
static uint32_t prev_dev             = 0x00;
static int      at_thread_init       = 0;
static int      beg_call             = 0;
static bool     tty_call             = false;
static bool     mixing_enable        = false;
static bool     voice_call_recording = false;
static uint32_t device_out_defaut    = 0x8000;


/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/
static status_t vpc_init()
{
    vpc_lock.lock();
    LOGD("Initialize VPC\n");

    if (at_thread_init == 0)
    {
        AT_STATUS cmdStatus = amc_start(AUDIO_AT_CHANNEL_NAME);
        int tries = 0;
        while (cmdStatus != AT_OK && tries < MODEM_TTY_RETRY)
        {
            cmdStatus = amc_start(AUDIO_AT_CHANNEL_NAME);
            LOGD("AT thread retry\n");
            tries++;
            sleep(1);
        }
        if (cmdStatus != AT_OK) goto return_error;

        LOGD("AT thread started\n");
        at_thread_init = 1;
    }

    msic::pcm_init();

#ifdef CUSTOM_BOARD_WITH_AUDIENCE
    int rc;
    rc = acoustic::process_init();
    if (rc) goto return_error;
#endif

    LOGD("VPC Init OK\n");
    vpc_lock.unlock();
    return NO_ERROR;

return_error:

    LOGE("VPC Init failed\n");
    vpc_lock.unlock();
    return NO_INIT;
}

/*---------------------------------------------------------------------------*/
/* Platform voice paths control                                              */
/*---------------------------------------------------------------------------*/
static status_t vpc(int Mode, uint32_t devices)
{
    vpc_lock.lock();

    /* Must be remove when gain will be integrated in the MODEM  (IMC) */
    int ModemGain = 100;

    /* ------------------------------------------------------------- */
    /* Enter in this loop only if previous mode != current mode      */
    /* or if previous device != current dive and not capture device  */
    /* ------------------------------------------------------------- */
    if ((prev_mode != Mode || prev_dev != devices) && ((devices & AudioSystem::DEVICE_OUT_ALL) != 0x0))
    {
        LOGD("mode = %d device = %d\n",Mode, devices);
        LOGD("previous mode = %d previous device = %d\n",prev_mode, prev_dev);

        /* Mode IN CALL */
        if (Mode == AudioSystem::MODE_IN_CALL)
        {
            LOGD("VPC IN_CALL\n");

            if (at_thread_init != 0)
            {
                AT_STATUS rts = check_tty();
                if (rts == AT_WRITE_ERROR || rts == AT_ERROR)
                {
                    LOGE("AMC Error\n");
                    amc_stop();
                    amc_start(AUDIO_AT_CHANNEL_NAME);
                }
            }

#ifdef CUSTOM_BOARD_WITH_AUDIENCE
            /* Audience configurations for each devices */
            uint32_t device_profile = (tty_call == true) ? device_out_defaut : devices;
            int ret;
            ret = acoustic::process_profile(device_profile, beg_call);

            if (ret) goto return_error;
#endif

            /* Modem configuration for each devices */
            switch (devices)
            {
            case AudioSystem::DEVICE_OUT_EARPIECE:
            case AudioSystem::DEVICE_OUT_SPEAKER:
            case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
            case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:
                if (prev_mode != AudioSystem::MODE_IN_CALL || prev_dev == AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET || beg_call == 0)
                {
                    bt::pcm_disable();
                    amc_disable(AMC_I2S1_RX);
                    amc_disable(AMC_I2S2_RX);
                    if (tty_call == false)
                    {
                        amc_configure_source(AMC_I2S1_RX, IFX_CLK1, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
                        amc_configure_source(AMC_I2S2_RX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
                        amc_configure_dest(AMC_I2S1_TX, IFX_CLK1, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
                        amc_configure_dest(AMC_I2S2_TX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
                    }
                    else if (tty_call == true)
                    {
                        amc_configure_source(AMC_I2S1_RX, IFX_CLK1, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_TTY_S);
                        amc_configure_source(AMC_I2S2_RX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_TTY_S);
                        amc_configure_dest(AMC_I2S1_TX, IFX_CLK1, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_TTY_D);
                        amc_configure_dest(AMC_I2S2_TX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_TTY_D);
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
                amc_configure_source(AMC_I2S1_RX, IFX_CLK1, IFX_MASTER, IFX_SR_8KHZ, IFX_SW_16, IFX_PCM, I2S_SETTING_NORMAL, IFX_MONO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
                amc_configure_source(AMC_I2S2_RX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
                amc_configure_dest(AMC_I2S1_TX, IFX_CLK1, IFX_MASTER, IFX_SR_8KHZ, IFX_SW_16, IFX_PCM, I2S_SETTING_NORMAL, IFX_MONO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
                amc_configure_dest(AMC_I2S2_TX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
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
        }
        /* Disable modem I2S at the end of the call */
        else if (prev_mode == AudioSystem::MODE_IN_CALL && Mode == AudioSystem::MODE_NORMAL)
        {
            LOGD("VPC from IN_CALL to NORMAL\n");
            bt::pcm_disable();
            msic::pcm_disable();
            amc_disable(AMC_I2S1_RX);
            amc_disable(AMC_I2S2_RX);
            mixing_enable = false;

#ifdef CUSTOM_BOARD_WITH_AUDIENCE
            acoustic::process_suspend();
#endif

            beg_call = 0;
            prev_mode = Mode;
            prev_dev = devices;
        }
        else
        {
            LOGD("Nothing to do with VPC\n");
        }
    }
    else
    {
        LOGD("Capture device nothing to do\n");
    }

    vpc_lock.unlock();
    return NO_ERROR;

return_error:

    vpc_lock.unlock();
    return NO_INIT;
}

/*---------------------------------------------------------------------------*/
/* Volume managment                                                          */
/*---------------------------------------------------------------------------*/
static status_t volume(float volume)
{
    vpc_lock.lock();

    int gain = 0;
    int range = 48; /* volume gain control must be remved when integrated in the MODEM */
    if (at_thread_init == 1)
    {
        gain = volume * range + 40;
        gain = (gain >= 88) ? 88 : gain;
        gain = (gain <= 40) ? 40 : gain;
        amc_setGaindest(AMC_I2S1_TX, gain);
    }

    vpc_lock.unlock();
    return NO_ERROR;
}

/*---------------------------------------------------------------------------*/
/* I2S2 disable                                                              */
/*---------------------------------------------------------------------------*/
static status_t disable_mixing(int mode)
{
    vpc_lock.lock();

    if (mixing_enable)
    {
        LOGD("disable mixing");
        amc_disable(AMC_I2S2_RX);
        mixing_enable = false;
    }
    if (voice_call_recording)
    {
        voice_call_recording = false;
    }

    vpc_lock.unlock();
    return NO_ERROR;
}

/*---------------------------------------------------------------------------*/
/* I2S2 enable                                                               */
/*---------------------------------------------------------------------------*/
static status_t enable_mixing(int mode, uint32_t device)
{
    vpc_lock.lock();

    if (mode == AudioSystem::MODE_IN_CALL)
    {
        if (device == AudioSystem::DEVICE_IN_VOICE_CALL)
        {
            if (!voice_call_recording)
            {
                // Enable voice call record
                LOGD("voice in call recording");
                amc_route(AMC_RADIO_RX, AMC_I2S1_TX, AMC_I2S2_TX, AMC_ENDD);
                amc_route(AMC_I2S1_RX, AMC_RADIO_TX, AMC_I2S2_TX, AMC_ENDD);
                voice_call_recording = true;
            }
        }
        else
        {
            if (!mixing_enable)
            {
                // Enable alert mixing
                LOGD("enable mixing");
                amc_enable(AMC_I2S2_RX);
                mixing_enable = true;
            }
        }
    }

    vpc_lock.unlock();
    return NO_ERROR;
}

/*---------------------------------------------------------------------------*/
/* Enable TTY                                                                */
/*---------------------------------------------------------------------------*/
static status_t enable_tty(bool tty)
{
    vpc_lock.lock();

    if (tty == true)
    {
        tty_call = true;
        LOGD("TTY TRUE\n");
    }
    else
    {
        tty_call = false;
    }

    vpc_lock.unlock();
    return NO_ERROR;
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
    id            : VPC_HARDWARE_MODULE_ID,
    name          : "mfld vpc module",
    author        : "Intel Corporation",
    methods       : &s_module_methods,
    dso           : 0,
    reserved      : { 0, },
};

static int s_device_open(const hw_module_t* module, const char* name,
                         hw_device_t** device)
{
    vpc_device_t *dev;
    dev = (vpc_device_t *) malloc(sizeof(*dev));
    if (!dev) return -ENOMEM;

    memset(dev, 0, sizeof(*dev));

    dev->common.tag     = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module  = (hw_module_t *) module;
    dev->common.close   = s_device_close;
    dev->init           = vpc_init;
    dev->amcontrol      = vpc;
    dev->amcvolume      = volume;
    dev->mix_disable    = disable_mixing;
    dev->mix_enable     = enable_mixing;
    dev->tty_enable     = enable_tty;
    *device = &dev->common;
    return 0;
}

static int s_device_close(hw_device_t* device)
{
    free(device);
    return 0;
}

}


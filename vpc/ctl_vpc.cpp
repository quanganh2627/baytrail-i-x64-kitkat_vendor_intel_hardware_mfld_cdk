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
#include <hardware_legacy/AudioSystemLegacy.h>

#include "acoustic.h"
#include "AudioModemControl.h"
#include "bt.h"
#include "msic.h"
#include "volume_keys.h"
#include "stmd.h"
#include "vpc_hardware.h"

namespace android_audio_legacy
{

/*===========================================================================*/
/* API                                                                       */
/*===========================================================================*/

static int vpc_init(void);
static int vpc_params(int mode, uint32_t device);
static void vpc_set_mode(int mode);
static void vpc_set_modem_state(int state);
static int vpc_route(vpc_route_t);
static int vpc_volume(float);
static int vpc_mixing_disable(bool isOut);
static int vpc_mixing_enable(bool isOut, uint32_t device);
static int vpc_tty(vpc_tty_t);
static int vpc_bt_nrec(vpc_bt_nrec_t);


/*===========================================================================*/
/* Definitions                                                               */
/*===========================================================================*/

#define DEVICE_OUT_BLUETOOTH_SCO_ALL (AudioSystem::DEVICE_OUT_BLUETOOTH_SCO | \
                                      AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET | \
                                      AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT)

static const AMC_TTY_STATE translate_vpc_to_amc_tty[] = {
    AMC_TTY_OFF, /*[VPC_TTY_OFF] */
    AMC_TTY_FULL, /*[VPC_TTY_FULL] */
    AMC_TTY_VCO, /*[VPC_TTY_VCO] */
    AMC_TTY_HCO, /*[VPC_TTY_HCO] */
};


/*---------------------------------------------------------------------------*/
/* Global variables                                                          */
/*---------------------------------------------------------------------------*/
using android::Mutex;
Mutex vpc_lock;

static int       prev_mode            = AudioSystem::MODE_NORMAL;
static int       current_mode         = AudioSystem::MODE_NORMAL;
static uint32_t  prev_device          = 0x0000;
static uint32_t  current_device       = 0x0000;
static uint32_t  device_out_defaut    = 0x8000;
static bool      at_thread_init       = false;
static bool      call_established     = false;
static AMC_TTY_STATE tty_call             = AMC_TTY_OFF;
static bool      mixing_enable        = false;
static bool      voice_call_recording = false;
static bool      bt_acoustic          = true;
static int       modem_gain_dl        = 0;
static int       modem_gain_ul        = 100; // +6 dB
static int       prev_modem_status    = MODEM_DOWN;
static int       modem_status         = MODEM_DOWN;

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/
static int vpc_init(void)
{
    vpc_lock.lock();
    LOGD("Initialize VPC\n");

    if (at_thread_init == false)
    {
        AT_STATUS cmd_status = at_start(AUDIO_AT_CHANNEL_NAME);
        if (cmd_status != AT_OK) goto return_error;
        LOGD("AT thread started\n");
        at_thread_init = true;
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
/* State machine parameters                                                  */
/*---------------------------------------------------------------------------*/
static int vpc_params(int mode, uint32_t device)
{
    vpc_lock.lock();

    current_mode = mode;
    current_device = device;

    LOGD("vpc_params mode = %d device = %d\n", current_mode, current_device);
    LOGD("vpc_params previous mode = %d previous device = %d\n", prev_mode, prev_device);

    vpc_lock.unlock();
    return NO_ERROR;
}

/*---------------------------------------------------------------------------*/
/* State machine parameters                                                  */
/*---------------------------------------------------------------------------*/
static void vpc_set_mode(int mode)
{
    vpc_lock.lock();

    current_mode = mode;

    LOGD("%s: mode = %d\n", __FUNCTION__, current_mode);
    LOGD("%s: previous mode = %d\n", __FUNCTION__, prev_mode);

    vpc_lock.unlock();
}

/*---------------------------------------------------------------------------*/
/* Modem Status: each time modem status is changed, this fonction will be    */
/* called from HAL                                                           */
/*---------------------------------------------------------------------------*/
static void vpc_set_modem_state(int status)
{
    LOGD("vpc_set_modem_state modem_status");
    vpc_lock.lock();

    if(status != modem_status) {
        prev_modem_status = modem_status;
        modem_status = status;
    }

    LOGD("vpc_set_modem_state modem_status = %d \n", modem_status);

    vpc_lock.unlock();
}

/*---------------------------------------------------------------------------*/
/* Platform voice paths control                                              */
/*---------------------------------------------------------------------------*/
static int vpc_route(vpc_route_t route)
{
    vpc_lock.lock();

    int ret;
    uint32_t device_profile;

    /* -------------------------------------------------------------- */
    /* Enter in this loop only for output device as they trig the     */
    /* establishment of the route                                     */
    /* -------------------------------------------------------------- */
    if (current_device & AudioSystem::DEVICE_OUT_ALL)
    {
        LOGD("mode = %d device = %d modem status = %d\n", current_mode, current_device, modem_status);
        LOGD("previous mode = %d previous device = %d previous modem status = %d\n", prev_mode, prev_device, prev_modem_status);

        if (route == VPC_ROUTE_OPEN)
        {
            LOGD("VPC_ROUTE_OPEN request\n");
            /* --------------------------------------------- */
            /* Volume buttons & power management             */
            /* --------------------------------------------- */
            if ((current_mode == AudioSystem::MODE_IN_CALL) && (prev_mode != AudioSystem::MODE_IN_CALL))
            {
                ret = volume_keys::wakeup_enable();
                if (ret) goto return_error;
            }
            else if ((current_mode != AudioSystem::MODE_IN_CALL) && (prev_mode == AudioSystem::MODE_IN_CALL))
            {
                ret = volume_keys::wakeup_disable();
                if (ret) goto return_error;
            }

#ifdef CUSTOM_BOARD_WITH_AUDIENCE
            if (!call_established) {
                ret = acoustic::process_wake();
                if (ret) goto return_error;
            }
#endif

            /* --------------------------------------------- */
            /* Voice paths control for MODE_IN_CALL          */
            /* --------------------------------------------- */
            if (current_mode == AudioSystem::MODE_IN_CALL)
            {
                LOGD("VPC IN_CALL\n");
                if(modem_status != MODEM_UP)
                {
                    LOGD("MODEM_DOWN or IN_RESET, cannot set a voicecall path!!!\n");
                    goto return_error;
                }
                /* MODEM is UP, apply the changes only if devices, or mode, or modem status was changed */
                if ((prev_mode != current_mode) || (prev_device != current_device) || (prev_modem_status != modem_status))
                {
                    switch (current_device)
                    {
                        /* ------------------------------------ */
                        /* Voice paths control for MSIC devices */
                        /* ------------------------------------ */
                        case AudioSystem::DEVICE_OUT_EARPIECE:
                        case AudioSystem::DEVICE_OUT_SPEAKER:
                        case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
                        case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:
                            amc_mute();

                            msic::pcm_disable();
                            bt::pcm_disable();
                            amc_off();

        #ifdef CUSTOM_BOARD_WITH_AUDIENCE
                            device_profile = (tty_call == AMC_TTY_OFF) ? current_device : device_out_defaut;
                            ret = acoustic::process_profile(device_profile, current_mode);
                            if (ret) goto return_error;
        #endif

                            if ((prev_mode != AudioSystem::MODE_IN_CALL) || (prev_device & DEVICE_OUT_BLUETOOTH_SCO_ALL) || (!call_established))
                            {
                                amc_modem_conf_msic_dev(tty_call);
                            }

                            amc_on();
                            msic::pcm_enable(current_mode, current_device);
                            mixing_enable = true;

                            amc_unmute(modem_gain_dl, modem_gain_ul);
                            break;
                        /* ------------------------------------ */
                        /* Voice paths control for BT devices   */
                        /* ------------------------------------ */
                        case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO:
                        case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
                        case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
                            amc_mute();

                            msic::pcm_disable();
                           /*
                            * Awfull Workaround: audience needs a 48k clock to apply its BT
                            * preset. If the call is not established, needs to start
                            * the Modem I2S in 48kHz i2s mode
                            */

                            if(!call_established)
                            {
                                amc_modem_conf_msic_dev(tty_call);
                                amc_on();
                            }

        #ifdef CUSTOM_BOARD_WITH_AUDIENCE
                            device_profile = (bt_acoustic == false) ? device_out_defaut : current_device;
                            ret = acoustic::process_profile(device_profile, current_mode);
                            if (ret) goto return_error;
                            usleep(50000);
        #endif
                            amc_off();
                            amc_modem_conf_bt_dev();

                            amc_on();
                            bt::pcm_enable();
                            mixing_enable = true;

                            amc_unmute(modem_gain_dl, modem_gain_ul);
                            break;
                        default:
                            break;
                    }

                    call_established = true;
                    prev_mode = current_mode;
                    prev_device = current_device;
                    prev_modem_status = modem_status;
                }
                /* Else: nothing to do, input params of VPC did not change */
            }
            /* --------------------------------------------- */
            /* Voice paths control for MODE_IN_COMMUNICATION */
            /* --------------------------------------------- */
            else if (current_mode == AudioSystem::MODE_IN_COMMUNICATION)
            {
                LOGD("VPC IN_COMMUNICATION\n");
                if(modem_status == MODEM_COLD_RESET)
                {
                    LOGD("MODEM_COLD_RESET, cannot set VoIP path !!!\n");
                    goto return_error;
                }

                /* MODEM is not in cold reset, apply the changes only if devices, or mode, or modem status was changed */
                if ((prev_mode != current_mode) || (prev_device != current_device) || (prev_modem_status != modem_status))
                {
                    switch (current_device)
                    {
                    /* ------------------------------------ */
                    /* Voice paths control for MSIC devices */
                    /* ------------------------------------ */
                    case AudioSystem::DEVICE_OUT_EARPIECE:
                    case AudioSystem::DEVICE_OUT_SPEAKER:
                    case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
                    case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:
                        msic::pcm_disable();
                        bt::pcm_disable();
                        if (prev_mode == AudioSystem::MODE_IN_CALL)
                        {
                            amc_off();
                        }

    #ifdef CUSTOM_BOARD_WITH_AUDIENCE
                        device_profile = (tty_call == true) ? device_out_defaut : current_device;
                        ret = acoustic::process_profile(device_profile, current_mode);
                        if (ret) goto return_error;
    #endif

                        msic::pcm_enable(current_mode, current_device);
                        break;
                    /* ------------------------------------ */
                    /* Voice paths control for BT devices   */
                    /* ------------------------------------ */
                    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO:
                    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
                    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
                        if (prev_mode == AudioSystem::MODE_IN_CALL)
                        {
                            amc_off();
                        }

    #ifdef CUSTOM_BOARD_WITH_AUDIENCE
                        /*
                         * Awfull Workaround: audience needs a 48k clock to apply its BT
                         * preset. Needs to start the Modem I2S in 48kHz i2s mode
                         */
                        msic::pcm_enable(AudioSystem::MODE_IN_COMMUNICATION, AudioSystem::DEVICE_OUT_EARPIECE);
                        device_profile = (bt_acoustic == false) ? device_out_defaut : current_device;
                        ret = acoustic::process_profile(current_device, current_mode);
                        if (ret) goto return_error;
                        usleep(50000);
    #endif
                        msic::pcm_disable();

                        bt::pcm_enable();
                        break;
                    default:
                        break;
                    }

                    call_established = true;
                    prev_mode = current_mode;
                    prev_device = current_device;
                    prev_modem_status = modem_status;
                }
                /* else: nothing to do, VPC input params did not change */
            }
            /* else: nothing to do, mode not handled */
        }
        /* Disable voice paths at the end of the call */
        else if (route == VPC_ROUTE_CLOSE)
        {
            LOGD("VPC_ROUTE_CLOSE request\n");
            if(current_mode == AudioSystem::MODE_IN_COMMUNICATION)
            {
                LOGD("current_mode: IN_COMMUNICATION\n");
                /*
                * We are still in VoIP call but a modem cold reset
                * is going to be performed.
                * Need to close immediately MSIC / BT
                */
                if(modem_status == MODEM_COLD_RESET)
                {
                    LOGD("VPC IN_COMMUNICATION & MODEM COLD RESET\n");
                    msic::pcm_disable();
                    bt::pcm_disable();

                    call_established = false;
#ifdef CUSTOM_BOARD_WITH_AUDIENCE
                    acoustic::process_suspend();
#endif
                }
                 /* Else: ignore this close request */

            }
            else if(current_mode == AudioSystem::MODE_IN_CALL)
            {
                LOGD("current_mode: IN_CALL");
                /* We are still in call but an accessory change occured
                 * and a close request was initiated
                 * Do not do anything except if the modem is not up anymore
                 */
                if(modem_status != MODEM_UP)
                {
                    LOGD("VPC from IN_CALL to IN_CALL with MODEM_DOWN\n");

                    ret = volume_keys::wakeup_disable();
                    if (ret) goto return_error;

                    msic::pcm_disable();
                    bt::pcm_disable();

                    call_established = false;
#ifdef CUSTOM_BOARD_WITH_AUDIENCE
                    acoustic::process_suspend();
#endif
                }
                /* Else: ignore this close request */

            } else
            {
                LOGD("current mode: out of CSV/VoIP call");
                if (prev_mode == AudioSystem::MODE_IN_CALL)
                {
                    LOGD("VPC from IN_CALL to NORMAL\n");

                    ret = volume_keys::wakeup_disable();
                    if (ret) goto return_error;

                    if(modem_status == MODEM_UP)
                        amc_mute();
                    msic::pcm_disable();
                    bt::pcm_disable();
                    if(modem_status == MODEM_UP)
                        amc_off();
                    mixing_enable = false;
                }
                else
                {
                    LOGD("VPC from IN_COMMUNICATION to NORMAL\n");
                    msic::pcm_disable();
                    bt::pcm_disable();
                }

    #ifdef CUSTOM_BOARD_WITH_AUDIENCE
                acoustic::process_suspend();
    #endif

                call_established = false;
                prev_mode = current_mode;
                prev_device = current_device;
            }
        }
        else
        {
            LOGD("Nothing to do with VPC\n");
        }
    }
    else
    {
        LOGD("Nothing to do with VPC\n");
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
static int vpc_volume(float volume)
{
    vpc_lock.lock();

    int gain = 0;
    int range = 48;

    gain = volume * range + 40;
    gain = (gain >= 88) ? 88 : gain;
    gain = (gain <= 40) ? 40 : gain;

    if ((at_thread_init == true) && (current_mode == AudioSystem::MODE_IN_CALL))
    {
        amc_setGaindest(AMC_I2S1_TX, gain);
    }

    // Backup modem gain
    modem_gain_dl = gain;

    vpc_lock.unlock();
    return NO_ERROR;
}

/*---------------------------------------------------------------------------*/
/* Voice Call Record Enable/disable                                          */
/*---------------------------------------------------------------------------*/
static void voice_call_record_on()
{
    if (!voice_call_recording)
    {
        // Enable voice call record
        LOGD("%s: voice in call recording", __FUNCTION__);
        amc_voice_record_on();
        voice_call_recording = true;
    }
}

static void voice_call_record_off()
{
    if (voice_call_recording)
    {
        LOGD("%s: disable recording", __FUNCTION__);
        voice_call_recording = false;
        amc_voice_record_off();
    }
}

/*---------------------------------------------------------------------------*/
/* Mixing Enable/disable                                                     */
/*---------------------------------------------------------------------------*/
static void mixing_on()
{
    if (!mixing_enable)
    {
        // Enable alert mixing
        LOGD("%s: enable mixing", __FUNCTION__);
        amc_enable(AMC_I2S2_RX);
        mixing_enable = true;
    }
}

static void mixing_off()
{
    if (mixing_enable)
    {
        LOGD("%s: disable mixing", __FUNCTION__);
        amc_disable(AMC_I2S2_RX);
        mixing_enable = false;
    }
}


/*---------------------------------------------------------------------------*/
/* I2S2 disable                                                              */
/*---------------------------------------------------------------------------*/
static int vpc_mixing_disable(bool isOut)
{
    vpc_lock.lock();

    // Request from an outStream? -> mix disable request
    if (isOut)
    {
        mixing_off();
    }
    // Request from an instream? -> record disable request
    else
    {
        voice_call_record_off();
    }

    vpc_lock.unlock();
    return NO_ERROR;
}

/*---------------------------------------------------------------------------*/
/* I2S2 enable                                                               */
/*---------------------------------------------------------------------------*/
static int vpc_mixing_enable(bool isOut, uint32_t device)
{
    vpc_lock.lock();

    // Request from an outstream? -> mix request
    if (isOut)
    {
        mixing_on();
    }
    // Request from an instream? -> record request
    else
    {
        if (device == AudioSystem::DEVICE_IN_VOICE_CALL)
        {
            voice_call_record_on();
		}
    }

    vpc_lock.unlock();
    return NO_ERROR;
}

/*---------------------------------------------------------------------------*/
/* Enable TTY                                                                */
/*---------------------------------------------------------------------------*/
static int vpc_tty(vpc_tty_t tty)
{
    vpc_lock.lock();
    tty_call = translate_vpc_to_amc_tty[tty];
    vpc_lock.unlock();

    return NO_ERROR;
}

/*---------------------------------------------------------------------------*/
/* Enable BT acoustic                                                        */
/*---------------------------------------------------------------------------*/

static int vpc_bt_nrec(vpc_bt_nrec_t bt_nrec)
{
    vpc_lock.lock();

    if (bt_nrec == VPC_BT_NREC_ON) {
        LOGD("BT acoustic On \n");
        bt_acoustic = true;
    }
    else {
        LOGD("BT acoustic Off \n");
        bt_acoustic = false;
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
    dev->params         = vpc_params;
    dev->set_mode       = vpc_set_mode;
    dev->set_modem_state = vpc_set_modem_state;
    dev->route          = vpc_route;
    dev->volume         = vpc_volume;
    dev->mix_disable    = vpc_mixing_disable;
    dev->mix_enable     = vpc_mixing_enable;
    dev->tty            = vpc_tty;
    dev->bt_nrec        = vpc_bt_nrec;
    *device = &dev->common;
    return 0;
}

static int s_device_close(hw_device_t* device)
{
    free(device);
    return 0;
}

}


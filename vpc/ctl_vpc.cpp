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

static int vpc_init(uint32_t uiIfxI2s1ClkSelect, uint32_t uiIfxI2s2ClkSelect);
static int vpc_params(int mode, uint32_t device);
static void vpc_set_mode(int mode);
static void vpc_set_modem_state(int state);
static int vpc_route(vpc_route_t);
static int vpc_volume(float);
static int vpc_mixing_disable(bool isOut);
static int vpc_mixing_enable(bool isOut, uint32_t device);
static int vpc_set_tty(vpc_tty_t);
static void translate_to_amc_device(const uint32_t current_device, IFX_TRANSDUCER_MODE_SOURCE* mode_source, IFX_TRANSDUCER_MODE_DEST* mode_dest);
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

const uint32_t DEFAULT_IS21_CLOCK_SELECTION = IFX_CLK1;
const uint32_t DEFAULT_IS22_CLOCK_SELECTION = IFX_CLK0;

/*---------------------------------------------------------------------------*/
/* Global variables                                                          */
/*---------------------------------------------------------------------------*/
using android::Mutex;
Mutex vpc_lock;

static int       prev_mode             = AudioSystem::MODE_NORMAL;
static int       current_mode          = AudioSystem::MODE_NORMAL;
static uint32_t  prev_device           = 0x0000;
static uint32_t  current_device        = 0x0000;
static uint32_t  device_out_defaut     = 0x8000;
static bool      at_thread_init        = false;
static AMC_TTY_STATE current_tty_call  = AMC_TTY_OFF;
static AMC_TTY_STATE previous_tty_call = AMC_TTY_OFF;
static bool      mixing_enable         = false;
static bool      voice_call_recording  = false;
static bool      acoustic_in_bt_device = false;
static int       modem_gain_dl         = 0;
static int       modem_gain_ul         = 100; // +6 dB
static int       modem_status          = MODEM_DOWN;
static bool      call_connected        = false;


static bool      vpc_audio_routed     = false;

#ifdef CUSTOM_BOARD_WITH_AUDIENCE
static bool      audience_awake        = false;
#endif
/* Forward declaration */
static void voice_call_record_restore();

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/
static int vpc_init(uint32_t uiIfxI2s1ClkSelect, uint32_t uiIfxI2s2ClkSelect)
{
    vpc_lock.lock();
    LOGD("Initialize VPC\n");

#ifndef CUSTOM_BOARD_WITHOUT_MODEM
    if (uiIfxI2s1ClkSelect == (uint32_t) -1) {
                // Not provided: use default
                uiIfxI2s1ClkSelect = DEFAULT_IS21_CLOCK_SELECTION;
    }
    if (uiIfxI2s2ClkSelect == (uint32_t) -1) {
                // Not provided: use default
                uiIfxI2s2ClkSelect = DEFAULT_IS22_CLOCK_SELECTION;
    }

    if (at_thread_init == false)
    {
        AT_STATUS cmd_status = at_start(AUDIO_AT_CHANNEL_NAME, uiIfxI2s1ClkSelect, uiIfxI2s2ClkSelect);
        if (cmd_status != AT_OK) goto return_error;
        LOGD("AT thread started\n");
        at_thread_init = true;
    }
#else
    LOGD("VPC with no Modem\n");
#endif

    msic::pcm_init();

#ifdef CUSTOM_BOARD_WITH_AUDIENCE
    LOGD("Initialize Audience\n");
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

#ifndef CUSTOM_BOARD_WITH_AUDIENCE
    //
    // Hack the mode here. Do not want to handle any configuration on VPC in
    // communication mode.
    // Default configuration allows BT SCO, which will be used for incommunication
    // on BT SCO.
    //
    current_mode =  mode == AudioSystem::MODE_IN_COMMUNICATION ? AudioSystem::MODE_NORMAL : mode;
#else
    current_mode = mode;
#endif
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

#ifndef CUSTOM_BOARD_WITH_AUDIENCE
    //
    // Hack the mode here. Do not want to handle any configuration on VPC in
    // communication mode.
    // Default configuration allows BT SCO, which will be used for incommunication
    // on BT SCO.
    //
    current_mode =  mode == AudioSystem::MODE_IN_COMMUNICATION ? AudioSystem::MODE_NORMAL : mode;
#else
    current_mode = mode;
#endif

    LOGD("%s: mode = %d\n", __FUNCTION__, current_mode);
    LOGD("%s: previous mode = %d\n", __FUNCTION__, prev_mode);

    vpc_lock.unlock();
}

/*---------------------------------------------------------------------------*/
/* Call status parameters                                                    */
/* Corresponds to the XPROGRESS information                                  */
/*---------------------------------------------------------------------------*/
static void vpc_set_call_status(bool isConnected)
{
    vpc_lock.lock();

    call_connected = isConnected;

    LOGD("%s: call_status = %d\n", __FUNCTION__, call_connected);

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
        modem_status = status;
        if(modem_status == MODEM_UP){
            /* set BT SCO Path to PCM by default when modem is up */
            /* this path has to be disabled only if a MSIC device is in use or when the modem is rebooting */
            bt::pcm_enable();
        }
        else{
            /* modem is unavailable: disable BT SCO path */
            bt::pcm_disable();
        }
    }

    LOGD("vpc_set_modem_state modem_status = %d \n", modem_status);

    vpc_lock.unlock();
}

/*-------------------------------*/
/* Get audio voice routing state */
/*-------------------------------*/

static bool vpc_get_audio_routed()
{
    return vpc_audio_routed;
}

/*-----------------------------------------------------------------------------------*/
/* Set audio voice routing state                                                     */
/* Enable/disable volume keys wake up capability given the audio voice routing state */
/* Set internal state variables                                                      */
/*-----------------------------------------------------------------------------------*/

static void vpc_set_audio_routed(bool isRouted)
{
    // Update internal state variables
    prev_mode = current_mode;
    prev_device = current_device;
    previous_tty_call = current_tty_call;

    if (vpc_audio_routed != isRouted) {
        // Volume buttons & power management
        if (isRouted) {
            volume_keys::wakeup_enable();
        } else {
            volume_keys::wakeup_disable();
        }
        vpc_audio_routed = isRouted;
    }
}

/*---------------------------------------------------------------------------*/
/* Route/unroute functions                                                   */
/*---------------------------------------------------------------------------*/

static int vpc_unroute_voip()
{
    LOGD("%s", __FUNCTION__);

    if (!vpc_get_audio_routed())
        return NO_ERROR;

    msic::pcm_disable();

    // Update internal state variables
    vpc_set_audio_routed(false);

    return NO_ERROR;
}

static int vpc_unroute_csvcall()
{
    LOGD("%s", __FUNCTION__);

    if (!vpc_get_audio_routed())
        return NO_ERROR;

    int ret = volume_keys::wakeup_disable();

    if(modem_status == MODEM_UP)
        amc_mute();
    msic::pcm_disable();

    if(modem_status == MODEM_UP)
        amc_off();
    mixing_enable = false;

    // Update internal state variables
    vpc_set_audio_routed(false);

    return ret;
}

#ifdef CUSTOM_BOARD_WITH_AUDIENCE
static int vpc_wakeup_audience()
{
    if (audience_awake)
        return NO_ERROR;

    if (acoustic::process_wake())
        return NO_INIT;

    audience_awake = true;

    return NO_ERROR;
}
#endif

static void vpc_suspend()
{
#ifdef CUSTOM_BOARD_WITH_AUDIENCE
    if (!audience_awake)
        return ;

    acoustic::process_suspend();
    audience_awake = false;
#endif

    /* enable BT SCO path by default except is MODEM is not UP*/
    if(modem_status == MODEM_UP)
        bt::pcm_enable();
}

static inline bool vpc_route_conditions_changed()
{
    return (prev_mode != current_mode ||
            prev_device != current_device ||
            !vpc_get_audio_routed() ||
            current_tty_call != previous_tty_call);
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
        LOGD("previous mode = %d previous device = %d\n", prev_mode, prev_device);
        LOGD("current tty = %d previous tty = %d\n", current_tty_call, previous_tty_call);

        if (route == VPC_ROUTE_OPEN)
        {
            LOGD("VPC_ROUTE_OPEN request\n");

#ifdef CUSTOM_BOARD_WITH_AUDIENCE
            if (vpc_wakeup_audience())
                goto return_error;
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
                /* MODEM is UP, apply the changes only if devices, or mode, or audio is not route due to modem reset or call disconnected */
                if (vpc_route_conditions_changed())
                {
                    IFX_TRANSDUCER_MODE_SOURCE mode_source;
                    IFX_TRANSDUCER_MODE_DEST mode_dest;
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
                            /* Disable SCO path if a MSIC device is in use  */
                            bt::pcm_disable();
                            amc_off();

#ifdef CUSTOM_BOARD_WITH_AUDIENCE
                            device_profile = (current_tty_call == AMC_TTY_OFF) ? current_device : device_out_defaut;
                            ret = acoustic::process_profile(device_profile, current_mode);
                            if (ret) goto return_error;
                            mode_source = IFX_USER_DEFINED_15_S;
                            mode_dest = IFX_USER_DEFINED_15_D;
#else
                            // No Audience, Acoustics in modem
                            translate_to_amc_device(current_device, &mode_source, &mode_dest);
#endif
                            // Configure modem I2S1
                            amc_conf_i2s1(current_tty_call, mode_source, mode_dest);

                            if ((prev_mode != AudioSystem::MODE_IN_CALL) || (prev_device & DEVICE_OUT_BLUETOOTH_SCO_ALL) || (!vpc_get_audio_routed()))
                            {
                                 // Configure modem I2S2 and do the modem routing
                                 amc_conf_i2s2_route();
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

                            // If acoustic_in_bt_device is true, bypass phone embedded algorithms
                            // and use acoustic alogrithms from Bluetooth headset.
                            device_profile = (acoustic_in_bt_device == true) ? device_out_defaut : current_device;

#ifdef CUSTOM_BOARD_WITH_AUDIENCE
                            /*
                             * Audience requirement: the previous mode clock must be running
                             * while the BT preset is selected, during 50ms at least.
                             */
                            if(!vpc_get_audio_routed())
                            {
                                // configure modem I2S1
                                amc_conf_i2s1(current_tty_call, IFX_USER_DEFINED_15_S, IFX_USER_DEFINED_15_D);
                                // Configure modem I2S2 and do the modem routing
                                amc_conf_i2s2_route();
                                amc_on();
                            }


                            ret = acoustic::process_profile(device_profile, current_mode);
                            if (ret) goto return_error;
                            usleep(50000);

                            mode_source = IFX_USER_DEFINED_15_S;
                            mode_dest = IFX_USER_DEFINED_15_D;

#else
                            // No Audience, Acoustics in modem
                            translate_to_amc_device(device_profile, &mode_source, &mode_dest);
#endif
                            amc_off();

                            // Do the modem config for BT devices
                            amc_modem_conf_bt_dev(mode_source, mode_dest);

                            amc_on();
                            bt::pcm_enable();
                            mixing_enable = true;

                            amc_unmute(modem_gain_dl, modem_gain_ul);
                            break;
                        default:
                            break;
                    }
                    // Restore record path if required
                    voice_call_record_restore();

                    // Update internal state variables
                    vpc_set_audio_routed(true);
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
                if (vpc_route_conditions_changed())
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
                        /* Disable SCO path if a MSIC device is in use  */
                        bt::pcm_disable();

#ifdef CUSTOM_BOARD_WITH_AUDIENCE
                        device_profile = (current_tty_call == AMC_TTY_OFF) ? current_device : device_out_defaut;
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
                         * Audience requirement: the previous mode clock must be running
                         * while the BT preset is selected, during 50ms at least. Since
                         * MSIC clock is already disabled, we have to enable it back.
                         */
                        msic::pcm_enable(AudioSystem::MODE_IN_COMMUNICATION, AudioSystem::DEVICE_OUT_EARPIECE);

                        // If acoustic_in_bt_device is true, bypass phone embedded algorithms
                        // and use acoustic alogrithms from Bluetooth headset.
                        device_profile = (acoustic_in_bt_device == true) ? device_out_defaut : current_device;
                        ret = acoustic::process_profile(device_profile, current_mode);
                        if (ret) goto return_error;
                        usleep(50000);
#endif
                        msic::pcm_disable();

                        bt::pcm_enable();
                        break;
                    default:
                        break;
                    }

                    // Update internal state variables
                    vpc_set_audio_routed(true);
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
                    vpc_unroute_voip();

                    vpc_suspend();
                }
                else if(prev_mode == AudioSystem::MODE_IN_CALL && call_connected)
                {

                    LOGD("VPC SWAP FROM IN_CALL TO IN_COMMUNICATION");
                    if (vpc_unroute_csvcall())
                        goto return_error;

                    /* Keep audience awaken */
                }
                /* Else: ignore this close request */
            }
            else if(current_mode == AudioSystem::MODE_IN_CALL)
            {
                LOGD("current_mode: IN_CALL");
                if(prev_mode == AudioSystem::MODE_IN_COMMUNICATION)
                {
                    LOGD("VPC SWAP from IN_COMMUNICATION to IN_CALL");
                    vpc_unroute_voip();

                    /* Keep audience awaken */
                }
                /* We are still in call but an accessory change occured
                 * and a close request was initiated
                 * Do not do anything except if the modem is not up anymore
                 */
                else if(modem_status != MODEM_UP || !call_connected)
                {
                    LOGD("VPC from IN_CALL to IN_CALL with MODEM_DOWN or CALL NOT CONNECTED");
                    if (vpc_unroute_csvcall())
                        goto return_error;

                    vpc_suspend();
                }
                /* Else: ignore this close request */

            } else
            {
                LOGD("current mode: out of CSV/VoIP call");
                if (prev_mode == AudioSystem::MODE_IN_CALL)
                {
                    LOGD("VPC from IN_CALL to NORMAL\n");
                    if (vpc_unroute_csvcall())
                        goto return_error;
                }
                else
                {
                    LOGD("VPC from IN_COMMUNICATION to NORMAL\n");
                    vpc_unroute_voip();
                }

                vpc_suspend();
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
        LOGD("%s: change modem volume", __FUNCTION__);
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

static void voice_call_record_restore()
{
    if (voice_call_recording)
    {
        // Restore voice call record
        LOGD("%s", __FUNCTION__);
        amc_voice_record_on();
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
static int vpc_set_tty(vpc_tty_t tty)
{
    vpc_lock.lock();
    current_tty_call = translate_vpc_to_amc_tty[tty];
    vpc_lock.unlock();

    return NO_ERROR;
}

/*---------------------------------------------------------------------------*/
/* Translate Device                                                          */
/*---------------------------------------------------------------------------*/
static void translate_to_amc_device(const uint32_t current_device, IFX_TRANSDUCER_MODE_SOURCE* mode_source, IFX_TRANSDUCER_MODE_DEST* mode_dest)
{
    switch (current_device)
    {
    /* ------------------------------------ */
    /* Voice paths control for MSIC devices */
    /* ------------------------------------ */
    case AudioSystem::DEVICE_OUT_EARPIECE:
        *mode_source = IFX_HANDSET_S;
        *mode_dest = IFX_HANDSET_D;
        break;
    case AudioSystem::DEVICE_OUT_SPEAKER:
        *mode_source = IFX_HF_S;
        *mode_dest = IFX_BACKSPEAKER_D;
        break;
    case AudioSystem::DEVICE_OUT_WIRED_HEADSET:
        *mode_source = IFX_HEADSET_S;
        *mode_dest = IFX_HEADSET_D;
        break;
    case AudioSystem::DEVICE_OUT_WIRED_HEADPHONE:
        *mode_source = IFX_HF_S;
        *mode_dest = IFX_HEADSET_D;
        break;
    /* ------------------------------------ */
    /* Voice paths control for BT devices   */
    /* ------------------------------------ */
    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO:
    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
        *mode_source = IFX_BLUETOOTH_S;
        *mode_dest = IFX_BLUETOOTH_D;
        break;
    case AudioSystem::DEVICE_OUT_DEFAULT:
        // Used for BT with acoustics devices
        *mode_source = IFX_USER_DEFINED_15_S;
        *mode_dest = IFX_USER_DEFINED_15_D;
        break;
    default:
        *mode_source  = IFX_DEFAULT_S;
        *mode_dest = IFX_DEFAULT_D;
        break;
    }
}

/*---------------------------------------------------------------------------*/
/* Enable/Disable BT acoustic with AT+NREC command in handset                */
/*---------------------------------------------------------------------------*/

static int vpc_bt_nrec(vpc_bt_nrec_t bt_nrec)
{
    vpc_lock.lock();

    if (bt_nrec == VPC_BT_NREC_ON) {
        LOGD("Enable in handset echo cancellation/noise reduction for BT\n");
        acoustic_in_bt_device = false;
    }
    else {
        LOGD("Disable in handset echo cancellation/noise reduction for BT. Use BT device embedded acoustics\n");
        acoustic_in_bt_device = true;
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

extern "C" hw_module_t HAL_MODULE_INFO_SYM =
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
    dev->set_call_status = vpc_set_call_status;
    dev->set_modem_state = vpc_set_modem_state;
    dev->route          = vpc_route;
    dev->volume         = vpc_volume;
    dev->mix_disable    = vpc_mixing_disable;
    dev->mix_enable     = vpc_mixing_enable;
    dev->set_tty        = vpc_set_tty;
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


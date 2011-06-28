/*
 **
 ** Copyright 2010 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **	 http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#ifndef AUDIO_MODEM_CONTROL_H
#define AUDIO_MODEM_CONTROL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "ATmodemControl.h"
#include <pthread.h>

    /*==============================================================================
    /*                        Audio Modem Control Library.
     *============================================================================*/

    /* Cross platform modem abstraction library regarding audio.*/

    /* Before use, (un)comment the wanted below customization defines
     *  - Below "Platform Customization"
     *  - in ATmodemControl.h "Platform Customization & DEBUG"*/


    /*------------------------------------------------------------------------------
     * Platform Customization
     *----------------------------------------------------------------------------*/


#define MODEM_IFX_XMM6160


    /*------------------------------------------------------------------------------
     * Communication Globals:
     *----------------------------------------------------------------------------*/
    /* String length needed to send a message:*/
#define AMC_MAX_CMD_LENGTH   AT_MAX_CMD_LENGTH
    /* String length needed to receive a message:*/
#define AMC_MAX_RESP_LENGTH  AT_MAX_RESP_LENGTH
    /* Mutex needed to access values returned from modem when unblocking
     * functions used:*/
#define amc_unsolicitedRespMutex at_unsolicitedRespMutex

    /* Return status:*/
    typedef enum {
        AMC_OK = AT_OK,
        AMC_RUNNING = AT_RUNNING, /*response pending*/
        AMC_ERROR = AT_ERROR,
        AMC_UNABLE_TO_CREATE_THREAD = AT_UNABLE_TO_CREATE_THREAD,
        AMC_UNABLE_TO_OPEN_DEVICE = AT_UNABLE_TO_OPEN_DEVICE,
        AMC_WRITE_ERROR = AT_WRITE_ERROR,
        AMC_READ_ERROR = AT_READ_ERROR,
        AMC_UNINITIALIZED = AT_UNINITIALIZED,
        AMC_NOT_SUPPORTED,
        AMC_PARAM_ERROR,
        AMC_NOT_YET_IMPLEMENTED,
    }
    AMC_STATUS;


    /*------------------------------------------------------------------------------
     * API constants / data:
     *----------------------------------------------------------------------------*/


    /* Modem hardware Interfaces that may be configured:*/
    typedef enum {
        AMC_I2S1_HW = 0,
        AMC_I2S2_HW = 1,
        INTERNAL_HW = 2
    } AMC_HW_INTERFACE;

    /* Function state:*/
    typedef enum {
        AMC_OFF = 0,
        AMC_ON = 1
    } AMC_STATE;


    /* available dest for IFX modem */
    typedef enum {
        AMC_RADIO_TX = 0,/* source(and sink for near-end loop back)*/
        AMC_RADIO_ANALOG_OUT = 1,/* sink(and source for far-end loop back)*/
        AMC_I2S1_TX = 2,/* source*/
        AMC_I2S2_TX = 3,/* sink*/
        AMC_PCM_GENERALD = 4,/* source*/
        AMC_SPEECH_DL_EXTRACT = 5,/* sink*/
        AMC_SPEECH_UL_EXTRACT = 6,/* source*/
        AMC_PROBE_OUT = 7,/* source*/
        AMC_ENDD = 8/* Nothing*/
    } AMC_DEST;

    /* available source for IFX modem */
    typedef enum {
        AMC_RADIO_RX = 0,/* source(and sink for near-end loop back)*/
        AMC_RADIO_ANALOG_IN = 1,/* sink(and source for far-end loop back)*/
        AMC_DIGITAL_MIC = 2,
        AMC_I2S1_RX = 3,
        AMC_I2S2_RX = 4,
        AMC_SIMPLE_TONES = 5,
        AMC_PCM_GENERALS = 6,
        AMC_SPEECH_DL_INJECT = 7,
        AMC_SPEECH_UL_INJECT = 8,
        AMC_INTERNAL_FM_RADIO= 9,
        AMC_PROBE_IN = 10,
        AMC_ENDS = 11
    } AMC_SOURCE;

    /* Clock selection */
    typedef enum {
        IFX_CLK0 = 0,/*Select clock 0 from I2S HW.  */
        IFX_CLK1 = 1/*Select clock 1 from I2S HW. */
    } IFX_CLK ;
    /* master_slave */
    typedef enum {
        IFX_MASTER = 0, /*Master mode(seen from the IFX audio sub system). */
        IFX_SLAVE = 1   /*Slave mode(seen from the IFX audio sub system)*/
    } IFX_MASTER_SLAVE;

    /* Sample Rate */
    typedef enum   {
        IFX_SR_8KHZ = 0,
        IFX_SR_11KHZ = 1 ,
        IFX_SR_12KHZ = 2,
        IFX_SR_16KHZ = 3,
        IFX_SR_22KHZ = 4,
        IFX_SR_24KHZ = 5,
        IFX_SR_32KHZ = 6,
        IFX_SR_44KHZ = 7,
        IFX_SR_48KHZ = 8,
        IFX_SR_96KHZ = 9
    } IFX_I2S_SR ;
    /* word length */
    typedef enum   {
        IFX_SW_16 = 0,
        IFX_SW_18 = 1,
        IFX_SW_20 = 2,
        IFX_SW_24 = 3,
        IFX_SW_32 = 4,
        IFX_SW_END = 5
    } IFX_I2S_SW ;

    /* transmission mode */
    typedef enum {
        IFX_PCM = 0,  /*Transmit data using normal PCM mode interface*/
        IFX_NORMAL = 1,  /*Use I2S standard protocol interface to transmit data*/
        IFX_PCM_BURST = 2 /*Use PCM burst protocol interface to transmit data */
    }  IFX_I2S_TRANS_MODE ;

    /* Settings */
    typedef enum {
        I2S_SETTING_NORMAL = 0, /*Use normal I2S interface.*/
        I2S_SETTING_SPECIAL_1 = 1, /*Special interface, project-specific.*/
        I2S_SETTING_SPECIAL_2 = 2,
        I2S_SETTING_END = 3
    } IFX_I2S_SETTINGS;

    /* Stereo/mono */
    typedef enum   {
        IFX_MONO = 0,
        IFX_DUAL_MONO = 1,
        IFX_STEREO = 2,
        IFX_END = 3
    } IFX_I2S_AUDIO_MODE ;

    /* update */
    typedef enum  {
        IFX_UPDATE_ALL = 0, /*Update all configuration parameters.*/
        IFX_UPDATE_HW = 1,  /*Update only hardware configuration parameters.  */
        IFX_UPDATE_TRANSDUCER = 2 /*Update only the transducer mode parameter. */
    } IFX_I2S_UPDATES ;

    /* source used */
    typedef enum   {
        IFX_DEFAULT_S = 0,/*Default value, used when only one transducer exists.*/
        IFX_HANDSET_S = 1,/*Handset microphone.*/
        IFX_HEADSET_S = 2,/*Headset microphone.*/
        IFX_HF_S = 3,/*Hands-free microphone.*/
        IFX_AUX_S = 4,/*Auxiliary input.*/
        IFX_TTY_S = 5,/*TTY mode for TTY-enabled calls.*/
        IFX_BLUETOOTH_S = 6,/*BlueTooth connection.*/
        IFX_USER_DEFINED_1_S = 7,/*Project-specific mode.*/
        IFX_USER_DEFINED_2_S = 8,/*Project-specific mode.*/
        IFX_USER_DEFINED_3_S = 9,/*Project-specific mode.*/
        IFX_USER_DEFINED_4_S = 10,/*Project-specific mode.*/
        IFX_USER_DEFINED_5_S = 11,/*Project-specific mode.*/
        IFX_USER_DEFINED_15_S = 21, /*No Processing.*/
    } IFX_TRANSDUCER_MODE_SOURCE;


    /* destination used */
    typedef enum   {
        IFX_DEFAULT_D = 0,/*Default value, used when only one transducer exists.*/
        IFX_HANDSET_D = 1,/*Handset microphone.*/
        IFX_HEADSET_D = 2,/*Headset microphone.*/
        IFX_BACKSPEAKER_D = 3,/*Hands-free microphone.*/
        IFX_HEADSET_PLUS_BACKSPEAKER_D = 4,/*Auxiliary input.*/
        IFX_HEADSET_PLUS_HANDSET_D = 5,/*TTY mode for TTY-enabled calls.*/
        IFX_TTY_D = 6,/*BlueTooth connection.*/
        IFX_BLUETOOTH_D = 7,/*Project-specific mode.*/
        IFX_USER_DEFINED_1_D = 8,/*Project-specific mode.*/
        IFX_USER_DEFINED_2_D = 9,/*Project-specific mode.*/
        IFX_USER_DEFINED_3_D = 10,/*Project-specific mode.*/
        IFX_USER_DEFINED_4_D = 11,/*Project-specific mode.*/
        IFX_USER_DEFINED_5_D = 12,
        IFX_USER_DEFINED_15_D = 22,/*No Processing.*/
    } IFX_TRANSDUCER_MODE_DEST;


    /* Modem Dependent Parameters*/
    typedef struct {
        int gainStepSizeDDB;/* gain step size(in 0.1 dB)*/
        int gainMinDDB;/* minimum gain(in 0.1 dB)*/
        int gainMaxDDB;/* maximum gain(in 0.1 dB)*/
    } volume_control;

    extern volume_control volumeCTRL; /*Modem dependent parameters*/

    /* Acoustics:*/
    typedef enum {
        AMC_NORMAL,
        AMC_HANDSET,
        AMC_HEADSET,
        AMC_HANDFREE
    } AMC_ACOUSTIC;

    /*------------------------------------------------------------------------------
     * API
     *----------------------------------------------------------------------------*/

    /* Each function exist in:
     *- standard mode ( wait up to modem ACK and return response/error code. )
     *- unblocking mode. A pointer to the modem ackn'ed or NULL should be provided.
     *synchronisation function should then be used prior accessing the response.
     *They internally contain a mutex and/or passive wait on signal.*/

    /* Starting, stopping:*/

    AMC_STATUS amc_start(const char *pATchannel, char *pUnsolicitedATresp);
    /* Start the Audio Modem Control library.
     **pATchanne: full path to the char device driver
     * pUnsolicitedATresponse of type char[AMC_MAX_RESP_LENGTH] or NULL.
     *- Will hold the unsolicited answers.
     *- Need to be accessed using 'amc_punsolicitedRespMutex'.
     * amc_start() tasks:
     *-start the AT library (open  handles to modem, start the reader thread)
     *-send the default modem config ( call amc_sendDefaultConfig() )*/

    AMC_STATUS amc_stop(void);
    /* Stop the Audio Modem Control library:
     *- Stop the AT library
     *- Free used ressources*/


    /* Configuring/activating hardware interfaces:*/

    AMC_STATUS amc_enable(AMC_SOURCE source);
    AMC_STATUS amc_disable(AMC_SOURCE source);

    AMC_STATUS amc_configure_dest(AMC_DEST dest, IFX_CLK clk, IFX_MASTER_SLAVE mode, IFX_I2S_SR sr, IFX_I2S_SW sw, IFX_I2S_TRANS_MODE trans, IFX_I2S_SETTINGS settings, IFX_I2S_AUDIO_MODE audio, IFX_I2S_UPDATES update, IFX_TRANSDUCER_MODE_DEST transducer_dest);
    AMC_STATUS amc_configure_source(AMC_SOURCE source, IFX_CLK clk, IFX_MASTER_SLAVE mode, IFX_I2S_SR sr, IFX_I2S_SW sw, IFX_I2S_TRANS_MODE trans, IFX_I2S_SETTINGS settings, IFX_I2S_AUDIO_MODE audio, IFX_I2S_UPDATES update, IFX_TRANSDUCER_MODE_SOURCE transducer_source);


    /* Configuring/routing/enabling modem internal path & ports:*/

    AMC_STATUS amc_route(AMC_SOURCE source, ...);
    AMC_STATUS amc_setGainsource(AMC_SOURCE source, int gainDDB);
    AMC_STATUS amc_setGaindest(AMC_DEST dest, int gainDDB);

    /* Other:*/

    AMC_STATUS amc_setAcoustic(AMC_ACOUSTIC acousticProfile);
    AMC_STATUS check_tty();

    /* Unblocking counter-parts:*/

    AMC_STATUS amc_configure_dest_UnBlocking(AMC_DEST dest, IFX_CLK clk, IFX_MASTER_SLAVE mode, IFX_I2S_SR sr, IFX_I2S_SW sw, IFX_I2S_TRANS_MODE trans, IFX_I2S_SETTINGS settings, IFX_I2S_AUDIO_MODE audio, IFX_I2S_UPDATES update, IFX_TRANSDUCER_MODE_DEST transducer_dest,
            AMC_STATUS *pCmdStatus);
    AMC_STATUS amc_configure_source_UnBlocking(AMC_SOURCE source, IFX_CLK clk, IFX_MASTER_SLAVE mode, IFX_I2S_SR sr, IFX_I2S_SW sw, IFX_I2S_TRANS_MODE trans, IFX_I2S_SETTINGS settings, IFX_I2S_AUDIO_MODE audio, IFX_I2S_UPDATES update, IFX_TRANSDUCER_MODE_SOURCE transducer_source,
            AMC_STATUS *pCmdStatus);
    AMC_STATUS amc_enableUnBlocking(AMC_SOURCE source, AMC_STATUS *pCmdStatus);
    AMC_STATUS amc_disableUnBlocking(AMC_SOURCE source, AMC_STATUS *pCmdStatus);
    AMC_STATUS amc_routeUnBlocking(AMC_SOURCE source,
                                   int dest[], int i, AMC_STATUS *pCmdStatus);
    AMC_STATUS amc_setGainsourceUnBlocking(AMC_SOURCE source, int gainIndex,
                                           AMC_STATUS *pCmdStatus);
    AMC_STATUS amc_setGaindestUnBlocking(AMC_DEST dest, int gainIndex,
                                         AMC_STATUS *pCmdStatus);
    AMC_STATUS amc_setAcousticUnBlocking(AMC_ACOUSTIC acousticProfile,
                                         AMC_STATUS *pCmdStatus);

    AMC_STATUS amc_check(AMC_STATUS *pCmdStatus);
    /* waiting passively for the completion of unblocking commands:*/
    void amc_waitForCmdCompletion(AMC_STATUS *pCmdStatus);


    /*------------------------------------------------------------------------------
     * Internal Modem Specific Implementation
     *----------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /*AUDIO_MODEM_CONTROL_H*/



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

#include "AudioModemControl.h"


/*==============================================================================
 * Audio Modem Control implementation part generic to all Modem types.
 *============================================================================*/

/*------------------------------------------------------------------------------
 * Log Settings:
 *----------------------------------------------------------------------------*/
#define LOG_TAG "AudiomodemControl_base"
#include <utils/Log.h>

#include <stdarg.h>

/*---------------------------------------------------------------------------
 * Internal globals and helpers
 *---------------------------------------------------------------------------*/

static bool isInitialized; /*= false Keep track of library initialization*/


/*---------------------------------------------------------------------------
 * AudioModemControl API implementation:
 *---------------------------------------------------------------------------*/

/*---- AMC API ---  */
AMC_STATUS amc_start(const char *pATchannel, char *pUnsolicitedATresp)
{
    AMC_STATUS cmdStatus;
    if (isInitialized) {
        LOGI("Library already started");
        return AMC_OK;
    }
    cmdStatus = at_start(pATchannel, pUnsolicitedATresp);
    if (cmdStatus == AT_OK) {
        isInitialized = true;
    }
    return cmdStatus;
}

AMC_STATUS amc_stop(void)
{
    isInitialized = false;
    return at_stop();
}

void amc_waitForCmdCompletion(AMC_STATUS *pCmdStatus)
{
    at_waitForCmdCompletion((AT_STATUS *) pCmdStatus);
}

/* Communication functions, blocking part implementation*/

AMC_STATUS amc_route(AMC_SOURCE source, ...)
{
    AMC_STATUS cmdStatus;
    va_list ap;
    int destab[7]={8,8,8,8,8,8,8};
    AMC_DEST nbrdest;
    int i=0;
    va_start(ap,source);
    do {
        nbrdest=va_arg(ap,AMC_DEST);
        if (nbrdest==AMC_ENDD)
            break;
        else
            destab[i]=(int)nbrdest;
        i++;
        LOGD("ROUTE ARG = %d",nbrdest);
    } while (1);
    va_end(ap);
    amc_routeUnBlocking(source, destab, i, &cmdStatus);
    at_waitForCmdCompletion((AT_STATUS *) &cmdStatus);
    return cmdStatus;
}

AMC_STATUS amc_enable(AMC_SOURCE source)
{
    AMC_STATUS cmdStatus;
    amc_enableUnBlocking(source, &cmdStatus);
    at_waitForCmdCompletion((AT_STATUS *) &cmdStatus);
    return cmdStatus;
}

AMC_STATUS amc_disable(AMC_SOURCE source)
{
    AMC_STATUS cmdStatus;
    amc_disableUnBlocking(source, &cmdStatus);
    at_waitForCmdCompletion((AT_STATUS *) &cmdStatus);
    return cmdStatus;
}
AMC_STATUS amc_configure_dest(AMC_DEST dest, IFX_CLK clk, IFX_MASTER_SLAVE mode, IFX_I2S_SR sr, IFX_I2S_SW sw, IFX_I2S_TRANS_MODE trans, IFX_I2S_SETTINGS settings, IFX_I2S_AUDIO_MODE audio, IFX_I2S_UPDATES update, IFX_TRANSDUCER_MODE_DEST transducer_dest)
{
    AMC_STATUS cmdStatus;
    amc_configure_dest_UnBlocking(dest,clk,mode,sr,sw,trans,settings,audio,update,transducer_dest,&cmdStatus);
    at_waitForCmdCompletion((AT_STATUS *) &cmdStatus);
    return cmdStatus;
}

AMC_STATUS amc_configure_source(AMC_SOURCE source, IFX_CLK clk, IFX_MASTER_SLAVE mode, IFX_I2S_SR sr, IFX_I2S_SW sw, IFX_I2S_TRANS_MODE trans, IFX_I2S_SETTINGS settings, IFX_I2S_AUDIO_MODE audio, IFX_I2S_UPDATES update, IFX_TRANSDUCER_MODE_SOURCE transducer_source)
{
    AMC_STATUS cmdStatus;
    amc_configure_source_UnBlocking(source,clk,mode,sr,sw,trans,settings,audio,update,transducer_source,&cmdStatus);
    at_waitForCmdCompletion((AT_STATUS *) &cmdStatus);
    return cmdStatus;
}

AMC_STATUS amc_setGainsource(AMC_SOURCE source, int gainIndex)
{
    AMC_STATUS cmdStatus;
    amc_setGainsourceUnBlocking(source,  gainIndex, &cmdStatus);
    at_waitForCmdCompletion((AT_STATUS *) &cmdStatus);
    return cmdStatus;
}

AMC_STATUS amc_setGaindest(AMC_DEST dest, int gainIndex)
{
    AMC_STATUS cmdStatus;
    amc_setGaindestUnBlocking(dest,  gainIndex, &cmdStatus);
    at_waitForCmdCompletion((AT_STATUS *) &cmdStatus);
    return cmdStatus;
}


AMC_STATUS amc_setAcoustic(AMC_ACOUSTIC acousticProfile)
{
    AMC_STATUS cmdStatus;
    amc_setAcousticUnBlocking(acousticProfile,  &cmdStatus);
    at_waitForCmdCompletion((AT_STATUS *) &cmdStatus);
    return cmdStatus;
}
AMC_STATUS dial(char *number)
{
    char *num;
    num = number;
    AMC_STATUS cmdStatus;
    amc_DialingUnBlocking(num,  &cmdStatus);
    at_waitForCmdCompletion((AT_STATUS *) &cmdStatus);
    return cmdStatus;
}

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
#define LOG_TAG "AudiomodemControl_base"
#include <utils/Log.h>
#include <stdarg.h>
static bool isInitialized; /*= false Keep track of library initialization*/

AT_STATUS amc_stop(void)
{
    isInitialized = false;
    return at_stop();
}

AT_STATUS amc_waitForCmdCompletion()
{
    AT_STATUS cmdStatus;
    at_waitForCmdCompletion();
    return cmdStatus;
}

AT_STATUS amc_route(AMC_SOURCE source, ...)
{
    AT_STATUS cmdStatus;
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
    cmdStatus = amc_routeUnBlocking(source, destab, i);
    return cmdStatus;
}

AT_STATUS amc_enable(AMC_SOURCE source)
{
    AT_STATUS cmdStatus;
    cmdStatus = amc_enableUnBlocking(source);
    return cmdStatus;
}

AT_STATUS amc_disable(AMC_SOURCE source)
{
    AT_STATUS cmdStatus;
    cmdStatus = amc_disableUnBlocking(source);
    return cmdStatus;
}
AT_STATUS amc_configure_dest(AMC_DEST dest, IFX_CLK clk, IFX_MASTER_SLAVE mode, IFX_I2S_SR sr, IFX_I2S_SW sw, IFX_I2S_TRANS_MODE trans, IFX_I2S_SETTINGS settings, IFX_I2S_AUDIO_MODE audio, IFX_I2S_UPDATES update, IFX_TRANSDUCER_MODE_DEST transducer_dest)
{
    AT_STATUS cmdStatus;
    cmdStatus = amc_configure_dest_UnBlocking(dest,clk,mode,sr,sw,trans,settings,audio,update,transducer_dest);
    return cmdStatus;
}

AT_STATUS amc_configure_source(AMC_SOURCE source, IFX_CLK clk, IFX_MASTER_SLAVE mode, IFX_I2S_SR sr, IFX_I2S_SW sw, IFX_I2S_TRANS_MODE trans, IFX_I2S_SETTINGS settings, IFX_I2S_AUDIO_MODE audio, IFX_I2S_UPDATES update, IFX_TRANSDUCER_MODE_SOURCE transducer_source)
{
    AT_STATUS cmdStatus;
    cmdStatus = amc_configure_source_UnBlocking(source,clk,mode,sr,sw,trans,settings,audio,update,transducer_source);
    return cmdStatus;
}

AT_STATUS amc_setGainsource(AMC_SOURCE source, int gainIndex)
{
    AT_STATUS cmdStatus;
    cmdStatus = amc_setGainsourceUnBlocking(source,  gainIndex);
    return cmdStatus;
}

AT_STATUS amc_setGaindest(AMC_DEST dest, int gainIndex)
{
    AT_STATUS cmdStatus;
    cmdStatus = amc_setGaindestUnBlocking(dest,  gainIndex);
    return cmdStatus;
}


AT_STATUS amc_setAcoustic(AMC_ACOUSTIC acousticProfile)
{
    AT_STATUS cmdStatus;
    cmdStatus = amc_setAcousticUnBlocking(acousticProfile);
    return cmdStatus;
}

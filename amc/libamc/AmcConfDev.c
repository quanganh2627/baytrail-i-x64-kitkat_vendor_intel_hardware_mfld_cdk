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
#include "ATmodemControl.h"
#define LOG_TAG "Amc_dev_conf"
#include <utils/Log.h>
#include <stdarg.h>
#include <malloc.h>
#include <unistd.h>

static  destForSourceRoute *pdestForSource[NBR_ROUTE] = { NULL, };
static void get_route_id(AMC_ROUTE_ID route, destForSourceRoute *pdestForSource);

void amc_dest_for_source()
{
    int i, j;
    LOGD("Dest for source init enter");
    destForSourceRoute *pdestForSourceTmp[NBR_ROUTE] = { NULL, };

    for (i = 0; i < NBR_ROUTE; i++) {
        pdestForSourceTmp[i] = (destForSourceRoute*) malloc(sizeof(destForSourceRoute));
        if (!pdestForSourceTmp[i]) {
            for (j = i; j >= 0; j--) {
                free(pdestForSourceTmp[j]);
            }
            LOGE("Dest for source error 1rst malloc");
            return;
        }
        else {
            pdestForSourceTmp[i]->dests = (AMC_DEST*) malloc(sizeof(AMC_DEST) * NBR_DEST);
            if (!pdestForSourceTmp[i]->dests) {
                for (j = i; j >= 0; j--) {
                    free(pdestForSourceTmp[j]);
                }
                LOGE("Dest for source error 2nd malloc");
                return;
            }
        }
        get_route_id((AMC_ROUTE_ID)i, &pdestForSourceTmp[i][0]);
        pdestForSource[i] = pdestForSourceTmp[i];
    }
}

void get_route_id(AMC_ROUTE_ID route, destForSourceRoute *pdestForSource)
{
    switch (route)
    {
    case ROUTE_RADIO:
        pdestForSource->nbrDest = 1;
        pdestForSource->source = AMC_RADIO_RX;
        pdestForSource->dests[0] = AMC_I2S1_TX;
        break;
    case ROUTE_I2S1:
        pdestForSource->nbrDest = 1;
        pdestForSource->source = AMC_I2S1_RX;
        pdestForSource->dests[0] = AMC_RADIO_TX;
        break;
    case ROUTE_I2S2:
        pdestForSource->nbrDest = 1;
        pdestForSource->source = AMC_I2S2_RX;
        pdestForSource->dests[0] = AMC_I2S1_TX;
        break;
    case ROUTE_TONE:
        pdestForSource->nbrDest = 1;
        pdestForSource->source = AMC_SIMPLE_TONES;
        pdestForSource->dests[0] = AMC_I2S1_TX;
        break;
    case ROUTE_RECORD_RADIO:
        pdestForSource->nbrDest = 2;
        pdestForSource->source = AMC_RADIO_RX;
        pdestForSource->dests[0] = AMC_I2S1_TX;
        pdestForSource->dests[1] = AMC_I2S2_TX;
        break;
    case ROUTE_RECORD_I2S1:
        pdestForSource->nbrDest = 2;
        pdestForSource->source = AMC_I2S1_RX;
        pdestForSource->dests[0] = AMC_RADIO_TX;
        pdestForSource->dests[1] = AMC_I2S2_TX;
        break;
    case ROUTE_DISCONNECT_RADIO:
        pdestForSource->nbrDest = 1;
        pdestForSource->source = AMC_RADIO_RX;
        pdestForSource->dests[0] = AMC_PCM_GENERALD;
        break;
    default :
        break;
    }
}

int amc_modem_conf_msic_dev(AMC_TTY_STATE tty)
{
    IFX_TRANSDUCER_MODE_SOURCE modeTtySource;
    IFX_TRANSDUCER_MODE_DEST modeTtyDest;

    switch (tty)
    {
    case AMC_TTY_OFF:
        modeTtySource = IFX_USER_DEFINED_15_S;
        modeTtyDest = IFX_USER_DEFINED_15_D;
        break;
    case AMC_TTY_FULL:
        modeTtySource = IFX_TTY_S;
        modeTtyDest = IFX_TTY_D;
        break;
    case AMC_TTY_VCO:
        modeTtySource = IFX_USER_DEFINED_15_S;
        modeTtyDest = IFX_TTY_D;
        break;
    case AMC_TTY_HCO:
        modeTtySource = IFX_TTY_S;
        modeTtyDest = IFX_USER_DEFINED_15_D;
        break;
    default:
        return -1;
    }
    amc_configure_source(AMC_I2S1_RX, IFX_CLK1, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, modeTtySource);
    amc_configure_dest(AMC_I2S1_TX, IFX_CLK1, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, modeTtyDest);
    amc_configure_source(AMC_I2S2_RX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
    amc_configure_dest(AMC_I2S2_TX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
    amc_route(&pdestForSource[ROUTE_DISCONNECT_RADIO][0]);
    amc_route(&pdestForSource[ROUTE_I2S1][0]);
    amc_route(&pdestForSource[ROUTE_I2S2][0]);
    amc_route(&pdestForSource[ROUTE_TONE][0]);
    return 0;
}

int amc_modem_conf_bt_dev()
{
    amc_configure_source(AMC_I2S1_RX, IFX_CLK1, IFX_MASTER, IFX_SR_8KHZ, IFX_SW_16, IFX_PCM, I2S_SETTING_NORMAL, IFX_MONO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
    amc_configure_source(AMC_I2S2_RX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
    amc_configure_dest(AMC_I2S1_TX, IFX_CLK1, IFX_MASTER, IFX_SR_8KHZ, IFX_SW_16, IFX_PCM, I2S_SETTING_NORMAL, IFX_MONO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
    amc_configure_dest(AMC_I2S2_TX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
    amc_route(&pdestForSource[ROUTE_DISCONNECT_RADIO][0]);
    amc_route(&pdestForSource[ROUTE_I2S1][0]);
    amc_route(&pdestForSource[ROUTE_I2S2][0]);
    amc_route(&pdestForSource[ROUTE_TONE][0]);
    return 0;
}

int amc_off()
{
    usleep(1000); // Time to disable MSIC...
    amc_route(&pdestForSource[ROUTE_DISCONNECT_RADIO][0]);
    amc_disable(AMC_I2S1_RX);
    amc_disable(AMC_I2S2_RX);
    usleep(80000); // Time to Disable modem I2S...
    return 0;
}

int amc_on()
{
    amc_enable(AMC_I2S2_RX);
    amc_enable(AMC_I2S1_RX);
    amc_route(&pdestForSource[ROUTE_RADIO][0]);
    usleep(40000); // Time to Enable modem I2S...
    return 0;
}

int amc_mute()
{
    amc_setGaindest(AMC_I2S1_TX, 0);
    amc_setGaindest(AMC_RADIO_TX, 0);
    usleep(1000); // Time to mute
    return 0;
}

int amc_unmute(int gainDL, int gainUL)
{
    usleep(40000); // Time to have MSIC in a "stable" state...
    amc_setGaindest(AMC_I2S1_TX, gainDL);
    amc_setGaindest(AMC_RADIO_TX, gainUL);
    return 0;
}

int amc_voice_record_on()
{
    amc_route(&pdestForSource[ROUTE_RECORD_RADIO][0]);
    amc_route(&pdestForSource[ROUTE_RECORD_I2S1][0]);
    return 0;
}

int amc_voice_record_off()
{
    amc_route(&pdestForSource[ROUTE_RADIO][0]);
    amc_route(&pdestForSource[ROUTE_I2S1][0]);
    return 0;
}

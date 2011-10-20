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

int amc_modem_conf_msic_dev(bool tty)
{
    if (tty == false)
    {
        amc_configure_source(AMC_I2S1_RX, IFX_CLK1, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
        amc_configure_source(AMC_I2S2_RX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
        amc_configure_dest(AMC_I2S1_TX, IFX_CLK1, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
        amc_configure_dest(AMC_I2S2_TX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
    }
    else if (tty == true)
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
    amc_setGaindest(AMC_RADIO_TX, MODEMGAIN); /*must be removed when gain will be integrated in MODEM (IMC) */
    return 0;
}

int amc_modem_conf_bt_dev()
{
    amc_configure_source(AMC_I2S1_RX, IFX_CLK1, IFX_MASTER, IFX_SR_8KHZ, IFX_SW_16, IFX_PCM, I2S_SETTING_NORMAL, IFX_MONO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
    amc_configure_source(AMC_I2S2_RX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_S);
    amc_configure_dest(AMC_I2S1_TX, IFX_CLK1, IFX_MASTER, IFX_SR_8KHZ, IFX_SW_16, IFX_PCM, I2S_SETTING_NORMAL, IFX_MONO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
    amc_configure_dest(AMC_I2S2_TX, IFX_CLK0, IFX_MASTER, IFX_SR_48KHZ, IFX_SW_16, IFX_NORMAL, I2S_SETTING_NORMAL, IFX_STEREO, IFX_UPDATE_ALL, IFX_USER_DEFINED_15_D);
    amc_route(AMC_RADIO_RX, AMC_I2S1_TX, AMC_ENDD);
    amc_route(AMC_I2S1_RX, AMC_RADIO_TX, AMC_ENDD);
    amc_route(AMC_I2S2_RX, AMC_I2S1_TX, AMC_ENDD);
    amc_route(AMC_SIMPLE_TONES, AMC_I2S1_TX, AMC_ENDD);
    amc_setGaindest(AMC_RADIO_TX, MODEMGAIN); /*must be removed when gain will be integrated in MODEM (IMC) */
    return 0;
}

int amc_off()
{
#if 0 // ISSUE when baseband modem is set for 2G call !!!!!!!!!!
    amc_disable(AMC_RADIO_RX);
#endif
    amc_disable(AMC_I2S1_RX);
    amc_disable(AMC_I2S2_RX);
    return 0;
}

int amc_on()
{
    amc_enable(AMC_I2S2_RX);
    amc_enable(AMC_I2S1_RX);
#if 0 // ISSUE when baseband modem is set for 2G call !!!!!!!!!!
    amc_enable(AMC_RADIO_RX);
#endif
    return 0;
}

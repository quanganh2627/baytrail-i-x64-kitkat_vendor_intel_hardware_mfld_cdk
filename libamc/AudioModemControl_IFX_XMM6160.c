/*
 **
 ** Copyright 2010 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **	 http: www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */


#include "AudioModemControl.h"
#ifdef MODEM_IFX_XMM6160

#include <sys/types.h>  /* Android needs this on top (size_t for string.h) */
#include <stdio.h>
#include <string.h>
#include "ATmodemControl.h"


/*==============================================================================
 * Audio Modem Control Implementation for IFX XMM6160
 * ========================================================================== */

/*------------------------------------------------------------------------------
 * Log Settings:
 *----------------------------------------------------------------------------*/

#define LOG_TAG "AudioModemControl"
#define LOG_LEVEL LOG_LEVEL_DEBUG /* for linux*/
#define LOCAL_LOGV 1		/* 0 / 1 */
#define LOCAL_LOGD 1		/* 0 / 1 */
#define KEEP_LOCAL_LOGV 0	/* 0 / 1 */
#include "log.h"

/*------------------------------------------------------------------------------
 * Modem Specific Information
 *------------------------------------------------------------------------------
 *
 * Modem Port   |	Software logical port
 *	I2SX	  |	I2S0
 *	I2SY	  |	I2S1
 */

volume_control volumeCTRL = {
	5,					/* gain step size(0.1 dB) */
	-440,				/* minimum gain(0.1 dB)  */
	10,					/* maximum gain(0.1 dB) */
};


/*------------------------------------------------------------------------------
 * Internal globals and helpers: AT Commands & I2S configuration
 *----------------------------------------------------------------------------*/

/* AT API  UTA Audio Commands */
#define GET_USE_CASE_SRC  "AT+XDRV=40,0,"
#define GET_USE_CASE_DEST  "AT+XDRV=40,1,"
#define EN_SRC  "AT+XDRV=40,2,%i"
#define EN_SRC_RESP "+XDRV: 40,2,"
#define DIS_SRC "AT+XDRV=40,3,%i"
#define DIS_SRC_RESP "+XDRV: 40,3,"
#define SET_SRC_CONF "AT+XDRV=40,4,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i"
#define SET_SRC_CONF_RESP "+XDRV: 40,4,"
#define SET_DEST_CONF "AT+XDRV=40,5,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i"
#define SET_DEST_CONF_RESP "+XDRV: 40,5,"
#define SET_RX_SRC_CONF "AT+XDRV=40,4,%i,%i"
#define SET_RX_SRC_CONF_RESP "+XDRV: 40,4,"
#define SET_TX_DEST_CONF "AT+XDRV=40,5,%i,%i"
#define SET_TX_DEST_CONF_RESP "+XDRV: 40,5,"
#define SET_SRC_DEST "AT+XDRV=40,6,%i,%i"
#define SET_SRC_DEST_RESP "+XDRV: 40,6,"
#define SET_SRC_VOL "AT+XDRV=40,7,%i,%i"
#define SET_SRC_VOL_RESP "+XDRV: 40,7,"
#define SET_DEST_VOL "AT+XDRV=40,8,%i,%i"
#define SET_DEST_VOL_RESP "+XDRV: 40,8,"
#define MODIFY_HF "AT+XDRV=40,11,%i,%i,%i"
#define MODIFY_HF_RESP "+XDRV: 40,11,"
#define SET_RTD "AT+XDRV = 40,12,"
#define ATD "ATD%s;"
#define DIAL_RESP "OK"


/*------------------------------------------------------------------------------
 * AudioModemControl API implementation:
 *----------------------------------------------------------------------------*/


AMC_STATUS amc_enableUnBlocking(AMC_SOURCE source, AMC_STATUS *pCmdStatus)
{
	char cmdStr[AT_MAX_CMD_LENGTH];
	char partStr[AT_MAX_CMD_LENGTH];
	AMC_STATUS rts;
	int i = 0;
	bool isFirstRoute = true;
	int srcPort;
	int hwSink;
	/* check for nothing to do: */

	sprintf(cmdStr, EN_SRC, source);
	rts = at_send(cmdStr, EN_SRC_RESP);
	if (rts != AMC_OK) {
		LOGW("Partial(de)activation.");
		if (pCmdStatus != NULL)
			(*pCmdStatus) = (AMC_STATUS)rts;
		return rts;
	}
	else
		return rts;
}
AMC_STATUS amc_disableUnBlocking(AMC_SOURCE source, AMC_STATUS *pCmdStatus)
{
	char cmdStr[AT_MAX_CMD_LENGTH];
	char partStr[AT_MAX_CMD_LENGTH];
	AMC_STATUS rts;
	int i = 0;
	bool isFirstRoute = true;
	int srcPort;
	int hwSink;
	/* check for nothing to do: */

	sprintf(cmdStr, DIS_SRC, source);
	rts = at_send(cmdStr, DIS_SRC_RESP);
	if (rts != AMC_OK) {
		LOGW("Partial(de)activation.");
		if (pCmdStatus != NULL)
			(*pCmdStatus) = (AMC_STATUS)rts;
		return rts;
	}
	else
		return rts;
}

AMC_STATUS amc_configure_dest_UnBlocking(AMC_DEST dest, IFX_CLK clk, IFX_MASTER_SLAVE mode, IFX_I2S_SR sr, IFX_I2S_SW sw, IFX_I2S_TRANS_MODE trans, IFX_I2S_SETTINGS settings, IFX_I2S_AUDIO_MODE audio, IFX_I2S_UPDATES update, IFX_TRANSDUCER_MODE_DEST transducer_dest,
		AMC_STATUS *pCmdStatus)
{
	char cmdStr[AT_MAX_CMD_LENGTH];
	char partStr[AT_MAX_CMD_LENGTH];
	AMC_STATUS rts;
	int i = 0;
	bool isFirstRoute = true;
	int srcPort;
	int hwSink;

	sprintf(cmdStr, SET_DEST_CONF, dest, 0, clk, mode, sr, sw, trans, settings, audio, update, transducer_dest);
	rts = at_send(cmdStr, SET_DEST_CONF_RESP);
	if (rts != AMC_OK) {
		LOGW("Partial configure.");
		if (pCmdStatus != NULL)
			(*pCmdStatus) = (AMC_STATUS)rts;
		return rts;
	}

	if (pCmdStatus != NULL)
		(*pCmdStatus) = (AMC_STATUS)rts;
	return rts;
	LOGW("Error Configure");

}


AMC_STATUS amc_configure_source_UnBlocking(AMC_SOURCE source, IFX_CLK clk, IFX_MASTER_SLAVE mode, IFX_I2S_SR sr, IFX_I2S_SW sw, IFX_I2S_TRANS_MODE trans, IFX_I2S_SETTINGS settings, IFX_I2S_AUDIO_MODE audio, IFX_I2S_UPDATES update, IFX_TRANSDUCER_MODE_SOURCE transducer_source, AMC_STATUS *pCmdStatus)
{
	char cmdStr[AT_MAX_CMD_LENGTH];
	char partStr[AT_MAX_CMD_LENGTH];
	AMC_STATUS rts;
	int i = 0;
	bool isFirstRoute = true;
	int srcPort;
	int hwSink;

	sprintf(cmdStr, SET_SRC_CONF, source, 0, clk, mode, sr, sw, trans, settings, audio, update, transducer_source);
	rts = at_send(cmdStr, SET_SRC_CONF_RESP);
	if (rts != AMC_OK) {
		LOGW("Partial configure.");
		if (pCmdStatus != NULL)
			(*pCmdStatus) = (AMC_STATUS)rts;
		return rts;
	}
	if (pCmdStatus != NULL)
		(*pCmdStatus) = (AMC_STATUS)rts;
	return rts;
}

AMC_STATUS amc_routeUnBlocking(AMC_SOURCE source,
							   AMC_DEST dest, AMC_STATUS *pCmdStatus)
{
	/* Warning; in IFX  AT API, the "routing" AT cmd automagically
	 * activate/deactivate the related TX part of hardware interface when
	 * the attached sources are enabled. Thus:
	 * - update the matrix according to the parameters
	 * - routing option:
	 *	AMC_ON: send the AT cmd only if the sinkPort hardware is active
	 *	AMC_OFF: always send the AT cmd */
	char cmdStr[AT_MAX_CMD_LENGTH];
	char partStr[AT_MAX_CMD_LENGTH];

	sprintf(cmdStr, SET_SRC_DEST, source, dest);
	return (AMC_STATUS)at_sendUnBlocking(
			   cmdStr, SET_SRC_DEST_RESP, (AT_STATUS *)pCmdStatus);
}


AMC_STATUS amc_setGainsourceUnBlocking(
	AMC_SOURCE source, int gainDDB, AMC_STATUS *pCmdStatus)
{
	/* Remind: Modem AT API integer 0 to 100, 0=mute, 100:+6dB, ssize: 0.5dB.*/
	int gainIndex;
	char cmdStr[AT_MAX_CMD_LENGTH];
	sprintf(cmdStr, SET_SRC_VOL, source, gainDDB);
	return (AMC_STATUS)at_sendUnBlocking(
			   cmdStr, SET_SRC_VOL_RESP, (AT_STATUS *)pCmdStatus);

}

AMC_STATUS amc_setGaindestUnBlocking(
	AMC_DEST dest, int gainDDB, AMC_STATUS *pCmdStatus)
{
	/* Remind: Modem AT API integer 0 to 100, 0=mute, 100:+6dB, ssize: 0.5dB.*/
	int gainIndex;
	char cmdStr[AT_MAX_CMD_LENGTH];
	sprintf(cmdStr, SET_DEST_VOL, dest, gainDDB);
	return (AMC_STATUS)at_sendUnBlocking(
			   cmdStr, SET_DEST_VOL_RESP, (AT_STATUS *)pCmdStatus);

}


AMC_STATUS amc_setAcousticUnBlocking(
	AMC_ACOUSTIC acousticProfile, AMC_STATUS *pCmdStatus)
{
	char cmdStr[AT_MAX_CMD_LENGTH];
	/*,<allow_echo_canceller>,<allow_agc>,<allow_tx_noise_reduction>*/
	if (acousticProfile == AMC_HANDFREE)
		sprintf(cmdStr, MODIFY_HF,1,1,1);
	else
		sprintf(cmdStr, MODIFY_HF,0,0,0);
	return (AMC_STATUS) at_sendUnBlocking(
			   cmdStr, MODIFY_HF_RESP, (AT_STATUS *)pCmdStatus);
}

AMC_STATUS amc_DialingUnBlocking(
	char *number, AMC_STATUS *pCmdStatus)
{
	char cmdStr[AT_MAX_CMD_LENGTH];
	sprintf(cmdStr,ATD, number);
	return (AMC_STATUS) at_sendUnBlocking(
			   cmdStr, DIAL_RESP, (AT_STATUS *)pCmdStatus);
}
#endif /*MODEM_IFX_XMM6160*/



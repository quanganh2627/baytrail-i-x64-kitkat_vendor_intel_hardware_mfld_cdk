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

#ifndef AT_MODEM_CONTROL_H
#define AT_MODEM_CONTROL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <pthread.h>

	/*==============================================================================
	 * AT Modem Control Library
	 *============================================================================*/

	/* Asumptions made:
	 * 	The modem response will be one of below:
	 * 	  a 2 line response: "echo" of the command send | "OK" or "ERROR"
	 *	  a 3 line response: "echo" of the command send | response | "OK" or "ERROR"
	 * 	Unsolicited response any time but not in the middle of a command response.*/

	/* Before use, (un)comment the wanted below
	 * platform customization & DEBUG defines*/


	/*------------------------------------------------------------------------------
	 * Platform Customization & DEBUG
	 *----------------------------------------------------------------------------*/
	/* Set to enable time stamped log for AT comm with modem:*/
	/*#define AT_DBG_LOG_TIME */
	/*Set to use below files for input/output instead of the char device driver:*/
	/*#define AT_DBG_FILE */
	/*IN/OUT <=> to/from Modem:*/
#define AT_DBG_FIN "/home/cducottx/cedric/work/w01-libmodem/03-file_sim/in.txt"
#define AT_DBG_FOUT "/home/cducottx/cedric/work/w01-libmodem/03-file_sim/out.txt"


	/*------------------------------------------------------------------------------
	 * OS / language specific:
	 *----------------------------------------------------------------------------*/

#ifndef __cplusplus
#define bool int
#define true 1
#define false 0
#endif /* #ifndef __cplusplus*/


	/*------------------------------------------------------------------------------
	 * Globals
	 *----------------------------------------------------------------------------*/

#define AT_MAX_CMD_LENGTH 80
#define AT_MAX_RESP_LENGTH 300

	/* Return status:*/
	typedef enum {
		AT_OK = 0,
		AT_RUNNING = 1, 		/* Command sent but no modem response yet.*/
		AT_ERROR = 2,
		AT_UNABLE_TO_CREATE_THREAD,
		AT_UNABLE_TO_OPEN_DEVICE,
		AT_WRITE_ERROR,
		AT_READ_ERROR,
		AT_UNINITIALIZED,
	}
	AT_STATUS;

	extern pthread_mutex_t at_unsolicitedRespMutex;
	/* Mutex needed to access unsolicited response*/

	/*------------------------------------------------------------------------------
	 * API
	 *----------------------------------------------------------------------------*/

	/* Initializing*/

	AT_STATUS at_start(const char *pATchannel, char *pUnsolicitedATresp);
	/* Start the AT Modem Control library.
	 * Params:
	 * 	*pATchannel: full path to the char device driver
	 * 	pUnsolicitedATresp[AMC_MAX_RESP_LENGTH] : hold the unsolicited resp.
	 *			may be NULL (response discarded).
	 *			Need to be accessed using 'at_unsolicitedRespMutex'.*/


	AT_STATUS at_stop(void);
	/* Stop the Audio Modem Control library and free ressources*/


	/* Communicating*/

	AT_STATUS at_askUnBlocking(const char *pATcmd, const char *pRespPrefix,
							   char *pATresp, AT_STATUS *pCmdStatus);
	/* Main communication function
	 * Params:
	 * 	*pATcmd[AT_MAX_CMD_LENGTH] : without ending control char (will be trim)
	 * 	*pRespPrefix[AT_MAX_CMD_LENGTH] : string to seek to find back the proper
	 * 										Modem presponse/status
	 * 	*pATresp[AT_MAX_RESP_LENGTH] will contains the response when available.
	 *						   may be NULL (response discarded)
	 * 	*pCmdStatus char[AT_MAX_CMD_LENGTH] will contains the status when
	 * 							available. May be NULL (status discarded)
	 * 	return value: AMC_RUNNING if the response is properly sent
	 * Response:
	 *   prior accessing  *pRespPrefix or *pCmdStatus one of the below 2 func
	 *   should be used: at_waitForCmdCompletion() or at_isCmdCompleted()*/

	AT_STATUS at_sendUnBlocking(const char *pATcmd, const char *pRespPrefix,
								AT_STATUS *pCmdStatus);
	/* same as at_askUnBlocking() but not responses is expected*/

	AT_STATUS at_ask(const char *pATcmd, const char *pRespPrefix,
					 char *pATresp);
	/* same as at_askUnBlocking() but block passively up to an answwer from
	 * the modem is available*/

	AT_STATUS at_send(const char *pATcmd, const char *pRespPrefix);
	/* same as at_sendUnBlocking() but block passively up to an answwer from
	 * the modem is available*/


	/* Getting the response*/

	void at_waitForCmdCompletion(AT_STATUS *pCmdStatus);
	/* Use with unblocking functions: wait passively for the completion of
	 * the related command.*/

	bool at_isCmdCompleted(AT_STATUS *pCmdStatus);
	/* Use with unblocking functions: check the availability of the command
	 * response.*/


#ifdef __cplusplus
}
#endif

#endif /*AT_MODEM_CONTROL_H*/


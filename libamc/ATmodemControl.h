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
#ifndef __cplusplus
#define bool int
#define true 1
#define false 0
#endif /* #ifndef __cplusplus*/
#define AT_MAX_CMD_LENGTH 80
#define AT_MAX_RESP_LENGTH 300
#define AUDIO_AT_CHANNEL_NAME "/dev/gsmtty13"

/* Return status:*/
typedef enum {
    AT_OK = 0,
    AT_RUNNING = 1,/* Command sent but no modem response yet.*/
    AT_ERROR = 2,
    AT_UNABLE_TO_CREATE_THREAD,
    AT_UNABLE_TO_OPEN_DEVICE,
    AT_WRITE_ERROR,
    AT_READ_ERROR,
    AT_UNINITIALIZED,
} AT_STATUS;

extern pthread_mutex_t at_unsolicitedRespMutex;
AT_STATUS at_start(const char *pATchannel);
AT_STATUS at_stop(void);
AT_STATUS at_askUnBlocking(const char *pATcmd, const char *pRespPrefix,
            char *pATresp);
AT_STATUS at_ask(const char *pATcmd, const char *pRespPrefix,
            char *pATresp);
AT_STATUS at_send(const char *pATcmd, const char *pRespPrefix);
AT_STATUS at_waitForCmdCompletion();
bool at_isCmdCompleted(AT_STATUS *pCmdStatus);
#ifdef __cplusplus
}
#endif

#endif /*AT_MODEM_CONTROL_H*/


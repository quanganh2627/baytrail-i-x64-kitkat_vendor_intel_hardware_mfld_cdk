/*
 **
 ** Copyright 2011 Intel Corporation
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
#define MAX_WAIT_ACK_SECONDS 2

/* TTY status */
typedef enum {
    AMC_TTY_OFF,
    AMC_TTY_FULL,
    AMC_TTY_VCO,
    AMC_TTY_HCO
} AMC_TTY_STATE;

/* Return status:*/
typedef enum {
    AT_OK = 0,
    AT_RUNNING = 1,/* Command sent but no modem response yet.*/
    AT_ERROR = 2,
    AT_BUSY,
    AT_UNABLE_TO_CREATE_THREAD,
    AT_UNABLE_TO_OPEN_DEVICE,
    AT_WRITE_ERROR,
    AT_READ_ERROR,
    AT_UNINITIALIZED,

    AT_STATUS_NB
} AT_STATUS;

AT_STATUS at_start(const char *pATchannel);
AT_STATUS at_send(const char *pATcmd, const char *pRespPrefix);

#ifdef __cplusplus
}
#endif

#endif /*AT_MODEM_CONTROL_H*/


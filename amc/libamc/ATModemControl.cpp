/*
 **
 ** Copyright 2011 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 ** http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#define LOG_TAG "AMC"
#include "ATManager.h"
#include "AudioModemControl.h"
#include "EventThread.h"
#include <utils/Log.h>

extern "C" {

static const AT_STATUS translate_error_code[] = {
    AT_OK, /* [AT_CMD_OK] */
    AT_RUNNING, /* [AT_CMD_RUNNING] */
    AT_ERROR, /* [AT_CMD_ERROR] */
    AT_BUSY, /* [AT_CMD_BUSY] */
    AT_UNABLE_TO_CREATE_THREAD, /* [AT_CMD_UNABLE_TO_CREATE_THREAD] */
    AT_UNABLE_TO_OPEN_DEVICE, /* [AT_CMD_UNABLE_TO_OPEN_DEVICE] */
    AT_WRITE_ERROR, /* [AT_CMD_WRITE_ERROR] */
    AT_READ_ERROR, /* [AT_CMD_READ_ERROR] */
    AT_UNINITIALIZED, /* [AT_CMD_UNINITIALIZED] */
};

AT_STATUS at_start(const char *pATchannel)
{
    AT_CMD_STATUS eStatus = CATManager::getInstance()->start(pATchannel, MAX_WAIT_ACK_SECONDS);

    if (eStatus != AT_CMD_OK) {

        return translate_error_code[eStatus];
    }
    LOGD("*** ATmodemControl started");
    amc_dest_for_source();
    LOGV("After dest for source init matrix");

    return translate_error_code[eStatus];
}

AT_STATUS at_send(const char *pATcmd, const char *pRespPrefix)
{
    CATCommand command(pATcmd, pRespPrefix);

    CATManager* atManager = CATManager::getInstance();

    command.setManager(atManager);

    AT_CMD_STATUS eStatus = atManager->sendCommand(&command, true);

    return translate_error_code[eStatus];
}

}


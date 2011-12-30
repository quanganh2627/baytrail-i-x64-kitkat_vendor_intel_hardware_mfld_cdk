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
#define LOG_TAG "ATMANAGER_XCALLSTAT"
#include "CallStatUnsollicitedATCommand.h"
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <utils/Log.h>

#define AT_XCALL_STAT "AT+XCALLSTAT=1"
#define AT_XCALL_STAT_PREFIX "XCALLSTAT:"

#define base CUnsollicitedATCommand

CCallStatUnsollicitedATCommand::CCallStatUnsollicitedATCommand()
    : base(AT_XCALL_STAT, AT_XCALL_STAT_PREFIX), _bAudioPathAvailable(false), _uiCallSession(0)
{
    LOGD("%s", __FUNCTION__);
}

// Indicate if Modem Audio Path is available
bool CCallStatUnsollicitedATCommand::isAudioPathAvailable()
{
    LOGD("%s: avail = %d", __FUNCTION__, _bAudioPathAvailable);

    return _bAudioPathAvailable;
}

// Inherited from CUnsollicitedATCommand
//
// Answer is formated:
// +XCALLSTAT: <callId>,<statusId>
// This code may be repeated so that for each call one line
// is displayed (up to 6)
//
void CCallStatUnsollicitedATCommand::doProcessAnswer()
{
    LOGD("%s", __FUNCTION__);

    string str = getAnswer();

    LOGD("%s: ans=(%s) %d", __FUNCTION__, str.c_str(), str.find(getPrefix()));

    // Assert the answer has the CallStat prefix...
    assert((str.find(getPrefix()) != string::npos));

    char* cstr = new char [str.size()+1];
    strcpy (cstr, str.c_str());

    // Parse the answer: "Prefix: Index,Status\n"
    char* prefix = strtok(cstr, " ");
    uint32_t iCallIndex = strtol(strtok(NULL, ","), NULL, 0);
    uint32_t iCallStatus = strtol(strtok(NULL, "\n"), NULL, 0);

    LOGD("%s: PREFIX=(%s) CALLINDEX=(%d) CALLSTATUS=(%d)", __FUNCTION__, prefix, iCallIndex, iCallStatus);

    if(iCallIndex > _uiCallSession){

        // Increment the number of Call Sessions
        LOGD("%s New Call session started", __FUNCTION__);
        _uiCallSession+=1;
    }

    if (iCallStatus == CallAlerting || iCallStatus == CallActive || iCallStatus == CallHold)
    {
        // If call is alerting (MT), active (MO) or on hold (to keep the path in case
        //  of multisession or call swap
        _bAudioPathAvailable = true;
        LOGD("%s AudioPath available =%d", __FUNCTION__, _bAudioPathAvailable);

    } else if (iCallStatus == CallDisconnected) {

        // Call is disconnected, decrement Call Sessions number
        _uiCallSession-=1;
        LOGD("%s Call session (#%d) disconnected, active session (%d)", __FUNCTION__, iCallIndex, _uiCallSession);

        // If no more Call Session are active, reset Audio Path flag
        _bAudioPathAvailable = _uiCallSession != 0;
        LOGD("%s AudioPath available =%d", __FUNCTION__, _bAudioPathAvailable);

    }

    // Clear the answer and the status
    clearStatus();
}

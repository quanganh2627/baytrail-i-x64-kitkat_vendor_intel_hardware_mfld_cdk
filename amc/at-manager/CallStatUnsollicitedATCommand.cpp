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
#include "Tokenizer.h"

#define AT_XCALL_STAT "AT+XCALLSTAT=1"
#define AT_XCALL_STAT_PREFIX "+XCALLSTAT:"

#define base CUnsollicitedATCommand

CCallStatUnsollicitedATCommand::CCallStatUnsollicitedATCommand()
    : base(AT_XCALL_STAT, AT_XCALL_STAT_PREFIX), _bAudioPathAvailable(false), _uiCallSession(0)
{
    LOGD("%s", __FUNCTION__);
    for (int i = 0; i < MAX_CALL_SESSIONS; i++)
    {
         _abCallSessionStat[i] = CallDisconnected;
    }
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

    // Remove the prefix from the answer
    string strwoPrefix = str.substr(str.find(getPrefix()) + getPrefix().size());

    // Extract the xcallstat params using "," token
    Tokenizer tokenizer(strwoPrefix, ",");
    vector<string> astrItems = tokenizer.split();

    // Each line should only have 2 parameters...
    if (astrItems.size() != 2)
    {
         LOGD("%s wrong answer format...", __FUNCTION__);
         return ;
    }

    int32_t iCallIndex = strtol(astrItems[0].c_str(), NULL, 0);
    uint32_t uiCallStatus = strtoul(astrItems[1].c_str(), NULL, 0);

    LOGD("%s: CALLINDEX=(%d) CALLSTATUS=(%d)", __FUNCTION__, iCallIndex, uiCallStatus);

    if (iCallIndex > MAX_CALL_SESSIONS){

        LOGD("%s invalid call index", __FUNCTION__);
    }

    _abCallSessionStat[iCallIndex] = uiCallStatus;

    _bAudioPathAvailable = isModemAudioPathEnabled();

    // Clear the answer and the status
    clearStatus();
}

bool CCallStatUnsollicitedATCommand::isModemAudioPathEnabled()
{
    // if at least one of the call within the array of call status
    // is in the state corresponding to an established call
    // answers true...
    for (int i = 0; i < MAX_CALL_SESSIONS; i++)
    {
        if (_abCallSessionStat[i] == CallAlerting
                || _abCallSessionStat[i] == CallActive
                || _abCallSessionStat[i] == CallHold
                ||_abCallSessionStat[i] == CallConnected) {
            return true;
        }
    }
    return false;
}

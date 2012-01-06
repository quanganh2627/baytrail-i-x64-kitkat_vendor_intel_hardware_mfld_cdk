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
#define LOG_TAG "ATMANAGER_XPROGRESS"
#include "ProgressUnsollicitedATCommand.h"
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <utils/Log.h>

#define AT_XPROGRESS "AT+XPROGRESS=1"
#define AT_XPROGRESS_PREFIX "XPROGRESS:"

#define base CUnsollicitedATCommand

CProgressUnsollicitedATCommand::CProgressUnsollicitedATCommand()
    : base(AT_XPROGRESS, AT_XPROGRESS_PREFIX), _bAudioPathAvailable(false)
{
    LOGD("%s", __FUNCTION__);
}

// Indicate if Modem Audio Path is available
bool CProgressUnsollicitedATCommand::isAudioPathAvailable()
{
    LOGD("%s: avail = %d", __FUNCTION__, _bAudioPathAvailable);

    return _bAudioPathAvailable;
}

// Inherited from CUnsollicitedATCommand
//
// Answer is formated:
// +XPROGRESS: <callId>,<statusId>
// This code may be repeated so that for each call one line
// is displayed (up to 6)
//
void CProgressUnsollicitedATCommand::doProcessAnswer()
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
    uint32_t iProgressStatus = strtol(strtok(NULL, "\n"), NULL, 0);

    LOGD("%s: PREFIX=(%s) CALLINDEX=(%d) CALLSTATUS=(%d)", __FUNCTION__, prefix, iCallIndex, iProgressStatus);

    //
    // MT Call: audio path established on MTAcceptedTCHYetAvailable
    // MO Call: audio path established on AlertingInBandOrTCHNotYetAvailable or InBandToneAvailable
    //
    // MO / MT: audio path disconnected on LastSpeechCallEndedSpeechCanBeDisabled
    // (Do not have to care about the # of session)
    //
    if (iProgressStatus == AlertingInBandOrTCHNotYetAvailable || iProgressStatus == InBandToneAvailable || iProgressStatus == MTAcceptedTCHYetAvailable)
    {
        // If call is alerting (MT), active (MO) or on hold (to keep the path in case
        //  of multisession or call swap
        _bAudioPathAvailable = true;
        LOGD("%s AudioPath available =%d", __FUNCTION__, _bAudioPathAvailable);

    } else if (iProgressStatus == LastSpeechCallEndedSpeechCanBeDisabled) {
        _bAudioPathAvailable = false;
    }

    // Clear the answer and the status
    clearStatus();
}

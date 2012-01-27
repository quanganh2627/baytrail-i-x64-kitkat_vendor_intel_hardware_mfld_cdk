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
#pragma once
#include "UnsollicitedATCommand.h"
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <string>


using namespace std;

static const int max_call_session  =  6;

class CCallStatUnsollicitedATCommand : public CUnsollicitedATCommand
{
    enum CallStat {
        CallActive = 0,
        CallHold,
        CallDialing,
        CallAlerting,
        CallRinging,
        CallWaiting,
        CallDisconnected,
        CallConnected
    };

public:
    CCallStatUnsollicitedATCommand();

    // Indicate if Modem Audio Path is available
    bool isAudioPathAvailable();

private:
    // Inherited from CUnsollicitedATCommand
    virtual void doProcessAnswer();

    bool isModemAudioPathEnabled();

public:
    // Flag to indicate if Modem Audio Path is available
    bool _bAudioPathAvailable;

private:

    // Number of active call sessions
    uint32_t _uiCallSession;

    int _abCallSessionStat[max_call_session];
};


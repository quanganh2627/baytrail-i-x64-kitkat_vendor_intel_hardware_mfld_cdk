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

#define LOG_TAG "MODEM_AUDIO_MANAGER_INSTANCE"

#include <sys/types.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <utils/Log.h>

#include "BooleanProperty.h"
#include "ModemAudioManagerInstance.h"

static const string gpcDualStackProperty = "audiocomms.atm.isDualSimModem";

CModemAudioManager* CModemAudioManagerInstance::_pModemAudioManager = NULL;

CModemAudioManager* CModemAudioManagerInstance::create(IModemStatusNotifier *observer)
{
    assert(!_pModemAudioManager);

    // Read AudioComms.dualstack property
    CBooleanProperty pDualSimProp(gpcDualStackProperty, false);

    if (pDualSimProp.isSet()) {

        LOGD("%s: DUAL STACK SUPPORTED", __FUNCTION__);
        _pModemAudioManager = new CDualSimModemAudioManager(observer);

    } else {

        _pModemAudioManager = new CModemAudioManager(observer);
    }

    return _pModemAudioManager;
}

CModemAudioManager* CModemAudioManagerInstance::get()
{
    assert(_pModemAudioManager);

    return _pModemAudioManager;
}


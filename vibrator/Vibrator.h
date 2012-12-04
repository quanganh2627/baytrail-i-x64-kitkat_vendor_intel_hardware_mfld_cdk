/*
 **
 ** Copyright 2012 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **      http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#pragma once

#include <stdint.h>
#include <utils/threads.h>
#include "EventListener.h"

class CEventThread;
class CParameterMgrPlatformConnector;
class ISelectionCriterionTypeInterface;
class ISelectionCriterionInterface;
class CParameterMgrPlatformConnectorLogger;


class CVibrator : public IEventListener {
public:
    CVibrator();
    virtual ~CVibrator();

    // Check if vibrator is present
    bool isPresent();

    // Vibrator switch on/off
    bool switchOn(int32_t iDurationMs);
    bool switchOff();

    enum state_type {
        STATE_OFF,
        STATE_ON
    };

private:
    // Event processing - From IEventListener
    virtual bool onEvent(int iFd);
    virtual bool onError(int iFd);
    virtual bool onHangup(int iFd);
    virtual void onTimeout();
    virtual void onPollError();
    virtual bool onProcess(uint16_t uiEvent);

    // Common request processing
    bool processSwitchRequest(bool bOn, uint32_t uiDurationMs = -1);

    // Switch implementation
    void doSwitch(bool bOn);

    // PFW type value pairs type
    struct SSelectionCriterionTypeValuePair
    {
        int iNumerical;
        const char* pcLiteral;
    };

    // Used to fill types for PFW
    void fillSelectionCriterionType(ISelectionCriterionTypeInterface* pSelectionCriterionType, const SSelectionCriterionTypeValuePair* pSelectionCriterionTypeValuePairs, uint32_t uiNbEntries) const;

    // Mode type
    static const SSelectionCriterionTypeValuePair mVibratorStateValuePairs[];
    static const uint32_t mNbVibratorStateValuePairs;

    // The connector
    CParameterMgrPlatformConnector* _parameterMgrPlatformConnector;

    // Logger
    CParameterMgrPlatformConnectorLogger* _parameterMgrPlatformConnectorLogger;
    // Criteria Types
    ISelectionCriterionTypeInterface* _vibratorStateType;
    // Criteria
    ISelectionCriterionInterface* _vibratorState;

    // Parameter framework of vibrator subsystem
    static const char _acVibraPath[];
    static const char _acLogsOnPropName[];

    // Vibrator requested state
    bool _bOnRequested; // held by client thread
    // Time to switch off
    uint32_t _uiRequestedDurationMs; // held by client thread
    // Thread
    CEventThread* _pEventThread;
    // Thread started status
    bool _bThreadStarted;
    // Vibrator actual state
    bool _bOn; // held by local thread
    // Vibrator logs enabled
    bool _bLogsOn;
    // Lock
    android::Mutex _lock;

    // Vibration reduction percentage android propery key
    static const char _vibrationReductionPercentagePropertyKey[];
    // Vibration reduction percentage value
    int iVibrationReductionPercentage;
};

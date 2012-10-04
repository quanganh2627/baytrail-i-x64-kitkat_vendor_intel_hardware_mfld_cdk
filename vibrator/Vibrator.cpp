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

#define LOG_TAG "Vibrator"

#include <utils/Log.h>
#include <assert.h>

#include "EventThread.h"
#include "ParameterMgrPlatformConnector.h"
#include "SelectionCriterionTypeInterface.h"
#include "SelectionCriterionInterface.h"
#include "Vibrator.h"
#include "Property.h"

// Parameter framework of vibrator subsystem
const char CVibrator::_acVibraPath[] =  "/etc/parameter-framework/ParameterFrameworkConfigurationVibrator.xml";
const char CVibrator::_acLogsOnPropName[] = "Audiocomms.Vibrator.LogsOn";

/// PFW related definitions
// Logger
class CParameterMgrPlatformConnectorLogger : public CParameterMgrPlatformConnector::ILogger
{
public:
    CParameterMgrPlatformConnectorLogger() {}

    virtual void log(const std::string& strLog)
    {
        LOGD("%s", strLog.c_str());
    }
};

// State type
const CVibrator::SSelectionCriterionTypeValuePair CVibrator::mVibratorStateValuePairs[] = {
    { CVibrator::STATE_OFF, "Off" },
    { CVibrator::STATE_ON,  "On"  }
};
const uint32_t CVibrator::mNbVibratorStateValuePairs = sizeof(CVibrator::mVibratorStateValuePairs)/sizeof(CVibrator::mVibratorStateValuePairs[0]);


CVibrator::CVibrator() :
    _parameterMgrPlatformConnector(new CParameterMgrPlatformConnector(_acVibraPath)),
    _parameterMgrPlatformConnectorLogger(new CParameterMgrPlatformConnectorLogger),
    _bOnRequested(false),
    _uiRequestedDurationMs(-1),
    _bOn(false),
    _bLogsOn(false)
{
    LOGD("New CVibrator object");

    /// Check if logs should be activated
    _bLogsOn = TProperty<bool>(_acLogsOnPropName, false);
    LOGD("Vibrator logs status: %s", _bLogsOn ? "on" : "off");

    /// Start thread
    _pEventThread =  new CEventThread(this, _bLogsOn);
    _bThreadStarted = _pEventThread->start();

    if (!_bThreadStarted) {

        LOGE("Failed to create event thread");
    }

    /// Attach a Logger if the corresponding Android Property is turned on
    if (_bLogsOn) {
        _parameterMgrPlatformConnector->setLogger(_parameterMgrPlatformConnectorLogger);
    }

    /// Criteria Types
    // Mode
    _vibratorStateType = _parameterMgrPlatformConnector->createSelectionCriterionType();
    fillSelectionCriterionType(_vibratorStateType, mVibratorStateValuePairs, mNbVibratorStateValuePairs);

    /// Criteria
    _vibratorState = _parameterMgrPlatformConnector->createSelectionCriterion("VibratorState", _vibratorStateType);

    /// Start
    std::string strError;
    if (!_parameterMgrPlatformConnector->start(strError)) {

        LOGE("parameter-framework [Vibrator] start error: %s", strError.c_str());
    } else {

        LOGI("parameter-framework [Vibrator] successfully started!");
    }
}

CVibrator::~CVibrator()
{
    LOGD("Delete CVibrator object");

    // Delete event thread
    delete _pEventThread;

    // Switch OFF vibrator
    doSwitch(false);

    // Unset logger
    _parameterMgrPlatformConnector->setLogger(NULL);
    // Remove logger
    delete _parameterMgrPlatformConnectorLogger;
    // Remove connector
    delete _parameterMgrPlatformConnector;
}

// Check if vibrator is present
bool CVibrator::isPresent()
{
    return _parameterMgrPlatformConnector->isStarted();
}


// Used to fill types for PFW
void CVibrator::fillSelectionCriterionType(ISelectionCriterionTypeInterface* pSelectionCriterionType, const SSelectionCriterionTypeValuePair* pSelectionCriterionTypeValuePairs, uint32_t uiNbEntries) const
{
    assert(pSelectionCriterionType);
    assert(pSelectionCriterionTypeValuePairs);

    uint32_t uiIndex;

    for (uiIndex = 0; uiIndex < uiNbEntries; uiIndex++) {

        const SSelectionCriterionTypeValuePair* pValuePair = &pSelectionCriterionTypeValuePairs[uiIndex];

        pSelectionCriterionType->addValuePair(pValuePair->iNumerical, pValuePair->pcLiteral);
    }
}

// Vibrator switch on/off
bool CVibrator::switchOn(int32_t iDurationMs)
{
    if (iDurationMs < 0) {

        LOGW("%s : Unable to switch on (negative duration required)", __FUNCTION__);
        return false;
    }

    if (iDurationMs == 0) {
        LOGD("%s : Switch off vibrator as requested vibration duration is 0 ms)", __FUNCTION__);
        return processSwitchRequest(false);
    }

    return processSwitchRequest(true, iDurationMs);
}

bool CVibrator::switchOff()
{
    return processSwitchRequest(false);
}

// Common request processing
bool CVibrator::processSwitchRequest(bool bOn, uint32_t uiDurationMs)
{
    // Lock
    android::Mutex::Autolock lock(_lock);

    // Check thread status
    if (!_bThreadStarted) {

        LOGE("%s : Unable to process vibrator request as thread not started", __FUNCTION__);

        return false;
    }

    // Assign request
    _bOnRequested = bOn;
    _uiRequestedDurationMs = uiDurationMs;

    // Process
    _pEventThread->trig();

    return true;
}

//
// Worker thread context
// Event processing
//
bool CVibrator::onEvent(int iFd)
{
  LOGD("%s, FD : %d", __FUNCTION__, iFd);

    return false;
}

//
// Worker thread context
//
bool CVibrator::onError(int iFd)
{
  LOGE("%s, FD : %d", __FUNCTION__, iFd);

    return false;
}

//
// Worker thread context
//
bool CVibrator::onHangup(int iFd)
{
    // Treat as error
    return onError(iFd);
}

//
// Worker thread context
//
void CVibrator::onTimeout()
{
    if (_bLogsOn) {

        LOGD("%s", __FUNCTION__);
    }

    // Do the switch
    doSwitch(false);

    // Sleep forever
    _pEventThread->setTimeoutMs(-1);
}

//
// Worker thread context
//
void CVibrator::onPollError()
{
    LOGD("%s", __FUNCTION__);
}

//
// Worker thread context
//
void CVibrator::onProcess()
{
    if (_bLogsOn) {

        LOGD("%s", __FUNCTION__);
    }

    // Do the switch
    doSwitch(_bOnRequested);

    // Timeout
    _pEventThread->setTimeoutMs(_bOnRequested ? _uiRequestedDurationMs : -1);
}

// Switch implementation
void CVibrator::doSwitch(bool bOn)
{
    if (bOn != _bOn) {

        // Criterion states
        _vibratorState->setCriterionState(bOn);

        // Apply confiurations
        string strError;

        if (!_parameterMgrPlatformConnector->applyConfigurations(strError)) {

            LOGE("%s, applyConfigurations error: %s", __FUNCTION__, strError.c_str());
        }
    }

    // State
    _bOn = bOn;
}

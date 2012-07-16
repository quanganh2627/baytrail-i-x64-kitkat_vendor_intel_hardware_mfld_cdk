/* ATCommand.h
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

#include <stdint.h>
#include <stddef.h>
#include <sys/time.h>
#include <string>

using namespace std;

class CATManager;

class CATCommand
{

    enum Status {
        StatusUnset,
        StatusError,
        StatusOK
    };

public:
    CATCommand(const string& strCommand, const string& strRespPrefix = "");
    virtual ~CATCommand();

    // Clear
    void clearStatus();

    // Command
    const string& getCommand() const;

    // Send
    bool send(CATCommand* pATCommand, bool bSynchronous);

    // Answer fragment
    virtual void addAnswerFragment(const string& strAnswerFragment);

    // Answer
    const string& getAnswer() const;

    // Set Status
    virtual void setAnswerOK(bool bIsOK);

    // Get Status
    bool isAnswerOK() const;

    // Get reception completion
    bool isComplete() const;

    // Prefix
    const string& getPrefix() const;

    // Has prefix
    bool hasPrefix() const;

    // Receive timeout reached
    bool receiveTimeoutReached();

protected:
    // Set the timeout
    void setTimeout(uint32_t uiTimeout);


private:
    // AT Command
    string _strCommand;
    // Expected answer prefix
    string _strRespPrefix;
    // Answer placeholder
    string _strAnswer;
    // Status
    Status _eStatus;
};


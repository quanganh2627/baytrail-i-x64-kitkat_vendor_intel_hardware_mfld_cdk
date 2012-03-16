/* ATManager.h
 **
 ** Copyright 2011 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */
#pragma once

#include "ATCommand.h"
#include "PeriodicATCommand.h"
#include "UnsollicitedATCommand.h"
#include "EventListener.h"
#include <pthread.h>
#include <semaphore.h>
#include <list>

class CEventThread;
class CATParser;
class IATNotifier;

typedef enum {
    AT_CMD_OK = 0,
    AT_CMD_RUNNING = 1,/* Command sent but no modem response yet.*/
    AT_CMD_ERROR = 2,
    AT_CMD_BUSY,
    AT_CMD_UNABLE_TO_CREATE_THREAD,
    AT_CMD_UNABLE_TO_OPEN_DEVICE,
    AT_CMD_WRITE_ERROR,
    AT_CMD_READ_ERROR,
    AT_CMD_UNINITIALIZED,

    AT_CMD_NB
} AT_CMD_STATUS;

using namespace std;

class CATManager : public IEventListener {

    typedef list<CATCommand*>::iterator CATCommandListIterator;
    typedef list<CATCommand*>::const_iterator CATCommandListConstIterator;

    typedef list<CPeriodicATCommand*>::iterator CPeriodicATCommandListIterator;
    typedef list<CPeriodicATCommand*>::const_iterator CPeriodicATCommandConstListIterator;

    typedef list<CUnsollicitedATCommand*>::iterator CUnsollicedATCommandListIterator;
    typedef list<CUnsollicitedATCommand*>::const_iterator CUnsollicedATCommandConstListIterator;

    enum FileDesc {
        FdStmd,
        FdFromModem,
        FdToModem,

        ENbFileDesc
    };


public:
    CATManager(IATNotifier *observer = NULL);
    ~CATManager();

    // Start
    AT_CMD_STATUS start(const char* pcModemTty, uint32_t uiTimeoutSec);

    // Stop
    void stop();

    // Add Periodic AT Command to the list
    void addPeriodicATCommand(CPeriodicATCommand* pPeriodicATCommand);

    // Remove Periodic AT Command from the list
    void removePeriodicATCommand(CPeriodicATCommand* pPeriodicATCommand);

    // Add Unsollicited AT Command to the list
    void addUnsollicitedATCommand(CUnsollicitedATCommand* pUnsollicitedATCommand);

    // Remove Unsollicited AT Command from the list
    void removeUnsollicitedATCommand(CUnsollicitedATCommand* pUnsollicitedATCommand);

    // Send
    AT_CMD_STATUS sendCommand(CATCommand* pATCommand, bool bSynchronous);

    // Cancel
    void cancelCommand();

    // Get AT Notifier
    const IATNotifier* getATNotifier() const;
    IATNotifier* getATNotifier();

    // Get the modem status
    int getModemStatus() const;

private:
    // Event processing - From IEventListener
    virtual bool onEvent(int iFd);
    virtual bool onError(int iFd);
    virtual bool onHangup(int iFd);
    virtual void onTimeout();
    virtual void onPollError();
    virtual void onProcess();
    // Push AT command to the send list
    void pushCommandToSendList(CATCommand* pATCommand);
    // Pop AT command from the send list
    CATCommand* popCommandFromSendList(void);
    // Wait for the end of the transaction
    AT_CMD_STATUS waitEndOfTransaction(CATCommand* pATCommand);
    // terminate the transaction
    void terminateTransaction(bool bSuccess);
    // Resend the current command
    void resendCurrentCommand();
    // Process the send command list
    void processSendList();
    // Get Next Periodic Timeout
    int32_t getNextPeriodicTimeout() const;
    // Get AT response
    void readResponse();
    // Send String
    bool sendString(const char* pcString, int iFd);
    // Update modem status
    void updateModemStatus();
    // Send modem cold reset Ack to STMD
    bool sendModemColdResetAck();
    // Wait semaphore with timeout
    bool waitSemaphore(sem_t* pSemaphore, uint32_t uiTimeoutSec);
    // Terminate answer reception
    void terminateAnswerReception(bool bSuccess);
    // Start listening to Modem TTYs
    bool startModemTtyListeners();
    // Stop listening to Modem TTYs
    void stopModemTtyListeners();
    // Find unsollicited AT command by prefix
    CUnsollicitedATCommand* findUnsollicitedCmdByPrefix(const string& strRespPrefix) const;
    // Set the modem status
    void setModemStatus(uint32_t status);
    // Modem tty are available, performs all required actions
    void onTtyStateChanged(bool available);
    // Clear the toSend commands list
    void clearToSendList();
    // Check if unsollicited is awaited
    void processUnsollicited(const string& strAnswerFragment);
    // Check if cmd has a prefix that matches with received answer
    inline bool cmdHasPrefixAndMatches(CATCommand* cmd, const string& strRespPrefix) const
    {
        return cmd->hasPrefix() && !strRespPrefix.find(cmd->getPrefix());
    }

    // Checks if need to send cmd
    void checksAndProcessSendList();

    // Recovery procedure: reset of the modem
    bool sendRequestCleanup();


public:
    // Periodic command list
    list<CPeriodicATCommand*> _periodicATList;

    // Periodic command list
    list<CUnsollicitedATCommand*> _unsollicitedATList;

    // Command to send list
    // By design, AT manager is able to enqueue command
    // from the client and from its periodic/unsollicited
    // command list
    list<CATCommand*> _toSendATList;

    // Client AT command
    CATCommand* _pAwaitedTransactionEndATCommand;

private:

    // Running state
    bool _bStarted;
    // Modem state
    bool _bModemAlive;
    // Waiting Client flag
    bool _bClientWaiting;
    sem_t _clientWaitSemaphore;
    // Current AT Command
    CATCommand* _pCurrentATCommand;
    // Pending Client Command
    CATCommand* _pPendingClientATCommand;
    // Transaction timeout
    uint32_t _uiTimeoutSec;
    // Thread
    CEventThread* _pEventThread;
    // Client mutex
    pthread_mutex_t _clientMutex;
    // Answer wait semaphore
    sem_t _answerWaitSemaphore;
    // Modem status wait semaphore
    sem_t _firstModemStatusReceivedSemaphore;
    // Modem status wait semaphore created state
    bool _bFirstModemStatusReceivedSemaphoreCreated;
    // AT Parser
    CATParser* _pATParser;
    // Modem tty
    string _strModemTty;
    // Modem tty listeners state
    bool _bTtyListenersStarted;
    // Notification Interface
    IATNotifier* _pIATNotifier;
    // Modem Status
    uint32_t _mModemStatus;
    // Timeout retry counter
    int _iTimeoutRetry;
};


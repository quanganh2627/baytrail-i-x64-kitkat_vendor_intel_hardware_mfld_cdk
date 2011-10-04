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



#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <cutils/sockets.h>
#include <sys/socket.h>
#include <termios.h>
#include <semaphore.h>
#include <utils/Log.h>
#include <errno.h>
#include "ATmodemControl.h"
#include "AudioModemControl.h"
#include "stmd.h"

/*---------------------------------------------------------------------------*/
/* Definitions                                                               */
/*---------------------------------------------------------------------------*/

#define LOG_TAG "ATmodemControl"
#define LOG_NDEBUG 1
#define MAX_TIME_MODEM_STATUS_CHECK 60
#define TRY_MAX 5

/*---------------------------------------------------------------------------*/
/* Global Variables                                                          */
/*---------------------------------------------------------------------------*/

typedef struct RunningATcmd {
    char prefix[AT_MAX_CMD_LENGTH];
    AT_STATUS cmdstatus;
} RunningATcmd;

static sem_t sem;
static pthread_mutex_t GsmTtyMutex;
static pthread_cond_t GsmTtyStatus;
static pthread_mutex_t at_dataMutex;
static pthread_cond_t newRespReceived;
static pthread_t  threadId;
static pthread_t  GsmTtyThreadId;

static RunningATcmd *pCurrentRunningATcmd = NULL;
static unsigned int modem_status = MODEM_DOWN;
static bool GsmStatusInitialized = false;
static bool isInitialized;
static int fdIn;
static int fdOut;

/*---------------------------------------------------------------------------*/
/* Function declaration                                                      */
/*---------------------------------------------------------------------------*/

static void removeCtrlChar(char *pDest, const char *pSrc);
static AT_STATUS readATline(int fd,  char *pStr);
static AT_STATUS writeATline(int fd, const char *pATcmd);
static AT_STATUS atSendBlocking( const char *pATcmd, const char *pRespPrefix,
           char *pATresp);
static void propagateATerror(AT_STATUS status);
static void SetModemStatus(int data);

/*---------------------------------------------------------------------------*/
/* Modem Status thread                                                       */
/*---------------------------------------------------------------------------*/

static void *GsmTtyStatusThread(void *arg)
{
    int fd_socket, rts, data, data_size;
    fd_set fdSetTty;

    fd_socket = socket_local_client(SOCKET_NAME_MODEM_STATUS,
            ANDROID_SOCKET_NAMESPACE_RESERVED,
            SOCK_STREAM);
    if (fd_socket < 0)
    {
        LOGE("Failed to connect to modem-status socket %s\n", strerror(errno));
        pthread_cond_broadcast(&GsmTtyStatus);
        close(fd_socket);
        return 0;
    }
    FD_ZERO(&fdSetTty);
    FD_SET(fd_socket, &fdSetTty);
    GsmStatusInitialized = true;
    for(;;)
    {
        rts = select(fd_socket+1, &fdSetTty, NULL, NULL, NULL);
        if (rts > 0) {
            data_size = recv(fd_socket, &data, sizeof(unsigned int), 0);
            if (data_size != sizeof(unsigned int)) {
                LOGE("Modem status handler: wrong size [%d]\n", data_size);
                continue;
            }

            if (data == MODEM_UP) {
                LOGD("Modem status received: MODEM_UP\n");
                if (modem_status == MODEM_DOWN)
                    SetModemStatus(data);
            }
            else if (data == MODEM_DOWN) {
                LOGE("Modem status received: MODEM_DOWN\n");
                if (modem_status == MODEM_UP)
                    SetModemStatus(data);
            }
            else
                SetModemStatus(data);
        }
        else if ((rts == -1) && (errno != EINTR) && (errno != EAGAIN)) {
            SetModemStatus(MODEM_DOWN);
            GsmStatusInitialized = false;
            return 0;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Set modem Status                                                          */
/*---------------------------------------------------------------------------*/

static void SetModemStatus(int data)
{
    if (data == MODEM_UP)
        modem_status = MODEM_UP;
    else if (data == MODEM_DOWN)
        modem_status = MODEM_DOWN;
    else
        modem_status = MODEM_DOWN;
    pthread_mutex_lock(&GsmTtyMutex);
    pthread_cond_broadcast(&GsmTtyStatus);
    pthread_mutex_unlock(&GsmTtyMutex);
}

/*---------------------------------------------------------------------------*/
/* AT reader Thread                                                          */
/*---------------------------------------------------------------------------*/

static void *atReaderThread(void *arg)
{
    char prefix[AT_MAX_CMD_LENGTH];
    char resp[AT_MAX_RESP_LENGTH];
    char strStatus[AT_MAX_RESP_LENGTH];
    AT_STATUS status;
    AT_STATUS readStatus;

    LOGD("*** AT Reader thread started");
    for (;;) {
        for (;;) {
        LOGV("... Waiting for new prefix to read ...");
        do {
            readStatus = readATline(fdOut, prefix);
            if (readStatus == AT_ERROR) {
                propagateATerror(readStatus);
                return 0;
            }
        } while (readStatus != AT_OK);
        pthread_mutex_lock(&at_dataMutex);
        if (pCurrentRunningATcmd != NULL) {
            if (strpbrk(pCurrentRunningATcmd->prefix, prefix) != NULL) {
                LOGV("Prefix OK %s ",pCurrentRunningATcmd->prefix);
                pthread_mutex_unlock(&at_dataMutex);
                break;
            }
            else
                LOGV("Prefix NOK %s",pCurrentRunningATcmd->prefix);
        }
        else
            LOGD(" <=> %s(Unsol Resp)", prefix);
        pthread_mutex_unlock(&at_dataMutex);
    }
    LOGV("... Reading response and/or status ...");
    status = readATline(fdOut, resp);
    if (status != AT_OK) {
        resp[0] = '\0';
        if (status == AT_ERROR) {
            LOGE("AT_ERROR");
            propagateATerror(readStatus);
            return 0;
        }
    }
    else {
        if ((strcmp(resp, "OK") == 0) || (strcmp(resp, "ERROR") == 0)) {
            status = (strcmp(resp, "OK") == 0) ?  AT_OK : AT_ERROR;
            resp[0] = '\0';
            if (status == AT_ERROR) {
                LOGE("AT_ERROR");
                pthread_mutex_lock(&at_dataMutex);
                pCurrentRunningATcmd->cmdstatus = status;
                pthread_cond_broadcast(&newRespReceived);
                pthread_mutex_unlock(&at_dataMutex);
            }
        } else {
            LOGV("... Reading status ...");
            status = readATline(fdOut, strStatus);
            if (status == AT_OK)
                status = (strcmp(strStatus, "OK") == 0) ?  AT_OK : AT_ERROR;
            if (status == AT_ERROR) {
                LOGE("AT_ERROR");
                pthread_mutex_lock(&at_dataMutex);
                pCurrentRunningATcmd->cmdstatus = status;
                pthread_cond_broadcast(&newRespReceived);
                pthread_mutex_unlock(&at_dataMutex);
            }
        }
    }
    LOGD(" <=> |%s|: Resp = |%s|, Status = |%i|.", prefix, resp, status);
        if (status != AT_ERROR) {
            LOGE("AT_OK");
            pthread_mutex_lock(&at_dataMutex);
            pCurrentRunningATcmd->cmdstatus = status;
            pthread_cond_broadcast(&newRespReceived);
            pthread_mutex_unlock(&at_dataMutex);
        }
    }
    return(0);
}
/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

AT_STATUS at_start(const char *pATchannel)
{
    struct timespec to;
    int try = 0;
    to.tv_sec = time(NULL) + MAX_TIME_MODEM_STATUS_CHECK;
    to.tv_nsec = 0;

    if (isInitialized) {
        LOGI("ATmodemControl already started");
        return AT_OK;
    }
    if (!GsmStatusInitialized) {
        pthread_mutex_init(&GsmTtyMutex, NULL);
        pthread_cond_init(&GsmTtyStatus, NULL);
    /*check gsmtty status*/
    if (pthread_create(&GsmTtyThreadId, NULL, GsmTtyStatusThread, NULL) != 0) {
        LOGW("Unable to start thread Status TTY: error:");
        return AT_UNABLE_TO_CREATE_THREAD;
    }
    pthread_mutex_lock(&GsmTtyMutex);
    pthread_cond_timedwait(&GsmTtyStatus, &GsmTtyMutex, &to);
    pthread_mutex_unlock(&GsmTtyMutex);
    }
    if (modem_status == MODEM_UP) {
        LOGV("Starting ATmodemControl ...");
        // Open handle to modem device.
        fdIn = open(pATchannel, O_WRONLY|CLOCAL);
        if (fdIn < 0) {
            LOGW("Unable to open device for writing: %s, error: %s",
            pATchannel, strerror(errno));
            return AT_UNABLE_TO_OPEN_DEVICE;
        }
        fdOut = open(pATchannel, O_RDONLY|CLOCAL);
        if (fdOut < 0) {
            LOGW("Unable to open device for reading: %s, error: %s",
            pATchannel, strerror(errno));
            return AT_UNABLE_TO_OPEN_DEVICE;
        }
        pthread_mutex_init(&at_dataMutex, NULL);
        pthread_cond_init(&newRespReceived, NULL);
        sem_init(&sem, 0, 1);
        /* start AT reader thread */
        LOGV("Starting AT Reader thread ...");
        if (pthread_create(&threadId, NULL, atReaderThread, NULL) != 0) {
            LOGW("Unable to start thread: error: %s", strerror(errno));
            return AT_UNABLE_TO_CREATE_THREAD;
        }
        isInitialized = true;
        LOGD("*** ATmodemControl started");
        amc_dest_for_source();
        LOGV("After dest for source init matrix");
        return AT_OK;
    }
    else
        return AT_UNABLE_TO_OPEN_DEVICE;
}

/*---------------------------------------------------------------------------*/
/* LibAmc Deconfigure                                                        */
/*---------------------------------------------------------------------------*/

AT_STATUS at_stop()
{
    LOGD("Stopping ATmodemControl ...");
    close(fdIn);
    close(fdOut);
    free(pCurrentRunningATcmd);
    pCurrentRunningATcmd = NULL;
    isInitialized = false;
    LOGD("*** ATmodemControl stopped.");
    return AT_OK;
}

/*---------------------------------------------------------------------------*/
/* Write AT Cmd, check Status Running                                        */
/*---------------------------------------------------------------------------*/

AT_STATUS at_askUnBlocking(const char *pATcmd, const char *pRespPrefix,
           char *pATresp)
{
    assert(pATcmd != NULL);
    assert(pRespPrefix != NULL);
    RunningATcmd *pNewCmd;
    AT_STATUS cmdStatus;
    /* Allow default initialization: */
    if (!isInitialized) {
        cmdStatus = AT_UNINITIALIZED;
        LOGW("AT Modem Control not initialized.");
        return AT_UNINITIALIZED;
    }
    pNewCmd = (RunningATcmd  *) malloc(sizeof(RunningATcmd));
    if(!pNewCmd) {
        LOGW("malloc pNewCmd error");
        return AT_UNINITIALIZED;
    }
    pthread_mutex_lock(&at_dataMutex);
    cmdStatus = writeATline(fdIn, pATcmd);
    if (cmdStatus == AT_WRITE_ERROR) {
        free(pNewCmd);
        pthread_mutex_unlock(&at_dataMutex);
        return AT_ERROR;
    }
    removeCtrlChar(pNewCmd->prefix, pRespPrefix);
    pNewCmd->cmdstatus = cmdStatus;
    if (pCurrentRunningATcmd == NULL) {
        pCurrentRunningATcmd = pNewCmd;
        LOGD("AT RUNNING %s",pNewCmd->prefix);
    /* Send Commend */
        if (cmdStatus != AT_RUNNING)
            LOGW("... %s not written.", pATcmd);
        pthread_mutex_unlock(&at_dataMutex);
        return cmdStatus;
    }
    else {
        free(pNewCmd);
        pthread_mutex_unlock(&at_dataMutex);
        return AT_ERROR;
    }
}

/*---------------------------------------------------------------------------*/
/* Send AT Cmd                                                               */
/*---------------------------------------------------------------------------*/

AT_STATUS at_send(const char *pATcmd, const char *pRespPrefix)
{
    /* Blocking Send: wait for the modem cmd status.*/
    assert(pATcmd != NULL);
    AT_STATUS cmdStatus;
    int try = 0;

    if (modem_status == MODEM_UP) {
        LOGV("modem-status %d\n",modem_status);
        sem_wait(&sem);
        do {
            cmdStatus = atSendBlocking(pATcmd, pRespPrefix, NULL);
            if (cmdStatus != AT_OK) {
                at_stop();
                at_start(AUDIO_AT_CHANNEL_NAME);
            }
        } while (cmdStatus != AT_OK && ++try < TRY_MAX);

        sem_post(&sem);
        return cmdStatus;
    }
    else
        return AT_WRITE_ERROR;
}

/*---------------------------------------------------------------------------*/
/* Acknoledge Waiting                                                        */
/*---------------------------------------------------------------------------*/

AT_STATUS at_waitForCmdCompletion()
{
    struct timespec to;
    int try = 0;
    to.tv_sec = time(NULL) + MAX_WAIT_ACK;
    to.tv_nsec = 0;

    pthread_mutex_lock(&at_dataMutex);
    LOGV("Broadcast enter for at cmd%s ",pNewCmd->prefix);
    while (pCurrentRunningATcmd->cmdstatus == AT_RUNNING  && try < TRY_MAX) {
        pthread_cond_timedwait(&newRespReceived, &at_dataMutex, &to);
        LOGV("Broadcast exit for %s",pNewCmd->prefix);
        if (pCurrentRunningATcmd->cmdstatus == AT_OK) {
            LOGV("Release AT CMD %s", pNewCmd->prefix);
            free(pCurrentRunningATcmd);
            pCurrentRunningATcmd = NULL;
            break;
        }
        else if (pCurrentRunningATcmd->cmdstatus == AT_ERROR) {
            free(pCurrentRunningATcmd);
            pCurrentRunningATcmd = NULL;
            pthread_mutex_unlock(&at_dataMutex);
            return AT_ERROR;
        }
        LOGV("Broadcast exit for %s",pCurrentRunningATcmd->prefix);
        try++;
    }
    LOGV("wait for completion end at cmd handled");
    pthread_mutex_unlock(&at_dataMutex);
    if (try == TRY_MAX)
        return AT_READ_ERROR;
    return AT_OK;
}

void removeCtrlChar(char *pDest, const char *pSrc)
{
    /* copy *pSrc to *pDest removing any possible ending control char */
    int cur = 0;
    strcpy(pDest, pSrc);
    cur = strlen(pDest);
    while (cur >= 0 && iscntrl(pDest[cur-1]))
    cur -=  1;
    pDest[cur] = '\0';
}

/*---------------------------------------------------------------------------*/
/* Read for Ack                                                              */
/*---------------------------------------------------------------------------*/

AT_STATUS readATline(int fd, char *pLine)
{
    static char buf[AT_MAX_RESP_LENGTH];
    static int iBufMax = -1;
    static int iBufCur;
    char *pStr = pLine;
    bool isLeadingCntrlChar = true;
    ssize_t count;
    int rts;
    fd_set fdSet;

    FD_ZERO(&fdSet);
    FD_SET(fd, &fdSet);

    for (;;) {
        if (iBufCur > iBufMax) {
        LOGV("... Waiting for data ...");
            do {
                do {
                    rts = select(fd+1, &fdSet, NULL, NULL, NULL);
                    if ((rts == -1) && (errno != EINTR) && (errno != EAGAIN)) {
                        LOGE("Read Error: %i / %s", errno, strerror(errno));
                        return AT_ERROR;
                    }
                    else
                        LOGV(" else select %d", rts);

            } while (rts == -1);
            count = read(fd, buf, AT_MAX_RESP_LENGTH);
            if ((count == -1) && (errno != EINTR) && (errno != EAGAIN)) {
                LOGE("Read Error: %i / %s", errno, strerror(errno));
                return AT_ERROR;
            }
                if(count == 0) {
                    LOGD(" read error %d", count);
                    return AT_ERROR;
                }
        } while (count < 1);
            iBufCur = 0;
            iBufMax = count-1;
            buf[iBufMax+1] = '\0';
            LOGV("... Data Received: |%s| ...", buf);

        }
        while (iBufCur <= iBufMax) {
            if (iscntrl(buf[iBufCur])) {
                if (!isLeadingCntrlChar) {
                    iBufCur = iBufCur+1;
                    *pStr = '\0';
                LOGD(" <== %s", pLine);
                return AT_OK;
                }
            } else {
                *pStr = buf[iBufCur];
                pStr++;
                isLeadingCntrlChar = false;
                }
            iBufCur++;
        }
        iBufMax = -1;
        iBufCur = 0;
    }
}

/*---------------------------------------------------------------------------*/
/* Write pATcmd string on gsmtty13                                           */
/*---------------------------------------------------------------------------*/

AT_STATUS writeATline(int fd, const char *pATcmd)
{
    int len, written, rts;
    int cur = 0;
    char buf[AT_MAX_CMD_LENGTH];
    fd_set fdSet;

    FD_ZERO(&fdSet);
    FD_SET(fd, &fdSet);
    len = strlen(pATcmd);
    if(len >= AT_MAX_CMD_LENGTH) {
        LOGW("AT cmd is too long");
        return AT_ERROR;
    }

    strncpy(buf,pATcmd,len);
    buf[len] = '\0';

    LOGV("*** Writing |%s| ...", pATcmd);
    while (len > 0 && iscntrl(buf[len-1]))
        len -=  1;
    if (len == 0)
        return AT_WRITE_ERROR;
    len +=  1;
    buf[len-1] = '\r';
    while (cur < len) {
        do {
            rts = select(fd+1, NULL, &fdSet, NULL, NULL);
            if ((rts == -1) && (errno != EINTR) && (errno != EAGAIN)) {
                LOGW("Write Error: %i / %s", errno, strerror(errno));
                return AT_WRITE_ERROR;
            }
        } while (rts == -1);
        written = write(fd, buf + cur , len - cur);
        if ((written == -1) && (errno != EINTR) && (errno != EAGAIN)) {
        LOGW("Unable to write to modem. Error: %i / %s",
        errno, strerror(errno));
        return AT_WRITE_ERROR;
        }
        cur += written;
    }
    LOGD(" ==> %s", pATcmd);

    return AT_RUNNING;
}

void propagateATerror(AT_STATUS status)
{
    pthread_mutex_lock(&at_dataMutex);
    LOGD("Propagate AT error");
    if (pCurrentRunningATcmd == NULL) {
        pthread_mutex_unlock(&at_dataMutex);
        return 0;
    }
    pCurrentRunningATcmd->cmdstatus = AT_ERROR;
    pthread_cond_broadcast(&newRespReceived);
    pthread_mutex_unlock(&at_dataMutex);
    LOGD("Propagate AT error end");
}

/*---------------------------------------------------------------------------*/
/* Send Blocking for ack                                                     */
/*---------------------------------------------------------------------------*/

AT_STATUS atSendBlocking(const char *pATcmd, const char *pRespPrefix,
           char *pATresp)
{
    AT_STATUS cmdStatus;
    cmdStatus = at_askUnBlocking(pATcmd, pRespPrefix, NULL);
    if (cmdStatus != AT_RUNNING)
        return cmdStatus;
    return at_waitForCmdCompletion();
}

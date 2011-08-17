/*
 **
 ** Copyright 2010 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **	 http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */



#include <sys/types.h>  /*Android needs this on top (size_t for string.h)*/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/ioctl.h>
#include <sys/time.h>
#include "ATmodemControl.h"
//#define LOG_NDEBUG 0

#ifdef AT_DBG_LOG_TIME
#include <sys/time.h>
#endif


/*==============================================================================
 * AT Modem Control Library
 *============================================================================*/

/* Asumption: the modem solicited response will be one of below:
 * a 2 line response:
 *- "prefix" of the command send
 *- "OK" or "ERROR"
 * a 3 line response:
 *- "echo" of the command send
 *- a response
 *- "OK" or "ERROR"*/



/*------------------------------------------------------------------------------
 * Log Settings:
 *----------------------------------------------------------------------------*/

#define LOG_TAG "ATmodemControl"
#include <utils/Log.h>

/*------------------------------------------------------------------------------
 * Internal Variables:
 *----------------------------------------------------------------------------*/

/*Internal list to keep track of the running AT commands:*/

typedef struct RunningATcmd {
    char prefix[AT_MAX_CMD_LENGTH]; /* used to identify the related write done*/
    char *pResp;/* used for user response feed-back*/
    AT_STATUS *pStatus; /* used for user status feed-back*/
    struct RunningATcmd *pNext;
#ifdef AT_DBG_LOG_TIME/* for delay measurementbetween write and acked*/
    long sentTime;
#endif
} RunningATcmd;

static RunningATcmd *pFirstRunningATcmd;  /*= NULL Linked list Starting*/

static pthread_mutex_t at_dataMutex;  	/*to access RunningATcmd list values*/


static pthread_cond_t newRespReceived;  /* broadcasted by the reader thread
            * at each new response received*/

#define MAX_RETRY 5 /* Guardrail in case of mis-use of the
* at_waitForCmdCompletion() function
* ie bad parameter passed to the function
* inducing a never-ending wait*/

/*Unsolicited response*/
static char *pUnsolicitedResp;  /*= NULL for user response feed-back*/
pthread_mutex_t at_unsolicitedRespMutex; /* Associated Mutex */


/*Other*/
static bool isInitialized;  /*= false keep track of library initialization*/
static pthread_t  threadId;

static int fdIn;  /* AT Channel File Descriptor: in/to modem*/
static int fdOut; /* AT Channel File Descriptor: out/from modem*/

#ifdef ANDROID
static bool isRequestToStop;  /*= false for pthread_cancel() hack*/
#endif

/*------------------------------------------------------------------------------
 * Internal helpers
 *----------------------------------------------------------------------------*/

static void removeCtrlChar(char *pDest, const char *pSrc);
static AT_STATUS readATline(int fd,  char *pStr);
static AT_STATUS writeATline(int fd, const char *pATcmd);
static void propagateATerror(AT_STATUS status);

#ifdef AT_DBG_LOG_TIME
static long getTime(void); /*Return the time in millisecond*/
#define LOGT(format, args...)  LOGD(format " [ %06lu ms ] ", ##args, getTime())
#else
#define LOGT(...)((void)0)
#endif

/*------------------------------------------------------------------------------
 * Internal Reader Thread Implementation
 *----------------------------------------------------------------------------*/

static void *atReaderThread(void *arg)
{
/* The AT reader thread  keeps on looping on below steps:
*  - 1-  read a line -
 *  - 2-  check in the runningATcmd list if it is an  unsolicited response
*yes ? -> go back to 1/
*  -3-  read one (two) other line -> it is a cmd (response and) status
*  -4- update the responses pointers accordingly
*  -5- remove one cell of the runningATcmd list
* Note:
*- a read error during prefix search is propagated on every runing cmd
*- a solicited response is broadcasted through a pthread_cond
*that could be caugth by the at_waitForCmdCompletion() function
*who implements a passive  wait*/

    char prefix[AT_MAX_CMD_LENGTH];
    char resp[AT_MAX_RESP_LENGTH];
    char strStatus[AT_MAX_RESP_LENGTH];
    int *pIntDummy;
    AT_STATUS status;
    AT_STATUS readStatus;
    RunningATcmd *pRunningATcmdCur, *pPreviousATcmd;

    LOGD("*** AT Reader thread started");

    for (;;) {  /* never ending reading loop*/
        for (;;) {/* wait loop for a solicited response.*/
        /* Get an AT response prefix*/
        LOGV("... Waiting for new prefix to read ...");
        do {
        readStatus = readATline(fdOut, prefix);
        if (readStatus != AT_OK)
            propagateATerror(readStatus);  /* to all running AT cmd*/
                if (readStatus == AT_ERROR)
                    return 0;
        } while (readStatus != AT_OK);
        /* Search for the related issued command:*/
        pthread_mutex_lock(&at_dataMutex);
        pPreviousATcmd = pRunningATcmdCur = pFirstRunningATcmd;
        while (pRunningATcmdCur != NULL) {
        if (strncmp(prefix, pRunningATcmdCur->prefix,
            strlen(pRunningATcmdCur->prefix)) == 0)
            break;
            pPreviousATcmd = pRunningATcmdCur;
            pRunningATcmdCur = pRunningATcmdCur->pNext;
        }
        pthread_mutex_unlock(&at_dataMutex);
        /*Solicited or unsolicited ?*/
        if (pRunningATcmdCur == NULL) { /* unsolicited response*/
            LOGD(" <=> %s(Unsol Resp)", prefix);
            if (pUnsolicitedResp != NULL) {
            pthread_mutex_lock(&at_unsolicitedRespMutex);
            strcpy(pUnsolicitedResp, prefix);
            pthread_mutex_unlock(&at_unsolicitedRespMutex);
            }
        } else
        break;
    } /* end of wait loop for a solicited response.*/

    LOGV("... Reading response and/or status ...");
    status = readATline(fdOut, resp);
    if (status != AT_OK)
    resp[0] = '\0';
        if (readStatus == AT_ERROR)
            return 0;
    else {
        if ((strcmp(resp, "OK") == 0) || (strcmp(resp, "ERROR") == 0)) {
            status = (strcmp(resp, "OK") == 0) ?  AT_OK : AT_ERROR;
            resp[0] = '\0';
        } else {/* cmd with response and status*/
            LOGV("... Reading status ...");
            status = readATline(fdOut, strStatus);
        if (status == AT_OK)
            status = (strcmp(strStatus, "OK") == 0) ?  AT_OK : AT_ERROR;
            if (readStatus == AT_ERROR)
                    return 0;
        }
    }
#ifdef AT_DBG_LOG_TIME
    LOGT(" <=> |%s|: Resp = |%s|, Status = |%i| in %lu ms",
    prefix, resp, status, (getTime()-pRunningATcmdCur->sentTime));
#else
    LOGD(" <=> |%s|: Resp = |%s|, Status = |%i|.", prefix, resp, status);
#endif

/* Fill in return buffers:
 * ( pPreviousATcmd and pRunningATcmdCur already set previously)*/
    pthread_mutex_lock(&at_dataMutex);
    if (pRunningATcmdCur->pResp != NULL)
        strcpy(pRunningATcmdCur->pResp, resp);
    if (pRunningATcmdCur->pStatus != NULL)
        *(pRunningATcmdCur->pStatus) = status;
    /*Not any more a running command:*/
    if (pFirstRunningATcmd == pRunningATcmdCur)
        pFirstRunningATcmd = pFirstRunningATcmd->pNext;
    else
        pPreviousATcmd->pNext = pRunningATcmdCur->pNext;
    free(pRunningATcmdCur);
    pthread_cond_broadcast(&newRespReceived);
    pthread_mutex_unlock(&at_dataMutex);
    } /* end of never ending reading loop*/
    return(0);
}

/*------------------------------------------------------------------------------
 * External API Implementation:
 *----------------------------------------------------------------------------*/

AT_STATUS at_start(const char *pATchannel, char* pUnsolicitedATresp)
{
/* Start the AT Modem Control library.
 * Params:
 * *pATchannel: full path to the char device driver
 * pUnsolicitedATresp[AMC_MAX_RESP_LENGTH] : to hold unsolicited.
 * may be NULL (response discarded).
 * Need to be accessed using 'at_unsolicitedRespMutex'.
 * Steps:
 *- open handles to the char device driver
 *- start the AT reader thread
 *_ initialize all the mutex stuff*/
    if (isInitialized) {
        LOGI("ATmodemControl already started");
        return AT_OK;
    }
    LOGV("Starting ATmodemControl ...");
    pUnsolicitedResp = pUnsolicitedATresp;
#ifdef ANDROID
    isRequestToStop = false;
#endif
/* Open handle to modem device.*/
#ifndef AT_DBG_FILE
    fdIn = open(pATchannel, O_WRONLY|O_NONBLOCK);
    if (fdIn < 0) {
        LOGW("Unable to open device for writing: %s, error: %s",
        pATchannel, strerror(errno));
        return AT_UNABLE_TO_OPEN_DEVICE;
    }
    fdOut = open(pATchannel, O_RDONLY|O_NONBLOCK);
    if (fdOut < 0) {
        LOGW("Unable to open device for reading: %s, error: %s",
        pATchannel, strerror(errno));
        return AT_UNABLE_TO_OPEN_DEVICE;
    }
#else
    fdIn = open(AT_DBG_FIN, O_WRONLY|O_NONBLOCK);
    if (fdIn < 0) {
        LOGW("Unable to open device: %s, error: %s",
        AT_DBG_FIN, strerror(errno));
    return AT_UNABLE_TO_OPEN_DEVICE;
    }
    fdOut = open(AT_DBG_FOUT, O_RDONLY|O_NONBLOCK);
    if (fdOut < 0) {
        LOGW("Unable to open device: %s, error: %s", AT_DBG_FOUT,
        strerror(errno));
    return AT_UNABLE_TO_OPEN_DEVICE;
    }
#endif
    pthread_mutex_init(&at_dataMutex, NULL);
    pthread_mutex_init(&at_unsolicitedRespMutex, NULL);
    pthread_cond_init(&newRespReceived, NULL);
    /* start AT reader thread*/
    LOGV("	Starting AT Reader thread ...");
    if (pthread_create(&threadId, NULL, atReaderThread, NULL) != 0) {
        LOGW("Unable to start thread: error: %s", strerror(errno));
        return AT_UNABLE_TO_CREATE_THREAD;
    }
    isInitialized = true;
    LOGD("*** ATmodemControl started");
    return AT_OK;
}


AT_STATUS at_stop()
{
/* Stop the Audio Modem Control library:
 *- Stop the AT reader thread
 *  - Close handles and free used ressource*/
    int i;
    RunningATcmd *pCur;
/* Stop Reader Thread*/
    LOGD("Stopping ATmodemControl ...");
    LOGV("Requesting to end Reader thread ...");
    close(fdIn);
    close(fdOut);
    isInitialized = false;
    LOGD("*** ATmodemControl stopped.");
    return AT_OK;
}

AT_STATUS at_askUnBlocking(const char *pATcmd, const char *pRespPrefix,
           char *pATresp, AT_STATUS *pCmdStatus)
{
/* Main communication function
 * Params:
 *  *pATcmd[AT_MAX_CMD_LENGTH] : without ending control char (will be trim)
 *  *pRespPrefix[AT_MAX_CMD_LENGTH] : to to find back the proper response
 *  *pATresp[AT_MAX_RESP_LENGTH] will hold the response when available.
 * may be NULL (response discarded)
 *  *pCmdStatus char[AT_MAX_CMD_LENGTH] will hold the status when available.
 * may be NULL (status discarded)
 *  return value: AMC_RUNNING if the response is properly sent
 * Steps:
 *  send *pATcmd to the modem, checking for proper control character ending.
 *  add to the runningATcmd list this now running ATcmd characteristics:
 *  - prefix to wait for
 *  - pointers to the response and status variables that will hold the
 *  read response values.*/

    assert(pATcmd != NULL);
    assert(pRespPrefix != NULL);
    RunningATcmd *pNewCmd;
    AT_STATUS cmdStatus;
    /* Allow default initialization:*/
    if (!isInitialized) {
        *pCmdStatus = AT_UNINITIALIZED;
        LOGW("AT Modem Control not initialized.");
        return AT_UNINITIALIZED;
    }
/* Log new cmd for reader thread: add it to the running list:*/
    pNewCmd = (RunningATcmd  *) malloc(sizeof(RunningATcmd));
    if(!pNewCmd) {
        LOGW("malloc pNewCmd error");
        return AT_UNINITIALIZED;
    }
    removeCtrlChar(pNewCmd->prefix, pRespPrefix);
    pNewCmd->pResp = pATresp;
    pNewCmd->pStatus = pCmdStatus;
#ifdef AT_DBG_LOG_TIME
    pNewCmd->sentTime = getTime();
#endif
    pthread_mutex_lock(&at_dataMutex);
    pNewCmd->pNext = pFirstRunningATcmd;
    pFirstRunningATcmd = pNewCmd;
    /* Send Commend*/
    cmdStatus = writeATline(fdIn, pATcmd);
    if (cmdStatus != AT_RUNNING)
        LOGW("... %s not written.", pATcmd);
    if (pNewCmd->pStatus != NULL)
        *(pNewCmd->pStatus) = cmdStatus;
        pthread_mutex_unlock(&at_dataMutex);
        return cmdStatus;
}

AT_STATUS at_ask(const char *pATcmd, const char *pRespPrefix, char *pATresp)
{
/* Blocking ask: wait for the modem response and cmd status.*/
assert(pATcmd != NULL);
AT_STATUS cmdStatus;
at_askUnBlocking(pATcmd, pRespPrefix, pATresp, &cmdStatus);
at_waitForCmdCompletion(&cmdStatus);
return cmdStatus;
}

AT_STATUS at_sendUnBlocking(const char *pATcmd, const char *pRespPrefix,
            AT_STATUS *pCmdStatus)
{
/* Helper: No response needed*/
assert(pATcmd != NULL);
return at_askUnBlocking(pATcmd, pRespPrefix, NULL, pCmdStatus);
}

AT_STATUS at_send(const char *pATcmd, const char *pRespPrefix)
{
/* Blocking Send: wait for the modem cmd status.*/
    assert(pATcmd != NULL);
    AT_STATUS cmdStatus;
    at_askUnBlocking(pATcmd, pRespPrefix, NULL, &cmdStatus);
    at_waitForCmdCompletion(&cmdStatus);
    return cmdStatus;
}

bool at_isCmdCompleted(AT_STATUS *pCmdStatus)
{
/* User helper to hide the mutex mechanism.
 * Will be used after unblocking functions to check the command
 * response availability*/
    bool rts;
    pthread_mutex_lock(&at_dataMutex);
    rts = (*pCmdStatus != AT_RUNNING);
    pthread_mutex_unlock(&at_dataMutex);
    return rts;
}

void at_waitForCmdCompletion(AT_STATUS *pCmdStatus)
{
/* Will be used after unblocking functions to passively wait up to the
 * response availability.
 * newRespReceived is triggered by the reader loop at each new solicited
 * response received.
 * MAX_RETRY is used as a user guardrail in case pCmdStatus is unrelated
 * to any AT command issued.*/
    assert(pCmdStatus != NULL);
    int try = 0;
    pthread_mutex_lock(&at_dataMutex);
    while ((*pCmdStatus == AT_RUNNING) && (try < MAX_RETRY)) {
        pthread_cond_wait(&newRespReceived, &at_dataMutex);
        try +=  1;
        LOGV("wait completion retry");
    }
    if(try == MAX_RETRY)
    {
    pthread_mutex_unlock(&at_dataMutex);
    return 0;}
    LOGV("WAIT COMPLETION SANS WHILE");
    pthread_mutex_unlock(&at_dataMutex);
}

/*------------------------------------------------------------------------------
 * Internal functions / helpers implementation:
 *----------------------------------------------------------------------------*/

void removeCtrlChar(char *pDest, const char *pSrc)
{
/* copy *pSrc to *pDest removing any possible ending control char*/
    int cur = 0;
    strcpy(pDest, pSrc);
    cur = strlen(pDest);
    while (cur >= 0 && iscntrl(pDest[cur-1]))
    cur -=  1;
    pDest[cur] = '\0';
}


AT_STATUS readATline(int fd, char *pLine)
{
/* Read a single AT line and remove any control char
 *  use of 'select' for passive wait. (cancelable function)
 *  use of an internal buffer to save reads that cross a line*/
    static char buf[AT_MAX_RESP_LENGTH]; /*keep partial reads
 * in between two function call*/
    static int iBufMax = -1; /*empty buffer*/
    static int iBufCur;  /*= 0 */
    char *pStr = pLine;
    bool isLeadingCntrlChar = true;
    ssize_t count;
    int rts;
    char *p_read = NULL;
    char *pEndOffLine = NULL;
    fd_set fdSet;/* file descriptor set. Used by select.*/

    /*Initialize the file descriptor set for select*/
    FD_ZERO(&fdSet);
    FD_SET(fd, &fdSet);
    /* Never ending reading loop. Cancelable point is the 'select' function.*/
    for (;;) {
        /*Fill in buffer if needed:*/
        if (iBufCur > iBufMax) {
        LOGV("... Waiting for data ...");
            do { /* Wait for no EINTR or EAGAIN on read*/
                do { /* Wait for no EINTR or EAGAIN on select*/
                    rts = select(fd+1, &fdSet, NULL, NULL, NULL);
                    if ((rts == -1) && (errno != EINTR) && (errno != EAGAIN)) {
                        LOGW("Read Error: %i / %s", errno, strerror(errno));
                        return AT_READ_ERROR;
                    }
                    else
                        LOGV(" ELSE SELECT %d", rts);

            } while (rts == -1);
            count = read(fd, buf, AT_MAX_RESP_LENGTH);
            if ((count == -1) && (errno != EINTR) && (errno != EAGAIN)) {
                LOGW("	Read Error: %i / %s", errno, strerror(errno));
                return AT_READ_ERROR;
            }
                if(count == 0){
                    LOGV(" ELSE READ %d", count);
                    return AT_ERROR;}
        } while (count < 1);
            iBufCur = 0;
            iBufMax = count-1;
            buf[iBufMax+1] = '\0';
            LOGV("... Data Received: |%s| ...", buf);

        }
    /*Consume buffer:*/
    while (iBufCur <= iBufMax) {
        if (iscntrl(buf[iBufCur])) {
        if (!isLeadingCntrlChar) { /*end of line reached*/
            iBufCur = iBufCur+1;
            *pStr = '\0';
#ifdef AT_DBG_LOG_TIME
        LOGT(" <== %s", pLine);
#else
        LOGD(" <== %s", pLine);
#endif
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

AT_STATUS writeATline(int fd, const char *pATcmd)
{
/* Write a single AT line.
 * Use of 'select' for passive wait. (cancelable function)
 * Remove any control char, send the expected \r at the end.*/
    int len, written, rts;
    int cur = 0;
    char buf[AT_MAX_CMD_LENGTH];
    fd_set fdSet;  /* file descriptor set. Used by select.*/

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
    /* Remove any unwanted ending control char and append \r*/
    while (len > 0 && iscntrl(buf[len-1]))
        len -=  1;
    if (len == 0)
        return AT_ERROR;
    len +=  1;
    buf[len-1] = '\r';
/* Send AT command:*/
    while (cur < len) { /*consume any character to write*/
        do { /* Wait for no EINTR or EAGAIN on select*/
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
#ifdef AT_DBG_LOG_TIME
    LOGT(" ==> %s", pATcmd);
#else
    LOGD(" ==> %s", pATcmd);
#endif

    return AT_RUNNING;
}

void propagateATerror(AT_STATUS status)
{
/* Called when read error is occuring during prefix search:
 * Propagate the read error on every running AT commands status
 * ( to avoid the user waiting for nothing )
 * and free internal linked-list.*/
    pthread_mutex_lock(&at_dataMutex);
    RunningATcmd *pRunningATcmdCur, *pPreviousATcmd;
    pRunningATcmdCur = pFirstRunningATcmd;
    while (pRunningATcmdCur != NULL) {
        *(pRunningATcmdCur->pStatus) = status;
        *(pRunningATcmdCur->pResp) = '\0';
        pPreviousATcmd = pRunningATcmdCur;
        pRunningATcmdCur = pRunningATcmdCur->pNext;
        free(pPreviousATcmd);
    }
    pFirstRunningATcmd = NULL;
    pthread_cond_broadcast(&newRespReceived);
    pthread_mutex_unlock(&at_dataMutex);
}


#ifdef AT_DBG_LOG_TIME
long getTime()
{
/*Return the time in millisecond. First call will reset the clock to 0.*/
    struct timeval current;
    static struct timeval init = {0, 0};
    long seconds, useconds;
    if ((init.tv_sec == 0) && (init.tv_usec == 0))
        gettimeofday(&init, NULL);
        gettimeofday(&current, NULL);
    seconds  =   current.tv_sec-init.tv_sec;
    useconds = current.tv_usec-init.tv_usec;
    return seconds * 1000 + (useconds+500) / 1000;
}
#endif


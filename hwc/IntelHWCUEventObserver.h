/*
 * IntelHWCUEventObserver.h
 *
 *  Created on: Jan 16, 2012
 *      Author: root
 */

#include <pthread.h>

#ifndef __INTEL_HWC_UEVENT_OBSERVER_H__
#define __INTEL_HWC_UEVENT_OBSERVER_H__

enum {
    UEVENT_MSG_LEN = 4096,
};

class IntelHWCUEventObserver {
private:
    pthread_t mThread;
    bool mReadyToRun;
private:
    static void *threadLoop(void *data);
    static void ueventHandler(void *data, const char *msg, int msgLen);
protected:
    virtual void onUEvent(const char *msg, int msgLen);
public:
    IntelHWCUEventObserver();
    virtual ~IntelHWCUEventObserver();
    bool isReadyToRun() { return mReadyToRun; }
    bool startObserver();
    bool stopObserver();
};

#endif /* __INTEL_HWC_UEVENT_OBSERVER_H__ */

/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __INTEL_EXTERNAL_DISPLAY_MONITOR_H__
#define __INTEL_EXTERNAL_DISPLAY_MONITOR_H__

#include <utils/threads.h>

#include "display/IExtendDisplayModeChangeListener.h"
#include "display/IMultiDisplayComposer.h"
#include "display/MultiDisplayClient.h"
#include "display/MultiDisplayType.h"

class IntelHWComposer;

class IntelExternalDisplayMonitor :
    public android::BnExtendDisplayModeChangeListener,
    public android::IBinder::DeathRecipient,
    protected android::Thread
{
public:
    enum {
        MSG_TYPE_UEVENT = 0,
        MSG_TYPE_MDS,
    };

    enum {
        UEVENT_MSG_LEN = 4096,
    };

    enum {
        INVALID_MDS_MODE = 0,
    };
public:
    IntelExternalDisplayMonitor(IntelHWComposer *hwc);
    virtual ~IntelExternalDisplayMonitor();
    void initialize();
public:
    // onModeChange() interface
    void onModeChange(int mode);
public:
    int getDisplayMode();
    bool isVideoPlaying();
    bool isOverlayOff();
    bool notifyWidi(bool);
    bool notifyMipi(bool);
    bool getVideoInfo(int *displayW, int *displayH, int *fps, int *isinterlace);
private:
    //DeathReipient interface
    virtual void binderDied(const android::wp<android::IBinder>& who);
private:
    virtual bool threadLoop();
    virtual android::status_t readyToRun();
    virtual void onFirstRef();
private:
    MultiDisplayClient* mMDClient;
    android::Mutex mLock;
    android::Condition mModeChanged;
    int mActiveDisplayMode;
    bool mWidiOn;
    bool mMipiOn;
    bool mInitialized;
    IntelHWComposer *mComposer;
    char mUeventMessage[UEVENT_MSG_LEN];
    int mUeventFd;
}; // IntelExternalDisplayMonitor

#endif // __INTEL_EXTERNAL_DISPLAY_MONITOR_H__


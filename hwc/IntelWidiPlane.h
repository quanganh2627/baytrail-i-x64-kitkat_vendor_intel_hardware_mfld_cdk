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
#ifndef __INTEL_WIDI_PLANE_H__
#define __INTEL_WIDI_PLANE_H__

#include <IntelDisplayPlaneManager.h>
#include <utils/threads.h>
#include <binder/IInterface.h>
#include <binder/IServiceManager.h>
#include "IHwWidiPlane.h"


class IntelWidiPlane : public IntelDisplayPlane , public intel::widi::BnHwWidiPlane {

public:
    IntelWidiPlane(int fd, int index, IntelBufferManager *bufferManager);
    ~IntelWidiPlane();
    virtual void setPosition(int left, int top, int right, int bottom);

    android::status_t  enablePlane(android::sp<android::IBinder> display);
    void  disablePlane();
    android::status_t  registerFlipListener(android::sp<IPageFlipListener> listener);
    void allowExtVideoMode(bool allow);
    bool isExtVideoAllowed() {return mAllowExtVideoMode;};

    bool flip(uint32_t flags);
    bool isActive();

protected:
    void init();

    class WidiInitThread: public android::Thread {
    public:
        WidiInitThread(IntelWidiPlane* me): android::Thread(false), mSelf(me) {LOGV("Widi Plane Init thread created");};
        ~WidiInitThread(){LOGV("Widi Plane Init thread destroyed");};

    private:
        bool  threadLoop();
    private:
        android::sp<IntelWidiPlane> mSelf;
    };
    typedef enum {
        WIDI_PLANE_STATE_UNINIT,
        WIDI_PLANE_STATE_INITIALIZED,
        WIDI_PLANE_STATE_ACTIVE,
        WIDI_PLANE_STATE_STREAMING
    }WidiPlaneState;

    class DeathNotifier: public android::IBinder::DeathRecipient
    {
    public:
                DeathNotifier(IntelWidiPlane* me): mSelf(me) {}
        virtual ~DeathNotifier();

        virtual void binderDied(const android::wp<android::IBinder>& who);
    private:
        android::sp<IntelWidiPlane> mSelf;
    };

    int32_t                         mAllowExtVideoMode;
    WidiPlaneState                  mState;
    android::Mutex                  mLock;
    android::sp<WidiInitThread>     mInitThread;
    android::sp<IPageFlipListener>  mFlipListener;
    android::sp<DeathNotifier>      mDeathNotifier;
    android::sp<IBinder>            mWirelesDisplayservice;
};


#endif /*__INTEL_OVERLAY_PLANE_H__*/

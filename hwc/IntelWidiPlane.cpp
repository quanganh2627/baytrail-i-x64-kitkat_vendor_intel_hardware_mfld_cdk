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

#include <utils/StopWatch.h>
#include <IntelWidiPlane.h>
#include <IntelOverlayUtil.h>

#include "streaming/IWirelessDisplayService.h"
#include "streaming/IWirelessDisplay.h"



using namespace android;
using namespace intel::widi;

bool
IntelWidiPlane::WidiInitThread::threadLoop() {

    sp<IBinder> service;
    sp<IServiceManager> sm = defaultServiceManager();
    LOGV("WidiPlane Init thread starting ");
    do {
        service = (sm->getService(String16("media.widi")));

        if (service != 0) {
            break;
         }
         LOGW("Wireless display service not published, waiting...");
         usleep(500000); // 0.5 s
    } while(true);

    LOGV("Widi service found = %p", service.get());

    mSelf->mWirelesDisplayservice = service;

    sp<IWirelessDisplayService> widiService = interface_cast<IWirelessDisplayService>(service);

    status_t s = widiService->registerHWPlane(mSelf);
    LOGV("Widi plane registered status = %d", s);

    service->linkToDeath(mSelf->mDeathNotifier);

    {
        Mutex::Autolock _l(mSelf->mLock);
        mSelf->mState = WIDI_PLANE_STATE_INITIALIZED;
        mSelf->mInitialized = true;
    }

    LOGV("WidiPlane Init thread completed ");
    return false;
}

IntelWidiPlane::IntelWidiPlane(int fd, int index, IntelBufferManager *bm)
    : IntelDisplayPlane(fd, IntelDisplayPlane::DISPLAY_PLANE_OVERLAY, index, bm),
      mState(WIDI_PLANE_STATE_UNINIT),
      mLock(NULL),
      mInitThread(NULL),
      mFlipListener(NULL)

{
    LOGV("Intel Widi Plane constructor");

    /* defer initialization of widi plane to another thread
     * we do this because the initialization may take long time and we do not
     * want to hold out the HWC initialization.
     * The initialization involves registering the plane to the Widi media server
     * over binder
     */

    mDeathNotifier = new DeathNotifier(this);

    init(); // defer the rest to the thread;

    LOGV("Intel Widi Plane constructor- DONE");
    return;

}

IntelWidiPlane::~IntelWidiPlane()
{
    LOGV("Intel Widi Plane destructor");
    if (initCheck()) {
        mInitialized = false;
        if(mInitThread == NULL) {
                mInitThread->requestExitAndWait();
                mInitThread.clear();
         }
    }
}

void
IntelWidiPlane::setPosition(int left, int top, int right, int bottom)
{
    if (initCheck()) {
    }
}


status_t
IntelWidiPlane::enablePlane(sp<IBinder> display) {
    Mutex::Autolock _l(mLock);

    if(mState == WIDI_PLANE_STATE_INITIALIZED) {
        LOGV("Plane Enabled !!");
        mState =WIDI_PLANE_STATE_ACTIVE;
    }

    return 0;
}

void
IntelWidiPlane::disablePlane() {

    Mutex::Autolock _l(mLock);
    if(mState == WIDI_PLANE_STATE_ACTIVE) {
        LOGV("Plane Disabled !!");
        mState = WIDI_PLANE_STATE_INITIALIZED;
    }
    return;
}

status_t
IntelWidiPlane::registerFlipListener(sp<IPageFlipListener> listener) {

    mFlipListener = listener;
    return NO_ERROR;
}

bool
IntelWidiPlane::flip(uint32_t flags) {

    LOGV("Widi Plane flip, flip listener = %p", mFlipListener.get());
    if (mFlipListener != NULL)
        mFlipListener->pageFlipped(systemTime(),0);

    return true;
}

bool
IntelWidiPlane::isActive() {

    Mutex::Autolock _l(mLock);

    return (mState==WIDI_PLANE_STATE_ACTIVE)?true:false;
}

void
IntelWidiPlane::init() {

    if(mInitThread != NULL) {
        mInitThread->requestExitAndWait();
        mInitThread.clear();
    }

    {
        Mutex::Autolock _l(mLock);
        mInitialized = false;
        mState = WIDI_PLANE_STATE_UNINIT;
    }

    mInitThread = new WidiInitThread(this);
    mInitThread->run();
}

void
IntelWidiPlane::DeathNotifier::binderDied(const wp<IBinder>& who) {
    LOGW("widi server died - trying to register again");

    mSelf->disablePlane();

    mSelf->init();

}

IntelWidiPlane::DeathNotifier::~DeathNotifier() {
    if (mSelf->mWirelesDisplayservice != 0) {
        mSelf->mWirelesDisplayservice->unlinkToDeath(this);
    }
}

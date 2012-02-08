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


static const int EXT_VIDEO_MODE_MAX_SURFACE = 32;
static const int EXT_VIDEO_START_DELAY = 20;

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
      mAllowExtVideoMode(true),
      mState(WIDI_PLANE_STATE_UNINIT),
      mLock(NULL),
      mInitThread(NULL),
      mFlipListener(NULL),
      mWirelessDisplay(NULL),
      mCurrentOrientation(0),
      mExtVideoStartDelay(EXT_VIDEO_START_DELAY)

{
    LOGV("Intel Widi Plane constructor");

    mExtVideoBuffers.setCapacity(EXT_VIDEO_MODE_MAX_SURFACE);

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
        mWirelessDisplay = display;
        mState = WIDI_PLANE_STATE_ACTIVE;
    }

    return 0;
}

void
IntelWidiPlane::disablePlane() {

    Mutex::Autolock _l(mLock);
    if(mState == WIDI_PLANE_STATE_ACTIVE) {
        LOGV("Plane Disabled !!");
        mWirelessDisplay = NULL;
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
    if (mFlipListener != NULL && mState == WIDI_PLANE_STATE_ACTIVE)
        mFlipListener->pageFlipped(systemTime(),mCurrentOrientation);

    return true;
}

void
IntelWidiPlane::allowExtVideoMode(bool allow) {

    LOGV("Allow Ext video mode = %d", allow);
    mAllowExtVideoMode = allow;
}

void
IntelWidiPlane::setOrientation(uint32_t orientation) {
    mCurrentOrientation = orientation;
}

void
IntelWidiPlane::setOverlayData(intel_gralloc_buffer_handle_t* nHandle, uint32_t width, uint32_t height) {

    status_t ret = NO_ERROR;
    sp<IWirelessDisplay> wd = interface_cast<IWirelessDisplay>(mWirelessDisplay);

    if(mState == WIDI_PLANE_STATE_ACTIVE) {

    	if (mExtVideoStartDelay) {
    	            mExtVideoStartDelay--;
    	            LOGI("delaying mode change");
    	            return;
    	}
        intel_gralloc_buffer_handle_t *p = mExtVideoBuffers.valueFor(nHandle);

        if( p == nHandle) {
            LOGI("We have all the  buffers (%d), lets move to streaming state and change the mode", mExtVideoBuffers.size());

            if(mCurrentOrientation){    // Adjust for orientations different than 0 (i.e. 90 and 270)
                uint32_t tmp = width;
                width = height;
                height = tmp;
            }

            ret = sendInitMode(IWirelessDisplay::WIDI_MODE_EXTENDED_VIDEO, width, height);
            if(ret == NO_ERROR) {
                mState = WIDI_PLANE_STATE_STREAMING;
                mExtVideoStartDelay = EXT_VIDEO_START_DELAY;

            }else {
                LOGE("Something went wrong setting the mode, we continue in clone mode");
                mExtVideoBuffers.clear();
            }

        } else {

            mExtVideoBuffers.add(nHandle, nHandle);
            LOGI("Adding gralloc handle %p, array size = %d", nHandle, mExtVideoBuffers.size());

        }
    } else if(mState == WIDI_PLANE_STATE_STREAMING) {

        ssize_t index = mExtVideoBuffers.indexOfKey(nHandle);

        if (index == NAME_NOT_FOUND) {
            LOGW("Unexpected buffer received, going back to clone mode");
            mState = WIDI_PLANE_STATE_ACTIVE;
            sendInitMode(IWirelessDisplay::WIDI_MODE_CLONE,0,0);
            mExtVideoBuffers.clear();

        } else {
            LOGV("Sending buffer from index %d", index);
            wd->sendBuffer(index);
        }


    }

}

bool
IntelWidiPlane::isActive() {

    Mutex::Autolock _l(mLock);
    if ( (mState == WIDI_PLANE_STATE_ACTIVE) ||
         (mState == WIDI_PLANE_STATE_STREAMING) )
        return true;

    return false;
}

/* overlayInUse
 *
 * This method is used to inform the wireless display plane if any
 * of the layers in the list contained data targeted to the overlay
 * This how the Wireless Plane notices that a video clip has finished
*
 * This method is used by the HWC updateLayersData method.
 *
 * */

void
IntelWidiPlane::overlayInUse(bool used) {

    Mutex::Autolock _l(mLock);

    if ( (mState == WIDI_PLANE_STATE_STREAMING) && !used) {
        LOGV("Let's go back to clone mode");
        mState = WIDI_PLANE_STATE_ACTIVE;
        mExtVideoBuffers.clear();
        sp<IWirelessDisplay> wd = interface_cast<IWirelessDisplay>(mWirelessDisplay);
        wd->initMode(NULL, 0, IWirelessDisplay::WIDI_MODE_CLONE,0, 0);
    }
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

status_t
IntelWidiPlane::sendInitMode(int mode, uint32_t width, uint32_t height) {

    status_t ret = NO_ERROR;
    sp<IWirelessDisplay> wd = interface_cast<IWirelessDisplay>(mWirelessDisplay);

    if(mode == IWirelessDisplay::WIDI_MODE_CLONE) {

        ret = wd->initMode(NULL, 0, IWirelessDisplay::WIDI_MODE_CLONE, 0, 0);

    }else if(mode == IWirelessDisplay::WIDI_MODE_EXTENDED_VIDEO) {

        intel_gralloc_buffer_handle_t *bufs;
        int bufferCount = mExtVideoBuffers.size();
        bufs = (intel_gralloc_buffer_handle_t *) malloc(bufferCount * sizeof(intel_gralloc_buffer_handle_t));
        for(int i= 0; i < bufferCount; i++) {
            bufs[i] = *mExtVideoBuffers.valueAt(i);
        }

        ret = wd->initMode(bufs, bufferCount, IWirelessDisplay::WIDI_MODE_EXTENDED_VIDEO, width, height);

        delete bufs;
    }

    return ret;

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

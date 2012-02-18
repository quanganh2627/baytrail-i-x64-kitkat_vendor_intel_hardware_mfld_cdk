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
      mAllowExtVideoMode(true),
      mState(WIDI_PLANE_STATE_UNINIT),
      mWidiStatusChanged(false),
      mLock(NULL),
      mInitThread(NULL),
      mFlipListener(NULL),
      mWirelessDisplay(NULL),
      mCurrentOrientation(0),
      mExtVideoBuffersCount(0)

{
    LOGV("Intel Widi Plane constructor");

    mExtVideoBuffersMapping.setCapacity(EXT_VIDEO_MODE_MAX_SURFACE);
    mExtVideoBuffersMapping.clear();

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
        mWidiStatusChanged = true;
    }

    return 0;
}

void
IntelWidiPlane::disablePlane() {

    Mutex::Autolock _l(mLock);
    if ((mState == WIDI_PLANE_STATE_ACTIVE) || (mState == WIDI_PLANE_STATE_STREAMING)) {
        LOGV("Plane Disabled !!");
        mWirelessDisplay = NULL;
        mState = WIDI_PLANE_STATE_INITIALIZED;
        memset(mExtVideoBuffers, 0, sizeof(intel_gralloc_buffer_handle_t)*EXT_VIDEO_MODE_MAX_SURFACE);
        mExtVideoBuffersCount = 0;
        mExtVideoBuffersMapping.clear();
        mWidiStatusChanged = true;

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
IntelWidiPlane::setPlayerStatus(bool status) {

    LOGV("%s(), status = %d", __func__, status);
    Mutex::Autolock _l(mLock);

    if ( (mState == WIDI_PLANE_STATE_STREAMING) && status == 0) {
        sendInitMode(IWirelessDisplay::WIDI_MODE_CLONE,0,0);

    }
}

void
IntelWidiPlane::setOrientation(uint32_t orientation) {
    mCurrentOrientation = orientation;
}

intel_gralloc_payload_t *
IntelWidiPlane::mapPayloadBuffer(intel_gralloc_buffer_handle_t* gHandle) {

    mCurrentDisplayBuffer =  mBufferManager->map(gHandle->fd[GRALLOC_SUB_BUFFER1]);
    if (!mCurrentDisplayBuffer) {
       LOGE("%s: failed to map payload buffer.\n", __func__);
       return NULL;
    }

    intel_gralloc_payload_t *payload =
       (intel_gralloc_payload_t*)mCurrentDisplayBuffer->getCpuAddr();
    if (!payload) {
       LOGE("%s: invalid address\n", __func__);
       return NULL;
    }

    return payload;
}

void
IntelWidiPlane::unmapPayloadBuffer(intel_gralloc_buffer_handle_t* gHandle) {

    mBufferManager->unmap( mCurrentDisplayBuffer);
    return ;
}

void
IntelWidiPlane::setOverlayData(intel_gralloc_buffer_handle_t* nHandle, uint32_t width, uint32_t height) {

    status_t ret = NO_ERROR;

    if (mState == WIDI_PLANE_STATE_STREAMING) {
        /* Retrieve index from mapping vector
         * send it to Widi Stack
         * handle error in case not found
         */

        ssize_t index = mExtVideoBuffersMapping.indexOfKey(nHandle);

        if ((index == NAME_NOT_FOUND) || (index >= mExtVideoBuffersCount)) {

            LOGE("Received an unexpectd gralloc handle while buffering --> going to CloneMode");
            sendInitMode(IWirelessDisplay::WIDI_MODE_CLONE,0,0);

        } else {

            sp<IWirelessDisplay> wd = interface_cast<IWirelessDisplay>(mWirelessDisplay);
            wd->sendBuffer(mExtVideoBuffersMapping.valueAt(index));
            LOGV("Sent handle %p Index %d, mapped index %d", nHandle, index, mExtVideoBuffersMapping.valueAt(index));

        }


    } else if (mState == WIDI_PLANE_STATE_ACTIVE) {
        /* Map payload buffer
         * get the decoder buffer count
         * initialize Widi to extVideo Mode
         * unmap payload
         */
        intel_gralloc_payload_t *payload = mapPayloadBuffer(nHandle);

        if (mExtVideoBuffersCount == 0) {
            mExtVideoBuffersCount = payload->nativebuf_count;
            LOGI("Got FIRST gralloc buffer with native handle index %d", payload->nativebuf_idx);
            LOGI("decoder context has %d surfaces", mExtVideoBuffersCount);
        }

        ssize_t index = mExtVideoBuffersMapping.indexOfKey(nHandle);
        if ((index == NAME_NOT_FOUND) && (payload->nativebuf_idx < (unsigned int)(mExtVideoBuffersCount))){

            LOGI("Mapping nHandle %p to index %d", nHandle, payload->nativebuf_idx);

            mExtVideoBuffersMapping.add(nHandle, payload->nativebuf_idx);
            mExtVideoBuffers[payload->nativebuf_idx] = *nHandle;

            int currentSize = mExtVideoBuffersMapping.size();

            if( currentSize == mExtVideoBuffersCount) {

                LOGI("Changing to ExtVideo with %d surfaces", mExtVideoBuffersMapping.size());
                ret = sendInitMode(IWirelessDisplay::WIDI_MODE_EXTENDED_VIDEO, width, height);
                if(ret != NO_ERROR) {

                  LOGE("Something went wrong setting the mode, we continue in clone mode");
                  memset(mExtVideoBuffers, 0, sizeof(intel_gralloc_buffer_handle_t)*EXT_VIDEO_MODE_MAX_SURFACE);
                  mExtVideoBuffersCount = 0;
                  mExtVideoBuffersMapping.clear();
                }

            }
        }
        unmapPayloadBuffer(nHandle);
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


bool
IntelWidiPlane::isWidiStatusChanged() {

    Mutex::Autolock _l(mLock);

    if (mWidiStatusChanged) {
        mWidiStatusChanged = false;
        return true;
    } else
        return false;
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

        ret = wd->initMode(NULL, 0,
                           IWirelessDisplay::WIDI_MODE_CLONE,
                           0, 0);
        mState = WIDI_PLANE_STATE_ACTIVE;
        memset(mExtVideoBuffers, 0, sizeof(intel_gralloc_buffer_handle_t)*EXT_VIDEO_MODE_MAX_SURFACE);
        mExtVideoBuffersCount = 0;
        mExtVideoBuffersMapping.clear();

    }else if(mode == IWirelessDisplay::WIDI_MODE_EXTENDED_VIDEO) {

        /* Adjust for orientations different than 0 (i.e. 90 and 270) */
       if(mCurrentOrientation) {
           uint32_t tmp = width;
           width = height;
           height = tmp;
       }
       ret = wd->initMode(mExtVideoBuffers, mExtVideoBuffersCount,
                          IWirelessDisplay::WIDI_MODE_EXTENDED_VIDEO,
                          width, height);

       if (ret == NO_ERROR )
           mState = WIDI_PLANE_STATE_STREAMING;
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

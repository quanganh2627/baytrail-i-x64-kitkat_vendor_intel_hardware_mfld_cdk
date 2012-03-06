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
      mNextExtFrame(-1),
      mExtVideoBuffersCount(0)

{
    LOGV("Intel Widi Plane constructor");

    mExtVideoBuffersMapping.setCapacity(EXT_VIDEO_MODE_MAX_SURFACE);
    clearExtVideoModeContext();


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
    if (mFlipListener != NULL && mState == WIDI_PLANE_STATE_ACTIVE) {
        mFlipListener->pageFlipped(systemTime(),mCurrentOrientation);

    } else if (mState == WIDI_PLANE_STATE_STREAMING) {
        sp<IWirelessDisplay> wd = interface_cast<IWirelessDisplay>(mWirelessDisplay);

        LOGV("Sending buffer, index %d", mNextExtFrame);
        if (mNextExtFrame != -1) {
            widiPayloadBuffer_t wPayload = mExtVideoPayloadBuffers[mNextExtFrame];
            status_t ret = wd->sendBuffer(wPayload.p->nativebuf_idx);

            if (ret == NO_ERROR) {
                wPayload.p->renderStatus = 1;
            } else {
                LOGV("Could not send buffer to Widi");
            }
        }
    }
    return true;
}

void
IntelWidiPlane::allowExtVideoMode(bool allow) {

    LOGV("Allow Ext video mode = %d", allow);
    mAllowExtVideoMode = allow;
}

void
IntelWidiPlane::setPlayerStatus(bool status) {

    LOGI("%s(), status = %d", __func__, status);
    Mutex::Autolock _l(mLock);

    mPlayerStatus = status;
    if ( (mState == WIDI_PLANE_STATE_STREAMING) && status == 0) {
        sendInitMode(IWirelessDisplay::WIDI_MODE_CLONE,0,0);

    }
}

void
IntelWidiPlane::setOrientation(uint32_t orientation) {
    mCurrentOrientation = orientation;
}

bool
IntelWidiPlane::mapPayloadBuffer(intel_gralloc_buffer_handle_t* gHandle, widiPayloadBuffer_t* wPayload) {

    wPayload->pDB =  mBufferManager->map(gHandle->fd[GRALLOC_SUB_BUFFER1]);
    if (!wPayload->pDB) {
       LOGE("%s: failed to map payload buffer.\n", __func__);
       return false;
    }

    wPayload->p = (intel_gralloc_payload_t*)wPayload->pDB->getCpuAddr();
    if (wPayload->p == NULL) {
       LOGE("%s: invalid address\n", __func__);
       mBufferManager->unmap(wPayload->pDB);
       return false;
    }

    return true;
}

void
IntelWidiPlane::unmapPayloadBuffer(widiPayloadBuffer_t* wPayload) {

    mBufferManager->unmap( wPayload->pDB );
    wPayload->pDB = NULL;
    wPayload->p = NULL;
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

            LOGE("Received an unexpectd gralloc handle while buffering %ld --> going to CloneMode", index);
            sendInitMode(IWirelessDisplay::WIDI_MODE_CLONE,0,0);

        } else {
            int native_index = mExtVideoBuffersMapping.valueAt(index);

            if (mNextExtFrame == native_index) {
                LOGV("Skipping duplicate index: %d", native_index);
                return;
            }

            LOGV("Next Buffer index = %d",  native_index);
            mNextExtFrame = native_index;
        }

    } else if ((mState == WIDI_PLANE_STATE_ACTIVE)  && (mPlayerStatus == 1)){
        /* Map payload buffer
         * get the decoder buffer count
         * Store handle to array until we have them all
         * initialize Widi to extVideo Mode
         *
         */
        widiPayloadBuffer_t payload;

        if (mapPayloadBuffer(nHandle, &payload)) {

            if (mExtVideoBuffersCount == 0) {
                mExtVideoBuffersCount = payload.p->nativebuf_count;
                LOGI("Got FIRST gralloc buffer with native handle index %d", payload.p->nativebuf_idx);
                LOGI("decoder context has %d surfaces", mExtVideoBuffersCount);
            }

            ssize_t index = mExtVideoBuffersMapping.indexOfKey(nHandle);
            if ((index == NAME_NOT_FOUND) && (payload.p->nativebuf_idx < (unsigned int)(mExtVideoBuffersCount))){

                LOGI("Mapping nHandle %p to index %d", nHandle, payload.p->nativebuf_idx);

                mExtVideoBuffersMapping.add(nHandle, payload.p->nativebuf_idx);
                mExtVideoBuffers[payload.p->nativebuf_idx] = *nHandle;
                mExtVideoPayloadBuffers[payload.p->nativebuf_idx] = payload;

                int currentSize = mExtVideoBuffersMapping.size();

                if( currentSize == mExtVideoBuffersCount) {

                    LOGI("Changing to ExtVideo with %d surfaces", mExtVideoBuffersMapping.size());
                    ret = sendInitMode(IWirelessDisplay::WIDI_MODE_EXTENDED_VIDEO, width, height);

                    if(ret != NO_ERROR) {
                      LOGE("Something went wrong setting the mode, we continue in clone mode");
                    }

                }

            } else {
                unmapPayloadBuffer(&payload);
            }

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
        clearExtVideoModeContext();

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

       if (ret == NO_ERROR ) {
           mState = WIDI_PLANE_STATE_STREAMING;
       } else
       {
           clearExtVideoModeContext();
           LOGE("Error setting Extended video mode ");
       }

    }

    return ret;

}

void
IntelWidiPlane::clearExtVideoModeContext() {

    LOGI("Clearing extVideo Mode context");
    for (int i = 0; i< mExtVideoBuffersCount; i++) {
        mExtVideoPayloadBuffers[i].p->renderStatus = 0;
        unmapPayloadBuffer(&mExtVideoPayloadBuffers[i]);
    }
    memset(mExtVideoPayloadBuffers, 0, sizeof(widiPayloadBuffer_t)*EXT_VIDEO_MODE_MAX_SURFACE);
    memset(mExtVideoBuffers, 0, sizeof(intel_gralloc_buffer_handle_t)*EXT_VIDEO_MODE_MAX_SURFACE);
    mExtVideoBuffersCount = 0;
    mExtVideoBuffersMapping.clear();
    mNextExtFrame = -1;
}

void
IntelWidiPlane::returnBuffer(int index) {
    LOGV("Buffer returned, index = %d", index);

    if (mExtVideoPayloadBuffers[index].p != NULL)
        mExtVideoPayloadBuffers[index].p->renderStatus = 0;
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

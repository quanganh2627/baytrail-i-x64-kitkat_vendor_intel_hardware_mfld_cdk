/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
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
      mPlayerStatus(false),
      mExtFrameRate(30),
      mInitThread(NULL),
      mFlipListener(NULL),
      mWirelessDisplay(NULL),
      mCurrentOrientation(0),
      mPrevExtFrame((uint32_t)-1),
      mExtVideoBuffersCount(0)

{
    LOGV("Intel Widi Plane constructor");

    memset(&mCurrExtFramePayload, 0, sizeof(mCurrExtFramePayload));
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
        mWidiStatusChanged = true;

        clearExtVideoModeContext();
    }
    return;
}

status_t
IntelWidiPlane::registerFlipListener(sp<IPageFlipListener> listener) {

    mFlipListener = listener;
    return NO_ERROR;
}

bool
IntelWidiPlane::flip(void *context, uint32_t flags) {

    LOGV("Widi Plane flip, flip listener = %p", mFlipListener.get());
    if (mFlipListener != NULL && mState == WIDI_PLANE_STATE_ACTIVE) {
        mFlipListener->pageFlipped(systemTime(),mCurrentOrientation);

    } else if (mState == WIDI_PLANE_STATE_STREAMING) {

        Mutex::Autolock _l(mExtBufferMapLock);

        sp<IWirelessDisplay> wd = interface_cast<IWirelessDisplay> (mWirelessDisplay);

        if(mCurrExtFramePayload.p == NULL) {
            return true;
        }

        if(mPrevExtFrame == mCurrExtFramePayload.p->khandle) {
            return true;
        }

        // Decoder may be destroyed before player status is set to be 0.
        // When decoder is destroyed, psb-video will zero payload.
        // We switch to clone mode here if this happens.
        if (mCurrExtFramePayload.p->khandle == 0) {
            goto switch_to_clone;
        }

        status_t ret = wd->sendBuffer(mCurrExtFramePayload.p->khandle,
                                      mCurrExtFramePayload.p->timestamp);
        if (ret == NO_ERROR) {
            mCurrExtFramePayload.p->renderStatus = 1;
        }

        mPrevExtFrame = mCurrExtFramePayload.p->khandle;
    }
    return true;

switch_to_clone:
    sendInitMode(IWirelessDisplay::WIDI_MODE_CLONE, 0, 0);
    return true;
}

void
IntelWidiPlane::allowExtVideoMode(bool allow) {

    LOGV("Allow Ext video mode = %d", allow);
    mAllowExtVideoMode = allow;
}

void
IntelWidiPlane::setPlayerStatus(bool status, int fps) {

    LOGI("%s(), status = %d fps = %d", __func__, status, fps);
    Mutex::Autolock _l(mLock);

    mExtFrameRate = fps;
    if(mPlayerStatus == status) {
        return;
    }

    mPlayerStatus = status;
    if ( (mState == WIDI_PLANE_STATE_STREAMING) && status == false) {
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
    widiPayloadBuffer_t payload;

    Mutex::Autolock _l(mExtBufferMapLock);

    ssize_t index = mExtVideoBuffersMapping.indexOfKey(nHandle);

    if (index == NAME_NOT_FOUND) {
        if (mapPayloadBuffer(nHandle, &payload)) {
            if(payload.p->used_by_widi != 0 || payload.p->khandles_count == 0) {
                unmapPayloadBuffer(&payload);
                return;
            }

            if ((mState == WIDI_PLANE_STATE_ACTIVE) && (mPlayerStatus == true)) {

                mExtVideoBuffersCount = payload.p->khandles_count;
                for (unsigned int i = 0; i < payload.p->khandles_count; i++) {
                    mExtVideoBuffers[i].khandle = payload.p->khandles[i];
                    mExtVideoBuffers[i].width = payload.p->width;
                    mExtVideoBuffers[i].height = payload.p->height;
                    mExtVideoBuffers[i].luma_stride = payload.p->luma_stride;
                    mExtVideoBuffers[i].chroma_u_stride = payload.p->chroma_u_stride;
                    mExtVideoBuffers[i].chroma_v_stride = payload.p->chroma_v_stride;
                    mExtVideoBuffers[i].format = payload.p->format;
                    LOGI("khandle = 0x%x width = %d height = %d luma_stride = %d chroma_u_stride = %d chroma_v_stride = %d format = 0x%x",
                         mExtVideoBuffers[i].khandle, mExtVideoBuffers[i].width,  mExtVideoBuffers[i].height, mExtVideoBuffers[i].luma_stride,
                         mExtVideoBuffers[i].chroma_u_stride, mExtVideoBuffers[i].chroma_v_stride, mExtVideoBuffers[i].format);
                }
                ret = sendInitMode(IWirelessDisplay::WIDI_MODE_EXTENDED_VIDEO, width, height);

                if (ret != NO_ERROR) {
                    LOGE("Something went wrong setting the mode, we continue in clone mode");
                }
            }

            payload.p->used_by_widi = 1;

            if(mState == WIDI_PLANE_STATE_STREAMING) {

                mExtVideoBuffersMapping.add(nHandle, payload);
            }
        }
    } else {

        payload = mExtVideoBuffersMapping.valueAt(index);

    }

    if (mState == WIDI_PLANE_STATE_STREAMING) {

        mCurrExtFramePayload = payload;

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
                           0, 0, 0, 0);
        mState = WIDI_PLANE_STATE_ACTIVE;
        clearExtVideoModeContext();

    } else if(mode == IWirelessDisplay::WIDI_MODE_EXTENDED_VIDEO) {

       ret = wd->initMode(mExtVideoBuffers, mExtVideoBuffersCount,
                          IWirelessDisplay::WIDI_MODE_EXTENDED_VIDEO,
                          width, height, mExtFrameRate, 1);

       if (ret == NO_ERROR ) {
           mState = WIDI_PLANE_STATE_STREAMING;
       } else {

           clearExtVideoModeContext();
           LOGE("Error setting Extended video mode ");
       }

    }

    return ret;

}

void
IntelWidiPlane::clearExtVideoModeContext() {

    Mutex::Autolock _l(mExtBufferMapLock);

    if(mExtVideoBuffersMapping.size()) {
        for(unsigned int i = 0; i < mExtVideoBuffersMapping.size(); i ++) {
            widiPayloadBuffer_t payload = mExtVideoBuffersMapping.valueAt(i);
            payload.p->renderStatus = 0;
            unmapPayloadBuffer(&payload);
        }
    }

    memset(mExtVideoBuffers, 0, sizeof(intel_widi_ext_video_buffer_t)*EXT_VIDEO_MODE_MAX_SURFACE);
    mExtVideoBuffersCount = 0;
    mExtVideoBuffersMapping.clear();
    memset(&mCurrExtFramePayload, 0, sizeof(mCurrExtFramePayload));
    mPrevExtFrame = (uint32_t)-1;
}

void
IntelWidiPlane::returnBuffer(int khandle) {
    LOGV("Buffer returned, index = %d", index);

    Mutex::Autolock _l(mExtBufferMapLock);

    if(mExtVideoBuffersMapping.size()) {
        for(unsigned int i = 0; i < mExtVideoBuffersMapping.size(); i++) {
            widiPayloadBuffer_t payload = mExtVideoBuffersMapping.valueAt(i);
            if(payload.p->khandle == (unsigned int) khandle) {
                payload.p->renderStatus = 0;
                break;
            }
        }
    }
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

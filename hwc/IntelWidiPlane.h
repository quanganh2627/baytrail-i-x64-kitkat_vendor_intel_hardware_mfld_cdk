/*
 * Copyright (c) 2008-2012, Intel Corporation. All rights reserved.
 *
 * Redistribution.
 * Redistribution and use in binary form, without modification, are
 * permitted provided that the following conditions are met:
 *  * Redistributions must reproduce the above copyright notice and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its
 * suppliers may be used to endorse or promote products derived from
 * this software without specific  prior written permission.
 *  * No reverse engineering, decompilation, or disassembly of this
 * software is permitted.
 *
 * Limited patent license.
 * Intel Corporation grants a world-wide, royalty-free, non-exclusive
 * license under patents it now or hereafter owns or controls to make,
 * have made, use, import, offer to sell and sell ("Utilize") this
 * software, but solely to the extent that any such patent is necessary
 * to Utilize the software alone, or in combination with an operating
 * system licensed under an approved Open Source license as listed by
 * the Open Source Initiative at http://opensource.org/licenses.
 * The patent license shall not apply to any other combinations which
 * include this software. No hardware per se is licensed hereunder.
 *
 * DISCLAIMER.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#ifndef __INTEL_WIDI_PLANE_H__
#define __INTEL_WIDI_PLANE_H__

#ifdef INTEL_WIDI

#include <IntelDisplayPlaneManager.h>
#include <utils/threads.h>
#include <utils/KeyedVector.h>
#include <binder/IInterface.h>
#include <binder/IServiceManager.h>
#include "IFrameListener.h"
#include "IFrameTypeChangeListener.h"

static const unsigned int EXT_VIDEO_MODE_MAX_SURFACE = 32;

typedef struct {
        intel_gralloc_payload_t *p;
        IntelDisplayBuffer      *pDB;
}widiPayloadBuffer_t;

class IntelWidiPlane : public IntelDisplayPlane, public RefBase {

public:
    IntelWidiPlane(int fd, int index, IntelBufferManager *bufferManager);
    ~IntelWidiPlane();
    virtual void setPosition(int left, int top, int right, int bottom);

    android::status_t  enablePlane(sp<IFrameTypeChangeListener> typeChangelistener);
    void  disablePlane(bool isConnected);
    bool isExtVideoAllowed() {return mAllowExtVideoMode;};
    void setBackgroundVideoMode(bool value);
    bool isBackgroundVideoMode();
    void setNativeWindow(int *nw);
    void getNativeWindow(int*& nativeWindow);
    bool isSurfaceMatching(intel_gralloc_buffer_handle_t* nHandle);
    void setPlayerStatus(bool status, int fps);
    void setOverlayData(intel_gralloc_buffer_handle_t* nHandle, uint32_t width, uint32_t height);
    bool isStreaming() { return (mState == WIDI_PLANE_STATE_STREAMING); };
    bool isPlayerOn() { return mPlayerStatus; };
    void returnBuffer(int index);

    bool flip(void *contexts, uint32_t flags);
    bool isActive();
    bool isWidiStatusChanged();

protected:
    void init();
    android::status_t notifyFrameTypeChange(HWCFrameType frameType, uint32_t width, uint32_t height);
    bool mapPayloadBuffer(intel_gralloc_buffer_handle_t* gHandle, widiPayloadBuffer_t* wPayload);
    void unmapPayloadBuffer(widiPayloadBuffer_t* wPayload);
    void clearExtVideoModeContext(bool lock = true);

    typedef enum {
        WIDI_PLANE_STATE_UNINIT, // TODO: UNINIT is not used anymore, remove?
        WIDI_PLANE_STATE_INITIALIZED,
        WIDI_PLANE_STATE_ACTIVE,
        WIDI_PLANE_STATE_STREAMING
    }WidiPlaneState;

    class DeathNotifier: public android::IBinder::DeathRecipient
    {
    public:
                DeathNotifier(IntelWidiPlane* me): mSelf(me) {}

        virtual void binderDied(const android::wp<android::IBinder>& who);
    private:
        android::sp<IntelWidiPlane> mSelf;
    };

    int32_t                         mAllowExtVideoMode;
    int32_t                         mSetBackgroudVideoMode;
    WidiPlaneState                  mState;
    bool                            mWidiStatusChanged;
    bool                            mPlayerStatus;
    bool                            mStreamingEnabled;
    int                             mExtFrameRate;

    android::Mutex                  mLock;
    android::sp<DeathNotifier>      mDeathNotifier;
    android::sp<IFrameListener>     mFrameListener; // to notify widi
    android::sp<IFrameTypeChangeListener> mFrameTypeChangeListener; // to notify widi
    uint32_t                        mCurrentOrientation;

    android::Mutex                  mExtBufferMapLock;
    widiPayloadBuffer_t             mCurrExtFramePayload;
    uint32_t                        mPrevExtFrame;
    intel_widi_ext_buffer_meta_t    mExtVideoBufferMeta;
    android::KeyedVector<intel_gralloc_buffer_handle_t*, widiPayloadBuffer_t> mExtVideoBuffersMapping;
    bool                            mUseRotateHandle;
    uint32_t                        mExtWidth;
    uint32_t                        mExtHeight;
    int32_t *                       mBackgroundWidiNw;
};

#else  // Stub implementation in case of widi module is not compiled

class IntelWidiPlane : public IntelDisplayPlane  {

public:
    IntelWidiPlane(int fd, int index, IntelBufferManager *bm):
        IntelDisplayPlane(fd, IntelDisplayPlane::DISPLAY_PLANE_OVERLAY, index, bm){};
    ~IntelWidiPlane(){};
    virtual void setPosition(int left, int top, int right, int bottom){return;};
    void setOverlayData(intel_gralloc_buffer_handle_t* nHandle, uint32_t width, uint32_t height){};

    bool isExtVideoAllowed() {return true;};
    void setBackgroundVideoMode(bool value){return;};
    bool isBackgroundVideoMode() {return false;};
    void setNativeWindow(int *nw){};
    void getNativeWindow(int*& nativeWindow){};
    bool isSurfaceMatching(intel_gralloc_buffer_handle_t* nHandle){return false;};
    void setPlayerStatus(bool status, int fps) {return;};

    bool flip(void *contexts, uint32_t flags){return true;};
    bool isActive(){return false;};
    bool isStreaming() { return false; };
    bool isPlayerOn() { return false; };
    bool isWidiStatusChanged(){return false;};


};
#endif

#endif /*__INTEL_OVERLAY_PLANE_H__*/

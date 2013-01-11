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
#include <utils/StopWatch.h>
#include <IntelWidiPlane.h>
#include <IntelOverlayUtil.h>

using namespace android;

IntelWidiPlane::IntelWidiPlane(int fd, int index, IntelBufferManager *bm)
    : IntelDisplayPlane(fd, IntelDisplayPlane::DISPLAY_PLANE_OVERLAY, index, bm),
      mAllowExtVideoMode(true),
      mSetBackgroudVideoMode(false),
      mBackgroundWidiNw(NULL),
      mState(WIDI_PLANE_STATE_INITIALIZED),
      mWidiStatusChanged(false),
      mPlayerStatus(false),
      mStreamingEnabled(false),
      mExtFrameRate(30),
      mFrameListener(NULL),
      mFrameTypeChangeListener(NULL),
      mPrevExtFrame((uint32_t)-1),
      mUseRotateHandle(false),
      mExtWidth(0),
      mExtHeight(0)
{
    ALOGD_IF(ALLOW_WIDI_PRINT, "Intel Widi Plane constructor");

    memset(&mCurrExtFramePayload, 0, sizeof(mCurrExtFramePayload));
    mExtVideoBuffersMapping.setCapacity(EXT_VIDEO_MODE_MAX_SURFACE);
    clearExtVideoModeContext();

    mDeathNotifier = new DeathNotifier(this);
    mInitialized = true;

    ALOGD_IF(ALLOW_WIDI_PRINT, "Intel Widi Plane constructor- DONE");
}

IntelWidiPlane::~IntelWidiPlane()
{
    ALOGD_IF(ALLOW_WIDI_PRINT, "Intel Widi Plane destructor");
    if (initCheck()) {
        mInitialized = false;
    }
}

void
IntelWidiPlane::setPosition(int left, int top, int right, int bottom)
{
    if (initCheck()) {
    }
}

status_t
IntelWidiPlane::enablePlane(sp<IFrameTypeChangeListener> typeChangeListener) {
    Mutex::Autolock _l(mLock);

    if(mState == WIDI_PLANE_STATE_INITIALIZED) {
        ALOGD_IF(ALLOW_WIDI_PRINT, "Plane Enabled !!");
        mFrameTypeChangeListener = typeChangeListener;
        if(mFrameTypeChangeListener != NULL) {
            sp<IBinder> b = mFrameTypeChangeListener->asBinder();
            b->linkToDeath(mDeathNotifier);
        }
        mState = WIDI_PLANE_STATE_ACTIVE;
        mWidiStatusChanged = true;
    }

    mStreamingEnabled = true;
    return 0;
}

void
IntelWidiPlane::disablePlane(bool isConnected) {

    Mutex::Autolock _l(mLock);
    mStreamingEnabled = false;
    if(isConnected)
    {
       if(mState == WIDI_PLANE_STATE_STREAMING)
       {
          mState = WIDI_PLANE_STATE_ACTIVE;
          mWidiStatusChanged = true;
          clearExtVideoModeContext();
       }
    }
    else {
       if ((mState == WIDI_PLANE_STATE_ACTIVE) || (mState == WIDI_PLANE_STATE_STREAMING)) {
           ALOGV("Plane Disabled !!");
           mState = WIDI_PLANE_STATE_INITIALIZED;
           mWidiStatusChanged = true;
           if(mFrameTypeChangeListener != NULL) {
               mFrameTypeChangeListener->asBinder()->unlinkToDeath(mDeathNotifier);
               mFrameTypeChangeListener = NULL;
           }
           if(mFrameListener != NULL) {
               mFrameListener->asBinder()->unlinkToDeath(mDeathNotifier);
               mFrameListener = NULL;
           }
           clearExtVideoModeContext();
        }
    }
    return;
}

bool
IntelWidiPlane::flip(void *context, uint32_t flags) {

    ALOGD_IF(ALLOW_HWC_PRINT, "Widi Plane flip");
    if (mState == WIDI_PLANE_STATE_STREAMING) {
        Mutex::Autolock _l(mExtBufferMapLock);

        if(mCurrExtFramePayload.p == NULL) {
            return true;
        }

        uint32_t khandle = mUseRotateHandle ?
                mCurrExtFramePayload.p->rotated_buffer_handle : mCurrExtFramePayload.p->khandle;

        if (mPrevExtFrame == khandle) {
            return true;
        }

        // Decoder may be destroyed before player status is set to be 0.
        // When decoder is destroyed, psb-video will zero payload.
        // We switch to clone mode here if this happens.
        if (mCurrExtFramePayload.p->khandle == 0) {
            goto switch_to_clone;
        }

        if(mFrameListener != NULL) {
            status_t ret = mFrameListener->bufferAvailable(khandle, mCurrExtFramePayload.p->timestamp);
            if (ret == NO_ERROR) {
                mCurrExtFramePayload.p->renderStatus = 1;
            }
        }

        mPrevExtFrame = khandle;
    }
    return true;

switch_to_clone:
    notifyFrameTypeChange(HWC_FRAMETYPE_FRAME_BUFFER, 0, 0);
    return true;
}

void
IntelWidiPlane::setBackgroundVideoMode(bool value) {

    ALOGD_IF(ALLOW_WIDI_PRINT, "Set Background video mode = %d", value);
    mSetBackgroudVideoMode = value;
}

bool
IntelWidiPlane::isBackgroundVideoMode() {
    return mSetBackgroudVideoMode;
}

void
IntelWidiPlane::getNativeWindow(int*& nativeWindow) {
    if(mState == WIDI_PLANE_STATE_STREAMING) {
        nativeWindow = (int *)(mCurrExtFramePayload.p->native_window);
        ALOGD_IF(ALLOW_WIDI_PRINT,"getNativeWindow nativeWindow = 0x%x", nativeWindow);
    }
    else {
        nativeWindow = NULL;
        ALOGD_IF(ALLOW_WIDI_PRINT, "Widi not streaming nativeWindow is Null");
    }
}

void
IntelWidiPlane::setNativeWindow(int *nw) {
   if(isBackgroundVideoMode())
       mBackgroundWidiNw = nw;
   else
       mBackgroundWidiNw = NULL;
}

bool
IntelWidiPlane::isSurfaceMatching(intel_gralloc_buffer_handle_t* nHandle) {
    if(!mSetBackgroudVideoMode || (mBackgroundWidiNw == NULL))
        return true;

    widiPayloadBuffer_t payload;
    ssize_t index = mExtVideoBuffersMapping.indexOfKey(nHandle);
    if (index == NAME_NOT_FOUND) {
        mapPayloadBuffer(nHandle, &payload);
    }
    else {
        payload = mExtVideoBuffersMapping.valueAt(index);
    }
    int *nativeWindow = (int *)(payload.p->native_window);

    if (index == NAME_NOT_FOUND)
        unmapPayloadBuffer(&payload);

    return(mBackgroundWidiNw == nativeWindow ? true:false);
}


void
IntelWidiPlane::setPlayerStatus(bool status, int fps) {

    ALOGI("%s(), status = %d fps = %d", __func__, status, fps);
    Mutex::Autolock _l(mLock);

    if(isBackgroundVideoMode() && (mPlayerStatus == true)) {
        ALOGD_IF(ALLOW_WIDI_PRINT,
               "Widi is in Extended Background Video mode, second playback at device");
    }
    else {
        mExtFrameRate = fps;
        if(mPlayerStatus == status) {
            return;
        }
        mPlayerStatus = status;
        if ( (mState == WIDI_PLANE_STATE_STREAMING) && status == false) {
            notifyFrameTypeChange(HWC_FRAMETYPE_FRAME_BUFFER, 0, 0);
        }
    }
}

bool
IntelWidiPlane::mapPayloadBuffer(intel_gralloc_buffer_handle_t* gHandle, widiPayloadBuffer_t* wPayload) {

    wPayload->pDB =  mBufferManager->map(gHandle->fd[GRALLOC_SUB_BUFFER1]);
    if (!wPayload->pDB) {
       ALOGE("%s: failed to map payload buffer.\n", __func__);
       return false;
    }

    wPayload->p = (intel_gralloc_payload_t*)wPayload->pDB->getCpuAddr();
    if (wPayload->p == NULL) {
       ALOGE("%s: invalid address\n", __func__);
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
    bool isResolutionChanged = false;

    Mutex::Autolock _l(mExtBufferMapLock);

    if(!isSurfaceMatching(nHandle))
        return;

    // If the resolution is changed, reset
    if(mExtWidth != width || mExtHeight != height) {

        if(mExtVideoBuffersMapping.size()) {
            for(unsigned int i = 0; i < mExtVideoBuffersMapping.size(); i ++) {
                widiPayloadBuffer_t payload = mExtVideoBuffersMapping.valueAt(i);
                payload.p->used_by_widi = 0;
            }
        }
        clearExtVideoModeContext(false);
        mUseRotateHandle = false;
        mExtWidth = width;
        mExtHeight = height;
        isResolutionChanged = true;
    }

    ssize_t index = mExtVideoBuffersMapping.indexOfKey(nHandle);

    if (index == NAME_NOT_FOUND) {
        if (mapPayloadBuffer(nHandle, &payload)) {
            if(payload.p->used_by_widi != 0 || payload.p->khandle == 0) {
                unmapPayloadBuffer(&payload);
                return;
            }

            if ((mState == WIDI_PLANE_STATE_ACTIVE || isResolutionChanged) && (mPlayerStatus == true)) {
                mUseRotateHandle = false;
                if (payload.p->metadata_transform != 0) {
                    mUseRotateHandle = true;
                }

                if (!mUseRotateHandle) {
                    mExtVideoBufferMeta.width = payload.p->width;
                    mExtVideoBufferMeta.height = payload.p->height;
                    mExtVideoBufferMeta.luma_stride = payload.p->luma_stride;
                    mExtVideoBufferMeta.chroma_u_stride = payload.p->chroma_u_stride;
                    mExtVideoBufferMeta.chroma_v_stride = payload.p->chroma_v_stride;
                    mExtVideoBufferMeta.format = payload.p->format;
                } else {
                    mExtVideoBufferMeta.width = payload.p->rotated_width;
                    mExtVideoBufferMeta.height = payload.p->rotated_height;
                    mExtVideoBufferMeta.luma_stride = payload.p->rotate_luma_stride;
                    mExtVideoBufferMeta.chroma_u_stride = payload.p->rotate_chroma_u_stride;
                    mExtVideoBufferMeta.chroma_v_stride = payload.p->rotate_chroma_v_stride;
                    mExtVideoBufferMeta.format = payload.p->format;
                }
                ALOGI("width = %d height = %d luma_stride = %d chroma_u_stride = %d chroma_v_stride = %d format = 0x%x",
                        mExtVideoBufferMeta.width,    mExtVideoBufferMeta.height, mExtVideoBufferMeta.luma_stride,
                        mExtVideoBufferMeta.chroma_u_stride, mExtVideoBufferMeta.chroma_v_stride,    mExtVideoBufferMeta.format);

                int widthr  = width;
                int heightr = height;
                if (mUseRotateHandle && (payload.p->metadata_transform == HAL_TRANSFORM_ROT_90
                        || payload.p->metadata_transform == HAL_TRANSFORM_ROT_270)) {
                    widthr  = height;
                    heightr = width;
                }

                ret = notifyFrameTypeChange(HWC_FRAMETYPE_VIDEO, widthr, heightr);
                if (ret != NO_ERROR) {
                    ALOGE("Something went wrong setting the mode, we continue in clone mode");
                }
            }

            if(mState == WIDI_PLANE_STATE_STREAMING) {

                payload.p->used_by_widi = 1;
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

status_t
IntelWidiPlane::notifyFrameTypeChange(HWCFrameType frameType, uint32_t width, uint32_t height) {

    status_t ret = NO_ERROR;
    if(frameType == HWC_FRAMETYPE_FRAME_BUFFER) {

        if(mFrameTypeChangeListener != NULL) {
            FrameInfo frameInfo;
            frameInfo.frameType = HWC_FRAMETYPE_FRAME_BUFFER;
            mFrameListener = mFrameTypeChangeListener->frameTypeChanged(frameInfo);
        }
        mState = WIDI_PLANE_STATE_ACTIVE;
        clearExtVideoModeContext();
        mUseRotateHandle = false;
        mExtWidth = 0;
        mExtHeight = 0;
        mWidiStatusChanged = true;
        mBackgroundWidiNw = NULL;
    } else if(frameType == HWC_FRAMETYPE_VIDEO) {

        if(mFrameTypeChangeListener != NULL) {
            FrameInfo frameInfo;
            frameInfo.frameType = HWC_FRAMETYPE_VIDEO;
            frameInfo.contentWidth = width;
            frameInfo.contentHeight = height;
            frameInfo.bufferWidth = mExtVideoBufferMeta.width;
            frameInfo.bufferHeight = mExtVideoBufferMeta.height;
            frameInfo.bufferFormat = mExtVideoBufferMeta.format;
            frameInfo.lumaUStride = mExtVideoBufferMeta.luma_stride;
            frameInfo.chromaUStride = mExtVideoBufferMeta.chroma_u_stride;
            frameInfo.chromaVStride = mExtVideoBufferMeta.chroma_v_stride;
            frameInfo.bufferChromaUStride = mExtVideoBufferMeta.chroma_u_stride;
            frameInfo.bufferChromaVStride = mExtVideoBufferMeta.chroma_v_stride;
            frameInfo.contentFrameRateN = mExtFrameRate;
            frameInfo.contentFrameRateD = 1;
            mFrameListener = mFrameTypeChangeListener->frameTypeChanged(frameInfo);
            if (mFrameListener != NULL) {
                mState = WIDI_PLANE_STATE_STREAMING;
                mWidiStatusChanged = true;
            } else {
                clearExtVideoModeContext();
                ALOGE("Error setting Extended video mode ");
            }
        }
        else {
            clearExtVideoModeContext();
            ALOGE("Error setting Extended video mode ");
        }
    }
    return ret;
}

void
IntelWidiPlane::clearExtVideoModeContext(bool lock) {

    if(lock) {
        mExtBufferMapLock.lock();
    }

    if(mExtVideoBuffersMapping.size()) {
        for(unsigned int i = 0; i < mExtVideoBuffersMapping.size(); i ++) {
            widiPayloadBuffer_t payload = mExtVideoBuffersMapping.valueAt(i);
            payload.p->renderStatus = 0;
            unmapPayloadBuffer(&payload);
        }
    }

    memset(&mExtVideoBufferMeta, 0, sizeof(intel_widi_ext_buffer_meta_t));
    mExtVideoBuffersMapping.clear();
    memset(&mCurrExtFramePayload, 0, sizeof(mCurrExtFramePayload));
    mPrevExtFrame = (uint32_t)-1;

    if(lock) {
        mExtBufferMapLock.unlock();
    }
}

void
IntelWidiPlane::returnBuffer(int khandle) {
    ALOGD_IF(ALLOW_WIDI_PRINT, "Buffer returned, index = %d", khandle);

    Mutex::Autolock _l(mExtBufferMapLock);

    if(mExtVideoBuffersMapping.size()) {
        for (unsigned int i = 0; i < mExtVideoBuffersMapping.size(); i++) {
            widiPayloadBuffer_t payload = mExtVideoBuffersMapping.valueAt(i);
            uint32_t _khandle = mUseRotateHandle ?
                    payload.p->rotated_buffer_handle : payload.p->khandle;
            if (_khandle == (unsigned int) khandle) {
                payload.p->renderStatus = 0;
                break;
            }
        }
    }
}

void
IntelWidiPlane::DeathNotifier::binderDied(const wp<IBinder>& who) {
    ALOGW("Frame listener died - disabling plane");
    mSelf->disablePlane(false);
}

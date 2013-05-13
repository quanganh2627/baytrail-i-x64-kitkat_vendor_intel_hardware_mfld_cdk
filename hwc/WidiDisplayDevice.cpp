/*
 * Copyright © 2013 Intel Corporation
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
 *    Brian Rogers <brian.e.rogers@intel.com>
 *
 */
#include <cutils/log.h>
#include <utils/Errors.h>

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>

#include <IntelHWComposer.h>
#include <IntelDisplayDevice.h>
#include <WidiDisplayDevice.h>
#include <IntelHWComposerCfg.h>

using namespace android;

WidiDisplayDevice::CachedBuffer::CachedBuffer(IntelBufferManager *gbm, IntelDisplayBuffer* buffer)
    : grallocBufferManager(gbm),
      displayBuffer(buffer)
{
}

WidiDisplayDevice::CachedBuffer::~CachedBuffer()
{
    grallocBufferManager->unmap(displayBuffer);
}

WidiDisplayDevice::WidiDisplayDevice(IntelBufferManager *bm,
                                     IntelBufferManager *gm,
                                     IntelDisplayPlaneManager *pm,
                                     IntelHWComposerDrm *drm,
                                     WidiExtendedModeInfo *extinfo,
                                     uint32_t index)
                                   : IntelDisplayDevice(pm, drm, bm, gm, index),
                                     mExtendedModeInfo(extinfo),
                                     mExtLastKhandle(0),
                                     mExtLastTimestamp(0)
{
    ALOGD_IF(ALLOW_WIDI_PRINT, "%s", __func__);

    //check buffer manager
    if (!mBufferManager) {
        ALOGE("%s: Invalid buffer manager", __func__);
        goto init_err;
    }

    // check buffer manager for gralloc buffer
    if (!mGrallocBufferManager) {
        ALOGE("%s: Invalid Gralloc buffer manager", __func__);
        goto init_err;
    }

    // check display plane manager
    if (!mPlaneManager) {
        ALOGE("%s: Invalid plane manager", __func__);
        goto init_err;
    }

    mLayerList = NULL; // not used

    mNextConfig.typeChangeListener = NULL;
    mNextConfig.policy.scaledWidth = 0;
    mNextConfig.policy.scaledHeight = 0;
    mNextConfig.policy.xdpi = 96;
    mNextConfig.policy.ydpi = 96;
    mNextConfig.policy.refresh = 60;
    mNextConfig.extendedModeEnabled = false;
    mNextConfig.forceNotify = false;
    mCurrentConfig = mNextConfig;

    memset(&mLastFrameInfo, 0, sizeof(mLastFrameInfo));

    mInitialized = true;

    {
        // Publish frame server service with service manager
        status_t ret = defaultServiceManager()->addService(String16("hwc.widi"), this);
        if (ret != NO_ERROR) {
            ALOGE("%s: Could not register hwc.widi with service manager, error = %d", __func__, ret);
        }
        ProcessState::self()->startThreadPool();
    }

    return;

init_err:
    mInitialized = false;
    return;
}

WidiDisplayDevice::~WidiDisplayDevice()
{
    ALOGI("%s", __func__);
}

sp<WidiDisplayDevice::CachedBuffer> WidiDisplayDevice::getDisplayBuffer(intel_gralloc_buffer_handle_t* handle)
{
    ssize_t index = mDisplayBufferCache.indexOfKey(handle);
    sp<CachedBuffer> cachedBuffer;
    if (index == NAME_NOT_FOUND) {
        IntelDisplayBuffer* displayBuffer = mGrallocBufferManager->map(handle->fd[GRALLOC_SUB_BUFFER1]);
        if (displayBuffer != NULL) {
            cachedBuffer = new CachedBuffer(mGrallocBufferManager, displayBuffer);
            mDisplayBufferCache.add(handle, cachedBuffer);
        }
    }
    else {
        cachedBuffer = mDisplayBufferCache[index];
    }
    return cachedBuffer;
}

status_t WidiDisplayDevice::start(sp<IFrameTypeChangeListener> typeChangeListener, bool disableExtVideoMode) {
    ALOGD_IF(ALLOW_WIDI_PRINT, "%s", __func__);
    Mutex::Autolock _l(mConfigLock);
    mNextConfig.typeChangeListener = typeChangeListener;
    mNextConfig.policy.scaledWidth = 0;
    mNextConfig.policy.scaledHeight = 0;
    mNextConfig.policy.xdpi = 96;
    mNextConfig.policy.ydpi = 96;
    mNextConfig.policy.refresh = 60;
    mNextConfig.extendedModeEnabled = !disableExtVideoMode;
    mNextConfig.forceNotify = true;
    return NO_ERROR;
}

status_t WidiDisplayDevice::stop(bool isConnected) {
    ALOGD_IF(ALLOW_WIDI_PRINT, "%s", __func__);
    Mutex::Autolock _l(mConfigLock);
    mNextConfig.typeChangeListener = NULL;
    mNextConfig.policy.scaledWidth = 0;
    mNextConfig.policy.scaledHeight = 0;
    mNextConfig.policy.xdpi = 96;
    mNextConfig.policy.ydpi = 96;
    mNextConfig.policy.refresh = 60;
    mNextConfig.extendedModeEnabled = false;
    mNextConfig.forceNotify = false;
    return NO_ERROR;
}

status_t WidiDisplayDevice::notifyBufferReturned(int khandle) {
    ALOGD_IF(ALLOW_WIDI_PRINT, "%s khandle=%x", __func__, (uint32_t)khandle);
    Mutex::Autolock _l(mHeldBuffersLock);
    ssize_t index = mHeldBuffers.indexOfKey(khandle);
    if (index == NAME_NOT_FOUND) {
        LOGE("Couldn't find returned khandle %x", khandle);
    }
    else {
        sp<CachedBuffer> cachedBuffer = mHeldBuffers.valueAt(index);
        intel_gralloc_payload_t* p = (intel_gralloc_payload_t*)cachedBuffer->displayBuffer->getCpuAddr();
        p->renderStatus = 0;
        mHeldBuffers.removeItemsAt(index, 1);
    }
    return NO_ERROR;
}

status_t WidiDisplayDevice::setResolution(const FrameProcessingPolicy& policy, sp<IFrameListener> listener) {
    Mutex::Autolock _l(mConfigLock);
    mNextConfig.frameListener = listener;
    mNextConfig.policy = policy;
    return NO_ERROR;
}

bool WidiDisplayDevice::prepare(hwc_display_contents_1_t *list)
{
    ALOGD_IF(ALLOW_WIDI_PRINT, "%s", __func__);

    if (!initCheck()) {
        ALOGE("%s: failed to initialize HWComposer", __func__);
        return false;
    }

    {
        Mutex::Autolock _l(mConfigLock);
        mCurrentConfig = mNextConfig;
        mNextConfig.forceNotify = false;
    }

    intel_gralloc_buffer_handle_t* videoFrame = NULL;
    hwc_layer_1_t* videoLayer = NULL;

    if (mCurrentConfig.extendedModeEnabled && mExtendedModeInfo->widiExtHandle) {
        for (size_t i = 0; i < list->numHwLayers-1; i++) {
            hwc_layer_1_t& layer = list->hwLayers[i];

            intel_gralloc_buffer_handle_t *grallocHandle = NULL;
            if (layer.compositionType != HWC_BACKGROUND)
                grallocHandle = (intel_gralloc_buffer_handle_t*)layer.handle;

            if (grallocHandle == mExtendedModeInfo->widiExtHandle)
            {
                videoFrame = grallocHandle;
                videoLayer = &layer;
                break;
            }
        }
    }

    bool extActive = false;
    if (mCurrentConfig.typeChangeListener != NULL) {
        FrameInfo frameInfo;

        if (videoFrame != NULL && mDrm->isVideoPlaying()) {
            sp<CachedBuffer> cachedBuffer;
            intel_gralloc_payload_t *p;
            if ((cachedBuffer = getDisplayBuffer(videoFrame)) == NULL) {
                ALOGE("%s: Failed to map display buffer", __func__);
            }
            else if ((p = (intel_gralloc_payload_t*)cachedBuffer->displayBuffer->getCpuAddr()) == NULL) {
                ALOGE("%s: Got null payload from display buffer", __func__);
            }
            else {
                // default fps to 0. widi stack will decide what correct fps should be
                int displayW = 0, displayH = 0, fps = 0, isInterlace = 0;
                mDrm->getVideoInfo(&displayW, &displayH, &fps, &isInterlace);
                if (fps < 0)
                    fps = 0;

                hwc_layer_1_t& layer = *videoLayer;
                memset(&frameInfo, 0, sizeof(frameInfo));
                frameInfo.frameType = HWC_FRAMETYPE_VIDEO;
                frameInfo.bufferFormat = p->format;

                if ((p->metadata_transform & HAL_TRANSFORM_ROT_90) == 0) {
                    frameInfo.contentWidth = layer.sourceCrop.right - layer.sourceCrop.left;
                    frameInfo.contentHeight = layer.sourceCrop.bottom - layer.sourceCrop.top;
                }
                else {
                    frameInfo.contentWidth = layer.sourceCrop.bottom - layer.sourceCrop.top;
                    frameInfo.contentHeight = layer.sourceCrop.right - layer.sourceCrop.left;
                }
                if (p->metadata_transform == 0) {
                    frameInfo.bufferWidth = p->width;
                    frameInfo.bufferHeight = p->height;
                    frameInfo.lumaUStride = p->luma_stride;
                    frameInfo.chromaUStride = p->chroma_u_stride;
                    frameInfo.chromaVStride = p->chroma_v_stride;
                }
                else {
                    frameInfo.bufferWidth = p->rotated_width;
                    frameInfo.bufferHeight = p->rotated_height;
                    frameInfo.lumaUStride = p->rotate_luma_stride;
                    frameInfo.chromaUStride = p->rotate_chroma_u_stride;
                    frameInfo.chromaVStride = p->rotate_chroma_v_stride;
                }
                frameInfo.contentFrameRateN = fps;
                frameInfo.contentFrameRateD = 1;

                if (frameInfo.bufferFormat != 0 &&
                    frameInfo.bufferWidth >= frameInfo.contentWidth &&
                    frameInfo.bufferHeight >= frameInfo.contentHeight &&
                    frameInfo.contentWidth > 0 && frameInfo.contentHeight > 0 &&
                    frameInfo.lumaUStride > 0 &&
                    frameInfo.chromaUStride > 0 && frameInfo.chromaVStride > 0)
                {
                    extActive = true;
                }
                else {
                    LOGI("Payload cleared or inconsistent info, aborting extended mode");
                }
            }
        }

        if (!extActive)
        {
            memset(&frameInfo, 0, sizeof(frameInfo));
            frameInfo.frameType = HWC_FRAMETYPE_NOTHING;
        }

        if (mCurrentConfig.forceNotify || memcmp(&frameInfo, &mLastFrameInfo, sizeof(frameInfo)) != 0) {
            // something changed, notify type change listener
            mCurrentConfig.typeChangeListener->frameTypeChanged(frameInfo);
            mCurrentConfig.typeChangeListener->bufferInfoChanged(frameInfo);

            mExtLastTimestamp = 0;
            mExtLastKhandle = 0;

            if (frameInfo.frameType == HWC_FRAMETYPE_NOTHING) {
                mDisplayBufferCache.clear();
                ALOGI("Clone mode");
            }
            else {
                ALOGI("Extended mode: %dx%d in %dx%d @ %d fps",
                      frameInfo.contentWidth, frameInfo.contentHeight,
                      frameInfo.bufferWidth, frameInfo.bufferHeight,
                      frameInfo.contentFrameRateN);
            }
            mLastFrameInfo = frameInfo;
        }
    }

    if (extActive) {
        // tell surfaceflinger to not render the layers if we're
        // in extended video mode
        for (size_t i = 0; i < list->numHwLayers-1; i++) {
            hwc_layer_1_t& layer = list->hwLayers[i];
            if (layer.compositionType != HWC_BACKGROUND) {
                layer.compositionType = HWC_OVERLAY;
                layer.flags |= HWC_HINT_DISABLE_ANIMATION;
            }
        }
        if (mCurrentConfig.frameListener != NULL) {
            sp<CachedBuffer> cachedBuffer = getDisplayBuffer(videoFrame);
            if (cachedBuffer == NULL) {
                ALOGE("%s: Failed to map display buffer", __func__);
                return true;
            }
            intel_gralloc_payload_t *p = (intel_gralloc_payload_t*)cachedBuffer->displayBuffer->getCpuAddr();
            if (p == NULL) {
                ALOGE("%s: Got null payload from display buffer", __func__);
                return true;
            }
            int64_t timestamp = p->timestamp;
            uint32_t khandle = (p->metadata_transform == 0) ? p->khandle : p->rotated_buffer_handle;
            if (timestamp == mExtLastTimestamp && khandle == mExtLastKhandle)
                return true;

            mExtLastTimestamp = timestamp;
            mExtLastKhandle = khandle;

            if (mCurrentConfig.frameListener->onFrameReady(khandle, HWC_HANDLE_TYPE_KBUF, systemTime(), timestamp) == OK) {
                p->renderStatus = 1;
                Mutex::Autolock _l(mHeldBuffersLock);
                mHeldBuffers.add(khandle, cachedBuffer);
            }
        }
    }
    else {
        mExtendedModeInfo->widiExtHandle = NULL;
    }

    // handle hotplug event here
    if (mHotplugEvent) {
        ALOGI("%s: reset hotplug event flag", __func__);
        mHotplugEvent = false;
    }

    return true;
}

bool WidiDisplayDevice::commit(hwc_display_contents_1_t *list,
                                    buffer_handle_t *bh,
                                    int &numBuffers)
{
    ALOGD_IF(ALLOW_WIDI_PRINT, "%s", __func__);

    if (!initCheck()) {
        ALOGE("%s: failed to initialize HWComposer", __func__);
        return false;
    }

    return true;
}

bool WidiDisplayDevice::dump(char *buff,
                           int buff_len, int *cur_len)
{
    bool ret = true;

    mDumpBuf = buff;
    mDumpBuflen = buff_len;
    mDumpLen = (int)(*cur_len);

    *cur_len = mDumpLen;
    return ret;
}

void WidiDisplayDevice::onHotplugEvent(bool hpd)
{
    // overriding superclass and doing nothing
}

bool WidiDisplayDevice::getDisplayConfig(uint32_t* configs,
                                        size_t* numConfigs)
{
    if (!numConfigs || !numConfigs[0])
        return false;

    *numConfigs = 1;
    configs[0] = 0;

    return true;

}

bool WidiDisplayDevice::getDisplayAttributes(uint32_t config,
            const uint32_t* attributes, int32_t* values)
{
    ALOGD_IF(ALLOW_WIDI_PRINT, "%s", __func__);
    if (config != 0)
        return false;

    if (!attributes || !values)
        return false;

    while (*attributes != HWC_DISPLAY_NO_ATTRIBUTE) {
        switch (*attributes) {
        case HWC_DISPLAY_VSYNC_PERIOD:
            *values = 1e9 / 60;
            break;
        case HWC_DISPLAY_WIDTH:
            *values = 1280;
            break;
        case HWC_DISPLAY_HEIGHT:
            *values = 720;
            break;
        case HWC_DISPLAY_DPI_X:
            *values = 0;
            break;
        case HWC_DISPLAY_DPI_Y:
            *values = 0;
            break;
        default:
            break;
        }
        attributes ++;
        values ++;
    }

    return true;
}

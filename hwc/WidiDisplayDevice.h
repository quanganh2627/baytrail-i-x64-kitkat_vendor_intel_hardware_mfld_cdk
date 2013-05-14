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

#ifndef __WIDI_DISPLAY_DEVICE_H__
#define __WIDI_DISPLAY_DEVICE_H__

#include <utils/KeyedVector.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>

#include "IntelDisplayDevice.h"
#include "IFrameServer.h"

class WidiDisplayDevice : public IntelDisplayDevice, public BnFrameServer {
protected:
    struct CachedBuffer : public android::RefBase {
        CachedBuffer(IntelBufferManager *gbm, IntelDisplayBuffer* buffer);
        ~CachedBuffer();
        IntelBufferManager* grallocBufferManager;
        IntelDisplayBuffer* displayBuffer;
    };
    struct Configuration {
        android::sp<IFrameTypeChangeListener> typeChangeListener;
        android::sp<IFrameListener> frameListener;
        FrameProcessingPolicy policy;
        bool extendedModeEnabled;
        bool forceNotify;
    };
    android::Mutex mConfigLock;
    Configuration mCurrentConfig;
    Configuration mNextConfig;

    WidiExtendedModeInfo *mExtendedModeInfo;
    uint32_t mExtLastKhandle;
    int64_t mExtLastTimestamp;

    android::Mutex mListenerLock;
    FrameInfo mLastFrameInfo;

    android::KeyedVector<IMG_native_handle_t*, android::sp<CachedBuffer> > mDisplayBufferCache;
    android::Mutex mHeldBuffersLock;
    android::KeyedVector<uint32_t, android::sp<CachedBuffer> > mHeldBuffers;

private:
    android::sp<CachedBuffer> getDisplayBuffer(IMG_native_handle_t* handle);

public:
    WidiDisplayDevice(IntelBufferManager *bm,
                      IntelBufferManager *gm,
                      IntelDisplayPlaneManager *pm,
                      IntelHWComposerDrm *drm,
                      WidiExtendedModeInfo *extinfo,
                      uint32_t index);

    ~WidiDisplayDevice();

    // IFrameServer methods
    virtual android::status_t start(android::sp<IFrameTypeChangeListener> frameTypeChangeListener, bool disableExtVideoMode);
    virtual android::status_t stop(bool isConnected);
    virtual android::status_t notifyBufferReturned(int index);
    virtual android::status_t setResolution(const FrameProcessingPolicy& policy, android::sp<IFrameListener> listener);

    virtual bool prepare(hwc_display_contents_1_t *list);
    virtual bool commit(hwc_display_contents_1_t *list,
                        buffer_handle_t *bh, int &numBuffers);
    virtual bool dump(char *buff, int buff_len, int *cur_len);

    virtual void onHotplugEvent(bool hpd);

    virtual bool getDisplayConfig(uint32_t* configs, size_t* numConfigs);
    virtual bool getDisplayAttributes(uint32_t config,
            const uint32_t* attributes, int32_t* values);

};

#endif /*__WIDI_DISPLAY_DEVICE_H__*/

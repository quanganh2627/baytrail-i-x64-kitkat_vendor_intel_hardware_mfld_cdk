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
#ifndef __INTEL_HWCOMPOSER_CPP__
#define __INTEL_HWCOMPOSER_CPP__

#define HWC_REMOVE_DEPRECATED_VERSIONS 1

#include <EGL/egl.h>
#include <hardware/hwcomposer.h>

#include <IntelHWComposerDrm.h>
#include <IntelBufferManager.h>
#include <IntelHWComposerLayer.h>
#include <IntelHWComposerDump.h>
#include <IntelVsyncEventHandler.h>
#include <IntelFakeVsyncEvent.h>
#include <IntelDisplayDevice.h>
#ifdef INTEL_RGB_OVERLAY
#include <IntelHWCWrapper.h>
#endif

class IntelHWComposer : public hwc_composer_device_1_t, public IntelHWCUEventObserver, public IntelHWComposerDump  {
public:
    enum {
        VSYNC_SRC_MIPI = 0,
        VSYNC_SRC_HDMI,
        VSYNC_SRC_FAKE,
        VSYNC_SRC_NUM,
    };
    enum {
        PRIMARY_DISPLAY = 0,
        SECOND_DISPLAY = 1,
        DISPLAY_NUM = 2,
    };
private:
    IntelHWComposerDrm *mDrm;
    IntelBufferManager *mBufferManager;
    IntelBufferManager *mGrallocBufferManager;
    IntelDisplayPlaneManager *mPlaneManager;
    IntelDisplayDevice *mDisplayDevice[DISPLAY_NUM];
    hwc_procs_t const *mProcs;
    android::sp<IntelVsyncEventHandler> mVsync;
    android::sp<IntelFakeVsyncEvent> mFakeVsync;
    nsecs_t mLastVsync;

    struct hdmi_fb_handler {
        uint32_t umhandle;
        uint32_t kmhandle;
        uint32_t size;
    } mHDMIFBHandle;

    int* mWidiNativeWindow;
    android::Mutex mLock;
    IMG_framebuffer_device_public_t *mFBDev;
    bool mInitialized;
    uint32_t mActiveVsyncs;
    uint32_t mVsyncsEnabled;
    uint32_t mVsyncsCount;
    nsecs_t mVsyncsTimestamp;

    mutable Mutex mHpdLock;
    Condition mHpdCondition;
    bool mHpdCompletion;
#ifdef INTEL_RGB_OVERLAY
    IntelHWCWrapper mWrapper;
#endif
private:
    bool handleHotplugEvent(int hdp, void *data);
    bool handleDisplayModeChange();
    bool handleDynamicModeSetting(void *data);
    uint32_t disableUnusedVsyncs(uint32_t target);
    uint32_t enableVsyncs(uint32_t target);
    void signalHpdCompletion();
    void waitForHpdCompletion();
public:
    bool onUEvent(const char *msg, int msgLen, int msgType, void *data);
    void vsync(int64_t timestamp, int pipe);
public:
    bool initCheck() { return mInitialized; }
    bool initialize();
    bool vsyncControl(int enabled);
    bool release();
    bool dump(char *buff, int buff_len, int *cur_len);
    bool dumpDisplayStat();
    void registerProcs(hwc_procs_t const *procs) { mProcs = procs; }
#ifdef INTEL_RGB_OVERLAY
    IntelHWCWrapper* getWrapper() { return &mWrapper; }
#endif
    bool prepareDisplays(size_t numDisplays, hwc_display_contents_1_t** displays);
    bool commitDisplays(size_t numDisplays, hwc_display_contents_1_t** displays);
    bool blankDisplay(int disp, int blank);
    bool getDisplayConfigs(int disp, uint32_t* configs, size_t* numConfigs);
    bool getDisplayAttributes(int disp, uint32_t config,
            const uint32_t* attributes, int32_t* values);

    IntelHWComposer()
        : IntelHWCUEventObserver(), IntelHWComposerDump(),
          mDrm(0), mBufferManager(0), mGrallocBufferManager(0),
          mPlaneManager(0),mProcs(0), mVsync(0), mFakeVsync(0),
          mLastVsync(0), mInitialized(false),
          mActiveVsyncs(0), mHpdCompletion(true) {}
    ~IntelHWComposer();
};

#endif /*__INTEL_HWCOMPOSER_CPP__*/

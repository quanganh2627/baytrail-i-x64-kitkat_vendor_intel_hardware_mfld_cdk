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

#ifndef __INTEL_HWCOMPOSER_CPP__
#define __INTEL_HWCOMPOSER_CPP__

#include <EGL/egl.h>
#include <hardware/hwcomposer.h>

#include <IntelHWComposerDrm.h>
#include <IntelBufferManager.h>
#include <IntelHWComposerLayer.h>
#include <IntelHWComposerDump.h>

class IntelHWComposer : public hwc_composer_device_t, public IntelHWCUEventObserver, public IntelHWComposerDump  {
private:
    IntelHWComposerDrm *mDrm;
    IntelBufferManager *mBufferManager;
    IntelBufferManager *mGrallocBufferManager;
    IntelDisplayPlaneManager *mPlaneManager;
    IntelHWComposerLayerList *mLayerList;
    hwc_procs_t const *mProcs;
    int mMonitoringMethod;
    bool mForceSwapBuffer;
    bool mHotplugEvent;
    android::Mutex mLock;
    IMG_framebuffer_device_public_t *mFBDev;
    bool mInitialized;
private:
    void onGeometryChanged(hwc_layer_list_t *list);
    bool overlayPrepare(int index, hwc_layer_t *layer, int flags);
    bool spritePrepare(int index, hwc_layer_t *layer, int flags);
    bool isOverlayHandle(uint32_t handle);
    bool isSpriteHandle(uint32_t);
    bool isOverlayLayer(hwc_layer_list_t *list,
                        int index,
                        hwc_layer_t *layer,
                        int& flags);
    bool isSpriteLayer(hwc_layer_list_t *list,
                       int index,
                       hwc_layer_t *layer,
                       int& flags);
    bool useOverlayRotation(hwc_layer_t *layer, int index, uint32_t& handle,
                           int& w, int& h,
                           int& srcX, int& srcY, int& srcW, int& srcH, uint32_t& transform);
    bool updateLayersData(hwc_layer_list_t *list);
    bool isHWCUsage(int usage);
    bool isHWCFormat(int format);
    bool isHWCTransform(uint32_t transform);
    bool isHWCBlending(uint32_t blending);
    bool isHWCLayer(hwc_layer_t *layer);
    bool isBobDeinterlace(hwc_layer_t *layer);
    bool isForceOverlay(hwc_layer_t *layer);
    bool areLayersIntersecting(hwc_layer_t *top, hwc_layer_t* bottom);
    void handleHotplugEvent();
public:
    void onUEvent(const char *msg, int msgLen, int msgType);
public:
    bool initCheck() { return mInitialized; }
    bool initialize();
    bool prepare(hwc_layer_list_t *list);
    bool commit(hwc_display_t dpy, hwc_surface_t sur, hwc_layer_list_t *list);
    bool dump(char *buff, int buff_len, int *cur_len);
    void registerProcs(hwc_procs_t const *procs) { mProcs = procs; }

    IntelHWComposer()
        : IntelHWCUEventObserver(), IntelHWComposerDump(),
          mDrm(0), mBufferManager(0), mGrallocBufferManager(0),
          mPlaneManager(0), mLayerList(0), mProcs(0), mMonitoringMethod(0),
          mForceSwapBuffer(false), mHotplugEvent(false), mInitialized(false) {}
    ~IntelHWComposer();
};

#endif /*__INTEL_HWCOMPOSER_CPP__*/

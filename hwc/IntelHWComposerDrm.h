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
#ifndef __INTEL_OVERLAY_HAL_H__
#define __INTEL_OVERLAY_HAL_H__

#include <IntelBufferManager.h>
#include <IntelDisplayPlaneManager.h>
#include <psb_drm.h>
#include <pthread.h>
#include <pvr2d.h>

extern "C" {
#include "xf86drm.h"
}

#define PVR_DRM_DRIVER_NAME     "pvrsrvkm"

#define DRM_PSB_GTT_MAP         0x0F
#define DRM_PSB_GTT_UNMAP       0x10

#define DRM_MODE_CONNECTOR_MIPI 15

typedef enum {
    PVR_OVERLAY_VSYNC_INIT,
    PVR_OVERLAY_VSYNC_DONE,
    PVR_OVERLAY_VSYNC_PENDING,
} eVsyncState;

/**
 * Class: Overlay HAL implementation
 * This is a singleton implementation of hardware overlay.
 * this object will be shared between mutiple overlay control/data devices.
 * FIXME: overlayHAL should contact to the h/w to track the overlay h/w
 * state.
 */
class IntelHWComposerDrm
{
private:
    int mDrmFd;
    IntelBufferManager *mBufferManager;
    static IntelHWComposerDrm *mInstance;
private:
    IntelHWComposerDrm()
        : mDrmFd(-1), mBufferManager(0) {}
    IntelHWComposerDrm(const IntelHWComposerDrm&);
    bool drmInit();
    void drmDestroy();
public:
    ~IntelHWComposerDrm();
    static IntelHWComposerDrm& getInstance() {
        IntelHWComposerDrm *instance = mInstance;
        if (instance == 0) {
            instance = new IntelHWComposerDrm();
            mInstance = instance;
        }
        return *instance;
    }
    bool initialize(int bufferType);
    IntelBufferManager* getBufferManager() const {
        return mBufferManager;
    }
    bool detectDrmModeInfo(IntelOverlayContext& context);
    intel_overlay_mode_t drmModeChanged(IntelOverlayContext& context);
    int getDrmFd() const { return mDrmFd; }
};

#endif /*__INTEL_OVERLAY_HAL_H__*/

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
#ifndef __INTEL_HWCOMPOSER_DRM_H__
#define __INTEL_HWCOMPOSER_DRM_H__

#include <IntelBufferManager.h>
#include <IntelHWCUEventObserver.h>
#include <psb_drm.h>
#include <pthread.h>
#include <pvr2d.h>
#include <IntelExternalDisplayMonitor.h>

extern "C" {
#include "xf86drm.h"
#include "xf86drmMode.h"
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

typedef enum {
    OUTPUT_MIPI0 = 0,
    OUTPUT_HDMI,
    OUTPUT_MIPI1,
    OUTPUT_MAX,
} intel_drm_output_t;

typedef enum {
    OVERLAY_CLONE_MIPI0 = 0,
    OVERLAY_CLONE_MIPI1,
    OVERLAY_CLONE_DUAL,
    OVERLAY_EXTEND,
    OVERLAY_UNKNOWN,
} intel_overlay_mode_t;

typedef struct {
    drmModeConnection connections[OUTPUT_MAX];
    drmModeModeInfo modes[OUTPUT_MAX];
    drmModeFB fbInfos[OUTPUT_MAX];
    bool mode_valid[OUTPUT_MAX];
    intel_overlay_mode_t display_mode;
    intel_overlay_mode_t old_display_mode;
} intel_drm_output_state_t;


/**
 * Class: Overlay HAL implementation
 * This is a singleton implementation of hardware overlay.
 * this object will be shared between mutiple overlay control/data devices.
 * FIXME: overlayHAL should contact to the h/w to track the overlay h/w
 * state.
 */
class IntelHWComposerDrm {
private:
    int mDrmFd;
    intel_drm_output_state_t mDrmOutputsState;
    static IntelHWComposerDrm *mInstance;
    android::sp<IntelExternalDisplayMonitor> mMonitor;
private:
    IntelHWComposerDrm()
        : mDrmFd(-1), mMonitor(0) {
        memset(&mDrmOutputsState, 0, sizeof(intel_drm_output_state_t));
    }
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
    bool initialize(IntelHWComposer *hwc);
    bool detectDrmModeInfo();
    int getDrmFd() const { return mDrmFd; }

    // DRM output states
    void setOutputConnection(const int output, drmModeConnection connection);
    drmModeConnection getOutputConnection(const int output);
    void setOutputMode(const int output, drmModeModeInfoPtr mode, int valid);
    drmModeModeInfoPtr getOutputMode(const int output);
    void setOutputFBInfo(const int output, drmModeFBPtr fbInfo);
    drmModeFBPtr getOutputFBInfo(const int output);
    bool isValidOutputMode(const int output);
    void setDisplayMode(intel_overlay_mode_t displayMode);
    intel_overlay_mode_t getDisplayMode();
    intel_overlay_mode_t getOldDisplayMode();
    bool isVideoPlaying();
    bool isOverlayOff();
    bool notifyMipi(bool);
    bool notifyWidi(bool);
};

#endif /*__INTEL_HWCOMPOSER_DRM_H__*/

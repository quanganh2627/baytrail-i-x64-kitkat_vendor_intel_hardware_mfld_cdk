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

#include <IntelHWComposerDrm.h>
#include <IntelOverlayUtil.h>
#include <IntelOverlayHW.h>
#include <bufferclass_video_linux.h>
#include <fcntl.h>
#include <errno.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/ashmem.h>
#include <sys/mman.h>

IntelHWComposerDrm *IntelHWComposerDrm::mInstance(0);

IntelHWComposerDrm:: ~IntelHWComposerDrm() {
    LOGV("%s: destroying overlay HAL...\n", __func__);

    drmDestroy();
    mInstance = NULL;
}

bool IntelHWComposerDrm::drmInit()
{
    int fd = open("/dev/card0", O_RDWR, 0);
    if (fd < 0) {
        LOGE("%s: drmOpen failed. %s\n", __func__, strerror(errno));
        return false;
    }

    mDrmFd = fd;
    LOGD("%s: successfully. mDrmFd %d\n", __func__, fd);
    return true;
}

void IntelHWComposerDrm::drmDestroy()
{
    LOGV("%s: destroying...\n", __func__);

    if(mDrmFd > 0) {
        drmClose(mDrmFd);
        mDrmFd = -1;
    }
}

bool IntelHWComposerDrm::initialize(int bufferType)
{
    bool ret = false;

    LOGV("%s: init...\n", __func__);

    if (mDrmFd < 0) {
        ret = drmInit();
        if(ret == false) {
            LOGE("%s: drmInit failed\n", __func__);
            return ret;
        }
    }

    /*create a buffer manager and initialize it*/
    IntelBufferManager *bufferManager;
    if (bufferType == IntelBufferManager::TTM_BUFFER)
	    bufferManager = new IntelTTMBufferManager(mDrmFd);
    else if (bufferType == IntelBufferManager::PVR_BUFFER)
	    bufferManager = new IntelPVRBufferManager(mDrmFd);
    else {
        LOGE("%s: invalid buffer type\n", __func__);
        drmDestroy();
        return false;
    }

    ret = bufferManager->initialize();
    if (ret == false) {
        LOGE("%s: failed to initialize", __func__);
        drmDestroy();
        return false;
    }

    mBufferManager = bufferManager;

    LOGV("%s: finish successfully.\n", __func__);
    return true;
}

void IntelHWComposerDrm::drmModeChanged(IntelOverlayContext& context)
{
    intel_overlay_mode_t oldDisplayMode;
    intel_overlay_mode_t newDisplayMode;
    struct drm_psb_register_rw_arg arg;
    uint32_t overlayAPipe = 0;
    bool ret = true;

    oldDisplayMode = context.getDisplayMode();

    /*detect new drm mode*/
    ret = detectDrmModeInfo(context);
    if (ret == false) {
        LOGE("%s: failed to detect DRM mode\n", __func__);
        goto mode_change_done;
    }

    /*get new drm mode*/
    newDisplayMode = context.getDisplayMode();

    LOGV("%s: old %d, new %d\n", __func__, oldDisplayMode, newDisplayMode);

    if (oldDisplayMode == newDisplayMode) {
        goto mode_change_done;
    }

    /*disable overlay*/
    context.setPipeByMode(oldDisplayMode);
    context.disable();
    /*switch pipe*/
    context.setPipeByMode(newDisplayMode);
mode_change_done:
    return;
}

bool IntelHWComposerDrm::detectDrmModeInfo(IntelOverlayContext& context)
{
    LOGV("%s: detecting drm mode info...\n", __func__);

    if (mDrmFd < 0) {
        LOGE("%s: invalid drm FD\n", __func__);
        return false;
    }

    /*try to get drm resources*/
    drmModeResPtr resources = drmModeGetResources(mDrmFd);
    if (!resources) {
        LOGE("%s: fail to get drm resources. %s\n", __func__, strerror(errno));
        return false;
    }

    /*get mipi0 info*/
    drmModeConnectorPtr connector = NULL;
    drmModeEncoderPtr encoder = NULL;
    drmModeCrtcPtr crtc = NULL;
    drmModeConnectorPtr connectors[OUTPUT_MAX];
    drmModeModeInfoPtr mode = NULL;

    for (int i = 0; i < resources->count_connectors; i++) {
        connector = drmModeGetConnector(mDrmFd, resources->connectors[i]);
        if (!connector) {
            LOGW("%s: fail to get drm connector\n", __func__);
            continue;
        }

        int outputIndex = -1;

        if (connector->connector_type == DRM_MODE_CONNECTOR_MIPI ||
            connector->connector_type == DRM_MODE_CONNECTOR_LVDS) {
            LOGV("%s: got MIPI/LVDS connector\n", __func__);
            if (connector->connector_type_id == 1)
                outputIndex = OUTPUT_MIPI0;
            else if (connector->connector_type_id == 2)
        	outputIndex = OUTPUT_MIPI1;
            else {
                LOGW("%s: unknown connector type\n", __func__);
                outputIndex = OUTPUT_MIPI0;
            }
        } else if (connector->connector_type == DRM_MODE_CONNECTOR_DVID) {
            LOGV("%s: got HDMI connector\n", __func__);
            outputIndex = OUTPUT_HDMI;
        }

        /*update connection status*/
        context.setOutputConnection(outputIndex, connector->connection);

        /*get related encoder*/
        encoder = drmModeGetEncoder(mDrmFd, connector->encoder_id);
        if (!encoder) {
            LOGV("%s: fail to get drm encoder\n", __func__);
            drmModeFreeConnector(connector);
            continue;
        }

        /*get related crtc*/
        crtc = drmModeGetCrtc(mDrmFd, encoder->crtc_id);
        if (!crtc) {
            LOGV("%s: fail to get drm crtc\n", __func__);
            drmModeFreeEncoder(encoder);
            drmModeFreeConnector(connector);
            continue;
        }

        /*set crtc mode*/
        context.setOutputMode(outputIndex, &crtc->mode, crtc->mode_valid);

        /*free all crtc/connector/encoder*/
        drmModeFreeCrtc(crtc);
        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
    }

    drmModeFreeResources(resources);

    drmModeConnection mipi0 =
        context.getOutputConnection(OUTPUT_MIPI0);
    drmModeConnection mipi1 =
        context.getOutputConnection(OUTPUT_MIPI1);
    drmModeConnection hdmi =
        context.getOutputConnection(OUTPUT_HDMI);

    if (hdmi == DRM_MODE_CONNECTED)
        context.setDisplayMode(OVERLAY_EXTEND);
    else if ((mipi0 == DRM_MODE_CONNECTED) && (mipi1 == mipi0))
        context.setDisplayMode(OVERLAY_CLONE_DUAL);
    else if (mipi0 == DRM_MODE_CONNECTED)
        context.setDisplayMode(OVERLAY_CLONE_MIPI0);
    else if (mipi1 == DRM_MODE_CONNECTED)
        context.setDisplayMode(OVERLAY_CLONE_MIPI1);
    else {
        LOGW("%s: unknown display mode\n", __func__);
        context.setDisplayMode(OVERLAY_UNKNOWN);
    }

    LOGV("%s: mipi/lvds %s, mipi1 %s, hdmi %s\n",
        __func__,
        ((mipi0 == DRM_MODE_CONNECTED) ? "connected" : "disconnected"),
        ((mipi1 == DRM_MODE_CONNECTED) ? "connected" : "disconnected"),
        ((hdmi == DRM_MODE_CONNECTED) ? "connected" : "disconnected"));

    return true;
}

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
#include <display/MultiDisplayType.h>

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

void IntelHWComposerDrm::setOutputConnection(const int output,
                                             drmModeConnection connection)
{
    if (output < 0 || output >= OUTPUT_MAX)
        return;

    mDrmOutputsState.connections[output] = connection;
}

drmModeConnection
IntelHWComposerDrm:: getOutputConnection(const int output)
{
    drmModeConnection connection = DRM_MODE_DISCONNECTED;

    if (output < 0 || output >= OUTPUT_MAX)
        return connection;

    connection = mDrmOutputsState.connections[output];

    return connection;
}

void IntelHWComposerDrm::setOutputMode(const int output,
                                       drmModeModeInfoPtr mode,
                                       int valid)
{
    if (output < 0 || output >= OUTPUT_MAX || !mode)
        return;

    if (valid) {
        memcpy(&mDrmOutputsState.modes[output],
               mode, sizeof(drmModeModeInfo));
        mDrmOutputsState.mode_valid[output] = true;
    } else
        mDrmOutputsState.mode_valid[output] = false;
}

drmModeModeInfoPtr IntelHWComposerDrm::getOutputMode(const int output)
{
    if (output < 0 || output >= OUTPUT_MAX)
        return 0;

    return &mDrmOutputsState.modes[output];
}

void IntelHWComposerDrm::setOutputFBInfo(const int output,
                                       drmModeFBPtr fbInfo)
{
    if (output < 0 || output >= OUTPUT_MAX || !fbInfo)
        return;

    memcpy(&mDrmOutputsState.fbInfos[output], fbInfo, sizeof(drmModeFB));
}

drmModeFBPtr IntelHWComposerDrm::getOutputFBInfo(const int output)
{
    if (output < 0 || output >= OUTPUT_MAX)
        return 0;

    return &mDrmOutputsState.fbInfos[output];
}

bool IntelHWComposerDrm::isValidOutputMode(const int output)
{
    if (output < 0 || output >= OUTPUT_MAX)
        return false;

    return mDrmOutputsState.mode_valid[output];
}

void IntelHWComposerDrm::setDisplayMode(intel_overlay_mode_t displayMode)
{
    mDrmOutputsState.old_display_mode = mDrmOutputsState.display_mode;
    mDrmOutputsState.display_mode = displayMode;
}

intel_overlay_mode_t IntelHWComposerDrm::getDisplayMode()
{
    intel_overlay_mode_t displayMode = OVERLAY_UNKNOWN;

    displayMode = mDrmOutputsState.display_mode;

    return displayMode;
}

bool IntelHWComposerDrm::isVideoPlaying()
{
    if (mMonitor != NULL)
        return mMonitor->isVideoPlaying();
    return false;
}

bool IntelHWComposerDrm::isOverlayOff()
{
    if (mMonitor != NULL)
        return mMonitor->isOverlayOff();
    return false;
}

bool IntelHWComposerDrm::notifyWidi(bool on)
{
    if (mMonitor != NULL)
        return mMonitor->notifyWidi(on);
    return false;
}

bool IntelHWComposerDrm::notifyMipi(bool on)
{
    if (mMonitor != NULL)
        return mMonitor->notifyMipi(on);
    return false;
}

bool IntelHWComposerDrm::getVideoInfo(int *displayW, int *displayH, int *fps, int *isinterlace)
{
    if (mMonitor != NULL)
        return mMonitor->getVideoInfo(displayW, displayH, fps, isinterlace);
    return false;
}

intel_overlay_mode_t IntelHWComposerDrm::getOldDisplayMode()
{
    intel_overlay_mode_t displayMode = OVERLAY_UNKNOWN;

    displayMode = mDrmOutputsState.old_display_mode;

    return displayMode;
}

bool IntelHWComposerDrm::initialize(IntelHWComposer *hwc)
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

    // create external display monitor
    mMonitor = new IntelExternalDisplayMonitor(hwc);
    if (mMonitor == 0) {
        LOGE("%s: failed to create external display monitor\n", __func__);
        drmDestroy();
        return false;
    }

    // detect display mode
    ret = detectDrmModeInfo();
    if (ret == false)
        LOGW("%s: failed to detect DRM modes\n", __func__);
    else
        LOGV("%s: finish successfully.\n", __func__);

    // set old display mode the same detect mode
    mDrmOutputsState.old_display_mode =  mDrmOutputsState.display_mode;
    return true;
}
bool IntelHWComposerDrm::detectDrmModeInfo()
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
    drmModeFBPtr fbInfo = NULL;

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
        setOutputConnection(outputIndex, connector->connection);

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
        setOutputMode(outputIndex, &crtc->mode, crtc->mode_valid);

        // get fb info
        fbInfo = drmModeGetFB(mDrmFd, crtc->buffer_id);
        if (!fbInfo) {
            LOGD("%s: fail to get fb info\n", __func__);
            drmModeFreeCrtc(crtc);
            drmModeFreeEncoder(encoder);
            drmModeFreeConnector(connector);
            continue;
        }

        setOutputFBInfo(outputIndex, fbInfo);

        /*free all crtc/connector/encoder*/
        drmModeFreeFB(fbInfo);
        drmModeFreeCrtc(crtc);
        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
    }

    drmModeFreeResources(resources);

    drmModeConnection mipi0 = getOutputConnection(OUTPUT_MIPI0);
    drmModeConnection mipi1 = getOutputConnection(OUTPUT_MIPI1);
    drmModeConnection hdmi = getOutputConnection(OUTPUT_HDMI);

    int mdsMode = IntelExternalDisplayMonitor::INVALID_MDS_MODE;
    if (mMonitor != 0)
        mdsMode = mMonitor->getDisplayMode();

    if (mdsMode != IntelExternalDisplayMonitor::INVALID_MDS_MODE) {
        setDisplayMode((intel_overlay_mode_t)mdsMode);
    } else {
        if (hdmi == DRM_MODE_CONNECTED)
            setDisplayMode(OVERLAY_EXTEND);
        else if ((mipi0 == DRM_MODE_CONNECTED) && (mipi1 == mipi0))
            setDisplayMode(OVERLAY_CLONE_DUAL);
        else if (mipi0 == DRM_MODE_CONNECTED)
            setDisplayMode(OVERLAY_CLONE_MIPI0);
        else if (mipi1 == DRM_MODE_CONNECTED)
            setDisplayMode(OVERLAY_CLONE_MIPI1);
        else {
            LOGW("%s: unknown display mode\n", __func__);
            setDisplayMode(OVERLAY_UNKNOWN);
        }
    }

    LOGV("%s: mipi/lvds %s, mipi1 %s, hdmi %s, displayMode %d\n",
        __func__,
        ((mipi0 == DRM_MODE_CONNECTED) ? "connected" : "disconnected"),
        ((mipi1 == DRM_MODE_CONNECTED) ? "connected" : "disconnected"),
        ((hdmi == DRM_MODE_CONNECTED) ? "connected" : "disconnected"),
        getDisplayMode());

    return true;
}

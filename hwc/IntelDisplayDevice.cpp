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
#include <cutils/log.h>
#include <cutils/atomic.h>

#include <IntelHWComposer.h>
#include <IntelDisplayDevice.h>
#include <IntelOverlayUtil.h>
#include <IntelHWComposerCfg.h>

IntelDisplayDevice::IntelDisplayDevice(IntelDisplayPlaneManager *pm,
                                IntelHWComposerDrm *drm,
                                IntelBufferManager *bm,
                                IntelBufferManager *gm,
                                uint32_t index)
       :  IntelHWComposerDump(),
          mPlaneManager(pm), mDrm(drm), mBufferManager(bm),
          mGrallocBufferManager(gm), mLayerList(0),
          mDisplayIndex(index), mForceSwapBuffer(false),
          mHotplugEvent(false), mIsConnected(false),
          mInitialized(false), mIsScreenshotActive(false),
          mIsBlank(false)
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);
}

IntelDisplayDevice::IntelDisplayDevice::~IntelDisplayDevice()
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);
}


int IntelDisplayDevice::getMetaDataTransform(hwc_layer_1_t *layer,
        uint32_t &transform) {
    if (!layer || !mGrallocBufferManager)
        return -1;

    intel_gralloc_buffer_handle_t *grallocHandle =
        (intel_gralloc_buffer_handle_t*)layer->handle;
    if(!grallocHandle) {
        ALOGE("%s: gralloc handle invalid.\n", __func__);
        return -1;
    }

    if (grallocHandle->format != HAL_PIXEL_FORMAT_INTEL_HWC_NV12_VED &&
        grallocHandle->format != HAL_PIXEL_FORMAT_INTEL_HWC_NV12_TILE) {
        ALOGV("%s: SW decoder, ignore this checking.", __func__);
        return 0;
    }

    IntelDisplayBuffer *buffer =
        mGrallocBufferManager->map(grallocHandle->fd[GRALLOC_SUB_BUFFER1]);
    if (!buffer) {
        ALOGE("%s: failed to map payload buffer.\n", __func__);
        return -1;
    }

    intel_gralloc_payload_t *payload =
        (intel_gralloc_payload_t*)buffer->getCpuAddr();
    if (!payload) {
        ALOGE("%s: invalid address\n", __func__);
        return -1;
    }

    transform = payload->metadata_transform;

    // unmap payload buffer
    mGrallocBufferManager->unmap(buffer);

    return 0;
}

bool IntelDisplayDevice::isVideoPutInWindow(int output, hwc_layer_1_t *layer) {
    bool inWindow = false;

    if (mDrm == NULL) {
        ALOGE("mDrm is NULL!");
        return false;
    }

    drmModeFBPtr fbInfo = mDrm->getOutputFBInfo(output);
    if (fbInfo == NULL) {
        ALOGE("Get FB info failed!");
        return false;
    }

    int fbW = fbInfo->width;
    int fbH = fbInfo->height;

    int srcWidth = layer->sourceCrop.right - layer->sourceCrop.left;
    int srcHeight = layer->sourceCrop.bottom - layer->sourceCrop.top;
    uint32_t metadata_transform = 0;
    if (getMetaDataTransform(layer, metadata_transform) == -1) {
        ALOGE("Get meta data transform failed!");
        return false;
    }

    if (metadata_transform == HAL_TRANSFORM_ROT_90 ||
            metadata_transform == HAL_TRANSFORM_ROT_270) {
        int temp;
        temp = srcWidth; srcWidth = srcHeight; srcHeight = temp;
    }

    int dstWidth = layer->displayFrame.right - layer->displayFrame.left;
    int dstHeight = layer->displayFrame.bottom - layer->displayFrame.top;

    LOGD_IF(ALLOW_HWC_PRINT,
            "output:%d fbW:%d fbH:%d dst_w:%d dst_h:%d src_w:%d src_h:%d",
            output, fbW, fbH, dstWidth, dstHeight, srcWidth, srcHeight);
    /*
     * Check if the app put the video to a window
     * 1) Both dst width and height are smaller than FB w/h
     * 2) For device(e.g. phone), fbW < fbH:
     check if it is back to portait mode.
     * 3) For device(e.g. tablet, hdtv), fbW > fbH:
     check if it is back to portait mode.
     */
    if (dstWidth < fbW && dstHeight < fbH) {
        inWindow = true;
    } else if (fbW < fbH) {
        if ((dstWidth > dstHeight && srcWidth >= srcHeight) ||
            (dstWidth < dstHeight && srcWidth <= srcHeight))
            inWindow = true;
    } else if (fbW > fbH) {
        if ((dstWidth > dstHeight && srcWidth <= srcHeight) ||
            (dstWidth < dstHeight && srcWidth >= srcHeight))
            inWindow = true;
    }

    return inWindow;
}

bool IntelDisplayDevice::overlayPrepare(int index, hwc_layer_1_t *layer, int flags)
{
    if (!layer) {
        ALOGE("%s: Invalid layer\n", __func__);
        return false;
    }

    // allocate overlay plane
    IntelDisplayPlane *plane = mPlaneManager->getOverlayPlane();
    if (!plane) {
        ALOGE("%s: failed to create overlay plane\n", __func__);
        return false;
    }

    int dstLeft = layer->displayFrame.left;
    int dstTop = layer->displayFrame.top;
    int dstRight = layer->displayFrame.right;
    int dstBottom = layer->displayFrame.bottom;

    IntelOverlayPlane *overlayP =
        reinterpret_cast<IntelOverlayPlane *>(plane);
    if (mDrm->getDisplayMode() == OVERLAY_EXTEND) {
        // Put overlay on top if no other layers exist
        overlayP->setOverlayOnTop(mLayerList->getLayersCount() == 1);

        // Check if the video is placed to a window
        if (isVideoPutInWindow(OUTPUT_MIPI0, layer)) {
            overlayP->setOverlayOnTop(true);

            struct drm_psb_disp_ctrl dp_ctrl;
            memset(&dp_ctrl, 0, sizeof(dp_ctrl));
            dp_ctrl.cmd = DRM_PSB_DISP_PLANEB_DISABLE;
            drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_HDMI_FB_CMD, &dp_ctrl, sizeof(dp_ctrl));
        }
    }

    // setup plane parameters
    overlayP->setPosition(dstLeft, dstTop, dstRight, dstBottom);

    overlayP->setPipeByMode(mDrm->getDisplayMode());

    // attach plane to hwc layer
    mLayerList->attachPlane(index, overlayP, flags);

    return true;
}

bool IntelDisplayDevice::rgbOverlayPrepare(int index,
                                            hwc_layer_1_t *layer, int flags)
{
    if (!layer) {
        LOGE("%s: Invalid layer\n", __func__);
        return false;
    }

    // allocate overlay plane
    IntelDisplayPlane *plane = mPlaneManager->getRGBOverlayPlane();
    if (!plane) {
        LOGE("%s: failed to create RGB overlay plane\n", __func__);
        return false;
    }

    int dstLeft = layer->displayFrame.left;
    int dstTop = layer->displayFrame.top;
    int dstRight = layer->displayFrame.right;
    int dstBottom = layer->displayFrame.bottom;

    // setup plane parameters
    plane->setPosition(dstLeft, dstTop, dstRight, dstBottom);

    // always assign RGB overlays to MIPI
    plane->setPipeByMode(OVERLAY_MIPI0);

    // attach plane to hwc layer
    mLayerList->attachPlane(index, plane, flags);

    return true;
}

bool IntelDisplayDevice::spritePrepare(int index, hwc_layer_1_t *layer, int flags)
{
    if (!layer) {
        ALOGE("%s: Invalid layer\n", __func__);
        return false;
    }

    // allocate sprite plane
    IntelDisplayPlane *plane = mPlaneManager->getSpritePlane();
    if (!plane) {
        ALOGE("%s: failed to create sprite plane\n", __func__);
        return false;
    }

    int dstLeft = layer->displayFrame.left;
    int dstTop = layer->displayFrame.top;
    int dstRight = layer->displayFrame.right;
    int dstBottom = layer->displayFrame.bottom;

    // setup plane parameters
    plane->setPosition(dstLeft, dstTop, dstRight, dstBottom);

    // attach plane to hwc layer
    mLayerList->attachPlane(index, plane, flags);

    return true;
}

bool IntelDisplayDevice::primaryPrepare(int index, hwc_layer_1_t *layer, int flags)
{
    if (!layer) {
        ALOGE("%s: Invalid layer\n", __func__);
        return false;
    }
    int dstLeft = layer->displayFrame.left;
    int dstTop = layer->displayFrame.top;
    int dstRight = layer->displayFrame.right;
    int dstBottom = layer->displayFrame.bottom;

    // allocate sprite plane
    IntelDisplayPlane *plane = mPlaneManager->getPrimaryPlane(mDisplayIndex);
    if (!plane) {
        ALOGE("%s: failed to create sprite plane\n", __func__);
        return false;
    }

    // TODO: check external display status, and attach plane

    // setup plane parameters
    plane->setPosition(dstLeft, dstTop, dstRight, dstBottom);

    // attach plane to hwc layer
    mLayerList->attachPlane(index, plane, flags);

    return true;
}

// TODO: re-implement it for each device
bool IntelDisplayDevice::isOverlayLayer(hwc_display_contents_1_t *list,
                                     int index,
                                     hwc_layer_1_t *layer,
                                     int& flags)
{
    return false;
}

// TODO: re-implement it for each device
bool IntelDisplayDevice::isRGBOverlayLayer(hwc_display_contents_1_t *list,
                                           int index,
                                           hwc_layer_1_t *layer,
                                           int& flags)
{
    return false;
}

// TODO: re-implement it for each device
bool IntelDisplayDevice::isSpriteLayer(hwc_display_contents_1_t *list,
                                    int index,
                                    hwc_layer_1_t *layer,
                                    int& flags)
{
   return false;
}

// TODO: re-implement it for each device
bool IntelDisplayDevice::isPrimaryLayer(hwc_display_contents_1_t *list,
                                     int index,
                                     hwc_layer_1_t *layer,
                                     int& flags)
{
   return false;
}

void IntelDisplayDevice::dumpLayerList(hwc_display_contents_1_t *list)
{
    if (!list)
        return;

    ALOGD("\n");
    ALOGD("DUMP LAYER LIST START");
    ALOGD("num of layers: %d", list->numHwLayers);
    for (size_t i = 0; i < list->numHwLayers; i++) {
        int srcLeft = list->hwLayers[i].sourceCrop.left;
        int srcTop = list->hwLayers[i].sourceCrop.top;
        int srcRight = list->hwLayers[i].sourceCrop.right;
        int srcBottom = list->hwLayers[i].sourceCrop.bottom;

        int dstLeft = list->hwLayers[i].displayFrame.left;
        int dstTop = list->hwLayers[i].displayFrame.top;
        int dstRight = list->hwLayers[i].displayFrame.right;
        int dstBottom = list->hwLayers[i].displayFrame.bottom;

        ALOGD("Layer type: %s",
        (mLayerList->getLayerType(i) != IntelHWComposerLayer::LAYER_TYPE_YUV) ?
        "RGB" : "YUV");
        ALOGD("Layer blending: 0x%x", list->hwLayers[i].blending);
        ALOGD("Layer source: (%d, %d) - (%dx%d)", srcLeft, srcTop,
             srcRight - srcLeft, srcBottom - srcTop);
        ALOGD("Layer positon: (%d, %d) - (%dx%d)", dstLeft, dstTop,
             dstRight - dstLeft, dstBottom - dstTop);
        ALOGD("Layer transform: 0x%x\n", list->hwLayers[i].transform);
        ALOGD("Layer handle: 0x%x\n", (uint32_t)list->hwLayers[i].handle);
        ALOGD("Layer flags: 0x%x\n", list->hwLayers[i].flags);
        ALOGD("Layer compositionType: 0x%x\n", list->hwLayers[i].compositionType);
        ALOGD("\n");
    }
    ALOGD("DUMP LAYER LIST END");
    ALOGD("\n");
}

bool IntelDisplayDevice::isScreenshotActive(hwc_display_contents_1_t *list)
{
    mIsScreenshotActive = false;

    if (!list || !list->numHwLayers)
        return false;

    // no layer in list except framebuffer target
    if (mLayerList->getLayersCount() <= 0)
        return false;

    // Bypass ext video mode
    if (mDrm->getDisplayMode() == OVERLAY_EXTEND)
        return false;

    // bypass protected video
    for (size_t i = 0; i < (size_t)mLayerList->getLayersCount(); i++) {
        if (mLayerList->isProtectedLayer(i))
            return false;
    }

    // check topLayer before getting its size
    hwc_layer_1_t *topLayer = &list->hwLayers[mLayerList->getLayersCount()-1];
    if (!topLayer || !(topLayer->flags & HWC_SKIP_LAYER))
        return false;

    int x = topLayer->displayFrame.left;
    int y = topLayer->displayFrame.top;
    int w = topLayer->displayFrame.right - topLayer->displayFrame.left;
    int h = topLayer->displayFrame.bottom - topLayer->displayFrame.top;

    drmModeFBPtr fbInfo =
        IntelHWComposerDrm::getInstance().getOutputFBInfo(mDisplayIndex);

    if (!fbInfo) {
        ALOGE("Invalid fbINfo\n");
        return false;
    }

    if (x == 0 && y == 0 &&
        w == int(fbInfo->width) && h == int(fbInfo->height)) {
        mIsScreenshotActive = true;
        return true;
    }

    return false;
}

// Check the usage of a buffer
// only following usages were accepted:
// TODO: list valid usages
bool IntelDisplayDevice::isHWCUsage(int usage)
{
#if 0
    // For SW access buffer, should handle with
    // FB in order to avoid tearing
    if ((usage & GRALLOC_USAGE_SW_WRITE_OFTEN) &&
         (usage & GRALLOC_USAGE_SW_READ_OFTEN))
        return false;
#endif

    if (!(usage & GRALLOC_USAGE_HW_COMPOSER))
        return false;

    return true;
}

// Check the data format of a buffer
// only following formats were accepted:
// TODO: list valid formats
bool IntelDisplayDevice::isHWCFormat(int format)
{
    return true;
}

// Check the transform of a layer
// we can only support 180 degree rotation
// TODO: list all supported transforms
// FIXME: for video playback we need to ignore this,
// since video decoding engine will take care of rotation.
bool IntelDisplayDevice::isHWCTransform(uint32_t transform)
{
    return false;
}

// Check the blending of a layer
// Currently, no blending support
bool IntelDisplayDevice::isHWCBlending(uint32_t blending)
{
    return false;
}

// Check whether a layer can be handle by our HWC
// if HWC_SKIP_LAYER was set, skip this layer
// TODO: add more
bool IntelDisplayDevice::isHWCLayer(hwc_layer_1_t *layer)
{
    if (!layer)
        return false;

    // if (layer->flags & HWC_SKIP_LAYER)
    //    return false;

    // bypass HWC_FRAMEBUFFER_TARGET
    if (layer->compositionType == HWC_FRAMEBUFFER_TARGET)
        return false;

    // check transform
    // if (!isHWCTransform(layer->transform))
    //    return false;

    // check blending
    // if (!isHWCBlending(layer->blending))
    //    return false;

    intel_gralloc_buffer_handle_t *grallocHandle =
        (intel_gralloc_buffer_handle_t*)layer->handle;

    if (!grallocHandle)
        return false;

    // check buffer usage
    if (!isHWCUsage(grallocHandle->usage))
        return false;

    // check format
    // if (!isHWCFormat(grallocHandle->format))
    //    return false;

    return true;
}

bool IntelDisplayDevice::isLayerSandwiched(int index,
                                           hwc_display_contents_1_t *list)
{
    return false;
}

// Check whehter two layers are intersect
bool IntelDisplayDevice::areLayersIntersecting(hwc_layer_1_t *top,
                                            hwc_layer_1_t* bottom)
{
    if (!top || !bottom)
        return false;

    hwc_rect_t *topRect = &top->displayFrame;
    hwc_rect_t *bottomRect = &bottom->displayFrame;

    if (bottomRect->right <= topRect->left ||
        bottomRect->left >= topRect->right ||
        bottomRect->top >= topRect->bottom ||
        bottomRect->bottom <= topRect->top)
        return false;

    return true;
}

bool IntelDisplayDevice::release()
{
    ALOGD("release");

    if (!initCheck() || !mLayerList)
        return false;

    // disable all attached planes
    for (int i=0 ; i<mLayerList->getLayersCount() ; i++) {
        IntelDisplayPlane *plane = mLayerList->getPlane(i);

        if (plane) {
            // disable all attached planes
            plane->disable();
            plane->waitForFlipCompletion();

            // release all data buffers
            plane->invalidateDataBuffer();
        }
    }

    return true;
}

bool IntelDisplayDevice::dump(char *buff,
                           int buff_len, int *cur_len)
{
    return true;
}

void IntelDisplayDevice::onHotplugEvent(bool hpd)
{
    // go through layer list and call plane's onModeChange()
    for (int i = 0 ; i < mLayerList->getLayersCount(); i++) {
        IntelDisplayPlane *plane = mLayerList->getPlane(i);
        if (plane)
            plane->onDrmModeChange();
    }

    mHotplugEvent = hpd;
    if (hpd && mDrm->isOverlayOff()) {
        if (mPlaneManager->hasReclaimedOverlays())
            mPlaneManager->disableReclaimedPlanes(IntelDisplayPlane::DISPLAY_PLANE_OVERLAY);
    }
}

bool IntelDisplayDevice::blank(int blank)
{
    bool ret=false;

    if (blank == 1)
        mIsBlank = true;
    else
        mIsBlank = false;

    if (mDrm)
        ret = mDrm->setDisplayDpms(mDisplayIndex, blank);

    return ret;
}

bool IntelDisplayDevice::getDisplayConfig(uint32_t* configs,
                                        size_t* numConfigs)
{
    return false;
}

bool IntelDisplayDevice::getDisplayAttributes(uint32_t config,
            const uint32_t *attributes, int32_t* values)
{
    return false;
}

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
#include <IntelOverlayUtil.h>
#include <IntelWidiPlane.h>

//#define INTEL_EXT_SF_NEED_SWAPBUFFER
//#define INTEL_EXT_SF_ANIMATION_HINT

#include <IntelHWComposerCfg.h>

IntelHWComposer::~IntelHWComposer()
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);

    delete mLayerList;
    delete mPlaneManager;
    delete mBufferManager;
    delete mDrm;

    // stop uevent observer
    stopObserver();
}

bool IntelHWComposer::overlayPrepare(int index, hwc_layer_1_t *layer, int flags)
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

    // setup plane parameters
    plane->setPosition(dstLeft, dstTop, dstRight, dstBottom);

    plane->setPipeByMode(mDrm->getDisplayMode());

    // attach plane to hwc layer
    mLayerList->attachPlane(index, plane, flags);

    return true;
}

bool IntelHWComposer::spritePrepare(int index, hwc_layer_1_t *layer, int flags)
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

bool IntelHWComposer::primaryPrepare(int index, hwc_layer_1_t *layer, int flags)
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
    IntelDisplayPlane *plane = mPlaneManager->getPrimaryPlane(0);
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

bool IntelHWComposer::isForceOverlay(hwc_layer_1_t *layer)
{
    if (!layer)
        return false;

    intel_gralloc_buffer_handle_t *grallocHandle =
        (intel_gralloc_buffer_handle_t*)layer->handle;

    if (!grallocHandle)
        return false;

    if (grallocHandle->format != HAL_PIXEL_FORMAT_INTEL_HWC_NV12)
        return false;

    // map payload buffer
    IntelPayloadBuffer buffer(mGrallocBufferManager, grallocHandle->fd[GRALLOC_SUB_BUFFER1]);

    intel_gralloc_payload_t *payload =
        (intel_gralloc_payload_t*)buffer.getCpuAddr();
    if (!payload) {
        LOGE("%s: invalid address\n", __func__);
        return false;
    }

    bool ret = (payload->force_output_method == OUTPUT_FORCE_OVERLAY) ? true : false;
    return ret;
}

// TODO: re-implement this function after video interface
// is ready.
// Currently, LayerTS::setGeometry will set compositionType
// to HWC_OVERLAY. HWC will change it to HWC_FRAMEBUFFER
// if HWC found this layer was NOT a overlay layer (can NOT
// be handled by hardware overlay)
bool IntelHWComposer::isOverlayLayer(hwc_display_contents_1_t *list,
                                     int index,
                                     hwc_layer_1_t *layer,
                                     int& flags)
{
    bool needClearFb = false;
    bool forceOverlay = false;
    bool useOverlay = false;

    if (!list || !layer)
        return false;

    intel_gralloc_buffer_handle_t *grallocHandle =
        (intel_gralloc_buffer_handle_t*)layer->handle;

    if (!grallocHandle)
        return false;

    IntelWidiPlane* widiPlane = (IntelWidiPlane*)mPlaneManager->getWidiPlane();

    // clear hints
    layer->hints = 0;

    // check format
    if (mLayerList->getLayerType(index) != IntelHWComposerLayer::LAYER_TYPE_YUV) {
        useOverlay = false;
        goto out_check;
    }

    // Got a YUV layer, check external display status for extend video mode
    if (widiPlane->isActive()) {

        int srcWidth = layer->sourceCrop.right - layer->sourceCrop.left;
        int srcHeight = layer->sourceCrop.bottom - layer->sourceCrop.top;

        if(widiPlane->isBackgroundVideoMode() && widiPlane->isStreaming() && (mWidiNativeWindow != NULL)) {
            if(!(widiPlane->isSurfaceMatching(grallocHandle))) {
                useOverlay = false;
                goto out_check;
            }
        }

        if(!(widiPlane->isExtVideoAllowed()) || (srcWidth < 176 || srcHeight < 144)
            || (grallocHandle->format != HAL_PIXEL_FORMAT_INTEL_HWC_NV12)) {
           /* if extended video mode is not allowed or the resolution of video less than
            * QCIF (176 x 144) or Software decoder (e.g. VP8) is used, we stop here and let
            * the video to be rendered via GFx plane by surface flinger. Video encoder has
            * limitation that HW encoder can't encode video that is less than QCIF
            */
            useOverlay = false;
            goto out_check;
        }
        if(widiPlane->isPlayerOn() && widiPlane->isExtVideoAllowed()) {
            ALOGD_IF(ALLOW_HWC_PRINT, "isOverlayLayer: widi video on and force overlay");
            forceOverlay = true;
        }
    }

    // force to use overlay in video extend mode
    if (mDrm->getDisplayMode() == OVERLAY_EXTEND)
        forceOverlay = true;

    // check buffer usage
    if ((grallocHandle->usage & GRALLOC_USAGE_PROTECTED) || isForceOverlay(layer)) {
        ALOGD_IF(ALLOW_HWC_PRINT, "isOverlayLayer: protected video/force Overlay");
        forceOverlay = true;
    }

    // check blending, overlay cannot support blending
    if (layer->blending != HWC_BLENDING_NONE) {
        useOverlay = false;
        goto out_check;
    }

    // fall back if HWC_SKIP_LAYER was set, if forced to use
    // overlay skip this check
    if (!forceOverlay && (layer->flags & HWC_SKIP_LAYER)) {
        ALOGD_IF(ALLOW_HWC_PRINT, "isOverlayLayer: skip layer was set");
        useOverlay = false;
        goto out_check;
   }

    // check visible regions
    if (layer->visibleRegionScreen.numRects > 1) {
        useOverlay = false;
        goto out_check;
    }

    // TODO: not support OVERLAY_CLONE_MIPI0
    if (mDrm->getDisplayMode() == OVERLAY_CLONE_MIPI0) {
        useOverlay = false;
        goto out_check;
    }

    // fall back if YUV Layer is in the middle of
    // other layers and covers the layers under it.
    if (!forceOverlay && index > 0 && index < list->numHwLayers - 1) {
        for (int i = index - 1; i >= 0; i--) {
            if (areLayersIntersecting(layer, &list->hwLayers[i])) {
                useOverlay = false;
                goto out_check;
            }
        }
    }

    // check whether layer are covered by layers above it
    // if layer is covered by a layer which needs blending,
    // clear corresponding region in frame buffer
    for (size_t i = index + 1; i < list->numHwLayers - 1; i++) {
        if (areLayersIntersecting(&list->hwLayers[i], layer)) {
            ALOGD_IF(ALLOW_HWC_PRINT,
                "%s: overlay %d is covered by layer %d\n", __func__, index, i);
                if (list->hwLayers[i].blending !=  HWC_BLENDING_NONE)
                    mLayerList->setNeedClearup(index, true);
        }
    }

    useOverlay = true;
    needClearFb = true;
out_check:
    if (forceOverlay) {
        // clear HWC_SKIP_LAYER flag so that force to use overlay
        ALOGD("isOverlayLayer: force to use overlay");
        layer->flags &= ~HWC_SKIP_LAYER;
        mLayerList->setForceOverlay(index, true);
        layer->compositionType = HWC_OVERLAY;
        useOverlay = true;
        needClearFb = true;
#ifdef INTEL_EXT_SF_ANIMATION_HINT
        layer->hints |= HWC_HINT_DISABLE_ANIMATION;
#endif
    }

    // check if frame buffer clear is needed
    if (useOverlay) {
        ALOGD("isOverlayLayer: got an overlay layer");
        if (needClearFb) {
            //layer->hints |= HWC_HINT_CLEAR_FB;
            ALOGD_IF(ALLOW_HWC_PRINT, "isOverlayLayer: clear fb");
            mForceSwapBuffer = true;
        }
        layer->compositionType = HWC_OVERLAY;
    }

    flags = 0;
    return useOverlay;
}

// isSpriteLayer: check whether a given @layer can be handled
// by a hardware sprite plane.
// A layer is a sprite layer when
// 1) layer is RGB layer &&
// 2) No active external display (TODO: support external display)
// 3) HWC_SKIP_LAYER flag wasn't set by surface flinger
// 4) layer requires no blending or premultipled blending
// 5) layer has no transform (rotation, scaling)
bool IntelHWComposer::isSpriteLayer(hwc_display_contents_1_t *list,
                                    int index,
                                    hwc_layer_1_t *layer,
                                    int& flags)
{
    bool needClearFb = false;
    bool forceSprite = false;
    bool useSprite = false;

    int srcWidth, srcHeight;
    int dstWidth, dstHeight;

    if (!list || !layer)
        return false;

    intel_gralloc_buffer_handle_t *grallocHandle =
        (intel_gralloc_buffer_handle_t*)layer->handle;

    if (!grallocHandle) {
        ALOGD_IF(ALLOW_HWC_PRINT, "%s: invalid gralloc handle\n", __func__);
        return false;
    }

    // don't handle target framebuffer layer
    if (layer->compositionType == HWC_FRAMEBUFFER_TARGET)
        return false;

    // check whether pixel format is supported RGB formats
    if (mLayerList->getLayerType(index) != IntelHWComposerLayer::LAYER_TYPE_RGB) {
        ALOGD_IF(ALLOW_HWC_PRINT,
                "%s: invalid format 0x%x\n", __func__, grallocHandle->format);
        useSprite = false;
        goto out_check;
    }

    // Got a RGB layer, disable sprite plane when Widi is active
    if (mPlaneManager->isWidiActive()) {
        useSprite = false;
        goto out_check;
    }

    // disable sprite plane when HDMI is connected
    // FIXME: add HDMI sprite support later
    //if (mDrm->getOutputConnection(OUTPUT_HDMI) == DRM_MODE_CONNECTED) {
    //    useSprite = false;
    //    goto out_check;
    //}

    // fall back if HWC_SKIP_LAYER was set
    if ((layer->flags & HWC_SKIP_LAYER)) {
        ALOGD_IF(ALLOW_HWC_PRINT, "isSpriteLayer: HWC_SKIP_LAYER");
        useSprite = false;
        goto out_check;
    }

    // check usage???

    // check blending, only support none & premultipled blending
    // clear frame buffer region if layer has no blending
    if (layer->blending != HWC_BLENDING_PREMULT &&
        layer->blending != HWC_BLENDING_NONE) {
        ALOGD("isSpriteLayer: unsupported blending");
        useSprite = false;
        goto out_check;
    }

    // check rotation
    if (layer->transform) {
        ALOGD_IF(ALLOW_HWC_PRINT, "isSpriteLayer: need do transform");
        useSprite = false;
        goto out_check;
    }

     // check scaling
    srcWidth = layer->sourceCrop.right - layer->sourceCrop.left;
    srcHeight = layer->sourceCrop.bottom - layer->sourceCrop.top;
    dstWidth = layer->displayFrame.right - layer->displayFrame.left;
    dstHeight = layer->displayFrame.bottom - layer->displayFrame.top;

    if ((srcWidth == dstWidth) && (srcHeight == dstHeight))
        useSprite = true;
    else
        ALOGD_IF(ALLOW_HWC_PRINT,
               "isSpriteLayer: src W,H [%d, %d], dst W,H [%d, %d]",
               srcWidth, srcHeight, dstWidth, dstHeight);

    if (layer->blending == HWC_BLENDING_NONE)
        needClearFb = true;
out_check:
    if (forceSprite) {
        // clear HWC_SKIP_LAYER flag so that force to use overlay
        ALOGD("isSpriteLayer: force to use sprite");
        layer->flags &= ~HWC_SKIP_LAYER;
        mLayerList->setForceOverlay(index, true);
        layer->compositionType = HWC_OVERLAY;
        useSprite = true;
    }

    // check if frame buffer clear is needed
    if (useSprite) {
        ALOGD("isSpriteLayer: got a sprite layer");
        if (needClearFb) {
            ALOGD_IF(ALLOW_HWC_PRINT, "isSpriteLayer: clear fb");
            //layer->hints |= HWC_HINT_CLEAR_FB;
            mForceSwapBuffer = true;
        }
        layer->compositionType = HWC_OVERLAY;
    }

    flags = 0;
    return useSprite;
}

// isPrimaryLayer: check whether we can use primary plane to handle
// the given @layer.
// primary plane can be used only when
// 1) @layer is on the top of other layers (FIXME: not necessary, remove it
//    after introducing z order configuration)
// 2) all other layers were handled by HWC.
// 3) @layer is a sprite layer
// 4) @layer wasn't handled by sprite
bool IntelHWComposer::isPrimaryLayer(hwc_display_contents_1_t *list,
                                     int index,
                                     hwc_layer_1_t *layer,
                                     int& flags)
{
#ifndef INTEL_RGB_OVERLAY
    // only use primary when layer is the top layer
    //if ((size_t)index != (list->numHwLayers - 1))
    //    return false;
#endif


    // if a layer has already been handled, further check if it's a
    // sprite layer/overlay layer, if so, we simply bypass this layer.
    if (layer->compositionType == HWC_OVERLAY) {
        IntelDisplayPlane *plane = mLayerList->getPlane(index);
        if (plane) {
            switch (plane->getPlaneType()) {
            case IntelDisplayPlane::DISPLAY_PLANE_PRIMARY:
                // detach plane & re-check it
                mLayerList->detachPlane(index, plane);
                layer->compositionType = HWC_FRAMEBUFFER;
                layer->hints = 0;
                break;
            case IntelDisplayPlane::DISPLAY_PLANE_OVERLAY:
            case IntelDisplayPlane::DISPLAY_PLANE_SPRITE:
            default:
                return false;
            }
        }
    }

    // check whether all other layers were handled by HWC
    for (size_t i = 0; i < list->numHwLayers - 1; i++) {
        if ((list->hwLayers[i].compositionType != HWC_OVERLAY) && (i != index))
            return false;
    }

    return isSpriteLayer(list, index, layer, flags);
}

void IntelHWComposer::revisitLayerList(hwc_display_contents_1_t *list, bool isGeometryChanged)
{
    int zOrderConfig = IntelDisplayPlaneManager::ZORDER_OcOaP;

    if (!list)
        return;

    for (size_t i = 0; i < list->numHwLayers - 1; i++) {
        int flags = 0;

        // also need check whether a layer can be handled in general
        if (!isHWCLayer(&list->hwLayers[i]))
            continue;

        // make sure all protected layers were marked as overlay
        if (mLayerList->isProtectedLayer(i))
            list->hwLayers[i].compositionType = HWC_OVERLAY;
        // check if we can apply primary plane to an RGB layer
        // if overlay plane was used for a YUV layer, force overlay layer to
        // be the bottom layer.
        if (mLayerList->getLayerType(i) != IntelHWComposerLayer::LAYER_TYPE_RGB) {
            zOrderConfig = IntelDisplayPlaneManager::ZORDER_POcOa;
            continue;
        }

        if (isPrimaryLayer(list, i, &list->hwLayers[i], flags)) {
            bool ret = primaryPrepare(i, &list->hwLayers[i], flags);
            if (!ret) {
                ALOGE("%s: failed to prepare primary\n", __func__);
                list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
                list->hwLayers[i].hints = 0;
            }
        }
    }

    if (isGeometryChanged)
        mPlaneManager->setZOrderConfig(zOrderConfig, 0);

}

void IntelHWComposer::dumpLayerList(hwc_display_contents_1_t *list)
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

bool IntelHWComposer::isScreenshotActive(hwc_display_contents_1_t *list)
{
    if (!list || !list->numHwLayers)
        return false;

    hwc_layer_1_t *topLayer = &list->hwLayers[list->numHwLayers - 1];
    IntelWidiPlane* widiPlane = (IntelWidiPlane*)mPlaneManager->getWidiPlane();

    if (mDrm->getDisplayMode() == OVERLAY_EXTEND)
        return false;

    if (widiPlane->isActive())
        return false;

    if (!topLayer) {
        ALOGW("This might be a surfaceflinger BUG\n");
        return false;
    }

    if (!(topLayer->flags & HWC_SKIP_LAYER))
        return false;

    for (size_t i = 0; i < list->numHwLayers; i++) {
        if (mLayerList->isProtectedLayer(i))
            return false;
    }

    int x = topLayer->displayFrame.left;
    int y = topLayer->displayFrame.top;
    int w = topLayer->displayFrame.right - topLayer->displayFrame.left;
    int h = topLayer->displayFrame.bottom - topLayer->displayFrame.top;

    drmModeFBPtr fbInfo =
        IntelHWComposerDrm::getInstance().getOutputFBInfo(OUTPUT_MIPI0);

    if (x == 0 && y == 0 && w == fbInfo->width && h == fbInfo->height)
        return true;

    return false;
}

// When the geometry changed, we need
// 0) reclaim all allocated planes, reclaimed planes will be disabled
//    on the start of next frame. A little bit tricky, we cannot disable the
//    planes right after geometry is changed since there's no data in FB now,
//    so we need to wait FB is update then disable these planes.
// 1) build a new layer list for the changed hwc_layer_list
// 2) attach planes to these layers which can be handled by HWC
void IntelHWComposer::onGeometryChanged(hwc_display_contents_1_t *list)
{
    bool firstTime = true;

    // reclaim all planes
    bool ret = mLayerList->invalidatePlanes();
    if (!ret) {
        ALOGE("%s: failed to reclaim allocated planes\n", __func__);
        return;
    }

    // update layer list with new list
    mLayerList->updateLayerList(list);

    // TODO: uncomment it to print out layer list info
    // dumpLayerList(list);

    if (isScreenshotActive(list)) {
        ALOGD_IF(ALLOW_HWC_PRINT, "%s: Screenshot Active!\n", __func__);
        goto out_check;
    }

    for (size_t i = 0; list && i < list->numHwLayers - 1; i++) {
        // check whether a layer can be handled in general
        if (!isHWCLayer(&list->hwLayers[i]))
            continue;

        // further check whether a layer can be handle by overlay/sprite
        int flags = 0;
        bool hasOverlay = mPlaneManager->hasFreeOverlays();
        bool hasSprite = mPlaneManager->hasFreeSprites();

        if (hasOverlay && isOverlayLayer(list, i, &list->hwLayers[i], flags)) {
            ret = overlayPrepare(i, &list->hwLayers[i], flags);
            if (!ret) {
                ALOGE("%s: failed to prepare overlay\n", __func__);
                list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
                list->hwLayers[i].hints = 0;
            }
        } else if (hasSprite && isSpriteLayer(list, i, &list->hwLayers[i], flags)) {
            ret = spritePrepare(i, &list->hwLayers[i], flags);
            if (!ret) {
                ALOGE("%s: failed to prepare sprite\n", __func__);
                list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
                list->hwLayers[i].hints = 0;
            }
        } else {
            list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
            if(firstTime && mPlaneManager->isWidiActive() ) {
                IntelWidiPlane *p = (IntelWidiPlane *)mPlaneManager->getWidiPlane();
                p->setOrientation(list->hwLayers[i].transform);
                firstTime = false;
            }
        }
    }

out_check:
    // revisit each layer, make sure protected layers were handled by hwc,
    // and check if we can make use of primary plane
    revisitLayerList(list, true);

    // disable reclaimed planes
    mPlaneManager->disableReclaimedPlanes(IntelDisplayPlane::DISPLAY_PLANE_SPRITE);
    mPlaneManager->disableReclaimedPlanes(IntelDisplayPlane::DISPLAY_PLANE_PRIMARY);
}

// This function performs:
// 1) update layer's transform to data buffer's payload buffer, so that video
//    driver can get the latest transform info of this layer
// 2) if rotation is needed, video driver would setup the rotated buffer, then
//    update buffer's payload to inform HWC rotation buffer is available.
// 3) HWC would keep using ST till all rotation buffers are ready.
// Return: false if HWC is NOT ready to switch to overlay, otherwise true.
bool IntelHWComposer::useOverlayRotation(hwc_layer_1_t *layer,
                                         int index,
                                         uint32_t& handle,
                                         int& w, int& h,
                                         int& srcX, int& srcY,
                                         int& srcW, int& srcH,
                                         uint32_t &transform)
{
    bool useOverlay = false;
    uint32_t hwcLayerTransform;
    IntelWidiPlane* widiplane = (IntelWidiPlane*)mPlaneManager->getWidiPlane();

    // FIXME: workaround for rotation issue, remove it later
    static int counter = 0;
    uint32_t metadata_transform = 0;
    uint32_t displayMode = 0;

    if (!layer)
        return false;

    intel_gralloc_buffer_handle_t *grallocHandle =
        (intel_gralloc_buffer_handle_t*)layer->handle;

    if (!grallocHandle)
        return false;

    // detect video mode change
    displayMode = mDrm->getDisplayMode();
    if(widiplane->isStreaming())
        displayMode = OVERLAY_EXTEND;

    if (grallocHandle->format == HAL_PIXEL_FORMAT_INTEL_HWC_NV12) {
        // map payload buffer
        IntelPayloadBuffer buffer(mGrallocBufferManager, grallocHandle->fd[GRALLOC_SUB_BUFFER1]);
        intel_gralloc_payload_t *payload = (intel_gralloc_payload_t*)buffer.getCpuAddr();
        if (!payload) {
            ALOGE("%s: invalid address\n", __func__);
            return false;
        }

        if (payload->force_output_method == OUTPUT_FORCE_GPU) {
            ALOGD_IF(ALLOW_HWC_PRINT,
                    "%s: force to use surface texture.", __func__);
            return false;
        }

        metadata_transform = payload->metadata_transform;

        //For extend mode, we ignore WM rotate info
        if (displayMode == OVERLAY_EXTEND) {
            transform = metadata_transform;
        }

        if (!transform) {
            ALOGD_IF(ALLOW_HWC_PRINT,
                    "%s: use overlay to display original buffer.", __func__);
            return true;
        }

        if (transform != payload->client_transform) {
            ALOGD_IF(ALLOW_HWC_PRINT,
                    "%s: rotation buffer was not prepared by client! ui64Stamp = %llu\n", __func__, grallocHandle->ui64Stamp);
            return false;
        }

        // update handle, w & h to rotation buffer
        handle = payload->rotated_buffer_handle;
        w = payload->rotated_width;
        h = payload->rotated_height;
        //wait video rotated buffer idle
        mGrallocBufferManager->waitIdle(handle);
        // NOTE: exchange the srcWidth & srcHeight since
        // video driver currently doesn't call native_window_*
        // helper functions to update info for rotation buffer.
        if (transform == HAL_TRANSFORM_ROT_90 ||
                transform == HAL_TRANSFORM_ROT_270) {
            int temp = srcH;
            srcH = srcW;
            srcW = temp;
            temp = srcX;
            srcX = srcY;
            srcY = temp;

        }

        // skip pading bytes in rotate buffer
        switch(transform) {
            case HAL_TRANSFORM_ROT_90:
                srcX += ((srcW + 0xf) & ~0xf) - srcW;
                break;
            case HAL_TRANSFORM_ROT_180:
                srcX += ((srcW + 0xf) & ~0xf) - srcW;
                srcY += ((srcH + 0xf) & ~0xf) - srcH;
                break;
            case HAL_TRANSFORM_ROT_270:
                srcY += ((srcH + 0xf) & ~0xf) - srcH;
                break;
            default:
                break;
        }
    } else {
        //For software codec, overlay can't handle rotate
        //and fallback to surface texture.
        if ((displayMode != OVERLAY_EXTEND) && transform) {
            ALOGD_IF(ALLOW_HWC_PRINT,
                    "%s: software codec rotation, back to ST!", __func__);
            return false;
        }
        //set this flag for mapping correct buffer
        transform = 0;
    }

    //for most of cases, we handle rotate by overlay
    useOverlay = true;
    return useOverlay;
}

// This function performs:
// Acquire bool BobDeinterlace from video driver
// If ture, use bob deinterlace, otherwise, use weave deinterlace
bool IntelHWComposer::isBobDeinterlace(hwc_layer_1_t *layer)
{
    bool bobDeinterlace = false;

    if (!layer)
        return bobDeinterlace;

    intel_gralloc_buffer_handle_t *grallocHandle =
        (intel_gralloc_buffer_handle_t*)layer->handle;

    if (!grallocHandle) {
        return bobDeinterlace;
    }

    if (grallocHandle->format != HAL_PIXEL_FORMAT_INTEL_HWC_NV12)
        return bobDeinterlace;

    // map payload buffer
    IntelPayloadBuffer buffer(mGrallocBufferManager, grallocHandle->fd[GRALLOC_SUB_BUFFER1]);

    intel_gralloc_payload_t *payload = (intel_gralloc_payload_t*)buffer.getCpuAddr();
    if (!payload) {
        ALOGE("%s: invalid address\n", __func__);
        return bobDeinterlace;
    }

    bobDeinterlace = (payload->bob_deinterlace == 1) ? true : false;
    return bobDeinterlace;
}

// when buffer handle is changed, we need
// 0) get plane's data buffer if a plane was attached to a layer
// 1) update plane's data buffer with the new buffer handle
// 2) set the updated data buffer back to plane
bool IntelHWComposer::updateLayersData(hwc_display_contents_1_t *list)
{
    IntelDisplayPlane *plane = 0;
    IntelWidiPlane* widiplane = NULL;
    bool ret = true;
    bool handled = true;

    if (mPlaneManager->isWidiActive()) {
         widiplane = (IntelWidiPlane*) mPlaneManager->getWidiPlane();
         mFBDev->bBypassPost = 0;
    } else
         mFBDev->bBypassPost = 0; //cfg.bypasspost;

    for (size_t i=0 ; i<list->numHwLayers - 1; i++) {
        hwc_layer_1_t *layer = &list->hwLayers[i];
        intel_gralloc_buffer_handle_t *grallocHandle =
            (intel_gralloc_buffer_handle_t*)layer->handle;

        if (grallocHandle &&
            grallocHandle->format == HAL_PIXEL_FORMAT_INTEL_HWC_NV12) {
            // map payload buffer
            IntelPayloadBuffer buffer(mGrallocBufferManager, grallocHandle->fd[GRALLOC_SUB_BUFFER1]);

            intel_gralloc_payload_t *payload = (intel_gralloc_payload_t*)buffer.getCpuAddr();

            if (!payload) {
                ALOGE("%s: invalid address\n", __func__);
                return false;
            }
            //wait video buffer idle
            mGrallocBufferManager->waitIdle(payload->khandle);
        }
        plane = mLayerList->getPlane(i);
        if (!plane)
            continue;

        // clear layer's visible region if need clear up flag was set
        // and sprite plane was used as primary plane (point to FB)
        if (mLayerList->getNeedClearup(i) &&
            mPlaneManager->primaryAvailable(0)) {
            ALOGD_IF(ALLOW_HWC_PRINT,
                  "updateLayersData: clear visible region of layer %d", i);
            list->hwLayers[i].hints |= HWC_HINT_CLEAR_FB;
        }

        int bobDeinterlace;
        int srcX = layer->sourceCrop.left;
        int srcY = layer->sourceCrop.top;
        int srcWidth = layer->sourceCrop.right - layer->sourceCrop.left;
        int srcHeight = layer->sourceCrop.bottom - layer->sourceCrop.top;
        int planeType = plane->getPlaneType();

        if(srcHeight == 1 || srcWidth == 1) {
            mLayerList->detachPlane(i, plane);
            layer->compositionType = HWC_FRAMEBUFFER;
            handled = false;
            continue;
        }

        // get & setup overlay data buffer
        IntelDisplayBuffer *buffer = plane->getDataBuffer();
        IntelDisplayDataBuffer *dataBuffer =
            reinterpret_cast<IntelDisplayDataBuffer*>(buffer);
        if (!dataBuffer) {
            ALOGE("%s: invalid overlay data buffer\n", __func__);
            continue;
        }

        // if invalid gralloc buffer handle, throw back this layer to SF
        if (!grallocHandle) {
                ALOGE("%s: invalid buffer handle\n", __func__);
                mLayerList->detachPlane(i, plane);
                layer->compositionType = HWC_FRAMEBUFFER;
                handled = false;
                continue;
        }

        int bufferWidth = grallocHandle->width;
        int bufferHeight = grallocHandle->height;
        uint32_t bufferHandle = grallocHandle->fd[GRALLOC_SUB_BUFFER0];
        int format = grallocHandle->format;
        uint32_t transform = layer->transform;

        if (planeType == IntelDisplayPlane::DISPLAY_PLANE_OVERLAY) {
            if (widiplane) {
                widiplane->setOverlayData(grallocHandle, srcWidth, srcHeight);
                if(widiplane->isBackgroundVideoMode()) {
                    if((mWidiNativeWindow == NULL) &&  widiplane->isStreaming()) {
                        widiplane->getNativeWindow(mWidiNativeWindow);
                        if(mWidiNativeWindow != NULL) {
                            if(mDrm->isMdsSurface(mWidiNativeWindow)) {
                                 widiplane->setNativeWindow(mWidiNativeWindow);
                                 ALOGD_IF(ALLOW_HWC_PRINT,
                                        "Native window is from MDS for widi at composer = 0x%x ", mWidiNativeWindow);
                            }
                            else {
                                mWidiNativeWindow = NULL;
                                ALOGD_IF(ALLOW_HWC_PRINT,"Native window is not from MDS");
                            }
                        }
                    }
                }
                else {
                    mWidiNativeWindow = NULL;
                    ALOGD_IF(ALLOW_HWC_PRINT,
                           "Native window from widiplane for background  = %d", mWidiNativeWindow);
                }
                continue;
            }

            IntelOverlayContext *overlayContext =
                reinterpret_cast<IntelOverlayContext*>(plane->getContext());
            int flags = mLayerList->getFlags(i);

            // disable overlay if DELAY_DISABLE flag was set
            if (flags & IntelDisplayPlane::DELAY_DISABLE) {
                ALOGD_IF(ALLOW_HWC_PRINT,
                       "updateLayerData: disable plane (DELAY)!");
                flags &= ~IntelDisplayPlane::DELAY_DISABLE;
                mLayerList->setFlags(i, flags);
                plane->disable();
            }

            // check if can switch to overlay
            bool useOverlay = useOverlayRotation(layer, i,
                                                 bufferHandle,
                                                 bufferWidth,
                                                 bufferHeight,
                                                 srcX,
                                                 srcY,
                                                 srcWidth,
                                                 srcHeight,
                                                 transform);

            if (!useOverlay) {
                ALOGD_IF(ALLOW_HWC_PRINT,
                       "updateLayerData: useOverlayRotation failed!");
                if (!mLayerList->getForceOverlay(i)) {
                    ALOGD_IF(ALLOW_HWC_PRINT,
                           "updateLayerData: fallback to ST to do rendering!");
                    // fallback to ST to render this frame
                    layer->compositionType = HWC_FRAMEBUFFER;
                    mForceSwapBuffer = true;
                    handled = false;
                }
                // disable overlay when rotated buffer is not ready
                flags |= IntelDisplayPlane::DELAY_DISABLE;
                mLayerList->setFlags(i, flags);
                continue;
            }

            bobDeinterlace = isBobDeinterlace(layer);
            if (bobDeinterlace) {
                flags |= IntelDisplayPlane::BOB_DEINTERLACE;
            } else {
                flags &= ~IntelDisplayPlane::BOB_DEINTERLACE;
            }
            mLayerList->setFlags(i, flags);

            // clear FB first on first overlay frame
            if (layer->compositionType == HWC_FRAMEBUFFER) {
                ALOGD_IF(ALLOW_HWC_PRINT,
                       "updateLayerData: first overlay frame clear fb");
                //layer->hints |= HWC_HINT_CLEAR_FB;
                mForceSwapBuffer = true;
            }

            // switch to overlay
            layer->compositionType = HWC_OVERLAY;

            // gralloc buffer is not aligned to 32 pixels
            uint32_t grallocStride = align_to(bufferWidth, 32);
            int format = grallocHandle->format;

            dataBuffer->setFormat(format);
            dataBuffer->setStride(grallocStride);
            dataBuffer->setWidth(bufferWidth);
            dataBuffer->setHeight(bufferHeight);
            dataBuffer->setCrop(srcX, srcY, srcWidth, srcHeight);
            dataBuffer->setDeinterlaceType(bobDeinterlace);
            // set the data buffer back to plane
            ret = ((IntelOverlayPlane*)plane)->setDataBuffer(bufferHandle,
                                                             transform,
                                                             grallocHandle);
            if (!ret) {
                ALOGE("%s: failed to update overlay data buffer\n", __func__);
                mLayerList->detachPlane(i, plane);
                layer->compositionType = HWC_FRAMEBUFFER;
                handled = false;
            }
        } else if (planeType == IntelDisplayPlane::DISPLAY_PLANE_SPRITE ||
                   planeType == IntelDisplayPlane::DISPLAY_PLANE_PRIMARY) {

            // adjust the buffer format if no blending is needed
            // some test cases would fail due to a weird format!
            if (layer->blending == HWC_BLENDING_NONE) {
                switch (format) {
                    case HAL_PIXEL_FORMAT_BGRA_8888:
                    format = HAL_PIXEL_FORMAT_BGRX_8888;
                    break;
                case HAL_PIXEL_FORMAT_RGBA_8888:
                    format = HAL_PIXEL_FORMAT_RGBX_8888;
                    break;
                }
            }

            // set data buffer format
            dataBuffer->setFormat(format);
            dataBuffer->setWidth(bufferWidth);
            dataBuffer->setHeight(bufferHeight);
            dataBuffer->setCrop(srcX, srcY, srcWidth, srcHeight);
            // set the data buffer back to plane
            ret = plane->setDataBuffer(bufferHandle, transform, grallocHandle);
            if (!ret) {
                ALOGE("%s: failed to update sprite data buffer\n", __func__);
                mLayerList->detachPlane(i, plane);
                layer->compositionType = HWC_FRAMEBUFFER;
                handled = false;
            }
        } else {
            ALOGW("%s: invalid plane type %d\n", __func__, planeType);
            continue;
        }
    }

    return handled;
}

// Check the usage of a buffer
// only following usages were accepted:
// TODO: list valid usages
bool IntelHWComposer::isHWCUsage(int usage)
{
    // For SW access buffer, should handle with
    // FB in order to avoid tearing
    if ((usage & GRALLOC_USAGE_SW_WRITE_OFTEN) &&
         (usage & GRALLOC_USAGE_SW_READ_OFTEN))
        return false;

    if (!(usage & GRALLOC_USAGE_HW_COMPOSER))
        return false;

    return true;
}

// Check the data format of a buffer
// only following formats were accepted:
// TODO: list valid formats
bool IntelHWComposer::isHWCFormat(int format)
{
    return true;
}

// Check the transform of a layer
// we can only support 180 degree rotation
// TODO: list all supported transforms
// FIXME: for video playback we need to ignore this,
// since video decoding engine will take care of rotation.
bool IntelHWComposer::isHWCTransform(uint32_t transform)
{
    return false;
}

// Check the blending of a layer
// Currently, no blending support
bool IntelHWComposer::isHWCBlending(uint32_t blending)
{
    return false;
}

// Check whether a layer can be handle by our HWC
// if HWC_SKIP_LAYER was set, skip this layer
// TODO: add more
bool IntelHWComposer::isHWCLayer(hwc_layer_1_t *layer)
{
    if (!layer)
        return false;

    // if (layer->flags & HWC_SKIP_LAYER)
    //    return false;

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

// Check whehter two layers are intersect
bool IntelHWComposer::areLayersIntersecting(hwc_layer_1_t *top,
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

void IntelHWComposer::signalHpdCompletion()
{
    Mutex::Autolock _l(mHpdLock);
    if (mHpdCompletion == false) {
        mHpdCompletion = true;
        mHpdCondition.signal();
        ALOGD("%s: send out hpd completion signal\n", __func__);
    }
}

void IntelHWComposer::waitForHpdCompletion()
{
    Mutex::Autolock _l(mHpdLock);

    // time out for 300ms
    nsecs_t reltime = 300000000;
    mHpdCompletion = false;
    while(!mHpdCompletion) {
        mHpdCondition.waitRelative(mHpdLock, reltime);
    }

    ALOGD("%s: receive hpd completion signal: %d\n", __func__, mHpdCompletion?1:0);
}

bool IntelHWComposer::handleDisplayModeChange()
{
    android::Mutex::Autolock _l(mLock);
    ALOGD_IF(ALLOW_HWC_PRINT, "handleDisplayModeChange");

    if (!mDrm) {
         ALOGW("%s: mDrm is not intilized!\n", __func__);
         return false;
    }

    mDrm->detectMDSModeChange();

    // go through layer list and call plane's onModeChange()
    for (size_t i = 0 ; i < mLayerList->getLayersCount(); i++) {
        IntelDisplayPlane *plane = mLayerList->getPlane(i);
        if (plane)
            plane->onDrmModeChange();
    }
    mHotplugEvent = true;

    return true;
}

bool IntelHWComposer::handleHotplugEvent(int hpd, void *data)
{
    bool ret = false;
    int disp = 1;

    ALOGD_IF(ALLOW_HWC_PRINT, "handleHotplugEvent");

    if (!mDrm) {
        ALOGW("%s: mDrm is not intilized!\n", __func__);
        goto out;
    }

    if (hpd) {
        // get display mode
        intel_display_mode_t *s_mode = (intel_display_mode_t *)data;
        drmModeModeInfoPtr mode;
        mode = mDrm->selectDisplayDrmMode(disp, s_mode);
        if (!mode) {
            ret = false;
            goto out;
        }

        // alloc buffer;
        mHDMIFBHandle.size = align_to(mode->vdisplay * mode->hdisplay * 4, 64);
        ret = mGrallocBufferManager->alloc(mHDMIFBHandle.size,
                                      &mHDMIFBHandle.umhandle,
                                      &mHDMIFBHandle.kmhandle);
        if (!ret)
            goto out;

        // mode setting;
        ret = mDrm->setDisplayDrmMode(disp, mHDMIFBHandle.kmhandle, mode);
        if (!ret)
            goto out;
    } else {
        // rm FB
        ret = mDrm->handleDisplayDisConnection(disp);
        if (!ret)
            goto out;

        // release buffer;
        ret = mGrallocBufferManager->dealloc(mHDMIFBHandle.umhandle);
        if (!ret)
            goto out;

        memset(&mHDMIFBHandle, 0, sizeof(mHDMIFBHandle));
    }

out:
    if (ret) {
        ALOGD("%s: detected hdmi hotplug event:%s\n", __func__, hpd?"IN":"OUT");
        handleDisplayModeChange();

        /* hwc_dev->procs is set right after the device is opened, but there is
         * still a race condition where a hotplug event might occur after the open
         * but before the procs are registered. */
        if (mProcs && mProcs->vsync) {
            mProcs->hotplug(mProcs, HWC_DISPLAY_EXTERNAL, hpd);
        }
    }

    return ret;
}

bool IntelHWComposer::handleDynamicModeSetting(void *data)
{
    bool ret = false;

    ALOGD_IF(ALLOW_HWC_PRINT, "%s: handle Dynamic mode setting!\n", __func__);
    // send plug-out to SF for mode changing on the same device
    // otherwise SF will bypass the plug-in message as there is
    // no connection change;
    ret = handleHotplugEvent(0, NULL);
    if (!ret) {
        ALOGW("%s: send fake unplug event failed!\n", __func__);
        goto out;
    }

    // TODO: here we need to wait for the plug-out take effect.
    waitForHpdCompletion();

    // then change the mode and send plug-in to SF
    ret = handleHotplugEvent(1, data);
    if (!ret) {
        ALOGW("%s: send plug in event failed!\n", __func__);
        goto out;
    }
out:
    return ret;
}

bool IntelHWComposer::onUEvent(const char *msg, int msgLen, int msgType, void *data)
{
    bool ret = false;
#ifdef TARGET_HAS_MULTIPLE_DISPLAY
    // if mds sent orientation change message, inform widi plane and return
    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS_ORIENTATION_CHANGE) {
        ALOGD("%s: got multiDisplay service orientation change event\n", __func__);
        if(mPlaneManager->isWidiActive()) {
            IntelWidiPlane* widiPlane = (IntelWidiPlane*)mPlaneManager->getWidiPlane();
            if (widiPlane->setOrientationChanged() != NO_ERROR) {
                ALOGE("%s: error in sending orientation change event to widiplane");
            }
        }
    }

    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS)
        ret = handleDisplayModeChange();

    // handle hdmi plug in;
    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS_HOTPLUG_IN)
        ret = handleHotplugEvent(1, data);

    // handle hdmi plug out;
    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS_HOTPLUG_OUT)
        ret = handleHotplugEvent(0, NULL);

    // handle dynamic mode setting
    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS_TIMING_DYNAMIC_SETTING)
        ret = handleDynamicModeSetting(data);

    return ret;
#endif

    if (strcmp(msg, "change@/devices/pci0000:00/0000:00:02.0/drm/card0"))
        return true;
    msg += strlen(msg) + 1;

    do {
        if (!strncmp(msg, "HOTPLUG_IN=1", strlen("HOTPLUG_IN=1"))) {
            ALOGD("%s: detected hdmi hotplug event:%s\n", __func__, msg);
            ret = handleHotplugEvent(1, NULL);
            break;
        } else if (!strncmp(msg, "HOTPLUG_OUT=1", strlen("HOTPLUG_OUT=1"))) {
            ret = handleHotplugEvent(0, NULL);
            break;
        }

        msg += strlen(msg) + 1;
    } while (*msg);

    return ret;
}

void IntelHWComposer::vsync(int64_t timestamp, int pipe)
{
    if (mProcs && mProcs->vsync) {
        ALOGV("%s: report vsync timestamp %llu, pipe %d, active 0x%x", __func__,
             timestamp, pipe, mActiveVsyncs);
        if ((1 << pipe) & mActiveVsyncs)
            mProcs->vsync(const_cast<hwc_procs_t*>(mProcs), 0, timestamp);
    }
    mLastVsync = timestamp;
}

bool IntelHWComposer::flipHDMIFramebufferContexts(void *contexts, hwc_layer_1_t *target_layer)
{
    ALOGD_IF(ALLOW_HWC_PRINT, "flipHDMIFrameBufferContexts");

    if (!contexts) {
        ALOGE("%s: Invalid plane contexts\n", __func__);
        return false;
    }

    if (target_layer == NULL) {
        ALOGE("%s: Invalid HDMI target layer\n", __func__);
        return false;
    }

    int output = 1;
    drmModeConnection connection = mDrm->getOutputConnection(output);
    if (connection != DRM_MODE_CONNECTED) {
        ALOGE("%s: HDMI does not connected\n", __func__);
        return false;
    }

    intel_overlay_mode_t mode = mDrm->getDisplayMode();
    if (mode == OVERLAY_EXTEND) {
        ALOGV("%s: Skip FRAMEBUFFER_TARGET on video_ext mode\n", __func__);
        return false;
    }

    //get target layer handler
    intel_gralloc_buffer_handle_t *grallocHandle =
    (intel_gralloc_buffer_handle_t*)target_layer->handle;

    if (!grallocHandle)
        return false;

    //map HDMI buffer handler
    IntelDisplayBuffer *buffer = NULL;

    for (int i = 0; i < HDMI_BUF_NUM; i++) {
	if (mHDMIBuffers[i].ui64Stamp == grallocHandle->ui64Stamp) {
            ALOGD_IF(ALLOW_HWC_PRINT, "%s: buf stamp %d...\n", __func__,grallocHandle->ui64Stamp);
            buffer = mHDMIBuffers[i].buffer;
            break;
        }
    }

    if (!buffer) {
        // release the buffer in the next slot
        if (mHDMIBuffers[mNextBuffer].ui64Stamp ||
                    mHDMIBuffers[mNextBuffer].buffer) {
            mGrallocBufferManager->unmap(mHDMIBuffers[mNextBuffer].buffer);
            mHDMIBuffers[mNextBuffer].ui64Stamp = 0;
            mHDMIBuffers[mNextBuffer].buffer = 0;
        }

        buffer = mGrallocBufferManager->map(grallocHandle->fd[GRALLOC_SUB_BUFFER0]);

        if (!buffer) {
            ALOGE("%s: failed to map HDMI handle !\n", __func__);
            return false;
        }

        mHDMIBuffers[mNextBuffer].ui64Stamp = grallocHandle->ui64Stamp;
        mHDMIBuffers[mNextBuffer].buffer = buffer;
        // move mNextBuffer pointer
        mNextBuffer = (mNextBuffer + 1) % HDMI_BUF_NUM;
    }

    // update layer info;
    uint32_t fbWidth = target_layer->sourceCrop.right;
    uint32_t fbHeight = target_layer->sourceCrop.bottom;
    uint32_t gttOffsetInPage = buffer->getGttOffsetInPage();
    uint32_t format = grallocHandle->format;
    uint32_t spriteFormat;

    switch (format) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
            spriteFormat = INTEL_SPRITE_PIXEL_FORMAT_RGBA8888;
            break;
        case HAL_PIXEL_FORMAT_RGBX_8888:
            spriteFormat = INTEL_SPRITE_PIXEL_FORMAT_RGBX8888;
            break;
        case HAL_PIXEL_FORMAT_BGRX_8888:
            spriteFormat = INTEL_SPRITE_PIXEL_FORMAT_BGRX8888;
            break;
        case HAL_PIXEL_FORMAT_BGRA_8888:
            spriteFormat = INTEL_SPRITE_PIXEL_FORMAT_BGRA8888;
            break;
        default:
            spriteFormat = INTEL_SPRITE_PIXEL_FORMAT_BGRX8888;
            ALOGE("%s: unsupported format 0x%x\n", __func__, format);
            return false;
    }

    // update context
    mdfld_plane_contexts_t *planeContexts;
    intel_sprite_context_t *context;

    planeContexts = (mdfld_plane_contexts_t*)contexts;
    context = &planeContexts->sprite_contexts[output];

    context->update_mask = SPRITE_UPDATE_ALL;
    context->index = output;
    context->pipe = output;
    context->linoff = 0;
    // Display requires 64 bytes align, Gralloc does 32 pixels align
    context->stride = align_to((4 * fbWidth), 128);
    context->pos = 0;
    context->size = ((fbHeight - 1) & 0xfff) << 16 | ((fbWidth - 1) & 0xfff);
    context->surf = gttOffsetInPage << 12;

    context->cntr = spriteFormat;
    context->cntr |= 0x80000000;

    ALOGD_IF(ALLOW_HWC_PRINT,
            "HDMI Contexts gttoff:0x%x;stride:%d;format:0x%x;fbH:%d;fbW:%d\n",
             gttOffsetInPage, context->stride, spriteFormat, fbHeight, fbWidth);

    // update active primary
    planeContexts->active_sprites |= (1 << output);

    return true;
}

bool IntelHWComposer::flipFramebufferContexts(void *contexts)
{
    ALOGD_IF(ALLOW_HWC_PRINT, "flipFrameBufferContexts");
    intel_sprite_context_t *context;
    mdfld_plane_contexts_t *planeContexts;
    uint32_t fbWidth, fbHeight;
    int zOrderConfig;
    bool forceBottom = false;

    if (!contexts) {
        ALOGE("%s: Invalid plane contexts\n", __func__);
        return false;
    }

    planeContexts = (mdfld_plane_contexts_t*)contexts;

    for (int output = 0; output < OUTPUT_MAX; output++) {
        // do not handle external display here
        if (output > 0)
            continue;

        drmModeConnection connection = mDrm->getOutputConnection(output);
        if (connection != DRM_MODE_CONNECTED)
            continue;

        drmModeFBPtr fbInfo = mDrm->getOutputFBInfo(output);

        fbWidth = fbInfo->width;
        fbHeight = fbInfo->height;

        zOrderConfig = mPlaneManager->getZOrderConfig(output);
        if ((zOrderConfig == IntelDisplayPlaneManager::ZORDER_OcOaP) ||
            (zOrderConfig == IntelDisplayPlaneManager::ZORDER_OaOcP))
            forceBottom = true;

        context = &planeContexts->primary_contexts[output];

        // update context
        context->update_mask = SPRITE_UPDATE_ALL;
        context->index = output;
        context->pipe = output;
        context->linoff = 0;
        context->stride = align_to((4 * fbWidth), 64);
        context->pos = 0;
        context->size = ((fbHeight - 1) & 0xfff) << 16 | ((fbWidth - 1) & 0xfff);
        context->surf = 0;

        // config z order; switch z order may cause flicker
        if (forceBottom) {
            context->cntr = INTEL_SPRITE_PIXEL_FORMAT_BGRX8888;
        } else {
            context->cntr = INTEL_SPRITE_PIXEL_FORMAT_BGRA8888;
        }

        context->cntr |= 0x80000000;

        // update active primary
        planeContexts->active_primaries |= (1 << output);
    }

    return true;
}

bool IntelHWComposer::prepare(int disp, hwc_display_contents_1_t *list)
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);

    if (disp) {
        if (!list)
            signalHpdCompletion();
        return true;
    }

    if (list && list->numHwLayers == 1) return true;

    if (!initCheck()) {
        ALOGE("%s: failed to initialize HWComposer\n", __func__);
        return false;
    }

    android::Mutex::Autolock _l(mLock);

    // delay 3 circles to disable useless overlay planes
    static int counter = 0;
    if (mPlaneManager->hasReclaimedOverlays()) {
        if (++counter == 3) {
            mPlaneManager->disableReclaimedPlanes(IntelDisplayPlane::DISPLAY_PLANE_OVERLAY);
            counter = 0;
        }
    }
    else
        counter = 0;


    // clear force swap buffer flag
    mForceSwapBuffer = false;

    bool widiStatusChanged = mPlaneManager->isWidiStatusChanged();

    // handle geometry changing. attach display planes to layers
    // which can be handled by HWC.
    // plane control information (e.g. position) will be set here
    if (!list || (list->flags & HWC_GEOMETRY_CHANGED) || mHotplugEvent
        || widiStatusChanged) {
        onGeometryChanged(list);

        IntelWidiPlane* widiPlane = (IntelWidiPlane*)mPlaneManager->getWidiPlane();
        if (widiStatusChanged && mDrm) {
            if(mPlaneManager->isWidiActive()) {
                mDrm->notifyWidi(true);
                if(widiPlane->isBackgroundVideoMode())
                    mDrm->notifyMipi(true);
                else
                    mDrm->notifyMipi(!widiPlane->isStreaming());
            }
            else
            {
                mDrm->notifyWidi(false);
            }
        }

        if(mHotplugEvent) {
            if(mPlaneManager->isWidiActive()) {
                if(widiPlane->isExtVideoAllowed()) {
                    // default fps to 0. widi stack will decide what correct fps should be
                    int displayW = 0, displayH = 0, fps = 0, isInterlace = 0;
                    if(mDrm->isVideoPlaying()) {
                        if(mDrm->getVideoInfo(&displayW, &displayH, &fps, &isInterlace)) {
                            if(fps < 0) fps = 0;
                        }
                    }
                    widiPlane->setPlayerStatus(mDrm->isVideoPlaying(), fps);
                }
            }
            mHotplugEvent = false;
        }

        intel_overlay_mode_t mode = mDrm->getDisplayMode();
        if (list && (mode == OVERLAY_EXTEND ||  widiPlane->isStreaming()) &&
            (list->flags & HWC_GEOMETRY_CHANGED)) {
            bool hasSkipLayer = false;
            ALOGD_IF(ALLOW_HWC_PRINT,
                    "layers num:%d", list->numHwLayers);
            for (size_t j = 0 ; j < list->numHwLayers ; j++) {
                if (list->hwLayers[j].flags & HWC_SKIP_LAYER) {
                hasSkipLayer = true;
                break;
                }
            }

            if (!hasSkipLayer) {
                if (list->numHwLayers == 1)
                    mDrm->notifyMipi(false);
                else
                    mDrm->notifyMipi(true);
            }
        }
    }

    // handle buffer changing. setup data buffer.
    if (list && !updateLayersData(list)) {
        ALOGD_IF(ALLOW_HWC_PRINT, "prepare: revisiting layer list\n");
        revisitLayerList(list, false);
    }

    //FIXME: we have to clear fb by hwc itself because
    //surfaceflinger can't do proper clear now.
    if (mForceSwapBuffer)
        mLayerList->clearWithOpenGL();

    return true;
}

bool IntelHWComposer::commit(int disp, hwc_display_contents_1_t *list)
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);

    if (!initCheck()) {
        ALOGE("%s: failed to initialize HWComposer\n", __func__);
        return false;
    }

    android::Mutex::Autolock _l(mLock);

    // if hotplug was happened & didn't be handled skip the flip
    if (mHotplugEvent)
        return true;

    void *context = mPlaneManager->getPlaneContexts();
    if (!context) {
        ALOGE("%s: invalid plane contexts\n", __func__);
        return false;
    }

    // need check whether eglSwapBuffers is necessary
    bool needSwapBuffer = false;

    // if all layers were attached with display planes then we don't need
    // swap buffers.
    if (!mLayerList->getLayersCount() ||
        mLayerList->getLayersCount() != mLayerList->getAttachedPlanesCount() ||
        mForceSwapBuffer) {
        ALOGD_IF(ALLOW_HWC_PRINT,
               "%s: mForceSwapBuffer: %d, layer count: %d, attached plane:%d\n",
               __func__, mForceSwapBuffer, mLayerList->getLayersCount(),
               mLayerList->getAttachedPlanesCount());

        // FIXME: it might be a surface flinger bug
        // surface flinger failed to render a layer to FB sometimes
	// because screen dirty region was unchanged, in this case
        // we don't to swap buffers.
#ifdef INTEL_EXT_SF_NEED_SWAPBUFFER
        if (!list || (list->flags & HWC_NEED_SWAPBUFFERS))
#endif
            needSwapBuffer = true;
    }

    // if primary plane is in use, skip eglSwapBuffers
    if (!mPlaneManager->primaryAvailable(0)) {
        ALOGD_IF(ALLOW_HWC_PRINT, "%s: primary plane in use\n", __func__);
        needSwapBuffer = false;
    }

    // check whether eglSwapBuffers is still needed for the given layer list
    /*if (needSwapBuffer) {
        ALOGD_IF(ALLOW_HWC_PRINT, "%s: eglSwapBuffers\n", __func__);
        EGLBoolean sucess = eglSwapBuffers((EGLDisplay)list->dpy, (EGLSurface)list->sur);
        if (!sucess) {
            return false;
        }
    }*/

    if(mPlaneManager->isWidiActive()) {
        IntelDisplayPlane *p = mPlaneManager->getWidiPlane();
        ALOGD_IF(ALLOW_HWC_PRINT, "Widi Plane is %p",p);
         if (p)
             p->flip(context, 0);
         else
             ALOGE("Widi Plane is NULL");
    }

    { //if (mFBDev->bBypassPost) {
        buffer_handle_t bufferHandles[INTEL_DISPLAY_PLANE_NUM];
        int numBuffers = 0;
        // setup primary plane contexts if swap buffers is needed
        if (needSwapBuffer && list->hwLayers[list->numHwLayers-1].handle) {
            flipFramebufferContexts(context);
            bufferHandles[numBuffers++] = list->hwLayers[list->numHwLayers-1].handle;
        }

        // Call plane's flip for each layer in hwc_layer_list, if a plane has
        // been attached to a layer
        // First post RGB layers, then overlay layers.
        for (size_t i=0 ; list && i<list->numHwLayers - 1; i++) {
            IntelDisplayPlane *plane = mLayerList->getPlane(i);
            int flags = mLayerList->getFlags(i);

            if (!plane)
                continue;
            if (list->hwLayers[i].flags & HWC_SKIP_LAYER)
                continue;
            if (list->hwLayers[i].compositionType != HWC_OVERLAY)
                continue;

            ALOGD_IF(ALLOW_HWC_PRINT, "%s: flip plane %d, flags: 0x%x\n",
                __func__, i, flags);

            bool ret = plane->flip(context, flags);
            if (!ret)
                ALOGW("%s: failed to flip plane %d context !\n", __func__, i);
            else
                bufferHandles[numBuffers++] =
                (buffer_handle_t)plane->getDataBufferHandle();

            // clear flip flags, except for DELAY_DISABLE
            mLayerList->setFlags(i, flags & IntelDisplayPlane::DELAY_DISABLE);

            // remove clear fb hints
            list->hwLayers[i].hints &= ~HWC_HINT_CLEAR_FB;
        }

        // commit plane contexts
        if (mFBDev && numBuffers) {
            ALOGD_IF(ALLOW_HWC_PRINT, "%s: commits %d buffers\n", __func__, numBuffers);
            int err = mFBDev->Post2(&mFBDev->base,
                                    bufferHandles,
                                    numBuffers,
                                    context,
                                    mPlaneManager->getContextLength());
            if (err) {
                ALOGE("%s: Post2 failed with errno %d\n", __func__, err);
                return false;
            }
        }
    }

        //make sure all flips were finished
        for (size_t i=0 ; list && i<list->numHwLayers - 1; i++) {
            IntelDisplayPlane *plane = mLayerList->getPlane(i);
            int flags = mLayerList->getFlags(i);
            if (!plane)
                continue;
            if (list->hwLayers[i].flags & HWC_SKIP_LAYER)
                continue;
            if (list->hwLayers[i].compositionType != HWC_OVERLAY)
                continue;
            if (mPlaneManager->isWidiActive())
                continue;

            plane->waitForFlipCompletion();
        }

    return true;
}

uint32_t IntelHWComposer::disableUnusedVsyncs(uint32_t target)
{
    uint32_t unusedVsyncs = mActiveVsyncs & (~target);
    struct drm_psb_vsync_set_arg arg;
    uint32_t vsync;
    int i, ret;

    ALOGV("disableVsync: unusedVsyncs 0x%x\n", unusedVsyncs);

    if (!unusedVsyncs)
        goto disable_out;

    /*disable unused vsyncs*/
    for (i = 0; i < VSYNC_SRC_NUM; i++) {
        vsync = (1 << i);
        if (!(vsync & unusedVsyncs))
            continue;

        /*disable vsync*/
        if (i == VSYNC_SRC_FAKE)
            mFakeVsync->setEnabled(false, mLastVsync);
        else {
            memset(&arg, 0, sizeof(struct drm_psb_vsync_set_arg));
            arg.vsync_operation_mask = VSYNC_DISABLE | GET_VSYNC_COUNT;

            // pipe select
            if (i == VSYNC_SRC_HDMI)
                arg.vsync.pipe = 1;
            else
                arg.vsync.pipe = 0;

            ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_VSYNC_SET,
                                      &arg, sizeof(arg));
            if (ret) {
                ALOGW("%s: failed to enable/disable vsync %d\n", __func__, ret);
                continue;
            }
            mVsyncsEnabled = 0;
            mVsyncsCount = arg.vsync.vsync_count;
            mVsyncsTimestamp = arg.vsync.timestamp;
        }

        /*disabled successfully, remove it from unused vsyncs*/
        unusedVsyncs &= ~vsync;
    }
disable_out:
    return unusedVsyncs;
}

uint32_t IntelHWComposer::enableVsyncs(uint32_t target)
{
    uint32_t enabledVsyncs = 0;
    struct drm_psb_vsync_set_arg arg;
    uint32_t vsync;
    int i, ret;

    ALOGV("enableVsyn: enable vsyncs 0x%x\n", target);

    if (!target) {
        enabledVsyncs = 0;
        goto enable_out;
    }

    // remove all active vsyncs from target
    target &= ~mActiveVsyncs;
    if (!target) {
        enabledVsyncs = mActiveVsyncs;
        goto enable_out;
    }

    // enable vsyncs which is currently inactive
    for (i = 0; i < VSYNC_SRC_NUM; i++) {
        vsync = (1 << i);
        if (!(vsync & target))
            continue;

        /*enable vsync*/
        if (i == VSYNC_SRC_FAKE)
            mFakeVsync->setEnabled(true, mLastVsync);
        else {
            memset(&arg, 0, sizeof(struct drm_psb_vsync_set_arg));
            arg.vsync_operation_mask = VSYNC_ENABLE | GET_VSYNC_COUNT;

            // pipe select
            if (i == VSYNC_SRC_HDMI)
                arg.vsync.pipe = 1;
            else
                arg.vsync.pipe = 0;

            ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_VSYNC_SET,
                                      &arg, sizeof(arg));
            if (ret) {
                ALOGW("%s: failed to enable vsync %d\n", __func__, ret);
                continue;
            }
            mVsyncsEnabled = 1;
            mVsyncsCount = arg.vsync.vsync_count;
            mVsyncsTimestamp = arg.vsync.timestamp;
        }

        /*enabled successfully*/
        enabledVsyncs |= vsync;
    }
enable_out:
    return enabledVsyncs;
}

bool IntelHWComposer::vsyncControl(int enabled)
{
    uint32_t targetVsyncs = 0;
    uint32_t activeVsyncs = 0;
    uint32_t enabledVsyncs = 0;
    IntelWidiPlane* widiPlane = 0;

    ALOGV("vsyncControl, enabled %d\n", enabled);

    if (enabled != 0 && enabled != 1)
        return false;

    android::Mutex::Autolock _l(mLock);

    // for disable vsync request, disable all active vsyncs
    if (!enabled) {
        targetVsyncs = 0;
        goto disable_vsyncs;
    }

    // use fake vsync for widi extend video mode
    widiPlane = (IntelWidiPlane*)mPlaneManager->getWidiPlane();
    if (widiPlane && widiPlane->isActive() &&
        widiPlane->isExtVideoAllowed() &&
        widiPlane->isPlayerOn()) {
        targetVsyncs |= (1 << VSYNC_SRC_FAKE);
    } else if (OVERLAY_EXTEND == mDrm->getDisplayMode()) {
        targetVsyncs |= (1 << VSYNC_SRC_HDMI);
    } else
        targetVsyncs |= (1 << VSYNC_SRC_MIPI);

    // enable selected vsyncs
    enabledVsyncs = enableVsyncs(targetVsyncs);

disable_vsyncs:
    // disable unused vsyncs
    activeVsyncs = disableUnusedVsyncs(targetVsyncs);

    // update active vsyncs
    mActiveVsyncs = enabledVsyncs | activeVsyncs;
    mVsync->setActiveVsyncs(mActiveVsyncs);

    ALOGV("vsyncControl: activeVsyncs 0x%x\n", mActiveVsyncs);
    return true;
}

bool IntelHWComposer::release()
{
    ALOGD("release");

    if (!initCheck() || !mLayerList)
        return false;

    // disable all attached planes
    for (size_t i=0 ; i<mLayerList->getLayersCount() ; i++) {
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

bool IntelHWComposer::dumpDisplayStat()
{
    struct drm_psb_register_rw_arg arg;
    struct drm_psb_vsync_set_arg vsync_arg;
    int ret;

    // dump vsync info
    memset(&vsync_arg, 0, sizeof(struct drm_psb_vsync_set_arg));
    vsync_arg.vsync_operation_mask = GET_VSYNC_COUNT;
    vsync_arg.vsync.pipe = 0;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_VSYNC_SET,
                               &vsync_arg, sizeof(vsync_arg));
    if (ret) {
        ALOGW("%s: failed to dump vsync info %d\n", __func__, ret);
        goto out;
    }

    dumpPrintf("-------------Display Stat -------------------\n");
    dumpPrintf("  + last vsync count: %d, timestamp %d ms \n",
                     mVsyncsCount, mVsyncsTimestamp/1000000);
    dumpPrintf("  + current vsync count: %d, timestamp %d ms \n",
                     vsync_arg.vsync.vsync_count,
                     vsync_arg.vsync.timestamp/1000000);

    // Read pipe stat register
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
    arg.display_read_mask = REGRWBITS_PIPEASTAT;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_REGISTER_RW,
                              &arg, sizeof(arg));
    if (ret) {
        ALOGW("%s: failed to dump display registers %d\n", __func__, ret);
        goto out;
    }

    dumpPrintf("  + PIPEA STAT: 0x%x \n", arg.display.pipestat_a);

    // Read interrupt mask register
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
    arg.display_read_mask = REGRWBITS_INT_MASK;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_REGISTER_RW,
                              &arg, sizeof(arg));
    if (ret) {
        ALOGW("%s: failed to dump display registers %d\n", __func__, ret);
        goto out;
    }

    dumpPrintf("  + INT_MASK_REG: 0x%x \n", arg.display.int_mask);

    // Read interrupt enable register
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
    arg.display_read_mask = REGRWBITS_INT_ENABLE;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_REGISTER_RW,
                              &arg, sizeof(arg));
    if (ret) {
        ALOGW("%s: failed to dump display registers %d\n", __func__, ret);
        goto out;
    }

    dumpPrintf("  + INT_ENABLE_REG: 0x%x \n", arg.display.int_enable);

    // open this if need to dump all display registers.
#if 0
    // dump all display regs in driver
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
    arg.display_read_mask = REGRWBITS_DISPLAY_ALL;

    ret = drmCommandWriteRead(mDrm->getDrmFd(), DRM_PSB_REGISTER_RW,
                              &arg, sizeof(arg));
    if (ret) {
        ALOGW("%s: failed to dump display registers %d\n", __func__, ret);
        goto out;
    }
#endif

out:
    return (ret == 0) ? true : false;
}

bool IntelHWComposer::dump(char *buff,
                           int buff_len, int *cur_len)
{
    IntelDisplayPlane *plane = NULL;
    bool ret = true;
    int i;

    mDumpBuf = buff;
    mDumpBuflen = buff_len;
    mDumpLen = 0;

    IntelWidiPlane* widiPlane = (IntelWidiPlane*)mPlaneManager->getWidiPlane();

    if (mLayerList) {
       dumpPrintf("------------ Totally %d layers -------------\n",
                                     mLayerList->getLayersCount());
       for (i = 0; i < mLayerList->getLayersCount(); i++) {
           plane = mLayerList->getPlane(i);

           if (plane) {
               int planeType = plane->getPlaneType();
               dumpPrintf("   # layer %d attached to %s plane \n", i,
                            (planeType == 3) ? "overlay" : "sprite");
           } else
               dumpPrintf("   # layer %d goes through eglswapbuffer\n ", i);
       }

       dumpPrintf("-------------runtime parameters -------------\n");
       dumpPrintf("  + bypassPost: %d \n", mFBDev->bBypassPost);
       dumpPrintf("  + mForceSwapBuffer: %d \n", mForceSwapBuffer);
       dumpPrintf("  + Display Mode: %d \n", mDrm->getDisplayMode());
       dumpPrintf("  + isHdmiConnected: %d \n",
        (mDrm->getOutputConnection(OUTPUT_HDMI) == DRM_MODE_CONNECTED) ? 1 : 0);
       dumpPrintf("  + isWidiActive: %d \n", (widiPlane->isActive()) ? 1 : 0);
       dumpPrintf("  + mActiveVsyncs: 0x%x, mVsyncsEnabled: %d \n", mActiveVsyncs, mVsyncsEnabled);
    }

    dumpDisplayStat();

    mPlaneManager->dump(mDumpBuf,  mDumpBuflen, &mDumpLen);

    return ret;
}

bool IntelHWComposer::initialize()
{
    bool ret = true;

    //TODO: replace the hard code buffer type later
    int bufferType = IntelBufferManager::TTM_BUFFER;

    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);

    // open IMG frame buffer device
    hw_module_t const* module;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) == 0) {
        IMG_gralloc_module_public_t *imgGrallocModule;
        imgGrallocModule = (IMG_gralloc_module_public_t*)module;
        mFBDev = imgGrallocModule->psFrameBufferDevice;
        mFBDev->bBypassPost = 0; //cfg.bypasspost;
    }

    if (!mFBDev) {
        framebuffer_open(module, (framebuffer_device_t**)&mFBDev);
    }

    if (!mFBDev) {
        ALOGE("%s: failed to open IMG FB device\n", __func__);
        return false;
    }
    mFBDev->hwcRef = true;

    //create new DRM object if not exists
    if (!mDrm) {
        mDrm = &IntelHWComposerDrm::getInstance();
        if (!mDrm) {
            ALOGE("%s: Invalid DRM object\n", __func__);
            ret = false;
            goto drm_err;
        }

        ret = mDrm->initialize(this);
        if (ret == false) {
            ALOGE("%s: failed to initialize DRM instance\n", __func__);
            goto drm_err;
        }
    }

    mVsync = new IntelVsyncEventHandler(this, mDrm->getDrmFd());

    mFakeVsync = new IntelFakeVsyncEvent(this);

    //create new buffer manager and initialize it
    if (!mBufferManager) {
        //mBufferManager = new IntelTTMBufferManager(mDrm->getDrmFd());
	mBufferManager = new IntelBCDBufferManager(mDrm->getDrmFd());
        if (!mBufferManager) {
            ALOGE("%s: Failed to create buffer manager\n", __func__);
            ret = false;
            goto bm_err;
        }
        // do initialization
        ret = mBufferManager->initialize();
        if (ret == false) {
            ALOGE("%s: Failed to initialize buffer manager\n", __func__);
            goto bm_init_err;
        }
    }

    // create buffer manager for gralloc buffer
    if (!mGrallocBufferManager) {
        //mGrallocBufferManager = new IntelPVRBufferManager(mDrm->getDrmFd());
	mGrallocBufferManager = new IntelGraphicBufferManager(mDrm->getDrmFd());
        if (!mGrallocBufferManager) {
            ALOGE("%s: Failed to create Gralloc buffer manager\n", __func__);
            ret = false;
            goto gralloc_bm_err;
        }

        ret = mGrallocBufferManager->initialize();
        if (ret == false) {
            ALOGE("%s: Failed to initialize Gralloc buffer manager\n", __func__);
            goto gralloc_bm_err;
        }
    }

    // create new display plane manager
    if (!mPlaneManager) {
        mPlaneManager =
            new IntelDisplayPlaneManager(mDrm->getDrmFd(),
                                         mBufferManager, mGrallocBufferManager);
        if (!mPlaneManager) {
            ALOGE("%s: Failed to create plane manager\n", __func__);
            goto bm_init_err;
        }
    }

    // create layer list
    mLayerList = new IntelHWComposerLayerList(mPlaneManager);
    if (!mLayerList) {
        ALOGE("%s: Failed to create layer list\n", __func__);
        goto pm_err;
    }

    // init mHDMIBuffers
    memset(mHDMIBuffers, 0, sizeof(mHDMIBuffers));
    memset(&mHDMIFBHandle, 0, sizeof(mHDMIFBHandle));
    mNextBuffer = 0;
    // do mode setting in HWC if HDMI is connected when boot up
    if (mDrm->detectDisplayConnection(OUTPUT_HDMI))
        handleHotplugEvent(1, NULL);

   // startObserver();

    mInitialized = true;

    ALOGD_IF(ALLOW_HWC_PRINT, "%s: successfully\n", __func__);
    return true;

pm_err:
    delete mPlaneManager;
bm_init_err:
    delete mGrallocBufferManager;
gralloc_bm_err:
    delete mBufferManager;
    mBufferManager = 0;
bm_err:
    stopObserver();
observer_err:
    delete mDrm;
    mDrm = 0;
drm_err:
    return ret;
}

bool IntelHWComposer::prepareDisplays(size_t numDisplays,
                                      hwc_display_contents_1_t** displays)
{
    int disp;

    for (disp = 0; disp < numDisplays; disp++) {
        prepare(disp, displays[disp]);
    }

    return true;
}

bool IntelHWComposer::commitDisplays(size_t numDisplays,
                                     hwc_display_contents_1_t** displays)
{

    if (!initCheck()) {
        ALOGE("%s: failed to initialize HWComposer\n", __func__);
        return false;
    }

    android::Mutex::Autolock _l(mLock);

    // if hotplug was happened & didn't be handled skip the flip
    if (mHotplugEvent)
        return true;

    void *context = mPlaneManager->getPlaneContexts();
    if (!context) {
        ALOGE("%s: invalid plane contexts\n", __func__);
        return false;
    }

    ALOGD_IF(ALLOW_HWC_PRINT,
        "%s: num of displays: %d\n", __func__, numDisplays);
    int disp;
    int numBuffers = 0;
    buffer_handle_t bufferHandles[INTEL_DISPLAY_PLANE_NUM];
    for (disp = 0; disp < numDisplays; disp ++) {

        hwc_display_contents_1_t *list = displays[disp];
        if (!list)
            continue;

        if (disp == 0) {
            //dumpLayerList(list);
            // need check whether eglSwapBuffers is necessary
            bool needSwapBuffer = false;

            // if all layers were attached with display planes then we don't need
            // swap buffers.

            if (!mLayerList->getLayersCount() ||
                    mLayerList->getLayersCount() != mLayerList->getAttachedPlanesCount() ||
                    mForceSwapBuffer) {
                ALOGD_IF(ALLOW_HWC_PRINT,
                        "%s: mForceSwapBuffer: %d, layer count: %d, attached plane:%d\n",
                        __func__, mForceSwapBuffer, mLayerList->getLayersCount(),
                        mLayerList->getAttachedPlanesCount());
                needSwapBuffer = true;
            }

            // setup primary plane contexts if swap buffers is needed
            if (needSwapBuffer && list->hwLayers[list->numHwLayers-1].handle) {
                flipFramebufferContexts(context);
                bufferHandles[numBuffers++] = list->hwLayers[list->numHwLayers-1].handle;
            }

            // Call plane's flip for each layer in hwc_layer_list, if a plane has
            // been attached to a layer
            // First post RGB layers, then overlay layers.
            for (size_t i=0 ; list && i<list->numHwLayers - 1; i++) {
                IntelDisplayPlane *plane = mLayerList->getPlane(i);
                int flags = mLayerList->getFlags(i);

                if (!plane)
                    continue;
                if (list->hwLayers[i].flags & HWC_SKIP_LAYER)
                    continue;

                if (list->hwLayers[i].compositionType != HWC_OVERLAY)
                    continue;

                ALOGD_IF(ALLOW_HWC_PRINT, "%s: flip plane %d, flags: 0x%x\n",
                        __func__, i, flags);

                bool ret = plane->flip(context, flags);
                if (!ret)
                    ALOGW("%s: skip to flip plane %d context !\n", __func__, i);
                else
                    bufferHandles[numBuffers++] =
                        (buffer_handle_t)plane->getDataBufferHandle();

                // clear flip flags, except for DELAY_DISABLE
                mLayerList->setFlags(i, flags & IntelDisplayPlane::DELAY_DISABLE);

                // remove clear fb hints
                list->hwLayers[i].hints &= ~HWC_HINT_CLEAR_FB;
            }
        } else if (disp == 1) {
            //dumpLayerList(list);

            hwc_layer_1_t *target_layer = NULL;
            if (list->hwLayers[list->numHwLayers-1].compositionType == HWC_FRAMEBUFFER_TARGET &&
                list->hwLayers[list->numHwLayers-1].handle) {
                target_layer = &list->hwLayers[list->numHwLayers-1];

                bool ret = flipHDMIFramebufferContexts(context, target_layer);
                if (!ret)
                    ALOGV("%s: skip to flip HDMI fb context !\n", __func__);
                else
                    bufferHandles[numBuffers++] = target_layer->handle;
            }
        }
    }

        // commit plane contexts
    if (mFBDev && numBuffers) {
        ALOGD_IF(ALLOW_HWC_PRINT, "%s: commits %d buffers\n", __func__, numBuffers);
        int err = mFBDev->Post2(&mFBDev->base,
                bufferHandles,
                numBuffers,
                context,
                mPlaneManager->getContextLength());
        if (err) {
            ALOGE("%s: Post2 failed with errno %d\n", __func__, err);
            return false;
        }
    }

    return true;
}

bool IntelHWComposer::blankDisplay(int disp, int blank)
{
    return true;
}

bool IntelHWComposer::getDisplayConfigs(int disp, uint32_t* configs, 
                                        size_t* numConfigs)
{
    if (!numConfigs || !numConfigs[0])
        return false;

    if (disp == HWC_DISPLAY_PRIMARY) {
        *numConfigs = 1;
        configs[0] = 0;
        return true;
    } else if (disp == HWC_DISPLAY_EXTERNAL) {
       if (mDrm->getOutputConnection(OUTPUT_HDMI) == DRM_MODE_CONNECTED)
       {
ALOGD("get display config for HDMI");
        *numConfigs = 1;
        configs[0] = 0;
        return true;
       }
    }

    return false;
}

bool IntelHWComposer::getDisplayAttributes(int disp, uint32_t config,
            const uint32_t* attributes, int32_t* values)
{
    if (!attributes || !values)
        return false;

    if (disp == HWC_DISPLAY_PRIMARY && config == 0) {
        while (*attributes != HWC_DISPLAY_NO_ATTRIBUTE) {
            switch (*attributes) {
            case HWC_DISPLAY_VSYNC_PERIOD:
                *values = 1e9 / mFBDev->base.fps;
                break;
            case HWC_DISPLAY_WIDTH:
                *values = mFBDev->base.width;
                break;
            case HWC_DISPLAY_HEIGHT:
                *values = mFBDev->base.height;
                break;
            case HWC_DISPLAY_DPI_X:
                *values =  mFBDev->base.xdpi;
                break;
            case HWC_DISPLAY_DPI_Y:
                *values =  mFBDev->base.ydpi;
                break;
            default:
                break;
            }
            attributes ++;
            values ++;
        }
        return true;
    }
    else if (disp == HWC_DISPLAY_EXTERNAL && config == 0) {
        if (mDrm->getOutputConnection(OUTPUT_HDMI) == DRM_MODE_CONNECTED)
        {
            drmModeModeInfoPtr mode = mDrm->getOutputMode(OUTPUT_HDMI);
   ALOGD("getDisplayAttribute for HDMI: %d x %d x %d", mode->hdisplay,
        mode->vdisplay, mode->vrefresh);
 
            while (*attributes != HWC_DISPLAY_NO_ATTRIBUTE) {
            switch (*attributes) {
            case HWC_DISPLAY_VSYNC_PERIOD:
                *values = 1e9 / mode->vrefresh;
                break;
            case HWC_DISPLAY_WIDTH:
                *values = mode->hdisplay;
                break;
            case HWC_DISPLAY_HEIGHT:
                *values = mode->vdisplay;
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
    }
    return false;
}

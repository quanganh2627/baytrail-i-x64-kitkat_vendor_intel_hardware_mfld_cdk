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

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <IntelHWComposer.h>
#include <IntelOverlayUtil.h>
#include <IntelWidiPlane.h>


IntelHWComposer::~IntelHWComposer()
{
    LOGV("%s\n", __func__);

    delete mLayerList;
    delete mPlaneManager;
    delete mBufferManager;
    delete mDrm;

    // stop uevent observer
    stopObserver();
}

bool IntelHWComposer::overlayPrepare(int index, hwc_layer_t *layer, int flags)
{
    if (!layer) {
        LOGE("%s: Invalid layer\n", __func__);
        return false;
    }

    // allocate overlay plane
    IntelDisplayPlane *plane = mPlaneManager->getOverlayPlane();
    if (!plane) {
        LOGE("%s: failed to create overlay plane\n", __func__);
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

    layer->hints = HWC_HINT_CLEAR_FB;
    layer->compositionType = HWC_OVERLAY;

    return true;
}

bool IntelHWComposer::spritePrepare(int index, hwc_layer_t *layer, int flags)
{
    if (!layer) {
        LOGE("%s: Invalid layer\n", __func__);
        return false;
    }
    int dstLeft = layer->displayFrame.left;
    int dstTop = layer->displayFrame.top;
    int dstRight = layer->displayFrame.right;
    int dstBottom = layer->displayFrame.bottom;

    // allocate sprite plane
    IntelDisplayPlane *plane = mPlaneManager->getSpritePlane();
    if (!plane) {
        LOGE("%s: failed to create sprite plane\n", __func__);
        return false;
    }

    // setup plane parameters
    plane->setPosition(dstLeft, dstTop, dstRight, dstBottom);

    // attach plane to hwc layer
    mLayerList->attachPlane(index, plane, flags);

    // Ask surfaceflinger to skip this layer only if surface was changed
    if (flags & IntelDisplayPlane::UPDATE_SURFACE) {
        layer->hints = HWC_HINT_TRIPLE_BUFFER;
        layer->compositionType = HWC_OVERLAY;
    } else
        layer->compositionType = HWC_FRAMEBUFFER;

    return true;
}

bool IntelHWComposer::isOverlayHandle(uint32_t handle)
{
    uint32_t devId = (handle & 0x0000ffff);
    uint32_t bufId = ((handle & 0xffff0000) >> 16);

    if (devId >= INTEL_BCD_DEVICE_NUM_MAX || bufId >= INTEL_BCD_BUFFER_NUM_MAX)
        return false;
    return true;
}

bool IntelHWComposer::isSpriteHandle(uint32_t handle)
{
    // handle exists and not a TS handle
    if (!handle || ((handle & 0xff000f) == handle))
        return false;
    return true;
}

// TODO: re-implement this function after video interface
// is ready.
// Currently, LayerTS::setGeometry will set compositionType
// to HWC_OVERLAY. HWC will change it to HWC_FRAMEBUFFER
// if HWC found this layer was NOT a overlay layer (can NOT
// be handled by hardware overlay)
bool IntelHWComposer::isOverlayLayer(hwc_layer_list_t *list,
                                     int index,
                                     hwc_layer_t *layer,
                                     int& flags)
{
    if (!list || !layer)
        return false;

    IntelWidiPlane* widiPlane = (IntelWidiPlane*)mPlaneManager->getWidiPlane();

    // TODO: enable this when ST is ready
    intel_gralloc_buffer_handle_t *grallocHandle =
        (intel_gralloc_buffer_handle_t*)layer->handle;

    if (!grallocHandle)
        return false;

    // TODO: check buffer usage
    if (grallocHandle->format != HAL_PIXEL_FORMAT_YV12 &&
        grallocHandle->format != HAL_PIXEL_FORMAT_INTEL_HWC_NV12 &&
        grallocHandle->format != HAL_PIXEL_FORMAT_INTEL_HWC_YUY2 &&
        grallocHandle->format != HAL_PIXEL_FORMAT_INTEL_HWC_UYVY &&
        grallocHandle->format != HAL_PIXEL_FORMAT_INTEL_HWC_I420)
        return false;

    // force to use overlay in video extend mode
    intel_overlay_mode_t displayMode = mDrm->getDisplayMode();
    if(widiPlane->isStreaming())
        displayMode = OVERLAY_EXTEND;

    if (displayMode == OVERLAY_EXTEND) {
        // clear HWC_SKIP_LAYER flag so that force to use overlay
        layer->flags &= ~HWC_SKIP_LAYER;
        mLayerList->setForceOverlay(index, true);
        goto use_overlay;
    }

    // fall back if HWC_SKIP_LAYER was set
    if ((layer->flags & HWC_SKIP_LAYER))
        return false;

    if (widiPlane->isActive() &&  !(widiPlane->isExtVideoAllowed())) {
        /* if extended video mode is not allowed we stop here and
         * let the video to be rendered via GFx plane by surface flinger
         */
        return false;
    }

    // check whether layer are covered by layers above it
    for (size_t i = index + 1; i < list->numHwLayers; i++) {
        if (areLayersIntersecting(&list->hwLayers[i], layer)) {
            LOGV("%s: overlay %d is covered by layer %d\n", __func__, index, i);
            return false;
        }
    }

use_overlay:
    LOGV("%s: switch to overlay\n", __func__);

    // set flags to 0, overlay plane will handle the flags itself
    // based on data buffer change
    flags = 0;
    return true;
}

bool IntelHWComposer::isSpriteLayer(hwc_layer_list_t *list,
                                    int index,
                                    hwc_layer_t *layer,
                                    int& flags)
{
    if (!list || !layer)
        return false;

    if ((mPlaneManager->isWidiActive() == false) &&
        (mDrm->getOutputConnection(OUTPUT_HDMI) != DRM_MODE_CONNECTED)) {
        intel_gralloc_buffer_handle_t *grallocHandle =
            (intel_gralloc_buffer_handle_t*)layer->handle;

        if (!grallocHandle) {
            LOGV("%s: invalid gralloc handle\n", __func__);
            return false;
        }

        // if buffer was accessed by SW don't handle it
        if (grallocHandle->usage &
           (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK))
            return false;

        // TODO: remove it after bypass composition enabled for rotated layers
        if (layer->transform)
            return false;

        // check pixel format
        if (grallocHandle->format != HAL_PIXEL_FORMAT_RGB_565 &&
            grallocHandle->format != HAL_PIXEL_FORMAT_BGRA_8888 &&
            grallocHandle->format != HAL_PIXEL_FORMAT_BGRX_8888 &&
            grallocHandle->format != HAL_PIXEL_FORMAT_RGBX_8888 &&
            grallocHandle->format != HAL_PIXEL_FORMAT_RGBA_8888) {
            LOGV("%s: invalid format 0x%x\n", __func__, grallocHandle->format);
            return false;
        }

        // use the sprite plane only when it's the top layer
        int srcWidth = grallocHandle->width;
        int srcHeight = grallocHandle->height;
        int dstWidth = layer->displayFrame.right - layer->displayFrame.left;
        int dstHeight = layer->displayFrame.bottom - layer->displayFrame.top;

        if (((size_t)index == (list->numHwLayers - 1)) &&
            (list->numHwLayers == 1) &&
            (srcWidth == dstWidth) &&
            (srcHeight == dstHeight)) {
            LOGV("%s: got a bypass layer, %dx%d\n", __func__, srcWidth, srcHeight);
            flags |= IntelDisplayPlane::UPDATE_SURFACE;
            return true;
        }
    }

    return false;
}

// When the geometry changed, we need
// 0) reclaim all allocated planes, reclaimed planes will be disabled
//    on the start of next frame. A little bit tricky, we cannot disable the
//    planes right after geometry is changed since there's no data in FB now,
//    so we need to wait FB is update then disable these planes.
// 1) build a new layer list for the changed hwc_layer_list
// 2) attach planes to these layers which can be handled by HWC
void IntelHWComposer::onGeometryChanged(hwc_layer_list_t *list)
{
     bool firstTime = true;
     // reclaim all planes
     bool ret = mLayerList->invalidatePlanes();
     if (!ret) {
         LOGE("%s: failed to reclaim allocated planes\n", __func__);
         return;
     }

     // update layer list with new list
     mLayerList->updateLayerList(list);

     for (size_t i = 0 ; i < list->numHwLayers ; i++) {
	 // check whether a layer can be handled in general
         if (!isHWCLayer(&list->hwLayers[i]))
             continue;

         // further check whether a layer can be handle by overlay/sprite
	 int flags = 0;
         if (isOverlayLayer(list, i, &list->hwLayers[i], flags)) {
             ret = overlayPrepare(i, &list->hwLayers[i], flags);
             if (!ret) {
                 LOGE("%s: failed to prepare overlay\n", __func__);
                 list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
             }
         } else if (isSpriteLayer(list, i, &list->hwLayers[i], flags)) {
             ret = spritePrepare(i, &list->hwLayers[i], flags);
             if (!ret) {
                 LOGE("%s: failed to prepare sprite\n", __func__);
                 list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
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

     // disable reclaimed planes
     mPlaneManager->disableReclaimedPlanes(IntelDisplayPlane::DISPLAY_PLANE_SPRITE);
}

// This function performs:
// 1) update layer's transform to data buffer's payload buffer, so that video
//    driver can get the latest transform info of this layer
// 2) if rotation is needed, video driver would setup the rotated buffer, then
//    update buffer's payload to inform HWC rotation buffer is available.
// 3) HWC would keep using ST till all rotation buffers are ready.
// Return: false if HWC is NOT ready to switch to overlay, otherwise true.
bool IntelHWComposer::useOverlayRotation(hwc_layer_t *layer,
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
        IntelDisplayBuffer *buffer =
            mGrallocBufferManager->map(grallocHandle->fd[GRALLOC_SUB_BUFFER1]);
        if (!buffer) {
            LOGE("%s: failed to map payload buffer.\n", __func__);
            return false;
        }

        intel_gralloc_payload_t *payload =
            (intel_gralloc_payload_t*)buffer->getCpuAddr();

        // unmap payload buffer
        mGrallocBufferManager->unmap(buffer);
        if (!payload) {
            LOGE("%s: invalid address\n", __func__);
            return false;
        }
        metadata_transform = payload->metadata_transform;

        //For extend mode, we ignore WM rotate info
        if (displayMode == OVERLAY_EXTEND) {
            transform = metadata_transform;
        }

        if (!transform) {
            LOGV("%s: use overlay to display original buffer.", __func__);
            return true;
        }

        if (transform != payload->client_transform) {
            LOGV("%s: rotation buffer is not done by client, fallback...need rotate buffer with degree %d\n",
                __func__, transform);
            return false;
        }

        // update handle, w & h to rotation buffer
        handle = payload->rotated_buffer_handle;
        w = payload->rotated_width;
        h = payload->rotated_height;
        // NOTE: exchange the srcWidth & srcHeight since
        // video driver currently doesn't call native_window_*
        // helper functions to update info for rotation buffer.
        if (transform == HAL_TRANSFORM_ROT_90 ||
                transform == HAL_TRANSFORM_ROT_270) {
            int temp = srcH;
            srcH = srcW;
            srcW = temp;
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
        if ((displayMode != OVERLAY_EXTEND) && transform)
            return false;
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
bool IntelHWComposer::isBobDeinterlace(hwc_layer_t *layer)
{
    int bobDeinterlace = 0;

    LOGE("useOverlayRotation enter.\n");

    if (!layer)
        return bobDeinterlace;

    intel_gralloc_buffer_handle_t *grallocHandle =
        (intel_gralloc_buffer_handle_t*)layer->handle;

    if (!grallocHandle) {
        LOGE("1.\n");
        return bobDeinterlace;
    }

    // map payload buffer
    IntelDisplayBuffer *buffer =
        mGrallocBufferManager->map(grallocHandle->fd[GRALLOC_SUB_BUFFER1]);
    if (!buffer) {
        LOGE("%s: failed to map payload buffer.\n", __func__);
        return false;
    }

    intel_gralloc_payload_t *payload =
        (intel_gralloc_payload_t*)buffer->getCpuAddr();
    if (!payload) {
        LOGE("%s: invalid address\n", __func__);
        return bobDeinterlace;
    }

    bobDeinterlace = payload->bob_deinterlace;

    mGrallocBufferManager->unmap(buffer);
    return bobDeinterlace;
}

// when buffer handle is changed, we need
// 0) get plane's data buffer if a plane was attached to a layer
// 1) update plane's data buffer with the new buffer handle
// 2) set the updated data buffer back to plane
bool IntelHWComposer::updateLayersData(hwc_layer_list_t *list)
{
    IntelDisplayPlane *plane = 0;
    IntelWidiPlane* widiplane = NULL;
    bool ret;

    drmModeConnection mipi0 = IntelHWComposerDrm::getInstance().getOutputConnection(OUTPUT_MIPI0);
    drmModeConnection hdmi = IntelHWComposerDrm::getInstance().getOutputConnection(OUTPUT_HDMI);

    drmModeFBPtr crtcMode;
    if (hdmi == DRM_MODE_CONNECTED) {
        crtcMode = IntelHWComposerDrm::getInstance().getOutputFBInfo(OUTPUT_HDMI);
    } else {
        crtcMode = IntelHWComposerDrm::getInstance().getOutputFBInfo(OUTPUT_MIPI0);
    }
    int active_width = crtcMode->width;
    int active_height = crtcMode->height;

    if (mPlaneManager->isWidiActive()) {
         widiplane = (IntelWidiPlane*) mPlaneManager->getWidiPlane();
    }


    for (size_t i=0 ; i<list->numHwLayers ; i++) {
        plane = mLayerList->getPlane(i);
        if (plane) {
            hwc_layer_t *layer = &list->hwLayers[i];
            int bobDeinterlace;
            int srcX = layer->sourceCrop.left;
            int srcY = layer->sourceCrop.top;
            int srcWidth = layer->sourceCrop.right - layer->sourceCrop.left;
            int srcHeight = layer->sourceCrop.bottom - layer->sourceCrop.top;
            int planeType = plane->getPlaneType();

            if(srcHeight == 1 || srcWidth == 1) {
                mLayerList->detachPlane(i, plane);
                layer->compositionType = HWC_FRAMEBUFFER;
                continue;
            }

            // get & setup overlay data buffer
            IntelDisplayBuffer *buffer = plane->getDataBuffer();
            IntelDisplayDataBuffer *dataBuffer =
                reinterpret_cast<IntelDisplayDataBuffer*>(buffer);
            if (!dataBuffer) {
                LOGE("%s: invalid overlay data buffer\n", __func__);
                continue;
            }

            intel_gralloc_buffer_handle_t *grallocHandle =
                    (intel_gralloc_buffer_handle_t*)layer->handle;

            // if invalid gralloc buffer handle, throw back this layer to SF
            if (!grallocHandle) {
                    LOGE("%s: invalid buffer handle\n", __func__);
                    mLayerList->detachPlane(i, plane);
                    layer->compositionType = HWC_FRAMEBUFFER;
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
                   continue;
                }

                IntelOverlayContext *overlayContext =
                    reinterpret_cast<IntelOverlayContext*>(plane->getContext());

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
                    layer->compositionType = HWC_FRAMEBUFFER;
                    layer->hints &= ~HWC_HINT_CLEAR_FB;
                    continue;
                }

                bobDeinterlace = isBobDeinterlace(layer);
                int flags = mLayerList->getFlags(i);
                if (bobDeinterlace) {
                    flags |= IntelDisplayPlane::BOB_DEINTERLACE;
                } else {
                    flags &= ~IntelDisplayPlane::BOB_DEINTERLACE;
                }
                mLayerList->setFlags(i, flags);

                // clear FB first on first overlay frame
                if (layer->compositionType == HWC_FRAMEBUFFER)
                    layer->hints = HWC_HINT_CLEAR_FB;

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

                // set the data buffer back to plane
                ret = ((IntelOverlayPlane*)plane)->setDataBuffer(bufferHandle,
                                                                 transform,
                                                                 grallocHandle);
                if (!ret) {
                    LOGE("%s: failed to update overlay data buffer\n", __func__);
                    mLayerList->detachPlane(i, plane);
                    layer->compositionType = HWC_FRAMEBUFFER;
                }


            } else if (planeType == IntelDisplayPlane::DISPLAY_PLANE_SPRITE) {

                // set data buffer format
                dataBuffer->setFormat(format);
                dataBuffer->setWidth(bufferWidth);
                dataBuffer->setHeight(bufferHeight);
                if (srcWidth > active_width)
                    srcWidth = active_width;
                if (srcHeight > active_height)
                    srcHeight = active_height;
                dataBuffer->setCrop(srcX, srcY, srcWidth, srcHeight);
                dataBuffer->setDeinterlaceType(bobDeinterlace);
                // set the data buffer back to plane
                ret = plane->setDataBuffer(bufferHandle, transform, grallocHandle);
                if (!ret) {
                    LOGE("%s: failed to update sprite data buffer\n", __func__);
                    mLayerList->detachPlane(i, plane);
                    layer->compositionType = HWC_FRAMEBUFFER;
                }
            } else {
                LOGW("%s: invalid plane type %d\n", __func__, planeType);
                continue;
            }
        }
    }

    return true;
}

// Check the usage of a buffer
// only following usages were accepted:
// TODO: list valid usages
bool IntelHWComposer::isHWCUsage(int usage)
{
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
bool IntelHWComposer::isHWCLayer(hwc_layer_t *layer)
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
bool IntelHWComposer::areLayersIntersecting(hwc_layer_t *top,
                                            hwc_layer_t* bottom)
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

void IntelHWComposer::handleHotplugEvent()
{
    LOGV("handleHotplugEvent");

    // go through layer list and call plane's onModeChange()
    for (size_t i = 0 ; i < mLayerList->getLayersCount(); i++) {
        IntelDisplayPlane *plane = mLayerList->getPlane(i);
        if (plane)
            plane->onDrmModeChange();
    }
}

void IntelHWComposer::onUEvent(const char *msg, int msgLen, int msgType)
{
    android::Mutex::Autolock _l(mLock);

    // if send by mds, set the hotplug event and return
    if (msgType == IntelExternalDisplayMonitor::MSG_TYPE_MDS) {
        LOGD("%s: got multiDisplay service event\n", __func__);
        mDrm->detectDrmModeInfo();
        handleHotplugEvent();
        mHotplugEvent = true;
        return;
    }

    if (strcmp(msg, "change@/devices/pci0000:00/0000:00:02.0/drm/card0"))
        return;
    msg += strlen(msg) + 1;

    do {
        if (!strncmp(msg, "HOTPLUG=1", strlen("HOTPLUG=1"))) {
            LOGD("%s: detected hdmi hotplug event\n", __func__);
            mDrm->detectDrmModeInfo();
            handleHotplugEvent();
            mHotplugEvent = true;
            break;
        }
        msg += strlen(msg) + 1;
    } while (*msg);
}

bool IntelHWComposer::prepare(hwc_layer_list_t *list)
{
    LOGV("%s\n", __func__);

    if (!initCheck()) {
        LOGE("%s: failed to initialize HWComposer\n", __func__);
        return false;
    }

    if (!list) {
        LOGE("%s: Invalid hwc_layer_list\n", __func__);
        return false;
    }

    android::Mutex::Autolock _l(mLock);

    // disable useless overlay planes
    mPlaneManager->disableReclaimedPlanes(IntelDisplayPlane::DISPLAY_PLANE_OVERLAY);

    // handle geometry changing. attach display planes to layers
    // which can be handled by HWC.
    // plane control information (e.g. position) will be set here
    if ((list->flags & HWC_GEOMETRY_CHANGED) || mHotplugEvent
        || mPlaneManager->isWidiStatusChanged()) {
        onGeometryChanged(list);
        if (mHotplugEvent)
            mHotplugEvent = false;
    }

    // handle buffer changing. setup data buffer.
    if (!updateLayersData(list)) {
        LOGE("%s: failed to update layer data\n", __func__);
        return false;
    }

    return true;
}

bool IntelHWComposer::commit(hwc_display_t dpy,
                             hwc_surface_t sur,
                             hwc_layer_list_t *list)
{
    LOGV("%s\n", __func__);
    if (!initCheck()) {
        LOGE("%s: failed to initialize HWComposer\n", __func__);
        return false;
    }

    android::Mutex::Autolock _l(mLock);

    //HWComposer::release(). Need to reclaim and disable all the planes.
    if (!dpy || !sur || !list) {
        bool ret = mLayerList->invalidatePlanes();
        if (!ret) {
            LOGE("%s: failed to reclaim allocated planes\n", __func__);
            return false;
        }
        mPlaneManager->disableReclaimedPlanes(IntelDisplayPlane::DISPLAY_PLANE_OVERLAY);
        mPlaneManager->disableReclaimedPlanes(IntelDisplayPlane::DISPLAY_PLANE_SPRITE);
        return true;
    }

    void *context = mPlaneManager->getPlaneContexts();
    if (!context) {
        LOGE("%s: invalid plane contexts\n", __func__);
        return false;
    }

    if(mPlaneManager->isWidiActive()) {
        IntelDisplayPlane *p = mPlaneManager->getWidiPlane();
        LOGV("Widi Plane is %p",p);
         if (p)
             p->flip(context, 0);
         else
             LOGE("Widi Plane is NULL");
    }

    // Call plane's flip for each layer in hwc_layer_list, if a plane has
    // been attached to a layer
    buffer_handle_t bufferHandles[INTEL_DISPLAY_PLANE_NUM];
    int numBuffers = 0;

    for (size_t i=0 ; i<list->numHwLayers ; i++) {
        IntelDisplayPlane *plane = mLayerList->getPlane(i);
        int flags = mLayerList->getFlags(i);
        if (plane &&
            (!(list->hwLayers[i].flags & HWC_SKIP_LAYER)) &&
            (list->hwLayers[i].compositionType == HWC_OVERLAY)) {
            bool ret = plane->flip(context, flags);
            if (!ret)
                LOGW("%s: failed to flip plane %d\n", __func__, i);

            // add bufferHandle
            // NOTE: since overlay flip was not using Post2 yet
            // so only append the sprite plane buffers right now
            // TODO: support both overlay & sprite flip via Post2
            if (plane->getPlaneType() == IntelDisplayPlane::DISPLAY_PLANE_SPRITE)
                bufferHandles[numBuffers++] =
                    (buffer_handle_t)plane->getDataBufferHandle();
        }

        // clear flip flags
        mLayerList->setFlags(i, 0);

        // remove hints
        list->hwLayers[i].hints = 0;
    }

    // commit plane contexts
    if (mFBDev && numBuffers) {
        LOGV("%s: commits %d buffers\n", __func__, numBuffers);
        int err = mFBDev->Post2(&mFBDev->base,
                                bufferHandles,
                                numBuffers,
                                context,
                                mPlaneManager->getContextLength());
        if (err) {
            LOGE("%s: Post2 failed with errno %d\n", __func__, err);
        }
    }

    // check whether eglSwapBuffers is still needed for the given layer list
    if (list->flags & HWC_NEED_SWAPBUFFERS) {
        EGLBoolean sucess = eglSwapBuffers((EGLDisplay)dpy, (EGLSurface)sur);
        if (!sucess) {
            return false;
        }
    }

    return true;
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

    if (mLayerList) {
           for (i = 0; i < mLayerList->getLayersCount(); i++) {
                       dumpPrintf("     layer %d:\n", i);

                       plane = mLayerList->getPlane(i);
                       if (plane) {
                               int planeType = plane->getPlaneType();

                               dumpPrintf("    PlaneType: %s\n",
                                   planeType ? "sprite" : "overlay");
                       }
           }
    }

    mPlaneManager->dump(mDumpBuf,  mDumpBuflen, &mDumpLen);

    return ret;
}

bool IntelHWComposer::initialize()
{
    bool ret = true;

    //TODO: replace the hard code buffer type later
    int bufferType = IntelBufferManager::TTM_BUFFER;

    LOGV("%s\n", __func__);

    // open IMG frame buffer device
    hw_module_t const* module;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) == 0) {
        IMG_gralloc_module_public_t *imgGrallocModule;
        imgGrallocModule = (IMG_gralloc_module_public_t*)module;
        mFBDev = imgGrallocModule->psFrameBufferDevice;
        //mFBDev->bBypassPost = 1;
    }

    if (!mFBDev) {
        LOGE("%s: failed to open IMG FB device\n", __func__);
        return false;
    }

    //create new DRM object if not exists
    if (!mDrm) {
        ret = IntelHWComposerDrm::getInstance().initialize(this);
        if (ret == false) {
            LOGE("%s: failed to initialize DRM instance\n", __func__);
            goto drm_err;
        }

        mDrm = &IntelHWComposerDrm::getInstance();
    }

    if (!mDrm) {
        LOGE("%s: Invalid DRM object\n", __func__);
        ret = false;
        goto drm_err;
    }

    //create new buffer manager and initialize it
    if (!mBufferManager) {
        //mBufferManager = new IntelTTMBufferManager(mDrm->getDrmFd());
	mBufferManager = new IntelBCDBufferManager(mDrm->getDrmFd());
        if (!mBufferManager) {
            LOGE("%s: Failed to create buffer manager\n", __func__);
            ret = false;
            goto bm_err;
        }
        // do initialization
        ret = mBufferManager->initialize();
        if (ret == false) {
            LOGE("%s: Failed to initialize buffer manager\n", __func__);
            goto bm_init_err;
        }
    }

    // create buffer manager for gralloc buffer
    if (!mGrallocBufferManager) {
        //mGrallocBufferManager = new IntelPVRBufferManager(mDrm->getDrmFd());
	mGrallocBufferManager = new IntelGraphicBufferManager(mDrm->getDrmFd());
        if (!mGrallocBufferManager) {
            LOGE("%s: Failed to create Gralloc buffer manager\n", __func__);
            ret = false;
            goto gralloc_bm_err;
        }

        ret = mGrallocBufferManager->initialize();
        if (ret == false) {
            LOGE("%s: Failed to initialize Gralloc buffer manager\n", __func__);
            goto gralloc_bm_err;
        }
    }

    // create new display plane manager
    if (!mPlaneManager) {
        mPlaneManager =
            new IntelDisplayPlaneManager(mDrm->getDrmFd(),
                                         mBufferManager, mGrallocBufferManager);
        if (!mPlaneManager) {
            LOGE("%s: Failed to create plane manager\n", __func__);
            goto bm_init_err;
        }
    }

    // create layer list
    mLayerList = new IntelHWComposerLayerList(mPlaneManager);
    if (!mLayerList) {
        LOGE("%s: Failed to create layer list\n", __func__);
        goto pm_err;
    }

    mInitialized = true;

    LOGV("%s: successfully\n", __func__);
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

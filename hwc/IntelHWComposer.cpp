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

IntelHWComposer::~IntelHWComposer()
{
    LOGV("%s\n", __func__);

    delete mLayerList;
    delete mPlaneManager;
    delete mBufferManager;
    delete mDrm;
}

bool IntelHWComposer::overlayPrepare(int index, hwc_layer_t *layer, int flags)
{
    if (!layer) {
        LOGE("%s: Invalid layer\n", __func__);
        return false;
    }

    int dstLeft = layer->displayFrame.left;
    int dstTop = layer->displayFrame.top;
    int dstRight = layer->displayFrame.right;
    int dstBottom = layer->displayFrame.bottom;

    // allocate overlay plane
    IntelDisplayPlane *plane = mPlaneManager->getOverlayPlane();
    if (!plane) {
        LOGE("%s: failed to create overlay plane\n", __func__);
        return false;
    }

    // setup plane parameters
    plane->setPosition(dstLeft, dstTop, dstRight, dstBottom);

    // attach plane to hwc layer
    mLayerList->attachPlane(index, plane, flags);

    // tell Surfaceflinger clear FB and skip this layer
    layer->hints = HWC_HINT_CLEAR_FB;
    layer->compositionType = HWC_OVERLAY;

    return true;
}

bool IntelHWComposer::spritePrepare(int index, hwc_layer_t *layer, int flags)
{
#if 0
    if (!layer) {
        LOGE("%s: Invalid layer\n", __func__);
        return false;
    }
    int srcX = layer->sourceCrop.left;
    int srcY = layer->sourceCrop.top;
    int srcWidth = layer->sourceCrop.right - layer->sourceCrop.left;
    int srcHeight = layer->sourceCrop.bottom - layer->sourceCrop.top;
    int dstLeft = layer->displayFrame.left;
    int dstTop = layer->displayFrame.top;
    int dstRight = layer->displayFrame.right;
    int dstBottom = layer->displayFrame.bottom;

    if (!isSpriteHandle((uint32_t)layer->handle))
        return false;

    // allocate sprite plane
    IntelDisplayPlane *plane = mPlaneManager->getSpritePlane();
    if (!plane) {
        LOGE("%s: failed to create sprite plane\n", __func__);
        return false;
    }

    // map data buffer
    intel_gralloc_buffer_handle_t *handle =
        (intel_gralloc_buffer_handle_t*)layer->handle;

    // invalidate plane's data buffer
    plane->invalidateDataBuffer();

    LOGE("%s: mapping fd %d\n", __func__, handle->fd);

    // update data buffer
    IntelDisplayBuffer *buffer = mGrallocBufferManager->map(handle->fd);
    if (!buffer) {
        LOGE("%s: failed to map buffer %d\n", __func__, handle->fd);
        mLayerList->detachPlane(index, plane);
        layer->compositionType = HWC_FRAMEBUFFER;
        return false;
    }

    LOGE("%s: mapped successfully\n", __func__);

    IntelDisplayDataBuffer *dataBuffer =
        reinterpret_cast<IntelDisplayDataBuffer*>(plane->getDataBuffer());

    dataBuffer->setBuffer(buffer);
    dataBuffer->setFormat(handle->format);
    dataBuffer->setWidth(handle->width);
    dataBuffer->setHeight(handle->height);
    dataBuffer->setCrop(srcX, srcY, srcWidth, srcHeight);

    // delete buffer
    delete buffer;

    // set the data buffer back to plane
    plane->setDataBuffer(*dataBuffer);

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
#endif

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

    // TODO: enable this when ST is ready
    //intel_gralloc_buffer_handle_t *grallocHandle =
    //    (intel_gralloc_buffer_handle_t*)layer->handle;

    //if (!grallocHandle)
    //    return false;

    // TODO: check buffer usage

    // TODO: check data format

    // TODO: enable this check after fully switching to Gralloc buffer
    //if (grallocHandle->format != HAL_PIXEL_FORMAT_YV12 &&
    //    grallocHandle->format != HAL_PIXEL_FORMAT_INTEL_HWC_NV12 &&
    //    grallocHandle->format != HAL_PIXEL_FORMAT_INTEL_HWC_YUY2 &&
    //    grallocHandle->format != HAL_PIXEL_FORMAT_INTEL_HWC_UYVY &&
    //    grallocHandle->format != HAL_PIXEL_FORMAT_INTEL_HWC_I420)
    //    return false;

    // FIXME: remove this later
    if (layer->compositionType != HWC_OVERLAY)
        return false;

    // TODO: enable this when ST is ready for video
    // check whether layer are covered by layers above it
    //for (size_t i = index + 1; i < list->numHwLayers; i++) {
    //    if (areLayersIntersecting(&list->hwLayers[i], layer)) {
    //        LOGD("%s: layer %d is covered by layer %d\n", __func__, index, i);
    //        return false;
    //    }
    //}

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

    bool ret = false;

    return false;

    if (layer->compositionType == HWC_FRAMEBUFFER) {
        if (!isSpriteHandle((uint32_t)layer->handle))
            return false;

        intel_gralloc_buffer_handle_t *gHandle =
            (intel_gralloc_buffer_handle_t*)layer->handle;

        LOGV("%s: check layer %dx%d\n",
             __func__,
             layer->displayFrame.right - layer->displayFrame.left,
             layer->displayFrame.bottom - layer->displayFrame.top);

        // if layer is on the top of overlay, use sprite plane
        if (gHandle->bpp == 4 &&
            --index >= 0 &&
            list->hwLayers[index].compositionType == HWC_OVERLAY) {
            flags |= IntelDisplayPlane::UPDATE_CONTROL;
            ret = true;
        }

        // if layer is on the top and full screen, use sprite plane
        // NOTE: This is how we can support bypass layer, however,
        // honeycomb barely has full screen apps, so this path won't
        // take any efforts.
        // TODO: remove the magic number 600 & 1024 later
        if (gHandle->bpp == 4 &&
            ((size_t)index == (list->numHwLayers - 1)) &&
            ((layer->displayFrame.right - layer->displayFrame.left) == 600) &&
            ((layer->displayFrame.bottom - layer->displayFrame.top) == 1024)) {
            flags |= IntelDisplayPlane::UPDATE_SURFACE;
            ret = true;
        }
    }

    return ret;
}

// When the geometry changed, we need
// 0) reclaim all allocated planes
// 1) build a new layer list for the changed hwc_layer_list
// 2) attach planes to these layers which can be handled by HWC
// 3) indicate whether eglSwapbuffers is needed
// 4) disable planes which are not used anymore
void IntelHWComposer::onGeometryChanged(hwc_layer_list_t *list)
{
     // reclaim all planes
     bool ret = mLayerList->invalidatePlanes();
     if (!ret) {
         LOGE("%s: failed to reclaim allocated planes\n", __func__);
         return;
     }

     // update layer list with new list
     mLayerList->updateLayerList(list);

     for (size_t i = 0 ; i < list->numHwLayers ; i++) {
	 // TODO: enable this checking after fully switch to Gralloc buffer
	 // check whether a layer can be handled in general
         //if (!isHWCLayer(&list->hwLayers[i]))
         //    continue;

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
         } else
             list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
     }

     // TODO: check whether eglSwapBuffers is needed

     // disable unused planes
     mPlaneManager->disableReclaimedPlanes();
}

// when buffer handle is changed, we need
// 0) get plane's data buffer if a plane was attached to a layer
// 1) update plane's data buffer with the new buffer handle
// 2) set the updated data buffer back to plane
bool IntelHWComposer::updateLayersData(hwc_layer_list_t *list)
{
    IntelDisplayPlane *plane = 0;
    bool ret;

    for (size_t i=0 ; i<list->numHwLayers ; i++) {
        plane = mLayerList->getPlane(i);
        if (plane) {
            hwc_layer_t *layer = &list->hwLayers[i];
            int srcX = layer->sourceCrop.left;
            int srcY = layer->sourceCrop.top;
            int srcWidth = layer->sourceCrop.right - layer->sourceCrop.left;
            int srcHeight = layer->sourceCrop.bottom - layer->sourceCrop.top;
            int planeType = plane->getPlaneType();

            // get & setup overlay data buffer
            IntelDisplayBuffer *buffer = plane->getDataBuffer();
            IntelDisplayDataBuffer *dataBuffer =
                reinterpret_cast<IntelDisplayDataBuffer*>(buffer);
            if (!dataBuffer) {
                LOGE("%s: invalid overlay data buffer\n", __func__);
                continue;
            }

            if (planeType == IntelDisplayPlane::DISPLAY_PLANE_OVERLAY) {
                IntelOverlayContext *overlayContext =
                    reinterpret_cast<IntelOverlayContext*>(plane->getContext());

                // detect video mode change
                mDrm->drmModeChanged(*overlayContext);

                uint32_t handle = (uint32_t)layer->handle;

                dataBuffer->setWidth(srcWidth);
                dataBuffer->setHeight(srcHeight);
                dataBuffer->setStride(srcWidth);
                dataBuffer->setCrop(srcX, srcY, srcWidth, srcHeight);

                // set the data buffer back to plane
                ret = plane->setDataBuffer(handle);
                if (!ret) {
                    LOGE("%s: failed to update overlay data buffer\n", __func__);
                    mLayerList->detachPlane(i, plane);
                    layer->compositionType = HWC_FRAMEBUFFER;
                }
            } else if (planeType == IntelDisplayPlane::DISPLAY_PLANE_SPRITE) {
                // do nothing for sprite plane for now
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

    if (layer->flags & HWC_SKIP_LAYER)
        return false;

    // check transform
    if (!isHWCTransform(layer->transform))
        return false;

    // check blending
    if (!isHWCBlending(layer->blending))
        return false;

    intel_gralloc_buffer_handle_t *grallocHandle =
        (intel_gralloc_buffer_handle_t*)layer->handle;

    if (!grallocHandle)
        return false;

    // check buffer usage
    if (!isHWCUsage(grallocHandle->usage))
        return false;

    // check format
    if (!isHWCFormat(grallocHandle->format))
        return false;

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

    // handle geometry changing. attach display planes to layers
    // which can be handled by HWC.
    // plane control information (e.g. position) will be set here
    if (list->flags & HWC_GEOMETRY_CHANGED) {
        onGeometryChanged(list);
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

    // Call plane's flip for each layer in hwc_layer_list, if a plane has
    // been attached to a layer
    for (size_t i=0 ; i<list->numHwLayers ; i++) {
        IntelDisplayPlane *plane = mLayerList->getPlane(i);
        int flags = mLayerList->getFlags(i);
        if (plane) {
            bool ret = plane->flip(flags);
            if (!ret)
                LOGW("%s: failed to flip plane %d\n", __func__, i);
        }
        // clear flip flags
        mLayerList->setFlags(i, 0);
    }

    // check whether eglSwapBuffers is still needed for the given layer list
    EGLBoolean sucess = eglSwapBuffers((EGLDisplay)dpy, (EGLSurface)sur);
    if (!sucess) {
        return false;
    }

    return true;
}

bool IntelHWComposer::initialize()
{
    bool ret = true;

    //TODO: replace the hard code buffer type later
    int bufferType = IntelBufferManager::TTM_BUFFER;

    LOGV("%s\n", __func__);

    //create new DRM object if not exists
    if (!mDrm) {
        ret = IntelHWComposerDrm::getInstance().initialize(bufferType);
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
    delete mDrm;
    mDrm = 0;
drm_err:
    return ret;
}

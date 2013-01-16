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
#include <IntelWidiPlane.h>
#include <IntelHWComposerCfg.h>

IntelHDMIDisplayDevice::IntelHDMIDisplayDevice(IntelBufferManager *bm,
                                    IntelBufferManager *gm,
                                    IntelDisplayPlaneManager *pm,
                                    IMG_framebuffer_device_public_t *fbdev,
                                    IntelHWComposerDrm *drm,
                                    uint32_t index)
                                  : IntelDisplayDevice(pm, drm, index),
                                    mBufferManager(bm),
                                    mGrallocBufferManager(gm),
                                    mFBDev(fbdev)
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);

    //check buffer manager
    if (!mBufferManager) {
        ALOGE("%s: Invalid buffer manager\n", __func__);
        goto init_err;
    }

    // check buffer manager for gralloc buffer
    if (!mGrallocBufferManager) {
        ALOGE("%s: Invalid Gralloc buffer manager\n", __func__);
        goto init_err;
    }

    // check IMG frame buffer device
    if (!mFBDev) {
        ALOGE("%s: failed to open IMG FB device\n", __func__);
        goto init_err;
    }

    //create new DRM object if not exists
    if (!mDrm) {
        ALOGE("%s: failed to initialize DRM instance\n", __func__);
        goto init_err;
    }

    // check display plane manager
    if (!mPlaneManager) {
        ALOGE("%s: Invalid plane manager\n", __func__);
        goto init_err;
    }

    // create layer list
    mLayerList = new IntelHWComposerLayerList(mPlaneManager);
    if (!mLayerList) {
        ALOGE("%s: Failed to create layer list\n", __func__);
        goto init_err;
    }

    memset(mHDMIBuffers, 0, sizeof(mHDMIBuffers));
    mNextBuffer = 0;

    mInitialized = true;
    return;

init_err:
    mInitialized = false;
    return;
}

IntelHDMIDisplayDevice::~IntelHDMIDisplayDevice()
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);
}

// When the geometry changed, we need
// 0) reclaim all allocated planes, reclaimed planes will be disabled
//    on the start of next frame. A little bit tricky, we cannot disable the
//    planes right after geometry is changed since there's no data in FB now,
//    so we need to wait FB is update then disable these planes.
// 1) build a new layer list for the changed hwc_layer_list
// 2) attach planes to these layers which can be handled by HWC
void IntelHDMIDisplayDevice::onGeometryChanged(hwc_display_contents_1_t *list)
{
    //TODO: need to implement this function for HDMI display.

    // update layer list with new list
    mLayerList->updateLayerList(list);

    //skip all layers handling for extended video mode
    if (mDrm->getDisplayMode() == OVERLAY_EXTEND) {
        for (size_t i = 0; list && i < list->numHwLayers-1; i++) {
            list->hwLayers[i].compositionType = HWC_OVERLAY;
            list->hwLayers[i].hints = 0;
        }
    }
}

bool IntelHDMIDisplayDevice::prepare(hwc_display_contents_1_t *list)
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s", __func__);

    if (!initCheck()) {
        ALOGE("%s: failed to initialize HWComposer\n", __func__);
        return false;
    }

    if (!list || (list->flags & HWC_GEOMETRY_CHANGED)) {
        onGeometryChanged(list);
    }

    // handle hotplug event here
    if (mHotplugEvent) {
        ALOGD_IF(ALLOW_HWC_PRINT, "%s: reset hotplug event flag\n", __func__);
        mHotplugEvent = false;
    }

    return true;
}

bool IntelHDMIDisplayDevice::commit(hwc_display_contents_1_t *list,
                                    buffer_handle_t *bh,
                                    int &numBuffers)
{
    ALOGD_IF(ALLOW_HWC_PRINT, "%s\n", __func__);

    if (!initCheck()) {
        ALOGE("%s: failed to initialize HWComposer\n", __func__);
        return false;
    }

    // if hotplug was happened & didn't be handled skip the flip
    if (mHotplugEvent) {
        ALOGW("%s: hotplug event is true\n", __func__);
        return true;
    }

    int output = 1;
    drmModeConnection connection = mDrm->getOutputConnection(output);
    if (connection != DRM_MODE_CONNECTED) {
        ALOGW("%s: HDMI does not connected\n", __func__);
        return false;
    }

    intel_overlay_mode_t mode = mDrm->getDisplayMode();
    if (mode != OVERLAY_CLONE_MIPI0) {
        //ALOGE("%s: Bypass HDMI post if mode isn't clone mode", __func__);
        return false;
    }

    void *context = mPlaneManager->getPlaneContexts();
    if (!context) {
        ALOGW("%s: invalid plane contexts\n", __func__);
        return false;
    }

    if (list &&
        list->numHwLayers>0 &&
        list->hwLayers[list->numHwLayers-1].handle &&
        list->hwLayers[list->numHwLayers-1].compositionType == HWC_FRAMEBUFFER_TARGET) {

        hwc_layer_1_t *target_layer = &list->hwLayers[list->numHwLayers-1];
        buffer_handle_t *bufferHandles = bh;

        bool ret = flipFramebufferContexts(context, target_layer);
        if (!ret)
            ALOGV("%s: skip to flip HDMI fb context !\n", __func__);
        else
            bufferHandles[numBuffers++] = target_layer->handle;
    } else if (list)
        ALOGW("%s: layernum: %d, no found of framebuffer_target!\n",
                                       __func__, list->numHwLayers);
    else
        ALOGW("%s: Invalid list, no found of framebuffer_target!\n",
                                       __func__);

    return true;
}

bool IntelHDMIDisplayDevice::updateLayersData(hwc_display_contents_1_t *list)
{
    return true;
}

bool IntelHDMIDisplayDevice::flipFramebufferContexts(void *contexts,
                                        hwc_layer_1_t *target_layer)
{
    ALOGD_IF(ALLOW_HWC_PRINT, "flipHDMIFrameBufferContexts");

    if (!contexts) {
        ALOGW("%s: Invalid plane contexts\n", __func__);
        return false;
    }

    if (target_layer == NULL) {
        ALOGW("%s: Invalid HDMI target layer\n", __func__);
        return false;
    }

    intel_overlay_mode_t mode = mDrm->getDisplayMode();
    if (mode == OVERLAY_EXTEND) {
        ALOGV("%s: Skip FRAMEBUFFER_TARGET on video_ext mode\n", __func__);
        return false;
    }

    int output = 1;
    //get target layer handler
    intel_gralloc_buffer_handle_t *grallocHandle =
    (intel_gralloc_buffer_handle_t*)target_layer->handle;

    if (!grallocHandle)
        return false;

    //map HDMI buffer handler
    IntelDisplayBuffer *buffer = NULL;

    for (int i = 0; i < HDMI_BUF_NUM; i++) {
        if (mHDMIBuffers[i].ui64Stamp == grallocHandle->ui64Stamp) {
            ALOGD_IF(ALLOW_HWC_PRINT,
                     "%s: buf stamp %lld...\n", __func__,grallocHandle->ui64Stamp);
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

bool IntelHDMIDisplayDevice::dump(char *buff,
                           int buff_len, int *cur_len)
{
    IntelDisplayPlane *plane = NULL;
    bool ret = true;
    int i;

    mDumpBuf = buff;
    mDumpBuflen = buff_len;
    mDumpLen = (int)(*cur_len);

    if (mLayerList) {
       dumpPrintf("------------ HDMI Totally %d layers -------------\n",
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
       dumpPrintf("-------------HDMI runtime parameters -------------\n");
       dumpPrintf("  + mHotplugEvent: %d \n", mHotplugEvent);

    }

    *cur_len = mDumpLen;
    return ret;
}

bool IntelHDMIDisplayDevice::getDisplayConfig(uint32_t* configs,
                                        size_t* numConfigs)
{
    if (!numConfigs || !numConfigs[0])
        return false;

    if (mDrm->getOutputConnection(mDisplayIndex) != DRM_MODE_CONNECTED)
        return false;

    *numConfigs = 1;
    configs[0] = 0;

    return true;

}

bool IntelHDMIDisplayDevice::getDisplayAttributes(uint32_t config,
            const uint32_t* attributes, int32_t* values)
{
    if (config != 0)
        return false;

    if (!attributes || !values)
        return false;

    if (mDrm->getOutputConnection(mDisplayIndex) != DRM_MODE_CONNECTED)
        return false;

    drmModeModeInfoPtr mode = mDrm->getOutputMode(mDisplayIndex);

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

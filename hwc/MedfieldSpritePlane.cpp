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

#include <IntelDisplayPlaneManager.h>
#include <IntelOverlayUtil.h>

MedfieldSpritePlane::MedfieldSpritePlane(int fd, int index, IntelBufferManager *bm)
    : IntelSpritePlane(fd, index, bm)
{
    memset(mDataBuffers, 0, sizeof(mDataBuffers));
    mNextBuffer = 0;
}

MedfieldSpritePlane::~MedfieldSpritePlane()
{

}

bool MedfieldSpritePlane::checkPosition(int& left, int& top,
                                        int& right, int& bottom)
{
    int output;
    drmModeFBPtr fbInfo;

    switch (mIndex) {
    case 1:
        output = OUTPUT_HDMI;
        break;
    case 2:
        output = OUTPUT_MIPI1;
        break;
    case 0:
    default:
        output = OUTPUT_MIPI0;
    }

    fbInfo = IntelHWComposerDrm::getInstance().getOutputFBInfo(output);

    // if intersect with frame buffer, get the intersection rectangle
    if (left < right && left < (int)fbInfo->width &&
        top < bottom && top < (int)fbInfo->height) {
        if (left < 0)
            left = 0;
        if (top < 0)
            top = 0;
        if (right > (int)fbInfo->width)
            right = fbInfo->width;
        if (bottom > (int)fbInfo->height)
            bottom = fbInfo->height;
    } else {
        // ignore this layer by setting postion to (0, 0) - (0, 0)
        left = top = right = bottom = 0;
    }

    return true;
}

void MedfieldSpritePlane::setPosition(int left, int top, int right, int bottom)
{
    // check position
    checkPosition(left, top, right, bottom);
    IntelSpritePlane::setPosition(left, top, right, bottom);
}

bool MedfieldSpritePlane::setDataBuffer(IntelDisplayBuffer& buffer)
{
    if (initCheck()) {
        IntelDisplayBuffer *bufferPtr = &buffer;
        IntelDisplayDataBuffer *spriteDataBuffer =
	            reinterpret_cast<IntelDisplayDataBuffer*>(bufferPtr);
        IntelSpriteContext *spriteContext =
            reinterpret_cast<IntelSpriteContext*>(mContext);
        intel_sprite_context_t *context = spriteContext->getContext();

        uint32_t format = spriteDataBuffer->getFormat();
        uint32_t spriteFormat;
        int bpp;

        switch (format) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
            spriteFormat = INTEL_SPRITE_PIXEL_FORMAT_RGBA8888;
            bpp = 4;
            break;
        case HAL_PIXEL_FORMAT_RGBX_8888:
            spriteFormat = INTEL_SPRITE_PIXEL_FORMAT_RGBX8888;
            bpp = 4;
            break;
        case HAL_PIXEL_FORMAT_BGRX_8888:
            spriteFormat = INTEL_SPRITE_PIXEL_FORMAT_BGRX8888;
            bpp = 4;
            break;
        case HAL_PIXEL_FORMAT_BGRA_8888:
            spriteFormat = INTEL_SPRITE_PIXEL_FORMAT_BGRA8888;
            bpp = 4;
            break;
        case HAL_PIXEL_FORMAT_RGB_565:
            spriteFormat = INTEL_SPRITE_PIXEL_FORMAT_BGRX565;
            bpp = 2;
            break;
        default:
            LOGE("%s: unsupported format 0x%x\n", __func__, format);
            return false;
        }

        // set offset;
        int srcX = spriteDataBuffer->getSrcX();
        int srcY = spriteDataBuffer->getSrcY();
        int bufferWidth = spriteDataBuffer->getWidth();
        int bufferHeight = spriteDataBuffer->getHeight();
        uint32_t stride = align_to(bpp * bufferWidth, 64);
        uint32_t linoff = srcY * stride + srcX * bpp;

        // unlikely happen, but still we need make sure linoff is valid
        if (linoff > (stride * bpp * bufferHeight)) {
            LOGE("%s: invalid source crop\n", __func__);
            return false;
        }

        // gtt
        uint32_t gttOffsetInPage = spriteDataBuffer->getGttOffsetInPage();

        // update context
        context->cntr = spriteFormat | 0x80000000;
        context->linoff = linoff;
        context->stride = stride;
        context->surf = gttOffsetInPage << 12;
        context->update_mask = SPRITE_UPDATE_ALL;

        LOGV("%s: cntr 0x%x, stride 0x%x, surf 0x%x\n",
             __func__, context->cntr, context->stride, context->surf);

        return true;
    }

    LOGE("%s: sprite plane was not initialized\n", __func__);
    return false;
}

bool MedfieldSpritePlane::setDataBuffer(uint32_t handle, uint32_t flags, intel_gralloc_buffer_handle_t* nHandle)
{
    unsigned long long ui64Stamp = nHandle->ui64Stamp;
    IntelDisplayBuffer *buffer = 0;
    int i;

    if (!initCheck()) {
        LOGE("%s: overlay plane wasn't initialized\n", __func__);
        return false;
    }

    LOGV("%s: next buffer %d\n", __func__, mNextBuffer);

    // Notice!!! Maybe handle can be reused, it will cause problem.
    for (i = 0; i < SPRITE_DATA_BUFFER_NUM_MAX; i++) {
        if (mDataBuffers[i].ui64Stamp == ui64Stamp) {
            buffer = mDataBuffers[i].buffer;
            break;
        }
    }

    if (!buffer) {
            // release the buffer in the next slot
            if (mDataBuffers[mNextBuffer].ui64Stamp ||
                            mDataBuffers[mNextBuffer].buffer) {
                    LOGV("%s: releasing buffer %d...\n", __func__, mNextBuffer);
                    mBufferManager->unmap(mDataBuffers[mNextBuffer].buffer);
                    mDataBuffers[mNextBuffer].ui64Stamp = 0;
                    mDataBuffers[mNextBuffer].handle = 0;
                    mDataBuffers[mNextBuffer].buffer = 0;
            }

            buffer = mBufferManager->map(handle);
            if (!buffer) {
                    LOGE("%s: failed to map handle %d\n", __func__, handle);
                    disable();
                    return false;
            }

            mDataBuffers[mNextBuffer].ui64Stamp = ui64Stamp;
            mDataBuffers[mNextBuffer].handle = handle;
            mDataBuffers[mNextBuffer].buffer = buffer;
            // move mNextBuffer pointer
            mNextBuffer = (mNextBuffer + 1) % SPRITE_DATA_BUFFER_NUM_MAX;
    }

    IntelDisplayDataBuffer *spriteDataBuffer =
        reinterpret_cast<IntelDisplayDataBuffer*>(mDataBuffer);
    spriteDataBuffer->setBuffer(buffer);

    mDataBufferHandle = (uint32_t)nHandle;

    // set data buffer :-)
    return setDataBuffer(*spriteDataBuffer);
}

bool MedfieldSpritePlane::flip(void *contexts, uint32_t flags)
{
    IntelSpriteContext *spriteContext =
            reinterpret_cast<IntelSpriteContext*>(mContext);
    intel_sprite_context_t *context = spriteContext->getContext();
    mdfld_plane_contexts_t *planeContexts;
    bool ret = true;

    if (!contexts) {
        LOGE("%s: Invalid plane contexts\n", __func__);
        return false;
    }

    planeContexts = (mdfld_plane_contexts_t*)contexts;

    // if no update, return
    if (!context->update_mask)
        return true;

    LOGV("%s: flip to surface 0x%x\n", __func__, context->surf);

    // update context
    for (int output = 0; output < OUTPUT_MAX; output++) {
        drmModeConnection connection =
            IntelHWComposerDrm::getInstance().getOutputConnection(output);

        if (connection != DRM_MODE_CONNECTED) {
            LOGV("%s: output %d not connected\n", __func__, output);
            continue;
        }

        context->index = output;
        context->pipe = output;
        // context->update_mask &= ~SPRITE_UPDATE_POSITION;

        // update plane contexts
        memcpy(&planeContexts->sprite_contexts[output],
               context, sizeof(intel_sprite_context_t));
        planeContexts->active_sprites |= (1 << output);
    }

    // clear update_mask
    context->update_mask = 0;
    return ret;
}

bool MedfieldSpritePlane::reset()
{
   return true;
}

bool MedfieldSpritePlane::disable()
{
    return true;
}

bool MedfieldSpritePlane::invalidateDataBuffer()
{
    LOGV("%s\n", __func__);

    // keep the mapping of sprite data buffers till HWC was unload
    // if we unmap them dynamically, post2 may be failed.
    // TODO: improve gralloc buffer manager, to get buffer info from
    // gralloc HAL directly.
    return true;
}


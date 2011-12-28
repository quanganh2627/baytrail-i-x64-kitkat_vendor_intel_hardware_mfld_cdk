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

void MedfieldSpritePlane::setPosition(int left, int top, int right, int bottom)
{
    // Never do this on Medfield
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
        int srcWidth = spriteDataBuffer->getSrcWidth();
        int srcHeight = spriteDataBuffer->getSrcHeight();
        uint32_t stride = align_to(bpp * srcWidth, 64);
        uint32_t linoff = srcY * stride + srcX * bpp;

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

bool MedfieldSpritePlane::setDataBuffer(uint32_t handle, uint32_t flags)
{
    IntelDisplayBuffer *buffer = 0;

    if (!initCheck()) {
        LOGE("%s: overlay plane wasn't initialized\n", __func__);
        return false;
    }

    LOGD("%s: next buffer %d\n", __func__, mNextBuffer);

    // make sure the buffer list is clean
    if (!mNextBuffer)
        invalidateDataBuffer();

    buffer = mBufferManager->map(handle);
    if (!buffer) {
         LOGE("%s: failed to map handle %d\n", __func__, handle);
         disable();
         return false;
    }

    mDataBuffers[mNextBuffer].handle = handle;
    mDataBuffers[mNextBuffer].buffer = buffer;

    // move mNextBuffer pointer
    mNextBuffer = (mNextBuffer + 1) % INTEL_DATA_BUFFER_NUM_MAX;

    IntelDisplayDataBuffer *spriteDataBuffer =
        reinterpret_cast<IntelDisplayDataBuffer*>(mDataBuffer);
    spriteDataBuffer->setBuffer(buffer);

    // set data buffer :-)
    return setDataBuffer(*spriteDataBuffer);
}

bool MedfieldSpritePlane::flip(uint32_t flags)
{
    IntelSpriteContext *spriteContext =
            reinterpret_cast<IntelSpriteContext*>(mContext);
    intel_sprite_context_t *context = spriteContext->getContext();

    // if no update, return
    if (!context->update_mask)
        return true;

    LOGD("%s: flip to surface 0x%x\n", __func__, context->surf);

    // update context
    struct drm_psb_register_rw_arg arg;
    memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
    memcpy(&arg.sprite_context, context, sizeof(intel_sprite_context_t));
    arg.sprite_context_mask = REGRWBITS_SPRITE_UPDATE;
    int ret = drmCommandWriteRead(mDrmFd,
                                  DRM_PSB_REGISTER_RW,
                                  &arg, sizeof(arg));
    if (ret) {
        LOGW("%s: sprite update failed with error code %d\n",
             __func__, ret);
        return false;
    }

    // try to flip HDMI
    // FIXME: check the HDMI connection state before doing this
    arg.sprite_context.index = 1;
    arg.sprite_context.update_mask &= ~SPRITE_UPDATE_POSITION;
    ret = drmCommandWriteRead(mDrmFd,
                                  DRM_PSB_REGISTER_RW,
                                  &arg, sizeof(arg));
    if (ret) {
        LOGW("%s: sprite update failed with error code %d\n",
             __func__, ret);
        return false;
    }

    // clear update_mask
    context->update_mask = 0;
    return true;
}

bool MedfieldSpritePlane::reset()
{
    if (initCheck()) {
        IntelSpriteContext *spriteContext =
            reinterpret_cast<IntelSpriteContext*>(mContext);
        intel_sprite_context_t *context = spriteContext->getContext();

        // only reset format
        // TODO: remove the magic number later
        context->pos = 0;
        context->size = ((1024 - 1) & 0xfff) << 16 | ((600 - 1) & 0xfff);
        context->cntr = INTEL_SPRITE_PIXEL_FORMAT_BGRX8888 | 0x80000000;
        context->linoff = 0;
        context->surf = 0;
        context->stride = align_to(4 * 600, 64);;
        context->update_mask = (SPRITE_UPDATE_POSITION |
                                SPRITE_UPDATE_SIZE |
                                SPRITE_UPDATE_CONTROL);

        struct drm_psb_register_rw_arg arg;
        memset(&arg, 0, sizeof(arg));
        memcpy(&arg.sprite_context, context, sizeof(intel_sprite_context_t));
        arg.sprite_context_mask = REGRWBITS_SPRITE_UPDATE;
        int ret = drmCommandWriteRead(mDrmFd,
                                      DRM_PSB_REGISTER_RW,
                                      &arg, sizeof(arg));
        if (ret) {
            LOGW("%s: sprite update failed with error code %d\n",
                 __func__, ret);
            return false;
        }

        // try to flip HDMI
        // FIXME: check the HDMI connection state before doing this
        arg.sprite_context.index = 1;
        arg.sprite_context.update_mask &= ~SPRITE_UPDATE_POSITION;
        ret = drmCommandWriteRead(mDrmFd,
                                      DRM_PSB_REGISTER_RW,
                                      &arg, sizeof(arg));
        if (ret) {
            LOGW("%s: sprite update failed with error code %d\n",
                 __func__, ret);
            return false;
        }

        return true;
    }
    LOGE("%s: sprite plane was not initialized\n", __func__);
    return false;
}

bool MedfieldSpritePlane::disable()
{
    return reset();
}

bool MedfieldSpritePlane::invalidateDataBuffer()
{
    LOGV("%s\n", __func__);
    if (initCheck()) {
	 // wait for vblank before unmapping from GTT
         // FIXME: use drmWaitVblank instead
	 struct drm_psb_register_rw_arg arg;
	 memset(&arg, 0, sizeof(arg));
	 arg.sprite_context.update_mask = SPRITE_UPDATE_WAIT_VBLANK;
	 arg.sprite_context_mask = REGRWBITS_SPRITE_UPDATE;
	 int ret = drmCommandWriteRead(mDrmFd,
	                               DRM_PSB_REGISTER_RW,
	                               &arg, sizeof(arg));

         for (int i = 0; i < INTEL_DATA_BUFFER_NUM_MAX; i++) {
             mBufferManager->unmap(mDataBuffers[i].handle,
                                   mDataBuffers[i].buffer);
         }

         // clear data buffers
         memset(mDataBuffers, 0, sizeof(mDataBuffers));
         mNextBuffer = 0;
	 return true;
    }

    return false;
}


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

IntelSpriteContext::~IntelSpriteContext()
{

}

IntelSpritePlane::IntelSpritePlane(int fd, int index, IntelBufferManager *bm)
    : IntelDisplayPlane(fd, IntelDisplayPlane::DISPLAY_PLANE_SPRITE, index, bm)
{
    bool ret;
    LOGV("%s\n", __func__);

    // create data buffer
    IntelDisplayBuffer *dataBuffer = new IntelDisplayDataBuffer(0, 0, 0);
    if (!dataBuffer) {
        LOGE("%s: Failed to create sprite data buffer\n", __func__);
        return;
    }

    // create sprite context
    IntelSpriteContext *spriteContext = new IntelSpriteContext();
    if (!spriteContext) {
        LOGE("%s: Failed to create sprite context\n", __func__);
        goto sprite_create_err;
    }

    // initialized successfully
    mDataBuffer = dataBuffer;
    mContext = spriteContext;
    mInitialized = true;
    return;
sprite_create_err:
    delete dataBuffer;
}

IntelSpritePlane::~IntelSpritePlane()
{
    if (mContext) {
	// flush context
        flip(IntelDisplayPlane::FLASH_NEEDED);

        // disable sprite
        disable();

        // delete sprite context;
        delete mContext;

        // destroy overlay data buffer;
        delete mDataBuffer;

        mContext = 0;
        mInitialized = false;
    }
}

void IntelSpritePlane::setPosition(int left, int top, int right, int bottom)
{
    if (initCheck()) {
        // TODO: check position

	int x = left;
        int y = top;
        int w = right - left;
        int h = bottom - top;

        IntelSpriteContext *spriteContext =
            reinterpret_cast<IntelSpriteContext*>(mContext);
        intel_sprite_context_t *context = spriteContext->getContext();

        // update dst position
        context->pos = (y & 0xfff) << 16 | (x & 0xfff);
        context->size = ((h - 1) & 0xfff) << 16 | ((w - 1) & 0xfff);
    }
}

bool IntelSpritePlane::setDataBuffer(IntelDisplayBuffer& buffer)
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

        // update context
        struct drm_psb_register_rw_arg arg;

        memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
        arg.sprite_context_mask = REGRWBITS_SPRITE_UPDATE;
        memcpy(&arg.sprite_context, context, sizeof(intel_sprite_context_t));

        int ret = drmCommandWriteRead(mDrmFd,
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

bool IntelSpritePlane::flip(uint32_t flags)
{
    if (initCheck()) {
        IntelSpriteContext *spriteContext =
            reinterpret_cast<IntelSpriteContext*>(mContext);
        intel_sprite_context_t *context = spriteContext->getContext();
        struct drm_psb_register_rw_arg arg;

        memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
        arg.sprite_context_mask = REGRWBITS_SPRITE_UPDATE;
        arg.sprite_context.update_mask = SPRITE_UPDATE_CONTROL;
        arg.sprite_context.linoff = context->linoff;
        arg.sprite_context.surf = context->surf;
        int ret = drmCommandWriteRead(mDrmFd,
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

bool IntelSpritePlane::reset()
{
    if (initCheck()) {
        IntelSpriteContext *spriteContext =
            reinterpret_cast<IntelSpriteContext*>(mContext);
        intel_sprite_context_t *context = spriteContext->getContext();
        struct drm_psb_register_rw_arg arg;
        memset(&arg, 0, sizeof(struct drm_psb_register_rw_arg));
        arg.sprite_context_mask = REGRWBITS_SPRITE_UPDATE;
        arg.sprite_context.update_mask = SPRITE_UPDATE_ALL;
        int ret = drmCommandWriteRead(mDrmFd,
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

bool IntelSpritePlane::disable()
{
    return reset();
}

bool IntelSpritePlane::invalidateDataBuffer()
{
    LOGD("%s\n", __func__);
    if (initCheck()) {
	 mBufferManager->unmap(mDataBuffer->getHandle(), mDataBuffer);
	 delete mDataBuffer;
	 mDataBuffer = new IntelDisplayDataBuffer(0, 0, 0);
	 return true;
    }

    return false;
}

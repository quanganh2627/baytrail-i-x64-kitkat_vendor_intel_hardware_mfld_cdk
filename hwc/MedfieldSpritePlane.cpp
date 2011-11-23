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

}

MedfieldSpritePlane::~MedfieldSpritePlane()
{

}

void MedfieldSpritePlane::setPosition(int left, int top, int right, int bottom)
{
    // Never do this on Medfield
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

        // only handle surface change on Medfield
        uint32_t linoff = 0;
        uint32_t gttOffsetInPage = spriteDataBuffer->getGttOffsetInPage();

        // update context
        context->linoff = linoff;
        context->surf = gttOffsetInPage << 12;

        // FIXME: should I enable Alpha here???
        context->cntr = INTEL_SPRITE_PIXEL_FORMAT_BGRA8888 | 0x80000000;
        return true;
    }
    LOGE("%s: sprite plane was not initialized\n", __func__);
    return false;
}

bool MedfieldSpritePlane::flip(uint32_t flags)
{
    IntelSpriteContext *spriteContext =
            reinterpret_cast<IntelSpriteContext*>(mContext);
    intel_sprite_context_t *context = spriteContext->getContext();

    if (flags & IntelDisplayPlane::UPDATE_CONTROL)
        context->update_mask |= SPRITE_UPDATE_CONTROL;
    if (flags & IntelDisplayPlane::UPDATE_SURFACE)
	context->update_mask |= SPRITE_UPDATE_SURFACE;

    LOGV("%s: update mask 0x%x, cntr 0x%x\n",
         __func__,
         context->update_mask,
         context->cntr);

    // if no update, return
    if (!context->update_mask)
        return true;

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
        context->cntr = INTEL_SPRITE_PIXEL_FORMAT_BGRX8888 | 0x80000000;

        struct drm_psb_register_rw_arg arg;
        memset(&arg, 0, sizeof(arg));
        memcpy(&arg.sprite_context, context, sizeof(intel_sprite_context_t));
        arg.sprite_context_mask = REGRWBITS_SPRITE_UPDATE;
        arg.sprite_context.update_mask = SPRITE_UPDATE_CONTROL;
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

bool MedfieldSpritePlane::disable()
{
    return reset();
}

bool MedfieldSpritePlane::invalidateDataBuffer()
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


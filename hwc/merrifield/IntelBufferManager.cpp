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
#include <IntelBufferManager.h>
#include <IntelHWComposerDrm.h>
#include <IntelHWComposerCfg.h>
#include <IntelOverlayUtil.h>
#include <IntelOverlayHW.h>
#include <fcntl.h>
#include <errno.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/ashmem.h>
#include <sys/mman.h>

IntelDisplayDataBuffer::IntelDisplayDataBuffer(uint32_t format,
                                               uint32_t w,
                                               uint32_t h)
        : mFormat(format), mWidth(w), mHeight(h), mBuffer(0), mBobDeinterlace(0)
{
    ALOGD_IF(ALLOW_BUFFER_PRINT, "%s: width %d, format 0x%x\n", __func__, w, format);


    mRawStride = 0;
    mYStride = 0;
    mUVStride = 0;
    mSrcX = 0;
    mSrcY = 0;
    mSrcWidth = w;
    mSrcHeight = h;
    mUpdateFlags = (BUFFER_CHANGE | SIZE_CHANGE);
}

void IntelDisplayDataBuffer::setBuffer(IntelDisplayBuffer *buffer)
{
    mBuffer = buffer;

    if (!buffer) return;

    if (mGttOffsetInPage != buffer->getGttOffsetInPage())
        mUpdateFlags |= BUFFER_CHANGE;

    mBufferObject = buffer->getBufferObject();
    mGttOffsetInPage = buffer->getGttOffsetInPage();
    mSize = buffer->getSize();
    mVirtAddr = buffer->getCpuAddr();
}

void IntelDisplayDataBuffer::setStride(uint32_t stride)
{
    if (stride == mRawStride)
        return;

    mRawStride = stride;
    mUpdateFlags |= SIZE_CHANGE;
}

void IntelDisplayDataBuffer::setStride(uint32_t yStride, uint32_t uvStride)
{
    if ((yStride == mYStride) && (uvStride == mUVStride))
        return;

    mYStride = yStride;
    mUVStride = uvStride;
    mUpdateFlags |= SIZE_CHANGE;
}

void IntelDisplayDataBuffer::setWidth(uint32_t w)
{
    if (w == mWidth)
        return;

    mWidth = w;
    mUpdateFlags |= SIZE_CHANGE;
}

void IntelDisplayDataBuffer::setHeight(uint32_t h)
{
    if (h == mHeight)
        return;

    mHeight = h;
    mUpdateFlags |= SIZE_CHANGE;
}

void IntelDisplayDataBuffer::setCrop(int x, int y, int w, int h)
{
    if ((x == (int)mSrcX) && (y == (int)mSrcY) &&
        (w == (int)mSrcWidth) && (h == (int)mSrcHeight))
        return;

    mSrcX = x;
    mSrcY = y;
    mSrcWidth = w;
    mSrcHeight = h;
    mUpdateFlags |= SIZE_CHANGE;
}

void IntelDisplayDataBuffer::setDeinterlaceType(uint32_t bob_deinterlace)
{
    mBobDeinterlace = bob_deinterlace;
}

bool IntelTTMBufferManager::getVideoBridgeIoctl()
{
    union drm_psb_extension_arg arg;
    /*video bridge ioctl = lnc_video_getparam + 1, I know it's ugly!!*/
    const char lncExt[] = "lnc_video_getparam";
    int ret = 0;

    ALOGD_IF(ALLOW_BUFFER_PRINT, "%s: get video bridge ioctl num...\n", __func__);

    if(mDrmFd <= 0) {
        ALOGE("%s: invalid drm fd %d\n", __func__, mDrmFd);
        return false;
    }

    ALOGD_IF(ALLOW_BUFFER_PRINT,
           "%s: DRM_PSB_EXTENSION %d\n", __func__, DRM_PSB_EXTENSION);

    /*get devOffset via drm IOCTL*/
    strncpy(arg.extension, lncExt, sizeof(lncExt));

    ret = drmCommandWriteRead(mDrmFd, DRM_PSB_EXTENSION, &arg, sizeof(arg));
    if(ret || !arg.rep.exists) {
        ALOGE("%s: get device offset failed with error code %d\n",
                  __func__, ret);
        return false;
    }

    ALOGD_IF(ALLOW_BUFFER_PRINT, "%s: video ioctl offset 0x%x\n",
              __func__,
              arg.rep.driver_ioctl_offset + 1);

    mVideoBridgeIoctl = arg.rep.driver_ioctl_offset + 1;

    return true;
}

IntelTTMBufferManager::~IntelTTMBufferManager()
{
    mVideoBridgeIoctl = 0;
    delete mWsbm;
}

bool IntelTTMBufferManager::initialize()
{
    bool ret;

    if (mDrmFd <= 0) {
        ALOGE("%s: invalid drm FD\n", __func__);
        return false;
    }

    /*get video bridge ioctl offset for external YUV buffer*/
    if (!mVideoBridgeIoctl) {
        ret = getVideoBridgeIoctl();
        if (ret == false) {
            ALOGE("%s: failed to video bridge ioctl\n", __func__);
            return ret;
        }
    }

    IntelWsbm *wsbm = new IntelWsbm(mDrmFd);
    if (!wsbm) {
        ALOGE("%s: failed to create wsbm object\n", __func__);
        return false;
    }

    ret = wsbm->initialize();
    if (ret == false) {
        ALOGE("%s: failed to initialize wsbm\n", __func__);
        delete wsbm;
        return false;
    }

    mWsbm = wsbm;

    ALOGD_IF(ALLOW_BUFFER_PRINT, "%s: done\n", __func__);
    return true;
}


IntelDisplayBuffer* IntelTTMBufferManager::map(uint32_t handle)
{
    if (!mWsbm) {
        ALOGE("%s: no wsbm found\n", __func__);
        return 0;
    }

    void *wsbmBufferObject;
    bool ret = mWsbm->wrapTTMBuffer(handle, &wsbmBufferObject);
    if (ret == false) {
        ALOGE("%s: wrap ttm buffer failed\n", __func__);
        return 0;
    }

    void *virtAddr = mWsbm->getCPUAddress(wsbmBufferObject);
    uint32_t gttOffsetInPage = mWsbm->getGttOffset(wsbmBufferObject);
    // FIXME: set the real size
    uint32_t size = 0;

    IntelDisplayBuffer *buf = new IntelDisplayBuffer(wsbmBufferObject,
                                                     virtAddr,
                                                     gttOffsetInPage,
                                                     size);

    ALOGD_IF(ALLOW_BUFFER_PRINT,
           "%s: mapped TTM overlay buffer. cpu %p, gtt %d\n",
         __func__, virtAddr, gttOffsetInPage);

    return buf;
}

void IntelTTMBufferManager::unmap(IntelDisplayBuffer *buffer)
{
    if (!mWsbm) {
        ALOGE("%s: no wsbm found\n", __func__);
        return;
    }
    mWsbm->unreferenceTTMBuffer(buffer->getBufferObject());
    // destroy it
    delete buffer;
}

IntelDisplayBuffer* IntelTTMBufferManager::get(int size, int alignment)
{
    if (!mWsbm) {
        ALOGE("%s: no wsbm found\n", __func__);
        return NULL;
    }

    void *wsbmBufferObject = NULL;
    bool ret = mWsbm->allocateTTMBuffer(size, alignment, &wsbmBufferObject);
    if (ret == false) {
        ALOGE("%s: failed to allocate buffer. size %d, alignment %d\n",
            __func__, size, alignment);
        return NULL;
    }

    void *virtAddr = mWsbm->getCPUAddress(wsbmBufferObject);
    uint32_t gttOffsetInPage = mWsbm->getGttOffset(wsbmBufferObject);
    uint32_t handle = mWsbm->getKBufHandle(wsbmBufferObject);
    // create new buffer
    IntelDisplayBuffer *buffer = new IntelDisplayBuffer(wsbmBufferObject,
                                                        virtAddr,
                                                        gttOffsetInPage,
                                                        size,
                                                        handle);
    ALOGD_IF(ALLOW_BUFFER_PRINT,
           "%s: created TTM overlay buffer. cpu %p, gtt %d\n",
         __func__, virtAddr, gttOffsetInPage);
    return buffer;
}

void IntelTTMBufferManager::put(IntelDisplayBuffer* buf)
{
    if (!buf || !mWsbm) {
        ALOGE("%s: Invalid parameter\n", __func__);
        return;
    }

    void *wsbmBufferObject = buf->getBufferObject();
    bool ret = mWsbm->destroyTTMBuffer(wsbmBufferObject);
    if (ret == false)
        ALOGW("%s: failed to free wsbmBO\n", __func__);

    // free overlay buffer
    delete buf;
}

IntelGraphicBufferManager::~IntelGraphicBufferManager()
{
    if (initCheck()) {

        // delete wsbm
        delete mWsbm;
    }
}

bool IntelGraphicBufferManager::gttMap(void *vaddr,
                                       uint32_t size,
                                       uint32_t gttAlign,
                                       int *offset)
{
    struct psb_gtt_mapping_arg arg;

    if (!vaddr || !size || !offset) {
        ALOGE("%s: invalid parameters.\n", __func__);
        return false;
    }

    if (mDrmFd < 0) {
        ALOGE("%s: drm is not ready\n", __func__);
        return false;
    }

    arg.type = PSB_GTT_MAP_TYPE_VIRTUAL;
    arg.page_align = gttAlign;
    arg.vaddr = (uint32_t)vaddr;
    arg.size = size;

    ALOGD_IF(ALLOW_BUFFER_PRINT, "gttMap: virt 0x%x, size %d\n", vaddr, size);

    int ret = drmCommandWriteRead(mDrmFd, DRM_PSB_GTT_MAP, &arg, sizeof(arg));
    if (ret) {
        ALOGE("%s: gtt mapping failed, err %d\n", __func__, ret);
        return false;
    }

    *offset =  arg.offset_pages;
    return true;
}

bool IntelGraphicBufferManager::gttUnmap(void *vaddr)
{
    struct psb_gtt_mapping_arg arg;

    if(!vaddr) {
        ALOGE("%s: invalid parameter\n", __func__);
        return false;
    }

    if(mDrmFd < 0) {
        ALOGE("%s: drm is not ready\n", __func__);
        return false;
    }

    arg.type = PSB_GTT_MAP_TYPE_VIRTUAL;
    arg.vaddr = (uint32_t)vaddr;

    int ret = drmCommandWrite(mDrmFd, DRM_PSB_GTT_UNMAP, &arg, sizeof(arg));
    if(ret) {
        ALOGE("%s: gtt unmapping failed\n", __func__);
        return false;
    }

    return true;
}

bool IntelGraphicBufferManager::initialize()
{
    hw_module_t const* module;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module) == 0)
        mIMGGrallocModule = (IMG_gralloc_module_public_t*)module;
    else {
        ALOGE("%s: failed to load gralloc module\n");
        return false;
    }

    IntelWsbm *wsbm = new IntelWsbm(mDrmFd);
    if (!wsbm) {
        ALOGE("%s: failed to create wsbm object\n", __func__);
        return false;
    }

    if (!(wsbm->initialize())) {
        ALOGE("%s: failed to initialize wsbm\n", __func__);
        delete wsbm;
        return false;
    }

    mWsbm = wsbm;

    mInitialized = true;
    return true;
}

IntelDisplayBuffer* IntelGraphicBufferManager::map(uint32_t handle,
                                                   uint32_t sub)
{
    IntelDisplayBuffer *buffer;
    void *vaddr = 0;
    uint32_t size = 0;
    int gttOffsetInPage = 0;
    intel_gralloc_buffer_handle_t *grallocHandle =
        (intel_gralloc_buffer_handle_t*)handle;

    if (!initCheck())
        return 0;

    if (!grallocHandle) {
        ALOGE("%s: invalid buffer handle\n", __func__);
        return 0;
    }

    int err = mIMGGrallocModule->getCpuAddress(mIMGGrallocModule,
                                               grallocHandle->ui64Stamp,
                                               sub,
                                               &vaddr,
                                               &size);
    if (err) {
        ALOGE("%s: failed to get virtual address %d\n", __func__, err);
        return 0;
    }

    // map to gtt
    bool ret = gttMap(vaddr, size, 0, &gttOffsetInPage);
    if (!ret) {
        ALOGE("%s: failed to map gtt\n", __func__);
        goto gtt_err;
    }

    buffer = new IntelDisplayBuffer(0,
                                    vaddr,
                                    gttOffsetInPage,
                                    size,
                                    handle);
    return buffer;
gtt_err:
    mIMGGrallocModule->putCpuAddress(mIMGGrallocModule,
                                     grallocHandle->ui64Stamp);
    return 0;
}

void IntelGraphicBufferManager::unmap(IntelDisplayBuffer *buffer)
{
    void *vaddr;
    intel_gralloc_buffer_handle_t *grallocHandle;

    if (!buffer)
        return;

    if (!initCheck())
        return;

    grallocHandle = (intel_gralloc_buffer_handle_t *)buffer->getHandle();
    vaddr = buffer->getCpuAddr();

    // unmap GTT
    bool ret = gttUnmap(vaddr);
    if (ret == false)
        ALOGE("%s: failed to unmap from GTT\n", __func__);

    // release CPU address
    mIMGGrallocModule->putCpuAddress(mIMGGrallocModule,
                                     grallocHandle->ui64Stamp);

    // destroy it
    delete buffer;
}

IntelDisplayBuffer* IntelGraphicBufferManager::get(int size, int alignment)
{
    if (!mWsbm) {
        ALOGE("%s: no wsbm found\n", __func__);
        return NULL;
    }

    void *wsbmBufferObject = NULL;
    bool ret = mWsbm->allocateTTMBuffer(size, alignment, &wsbmBufferObject);
    if (ret == false) {
        ALOGE("%s: failed to allocate buffer. size %d, alignment %d\n",
            __func__, size, alignment);
        return NULL;
    }

    void *virtAddr = mWsbm->getCPUAddress(wsbmBufferObject);
    uint32_t gttOffsetInPage = mWsbm->getGttOffset(wsbmBufferObject);
    uint32_t handle = mWsbm->getKBufHandle(wsbmBufferObject);
    // create new buffer
    IntelDisplayBuffer *buffer = new IntelDisplayBuffer(wsbmBufferObject,
                                                        virtAddr,
                                                        gttOffsetInPage,
                                                        size,
                                                        handle);
    ALOGD_IF(ALLOW_BUFFER_PRINT,
            "%s: created TTM overlay buffer. cpu %p, gtt %d\n",
         __func__, virtAddr, gttOffsetInPage);
    return buffer;
}

void IntelGraphicBufferManager::put(IntelDisplayBuffer* buf)
{
    if (!buf || !mWsbm) {
        ALOGE("%s: Invalid parameter\n", __func__);
        return;
    }

    void *wsbmBufferObject = buf->getBufferObject();
    bool ret = mWsbm->destroyTTMBuffer(wsbmBufferObject);
    if (ret == false)
        ALOGW("%s: failed to free wsbmBO\n", __func__);

    // free overlay buffer
    delete buf;
}

IntelDisplayBuffer* IntelGraphicBufferManager::wrap(void *addr, int size)
{
    if (!mWsbm) {
        ALOGE("%s: no wsbm found\n", __func__);
        return 0;
    }

    void *wsbmBufferObject;
    uint32_t handle = (uint32_t)addr;
    bool ret = mWsbm->wrapTTMBuffer(handle, &wsbmBufferObject);
    if (ret == false) {
        ALOGE("%s: wrap ttm buffer failed\n", __func__);
        return 0;
    }

    ret = mWsbm->waitIdleTTMBuffer(wsbmBufferObject);
    if (ret == false) {
        ALOGE("%s: wait ttm buffer idle failed\n", __func__);
        return 0;
    }

    void *virtAddr = mWsbm->getCPUAddress(wsbmBufferObject);
    uint32_t gttOffsetInPage = mWsbm->getGttOffset(wsbmBufferObject);

    if (!gttOffsetInPage || !virtAddr) {
        ALOGW("GTT offset:%x Virtual addr: %p.", gttOffsetInPage, virtAddr);
        return 0;
    }

    IntelDisplayBuffer *buf = new IntelDisplayBuffer(wsbmBufferObject,
                                                     virtAddr,
                                                     gttOffsetInPage,
                                                     0);
    return buf;
}

void IntelGraphicBufferManager::unwrap(IntelDisplayBuffer *buffer)
{
    if (!mWsbm) {
        ALOGE("%s: no wsbm found\n", __func__);
        return;
    }

    if (!buffer)
        return;

    mWsbm->unreferenceTTMBuffer(buffer->getBufferObject());
    // destroy it
    delete buffer;
}

void IntelGraphicBufferManager::waitIdle(uint32_t khandle)
{
    if (!mWsbm) {
        ALOGE("%s: no wsbm found\n", __func__);
        return;
    }

    if (!khandle)
        return;

    void *wsbmBufferObject;;
    bool ret = mWsbm->wrapTTMBuffer(khandle, &wsbmBufferObject);
    if (ret == false) {
        ALOGE("%s: wrap ttm buffer failed\n", __func__);
        return;
    }

    ret = mWsbm->waitIdleTTMBuffer(wsbmBufferObject);
    if (ret == false) {
        ALOGE("%s: wait ttm buffer idle failed\n", __func__);
        return;
    }

    mWsbm->unreferenceTTMBuffer(wsbmBufferObject);
    return;
}

bool IntelGraphicBufferManager::alloc(uint32_t size,
                          uint32_t* um_handle, uint32_t* km_handle)
{
    return true;
}

bool IntelGraphicBufferManager::dealloc(uint32_t um_handle)
{
    return true;
}

IntelPayloadBuffer::IntelPayloadBuffer(IntelBufferManager* bufferManager,
                                         uint32_t handle, int sub)
        : mBufferManager(bufferManager), mBuffer(0), mPayload(0)
{
    if (!handle || sub < 0 || sub >= GRALLOC_SUB_BUFFER_MAX)
        return;

    mBuffer = mBufferManager->map(handle, sub);
    if (!mBuffer) {
        LOGE("%s: failed to map payload buffer.\n", __func__);
        return;
    }
    mPayload = mBuffer->getCpuAddr();
}

IntelPayloadBuffer::~IntelPayloadBuffer()
{
    if (mBufferManager && mBuffer) {
        mBufferManager->unmap(mBuffer);
    }
}

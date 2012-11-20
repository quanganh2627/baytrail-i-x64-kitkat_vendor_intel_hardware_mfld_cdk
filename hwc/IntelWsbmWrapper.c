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

#include <wsbm_pool.h>
#include <wsbm_driver.h>
#include <wsbm_manager.h>
#include <wsbm_util.h>
#include <linux/psb_drm.h>
#include <cutils/log.h>
#include <xf86drm.h>
#include <IntelOverlayUtil.h>

struct _WsbmBufferPool * mainPool = NULL;

struct PVRWsbmValidateNode
{
struct  _ValidateNode base;
    struct psb_validate_arg arg;
};

static struct _ValidateNode * pvrAlloc(struct _WsbmVNodeFuncs * func,
                                       int typeId)
{
    ALOGV("%s: allocating ...\n", __func__);

    if(typeId == 0) {
        struct PVRWsbmValidateNode * vNode = malloc(sizeof(*vNode));
        if(!vNode) {
            ALOGE("%s: allocate memory failed\n", __func__);
            return NULL;
        }

        vNode->base.func = func;
        vNode->base.type_id = 0;
        return &vNode->base;
    } else {
        struct _ValidateNode * node = malloc(sizeof(*node));
        if(!node) {
            ALOGE("%s: allocate node failed\n", __func__);
            return NULL;
        }

        node->func = func;
        node->type_id = 1;
        return node;
    }
}

static void pvrFree(struct _ValidateNode * node)
{
    ALOGV("%s: free ...\n", __func__);
    if(node->type_id == 0) {
        free(containerOf(node, struct PVRWsbmValidateNode, base));
    } else {
        free(node);
    }
}

static void pvrClear(struct _ValidateNode * node)
{
    ALOGV("%s: clearing ...\n", __FUNCTION__);
    if(node->type_id == 0) {
        struct PVRWsbmValidateNode * vNode =
            containerOf(node, struct PVRWsbmValidateNode, base);
        memset(&vNode->arg.d.req, 0, sizeof(vNode->arg.d.req));
    }
}

static struct _WsbmVNodeFuncs vNodeFuncs = {
    .alloc  = pvrAlloc,
    .free   = pvrFree,
    .clear  = pvrClear,
};

int pvrWsbmInitialize(int drmFD)
{
    union drm_psb_extension_arg arg;
    const char drmExt[] = "psb_ttm_placement_alphadrop";
    int ret = 0;

    ALOGV("%s: wsbm initializing...\n", __func__);

    if(drmFD <= 0) {
        ALOGE("%s: invalid drm fd %d\n", __func__, drmFD);
        return drmFD;
    }

    /*init wsbm*/
    ret = wsbmInit(wsbmNullThreadFuncs(), &vNodeFuncs);
    if (ret) {
        ALOGE("%s: WSBM init failed with error code %d\n",
             __func__, ret);
        return ret;
    }

    ALOGV("%s: DRM_PSB_EXTENSION %d\n", __func__, DRM_PSB_EXTENSION);

    /*get devOffset via drm IOCTL*/
    strncpy(arg.extension, drmExt, sizeof(drmExt));

    ret = drmCommandWriteRead(drmFD, 6/*DRM_PSB_EXTENSION*/, &arg, sizeof(arg));
    if(ret || !arg.rep.exists) {
        ALOGE("%s: get device offset failed with error code %d\n",
             __func__, ret);
        goto out;
    }

    ALOGV("%s: ioctl offset 0x%x\n", __func__, arg.rep.driver_ioctl_offset);

    mainPool = wsbmTTMPoolInit(drmFD, arg.rep.driver_ioctl_offset);
    if(!mainPool) {
        ALOGE("%s: TTM Pool init failed\n", __func__);
        ret = -EINVAL;
        goto out;
    }

    ALOGV("%s: PVRWsbm initialized successfully. mainPool %p\n",
         __func__, mainPool);

    return 0;

out:
    wsbmTakedown();
    return ret;
}

void pvrWsbmTakedown()
{
    ALOGV("%s: Takedown wsbm...\n", __func__);

    wsbmPoolTakeDown(mainPool);
    wsbmTakedown();
}

int pvrWsbmAllocateTTMBuffer(uint32_t size, uint32_t align, void ** buf)
{
    struct _WsbmBufferObject * wsbmBuf = NULL;
    int ret = 0;
    int offset = 0;

    ALOGV("%s: allocating TTM buffer... size %d\n",
         __func__, align_to(size, 4096));

    if(!buf) {
        ALOGE("%s: invalid parameter\n", __func__);
        return -EINVAL;
    }

    ALOGV("%s: mainPool %p\n", __func__, mainPool);

    ret = wsbmGenBuffers(mainPool, 1, &wsbmBuf, align,
                        (WSBM_PL_FLAG_VRAM | WSBM_PL_FLAG_TT |
                         WSBM_PL_FLAG_SHARED | WSBM_PL_FLAG_NO_EVICT));
    if(ret) {
        ALOGE("%s: wsbmGenBuffers failed with error code %d\n",
             __func__, ret);
        return ret;
    }

    ret = wsbmBOData(wsbmBuf, align_to(size, 4096), NULL, NULL, 0);
    if(ret) {
        ALOGE("%s: wsbmBOData failed with error code %d\n",
             __func__, ret);
        /*FIXME: should I unreference this buffer here?*/
        return ret;
    }

    wsbmBOReference(wsbmBuf);

    *buf = wsbmBuf;

    ALOGV("%s: ttm buffer allocated. %p\n", __func__, *buf);
    return 0;
}

int pvrWsbmWrapTTMBuffer(uint32_t handle, void **buf)
{
    int ret = 0;
    struct _WsbmBufferObject *wsbmBuf;

    if (!buf) {
        ALOGE("%s: Invalid parameter\n", __func__);
        return -EINVAL;
    }

    ret = wsbmGenBuffers(mainPool, 1, &wsbmBuf, 0,
                        (WSBM_PL_FLAG_VRAM | WSBM_PL_FLAG_TT |
                        /*WSBM_PL_FLAG_NO_EVICT |*/ WSBM_PL_FLAG_SHARED));

    if (ret) {
        ALOGE("%s: wsbmGenBuffers failed with error code %d\n",
             __func__, ret);
        return ret;
    }

    ret = wsbmBOSetReferenced(wsbmBuf, handle);
    if (ret) {
        ALOGE("%s: wsbmBOSetReferenced failed with error code %d\n",
             __func__, ret);

        return ret;
    }

    *buf = (void *)wsbmBuf;

    ALOGV("%s: wrap buffer %p for handle 0x%x\n", __func__, wsbmBuf, handle);
    return 0;
}

int pvrWsbmUnReference(void *buf)
{
    struct _WsbmBufferObject *wsbmBuf;

    if (!buf) {
        ALOGE("%s: Invalid parameter\n", __func__);
        return -EINVAL;
    }

    wsbmBuf = (struct _WsbmBufferObject *)buf;

    wsbmBOUnreference(&wsbmBuf);

    return 0;
}

int pvrWsbmDestroyTTMBuffer(void * buf)
{
    ALOGV("%s: destroying buffer...\n", __func__);

    if(!buf) {
        ALOGE("%s: invalid ttm buffer\n", __func__);
        return -EINVAL;
    }

    /*FIXME: should I unmap this buffer object first?*/
    wsbmBOUnmap((struct _WsbmBufferObject *)buf);

    wsbmBOUnreference((struct _WsbmBufferObject **)&buf);

    ALOGV("%s: destroyed\n", __func__);

    return 0;
}

void * pvrWsbmGetCPUAddress(void * buf)
{
    if(!buf) {
        ALOGE("%s: Invalid ttm buffer\n", __func__);
        return NULL;
    }

    ALOGV("%s: getting cpu address. buffer object %p\n", __func__, buf);

    void * address = wsbmBOMap((struct _WsbmBufferObject *)buf,
                                WSBM_ACCESS_READ | WSBM_ACCESS_WRITE);
    if(!address) {
        ALOGE("%s: buffer object mapping failed\n", __func__);
        return NULL;
    }

    ALOGV("%s: mapped successfully. %p, size %ld\n", __func__,
        address, wsbmBOSize((struct _WsbmBufferObject *)buf));

    return address;
}

uint32_t pvrWsbmGetGttOffset(void * buf)
{
    if(!buf) {
        ALOGE("%s: Invalid ttm buffer\n", __func__);
        return 0;
    }

    ALOGV("%s: getting gtt offset... buffer object %p\n", __func__, buf);

    uint32_t offset =
        wsbmBOOffsetHint((struct _WsbmBufferObject *)buf) & 0x0fffffff;

    ALOGV("%s: successfully. offset %x\n", __func__, offset >> 12);

    return offset >> 12;
}

uint32_t pvrWsbmGetKBufHandle(void *buf)
{
    if (!buf) {
        ALOGE("%s: Invalid ttm buffer\n", __func__);
        return 0;
    }

    return (wsbmKBufHandle(wsbmKBuf((struct _WsbmBufferObject *)buf)));
}

uint32_t pvrWsbmWaitIdle(void *buf)
{
    if (!buf) {
        ALOGE("%s: Invalid ttm buffer\n", __func__);
        return -EINVAL;
    }

    wsbmBOWaitIdle(buf, 0);
    return 0;
}

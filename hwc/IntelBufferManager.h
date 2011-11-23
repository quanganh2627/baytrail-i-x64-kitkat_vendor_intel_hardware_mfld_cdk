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

#ifndef __INTEL_BUFFER_MANAGER_H__
#define __INTEL_BUFFER_MANAGER_H__

#include <ui/GraphicBufferMapper.h>
#include <hardware/hardware.h>
#include <IntelWsbm.h>
#include <pvr2d.h>
#include <pthread.h>
#include <services.h>

class IntelDisplayBuffer
{
protected:
    void *mBufferObject;
    uint32_t mHandle;
    uint32_t mSize;
    void *mVirtAddr;
    uint32_t mGttOffsetInPage;
    uint32_t mStride;
public:
     IntelDisplayBuffer()
         : mBufferObject(0), mHandle(0),
           mSize(0), mVirtAddr(0), mGttOffsetInPage(0) {}
     IntelDisplayBuffer(void *buf, void *addr,
                        uint32_t gtt, uint32_t size, uint32_t handle = 0)
         : mBufferObject(buf),
           mHandle(handle),
           mSize(size),
           mVirtAddr(addr),
           mGttOffsetInPage(gtt) {}
     ~IntelDisplayBuffer() {}
     void* getBufferObject() const { return mBufferObject; }
     void* getCpuAddr() const { return mVirtAddr; }
     uint32_t getGttOffsetInPage() const { return mGttOffsetInPage; }
     uint32_t getSize() const { return mSize; }
     uint32_t getHandle() const { return mHandle; }
     void setStride(int stride) { mStride = stride; }
     uint32_t getStride() const { return mStride; }
};

// pixel format supported by HWC
// TODO: share the extended pixel format with gralloc HAL
enum {
    HAL_PIXEL_FORMAT_INTEL_HWC_NV12 = 0x100,
    HAL_PIXEL_FORMAT_INTEL_HWC_YUY2 = 0x101,
    HAL_PIXEL_FORMAT_INTEL_HWC_UYVY = 0x102,
    HAL_PIXEL_FORMAT_INTEL_HWC_I420 = 0x103,
};

class IntelDisplayDataBuffer : public IntelDisplayBuffer
{
public:
    enum {
        BUFFER_CHANGE = 0x00000001UL,
        SIZE_CHANGE   = 0x00000002UL,
    };
private:
    uint32_t mFormat;
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mYStride;
    uint32_t mUVStride;
    uint32_t mSrcX;
    uint32_t mSrcY;
    uint32_t mSrcWidth;
    uint32_t mSrcHeight;
    uint32_t mUpdateFlags;
    IntelDisplayBuffer *mBuffer;
public:
    IntelDisplayDataBuffer();
    IntelDisplayDataBuffer(uint32_t format, uint32_t w, uint32_t h);
    ~IntelDisplayDataBuffer() {}
    void setFormat(int format) { mFormat = format; }
    void setBuffer(IntelDisplayBuffer *buffer);
    IntelDisplayBuffer* getBuffer() { return mBuffer; }
    void setWidth(uint32_t w);
    void setHeight(uint32_t h);
    void setStride(uint32_t width);
    void setStride(uint32_t yStride, uint32_t uvStride);
    void setCrop(int x, int y, int w, int h);
    void inline appendFlags(uint32_t flags) { mUpdateFlags |= flags; }
    void inline removeFlags(uint32_t flags) { mUpdateFlags &= ~flags; }
    void inline clearFlags() { mUpdateFlags = 0; }
    bool inline isFlags(uint32_t flags) {
        return (mUpdateFlags & flags) ? true : false;
    }
    uint32_t inline getFormat() const { return mFormat; }
    uint32_t inline getWidth() const { return mWidth; }
    uint32_t inline getHeight() const { return mHeight; }
    uint32_t inline getYStride() const { return mYStride; }
    uint32_t inline getUVStride() const { return mUVStride; }
    uint32_t inline getSrcX() const { return mSrcX; }
    uint32_t inline getSrcY() const { return mSrcY; }
    uint32_t inline getSrcWidth() const { return mSrcWidth; }
    uint32_t inline getSrcHeight() const { return mSrcHeight; }
};

class IntelBufferManager
{
public:
    enum {
        TTM_BUFFER = 1,
        PVR_BUFFER,
    };
protected:
    int mDrmFd;
    bool mInitialized;
public:
    virtual bool initialize() { return true; }
    virtual IntelDisplayBuffer* get(int size, int gttAlignment) { return 0; }
    virtual void put(IntelDisplayBuffer *buf) {}
    virtual IntelDisplayBuffer* map(uint32_t handle) { return 0; }
    virtual IntelDisplayBuffer* map(uint32_t device, uint32_t handle) {
        return 0;
    }
    virtual void unmap(uint32_t handle, IntelDisplayBuffer *buffer) {}
    virtual IntelDisplayBuffer* wrap(void *virt, int size) {
        return 0;
    }
    virtual void unwrap(IntelDisplayBuffer *buffer) {}
    bool initCheck() const { return mInitialized; }
    IntelBufferManager(int fd)
        : mDrmFd(fd), mInitialized(false) {
    }
    virtual ~IntelBufferManager() {};
};

class IntelTTMBufferManager : public IntelBufferManager
{
private:
    IntelWsbm *mWsbm;
    uint32_t mVideoBridgeIoctl;
private:
    bool getVideoBridgeIoctl();
    uint32_t getBufferHandle(uint32_t device, uint32_t handle);
public:
    IntelTTMBufferManager(int fd)
        : IntelBufferManager(fd), mWsbm(0), mVideoBridgeIoctl(0) {}
    ~IntelTTMBufferManager();
    bool initialize();
    IntelDisplayBuffer* map(uint32_t handle);
    IntelDisplayBuffer* map(uint32_t device, uint32_t handle);
    IntelDisplayBuffer* get(int size, int gttAlignment);
    void put(IntelDisplayBuffer *buf);
    void unmap(uint32_t handle, IntelDisplayBuffer *buffer);
};

// FIXME: the same struct as IMG_native_handle_t
// include hal.h instead of defining it here
typedef struct {
    native_handle_t base;
    int fd;
    unsigned long long ui64Stamp;
    int usage;
    int width;
    int height;
    int bpp;
    int format;
}__attribute__((aligned(sizeof(int)),packed)) intel_gralloc_buffer_handle_t;

class IntelPVRBufferManager : public IntelBufferManager {
private:
    PVR2DCONTEXTHANDLE mPVR2DHandle;
private:
    bool pvr2DInit();
    void pvr2DDestroy();
    bool gttMap(PVR2DMEMINFO *buf, int *offset,
                uint32_t virt, uint32_t size,  uint32_t gttAlign);
    bool gttUnmap(PVR2DMEMINFO *buf);
public:
    IntelPVRBufferManager(int fd)
        : IntelBufferManager(fd), mPVR2DHandle(0) {}
    ~IntelPVRBufferManager();
    bool initialize();
    IntelDisplayBuffer* wrap(void *virt, int size);
    void unwrap(IntelDisplayBuffer *buffer);
    IntelDisplayBuffer* map(uint32_t handle);
    void unmap(uint32_t handle, IntelDisplayBuffer *buffer);
};

// NOTE: the number of max device devices should be aligned with kernel driver
#define INTEL_BCD_DEVICE_NUM_MAX    9
#define INTEL_BCD_BUFFER_NUM_MAX    20

class IntelBCDBufferManager : public IntelBufferManager {
private:
    // FIXME: remove wsbm later
    IntelWsbm *mWsbm;
private:
    bool gttMap(uint32_t devId, uint32_t bufferId,
                uint32_t gttAlign, int *offset);
    bool gttUnmap(uint32_t devId, uint32_t bufferId);
    bool getBCDInfo(uint32_t devId, uint32_t *count, uint32_t *stride);
public:
    IntelBCDBufferManager(int fd);
    ~IntelBCDBufferManager();
    bool initialize();
    IntelDisplayBuffer* map(uint32_t device, uint32_t handle);
    void unmap(uint32_t handle, IntelDisplayBuffer *buffer);
    IntelDisplayBuffer** map(uint32_t device, uint32_t *count);
    void unmap(IntelDisplayBuffer **buffer, uint32_t count);
    IntelDisplayBuffer* get(int size, int alignment);
    void put(IntelDisplayBuffer* buf);
};

class IntelGraphicBufferManager : public IntelBufferManager {
private:
    PVRSRV_CONNECTION *mPVRSrvConnection;
    PVRSRV_DEV_DATA mDevData;
    IMG_HANDLE mDevMemContext;
    IMG_HANDLE mGeneralHeap;
private:
    bool gttMap(PVRSRV_CLIENT_MEM_INFO *memInfo,
                uint32_t gttAlign, int *offset);
    bool gttUnmap(PVRSRV_CLIENT_MEM_INFO *memInfo);
public:
    IntelGraphicBufferManager(int fd)
        : IntelBufferManager(fd) {}
    ~IntelGraphicBufferManager();
    bool initialize();
    IntelDisplayBuffer* map(uint32_t handle);
    void unmap(uint32_t handle, IntelDisplayBuffer *buffer);
};
#endif /*__INTEL_PVR_BUFFER_MANAGER_H__*/

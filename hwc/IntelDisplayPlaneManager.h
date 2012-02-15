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

#ifndef __INTEL_DISPLAY_PLANE_MANAGER_H__
#define __INTEL_DISPLAY_PLANE_MANAGER_H__

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <IntelHWComposerDrm.h>
#include <IntelBufferManager.h>
#include <IntelOverlayHW.h>

#include <psb_drm.h>

class IntelDisplayPlaneContext {
public:
    IntelDisplayPlaneContext() {}
    virtual ~IntelDisplayPlaneContext() {}
};

typedef struct intel_display_plane_position {
    int left;
    int top;
    int right;
    int bottom;
} intel_display_plane_position_t;

typedef enum {
    PIPE_MIPI0 = 0,
    PIPE_MIPI1,
    PIPE_HDMI,
} intel_display_pipe_t;

class IntelDisplayPlane {
public:
	enum {
            DISPLAY_PLANE_SPRITE = 1,
            DISPLAY_PLANE_OVERLAY,
	};

	// flush flags
        enum {
            FLASH_NEEDED     = 0x00000001UL,
            WAIT_VBLANK      = 0x00000002UL,
            UPDATE_COEF      = 0x00000004UL,
            UPDATE_CONTROL   = 0x00000008UL,
            UPDATE_SURFACE   = 0x00000010UL,
        };
protected:
    int mDrmFd;
    int mType;
    int mIndex;
    IntelBufferManager *mBufferManager;
    IntelDisplayPlaneContext *mContext;

    // plane parameters
    IntelDisplayBuffer *mDataBuffer;
    intel_display_plane_position_t mPosition;

    bool mInitialized;
public:
    IntelDisplayPlane(int fd, int type,
                      int index, IntelBufferManager *bufferManager)
        : mDrmFd(fd), mType(type), mIndex(index),
          mBufferManager(bufferManager),
          mContext(0), mDataBuffer(0), mInitialized(false) {
	    memset(&mPosition, 0, sizeof(intel_display_plane_position_t));
    }
    virtual ~IntelDisplayPlane() {}
    virtual bool initCheck() const { return mInitialized; }
    virtual int getPlaneType() const { return mType; }
    virtual IntelDisplayPlaneContext* getContext() { return mContext; }
    virtual void setPosition(int left, int top, int right, int bottom) {
        mPosition.left = left;
        mPosition.top = top;
        mPosition.right = right;
        mPosition.bottom = bottom;
    }
    virtual bool setDataBuffer(uint32_t hanlde, uint32_t flags) { return true; }
    virtual bool setDataBuffer(IntelDisplayBuffer& buffer) {
        mDataBuffer = &buffer;
        return true;
    }
    virtual IntelDisplayBuffer* getDataBuffer() {
        return mDataBuffer;
    }
    virtual bool invalidateDataBuffer() { return true; }
    virtual bool flip(uint32_t flags) { return true; }
    virtual bool enable() { return true; }
    virtual bool disable() { return true; }
    virtual bool reset() { return true; }
    virtual void setPipe(intel_display_pipe_t pipe) {}
    virtual void setPipeByMode(intel_overlay_mode_t displayMode) {}

    // DRM mode change handler
    virtual uint32_t onDrmModeChange() { return 0; }

    friend class IntelDisplayPlaneManager;
};

typedef struct {
    int x;
    int y;
    int w;
    int h;
} intel_overlay_position_t;

typedef enum {
    OVERLAY_INIT = 0,
    OVERLAY_RESET,
    OVERLAY_DISABLED,
    OVERLAY_ENABLED,
} intel_overlay_state_t;

typedef enum {
    OVERLAY_ROTATE_0 = 0,
    OVERLAY_ROTATE_90,
    OVERLAY_ROTATE_180,
    OVERLAY_ROTATE_270,
} intel_overlay_rotation_t;

typedef enum {
    OVERLAY_ORIENTATION_PORTRAINT = 1,
    OVERLAY_ORIENTATION_LANDSCAPE,
} intel_overlay_orientation_t;

typedef struct {
    // back buffer info
    uint32_t back_buffer_handle;
    uint32_t gtt_offset_in_page;

    // pipe configuration
    uint32_t pipe;

    // dest info
    intel_overlay_position_t position;
    intel_overlay_rotation_t rotation;
    intel_overlay_orientation_t orientation;
    bool is_rotated;
    bool position_changed;

    // power info
    intel_overlay_state_t state;

    // ashmem related
    pthread_mutex_t lock;
    pthread_mutexattr_t attr;
    volatile int32_t refCount;
} intel_overlay_context_t;

class IntelOverlayContext : public IntelDisplayPlaneContext
{
private:
    int mHandle;
    intel_overlay_context_t *mContext;
    intel_overlay_back_buffer_t *mOverlayBackBuffer;
    IntelDisplayBuffer *mBackBuffer;
    int mSize;
    int mDrmFd;
    IntelBufferManager *mBufferManager;
protected:
    bool backBufferInit();
    bool bufferOffsetSetup(IntelDisplayDataBuffer& buf);
    uint32_t calculateSWidthSW(uint32_t offset, uint32_t width);
    bool coordinateSetup(IntelDisplayDataBuffer& buf);
    bool setCoeffRegs(double *coeff, int mantSize, coeffPtr pCoeff, int pos);
    void updateCoeff(int taps, double fCutoff, bool isHoriz, bool isY,
                     coeffPtr pCoeff);
    bool scalingSetup(IntelDisplayDataBuffer& buffer);
    intel_overlay_state_t getOverlayState() const;
    void setOverlayState(intel_overlay_state_t state);
    void checkPosition(int& x, int& y, int& w, int& h);

    intel_overlay_mode_t drmModeChanged(IntelOverlayContext& context);

    void lock();
    void unlock();
public:
    IntelOverlayContext(int drmFd, IntelBufferManager *bufferManager = NULL)
        :mHandle(0),
         mContext(0),
         mOverlayBackBuffer(0),
         mBackBuffer(0),
         mSize(0), mDrmFd(drmFd),
         mBufferManager(bufferManager) {}
    ~IntelOverlayContext();

    // ashmen operations
    bool create();
    bool open(int handle, int size);
    bool destroy();
    void clean();
    int getSize() const { return mSize; }

    // operations to context
    void setBackBufferGttOffset(const uint32_t gttOffset);
    uint32_t getGttOffsetInPage();

    intel_overlay_orientation_t getOrientation();
    intel_overlay_back_buffer_t* getBackBuffer() { return mOverlayBackBuffer; }

    // interfaces for data device
    bool setDataBuffer(IntelDisplayDataBuffer& dataBuffer);

    // interfaces for control device
    void setRotation(int rotation);
    void setPosition(int x, int y, int w, int h);

    // interfaces for both data & control devices
    bool flush(uint32_t flags);
    bool enable();
    bool disable();
    bool reset();
    void setPipe(intel_display_pipe_t pipe);
    void setPipeByMode(intel_overlay_mode_t displayMode);

    // DRM mode change handle
    intel_overlay_mode_t onDrmModeChange();
};

class IntelWidiPlane; // forward declaration

class IntelOverlayPlane : public IntelDisplayPlane {
private:
    enum {
        OVERLAY_DATA_BUFFER_NUM_MAX = 30,
    };
    // overlay mapped data buffers
    struct {
        uint32_t handle;
        IntelDisplayBuffer *buffer;
        uint32_t bufferType;
    } mDataBuffers[OVERLAY_DATA_BUFFER_NUM_MAX];
    int mNextBuffer;
    IntelWidiPlane* mWidiPlane;

public:
    IntelOverlayPlane(int fd, int index, IntelBufferManager *bufferManager);
    ~IntelOverlayPlane();
    virtual void setPosition(int left, int top, int right, int bottom);
    virtual bool setDataBuffer(uint32_t handle, uint32_t flags, intel_gralloc_buffer_handle_t* nHandle);
    virtual bool setDataBuffer(IntelDisplayBuffer& buffer);

    virtual bool invalidateDataBuffer();
    virtual bool flip(uint32_t flags);
    virtual bool reset();
    virtual bool disable();
    virtual void setPipe(intel_display_pipe_t pipe);
    virtual void setPipeByMode(intel_overlay_mode_t displayMode);
    virtual uint32_t onDrmModeChange();

    virtual bool setWidiPlane(IntelDisplayPlane*);

};

enum {
    INTEL_SPRITE_PIXEL_FORMAT_BGRX565  = 0x14000000UL,
    INTEL_SPRITE_PIXEL_FORMAT_BGRX8888 = 0x18000000UL,
    INTEL_SPRITE_PIXEL_FORMAT_BGRA8888 = 0x1c000000UL,
    INTEL_SPRITE_PIXEL_FORMAT_RGBX8888 = 0x38000000UL,
    INTEL_SPRITE_PIXEL_FORMAT_RGBA8888 = 0x3c000000UL,
};

typedef struct intel_sprite_context intel_sprite_context_t;

class IntelSpriteContext : public IntelDisplayPlaneContext {
private:
    intel_sprite_context_t mContext;
public:
    IntelSpriteContext() {
        memset(&mContext, 0, sizeof(mContext));
    }
    ~IntelSpriteContext();
    intel_sprite_context_t* getContext() { return &mContext; }
};

class IntelSpritePlane : public IntelDisplayPlane {
public:
    IntelSpritePlane(int fd, int index, IntelBufferManager *bufferManager);
    virtual ~IntelSpritePlane();
    virtual void setPosition(int left, int top, int right, int bottom);
    virtual bool setDataBuffer(IntelDisplayBuffer& buffer);
    virtual bool invalidateDataBuffer();
    virtual bool flip(uint32_t flags);
    virtual bool reset();
    virtual bool disable();
};

class MedfieldSpritePlane : public IntelSpritePlane {
private:
    enum {
        SPRITE_DATA_BUFFER_NUM_MAX = 3,
    };

    struct {
        uint32_t handle;
        IntelDisplayBuffer *buffer;
    } mDataBuffers[SPRITE_DATA_BUFFER_NUM_MAX];
    int mNextBuffer;
public:
    MedfieldSpritePlane(int fd, int index, IntelBufferManager *bufferManager);
    ~MedfieldSpritePlane();
    virtual void setPosition(int left, int top, int right, int bottom);
    virtual bool setDataBuffer(IntelDisplayBuffer& buffer);
    virtual bool setDataBuffer(uint32_t handle, uint32_t flags);
    virtual bool invalidateDataBuffer();
    virtual bool flip(uint32_t flags);
    virtual bool reset();
    virtual bool disable();
};

class IntelDisplayPlaneManager {
private:
    int mSpritePlaneCount;
    int mOverlayPlaneCount;
    IntelDisplayPlane **mSpritePlanes;
    IntelDisplayPlane **mOverlayPlanes;
    IntelDisplayPlane *mWidiPlane;

    // Bitmap of free planes. Bit0 - plane A, bit 1 - plane B, etc.
    uint32_t mFreeSpritePlanes;
    uint32_t mFreeOverlayPlanes;
    uint32_t mReclaimedSpritePlanes;
    uint32_t mReclaimedOverlayPlanes;

    int mDrmFd;
    IntelBufferManager *mBufferManager;
    IntelBufferManager *mGrallocBufferManager;

    bool mInitialized;
private:
    int getPlane(uint32_t& mask);
    void putPlane(int index, uint32_t& mask);
public:
    IntelDisplayPlaneManager(int fd,
                             IntelBufferManager *bm,
                             IntelBufferManager *gm);
    virtual ~IntelDisplayPlaneManager();
    virtual bool initCheck() { return mInitialized; }
    virtual void detect();

    // plane allocation & free
    IntelDisplayPlane* getSpritePlane();
    IntelDisplayPlane* getOverlayPlane();
    IntelDisplayPlane* getWidiPlane();
    bool isWidiActive();
    void reclaimPlane(IntelDisplayPlane *plane);
    void disableReclaimedPlanes(int type);
};

#endif /*__INTEL_DISPLAY_PLANE_MANAGER_H__*/

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
#include <IntelWidiPlane.h>

IntelDisplayPlaneManager::IntelDisplayPlaneManager(int fd,
                                                   IntelBufferManager *bm,
                                                   IntelBufferManager *gm)
    : mSpritePlaneCount(0), mOverlayPlaneCount(0),
      mFreeSpritePlanes(0), mFreeOverlayPlanes(0),
      mReclaimedSpritePlanes(0), mReclaimedOverlayPlanes(0),
      mDrmFd(fd), mBufferManager(bm), mGrallocBufferManager(gm),
      mInitialized(false)
{
    int i;

    LOGV("%s\n", __func__);

    // detect display plane usage. Hopefully throw DRM ioctl
    detect();

    // allocate plane context
    mContextLength = 2 * sizeof(uint32_t);
    mContextLength += mSpritePlaneCount * sizeof(sprite_plane_context_t);
    mContextLength += mOverlayPlaneCount * sizeof(overlay_plane_context_t);

    mPlaneContexts = malloc(mContextLength);
    if (!mPlaneContexts) {
        LOGE("%s: failed to allocate plane contexts\n", __func__);
        return;
    }
    memset(mPlaneContexts, 0, mContextLength);

    // allocate sprite plane pool
    mSpritePlanes =
        (IntelDisplayPlane**)malloc(mSpritePlaneCount *sizeof(IntelDisplayPlane*));
    if (!mSpritePlanes) {
        LOGE("%s: failed to allocate sprite plane pool\n", __func__);
        goto sprite_alloc_err;
    }

    for (i = 0; i < mSpritePlaneCount; i++) {
        mSpritePlanes[i] =
            new MedfieldSpritePlane(mDrmFd, i, mGrallocBufferManager);
        if (!mSpritePlanes[i]) {
            LOGE("%s: failed to allocate sprite plane %d\n", __func__, i);
            goto sprite_init_err;
        }
        // reset overlay plane
        mSpritePlanes[i]->reset();
    }

    // allocate overlay plane pool
    mOverlayPlanes =
        (IntelDisplayPlane**)malloc(mOverlayPlaneCount *sizeof(IntelDisplayPlane*));
    if (!mOverlayPlanes) {
        LOGE("%s: failed to allocate overlay plane pool\n", __func__);
        goto sprite_init_err;
    }

    for (i = 0; i < mOverlayPlaneCount; i++) {
        mOverlayPlanes[i] =
            new IntelOverlayPlane(mDrmFd, i, mGrallocBufferManager);
        if (!mOverlayPlanes[i]) {
            LOGE("%s: failed to allocate overlay plane %d\n", __func__, i);
            goto overlay_alloc_err;
        }
        // reset overlay plane
        mOverlayPlanes[i]->reset();
    }

    // allocate Widi plane
    mWidiPlane = new IntelWidiPlane(mDrmFd,mOverlayPlaneCount, mGrallocBufferManager);
    if (!mWidiPlane) {
        LOGE("%s: failed to allocate widi plane %d\n", __func__, i);
        goto overlay_alloc_err;
    }

    mInitialized = true;
    return;

overlay_alloc_err:
    for (; i >= 0; i--)
        delete mOverlayPlanes[i];
    free(mOverlayPlanes);
    mOverlayPlanes = 0;
sprite_init_err:
    for (; i >= 0; i--)
	delete mSpritePlanes[i];
    free(mSpritePlanes);
    mSpritePlanes = 0;
sprite_alloc_err:
    free(mPlaneContexts);
    mInitialized = false;
}

IntelDisplayPlaneManager::~IntelDisplayPlaneManager()
{
    if (!initCheck())
        return;

    // delete sprite planes
    if (mSpritePlanes) {
        for (int i = 0; i < mSpritePlaneCount; i++) {
            if (mSpritePlanes[i]) {
                mSpritePlanes[i]->reset();
                delete mSpritePlanes[i];
            }
        }
        free(mSpritePlanes);
        mSpritePlanes = 0;
    }

    // delete overlay planes
    if (mOverlayPlanes) {
        for (int i = 0; i < mOverlayPlaneCount; i++) {
            if (mOverlayPlanes[i]) {
                mOverlayPlanes[i]->reset();
                delete mOverlayPlanes[i];
            }
        }
        free(mOverlayPlanes);
        mSpritePlanes = 0;
    }

    if (mWidiPlane)
        delete mWidiPlane;

    mInitialized = false;
}

void IntelDisplayPlaneManager::detect()
{
    mSpritePlaneCount = INTEL_SPRITE_PLANE_NUM;
    mOverlayPlaneCount = INTEL_OVERLAY_PLANE_NUM;
    // plane C
    mFreeSpritePlanes = 0x4;
    // both overlay A & C
    mFreeOverlayPlanes = 0x3;
}

int IntelDisplayPlaneManager::getPlane(uint32_t& mask)
{
    if (!mask)
        return -1;

    for (int i = 0; i < 32; i++) {
	int bit = (1 << i);
        if (bit & mask) {
            mask &= ~bit;
            return i;
        }
    }

    return -1;
}

void IntelDisplayPlaneManager::putPlane(int index, uint32_t& mask)
{
    if (index < 0 || index >= 32)
        return;

    int bit = (1 << index);

    if (bit & mask) {
        LOGW("%s: bit %d was set\n", __func__, index);
        return;
    }

    mask |= bit;
}

IntelDisplayPlane* IntelDisplayPlaneManager::getSpritePlane()
{
    if (!initCheck()) {
        LOGE("%s: plane manager was not initialized\n", __func__);
        return 0;
    }

    int freePlaneIndex;

    // check reclaimed overlay planes
    freePlaneIndex = getPlane(mReclaimedSpritePlanes);
    if (freePlaneIndex >= 0)
        return mSpritePlanes[freePlaneIndex];

    // check free overlay planes
    freePlaneIndex = getPlane(mFreeSpritePlanes);
    if (freePlaneIndex >= 0)
        return mSpritePlanes[freePlaneIndex];
    LOGE("%s: failed to get a sprite plane\n", __func__);
    return 0;
}

IntelDisplayPlane* IntelDisplayPlaneManager::getOverlayPlane()
{
    if (!initCheck()) {
        LOGE("%s: plane manager was not initialized\n", __func__);
        return 0;
    }

    int freePlaneIndex;

    // check reclaimed overlay planes
    freePlaneIndex = getPlane(mReclaimedOverlayPlanes);
    if (freePlaneIndex < 0) {
       // check free overlay planes
       freePlaneIndex = getPlane(mFreeOverlayPlanes);
    }

    if (freePlaneIndex < 0) {
       LOGE("%s: failed to get a overlay plane\n", __func__);
       return 0;
    }

    if (isWidiActive()) {
       ((IntelOverlayPlane*)mOverlayPlanes[freePlaneIndex])->setWidiPlane(mWidiPlane);
    }

    return mOverlayPlanes[freePlaneIndex];
}

void IntelDisplayPlaneManager::reclaimPlane(IntelDisplayPlane *plane)
{
    if (!plane)
        return;

    if (!initCheck()) {
        LOGE("%s: plane manager is not initialized\n", __func__);
        return;
    }

    int index = plane->mIndex;

    LOGV("%s: reclaimPlane %d\n", __func__, index);

    if (plane->mType == IntelDisplayPlane::DISPLAY_PLANE_OVERLAY)
        putPlane(index, mReclaimedOverlayPlanes);
    else if (plane->mType == IntelDisplayPlane::DISPLAY_PLANE_SPRITE)
        putPlane(index, mReclaimedSpritePlanes);
    else
        LOGE("%s: invalid plane type %d\n", __func__, plane->mType);
}

void IntelDisplayPlaneManager::disableReclaimedPlanes(int type)
{
    if (!initCheck()) {
        LOGE("%s: plane manager is not initialized\n", __func__);
        return;
    }

    // disable reclaimed sprite planes
    if (type == IntelDisplayPlane::DISPLAY_PLANE_SPRITE &&
        mSpritePlanes && mReclaimedSpritePlanes) {
        for (int i = 0; i < mSpritePlaneCount; i++) {
            int bit = (1 << i);
            if (mReclaimedSpritePlanes & bit) {
                if (mSpritePlanes[i]) {
                    // disable plane
                    mSpritePlanes[i]->disable();
                    // invalidate plane's data buffer
                    // mSpritePlanes[i]->invalidateDataBuffer();
                }
            }
        }
        // merge into free sprite bitmap
        mFreeSpritePlanes |= mReclaimedSpritePlanes;
        mReclaimedSpritePlanes = 0;
    }

    // disable reclaimed overlay planes
    if (type == IntelDisplayPlane::DISPLAY_PLANE_OVERLAY &&
        mOverlayPlanes && mReclaimedOverlayPlanes) {
        for (int i = 0; i < mOverlayPlaneCount; i++) {
            int bit = (1 << i);
            if (mReclaimedOverlayPlanes & bit) {
                if (mOverlayPlanes[i])
                    mOverlayPlanes[i]->disable();
                    mOverlayPlanes[i]->invalidateDataBuffer();
            }
        }
        // merge into free overlay bitmap
        mFreeOverlayPlanes |= mReclaimedOverlayPlanes;
        mReclaimedOverlayPlanes = 0;
    }
}

void* IntelDisplayPlaneManager::getPlaneContexts() const
{
    return mPlaneContexts;
}

int IntelDisplayPlaneManager::getContextLength() const
{
    return mContextLength;
}

IntelDisplayPlane*
IntelDisplayPlaneManager::getWidiPlane() {

    return mWidiPlane;
}

 bool
 IntelDisplayPlaneManager::isWidiActive() {


     if(mWidiPlane)
         return ((IntelWidiPlane*)mWidiPlane)->isActive();

     return false;
 }


bool IntelDisplayPlaneManager::dump(char *buff,
                                 int buff_len, int *cur_len)
{
    bool ret = true;

    mDumpBuf = buff;
    mDumpBuflen = buff_len;
    mDumpLen = *cur_len;

    dumpPrintf("     sprite plane count %d:\n", mSpritePlaneCount);
    dumpPrintf("     overlay plane count %d:\n", mOverlayPlaneCount);
    dumpPrintf("     free sprite plane %x:\n", mFreeSpritePlanes);
    dumpPrintf("     sprite plane count %d:\n", mOverlayPlaneCount);

    *cur_len = mDumpLen;

    return ret;
}


bool
 IntelDisplayPlaneManager::isWidiStatusChanged() {


     if(mWidiPlane)
         return ((IntelWidiPlane*)mWidiPlane)->isWidiStatusChanged();

     return false;
 }

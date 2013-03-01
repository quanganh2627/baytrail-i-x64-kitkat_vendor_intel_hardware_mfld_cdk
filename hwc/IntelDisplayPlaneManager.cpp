/*
 * Copyright (c) 2008-2012, Intel Corporation. All rights reserved.
 *
 * Redistribution.
 * Redistribution and use in binary form, without modification, are
 * permitted provided that the following conditions are met:
 *  * Redistributions must reproduce the above copyright notice and
 * the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its
 * suppliers may be used to endorse or promote products derived from
 * this software without specific  prior written permission.
 *  * No reverse engineering, decompilation, or disassembly of this
 * software is permitted.
 *
 * Limited patent license.
 * Intel Corporation grants a world-wide, royalty-free, non-exclusive
 * license under patents it now or hereafter owns or controls to make,
 * have made, use, import, offer to sell and sell ("Utilize") this
 * software, but solely to the extent that any such patent is necessary
 * to Utilize the software alone, or in combination with an operating
 * system licensed under an approved Open Source license as listed by
 * the Open Source Initiative at http://opensource.org/licenses.
 * The patent license shall not apply to any other combinations which
 * include this software. No hardware per se is licensed hereunder.
 *
 * DISCLAIMER.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#include <IntelDisplayPlaneManager.h>
#include <IntelWidiPlane.h>
#include <binder/ProcessState.h>
#ifdef INTEL_WIDI
#include <utils/Errors.h>
#include "FrameServer.h"
#endif

IntelDisplayPlaneManager::IntelDisplayPlaneManager(int fd,
                                                   IntelBufferManager *bm,
                                                   IntelBufferManager *gm)
    : mSpritePlaneCount(0), mPrimaryPlaneCount(0), mOverlayPlaneCount(0),
      mTotalPlaneCount(0), mMaxPlaneCount(0),
      mFreeSpritePlanes(0), mFreePrimaryPlanes(0), mFreeOverlayPlanes(0),
      mReclaimedSpritePlanes(0), mReclaimedPrimaryPlanes(0),
      mReclaimedOverlayPlanes(0),
      mDrmFd(fd), mBufferManager(bm), mGrallocBufferManager(gm),
      mInitialized(false)
{
    int i = 0;
#ifdef INTEL_WIDI
    status_t ret = NO_ERROR;
#endif

    ALOGD_IF(ALLOW_PLANE_PRINT, "%s\n", __func__);

    // detect display plane usage. Hopefully throw DRM ioctl
    detect();

    // allocate plane context
    mContextLength = sizeof(struct mdfld_plane_contexts);

    mPlaneContexts = malloc(mContextLength);
    if (!mPlaneContexts) {
        ALOGE("%s: failed to allocate plane contexts\n", __func__);
        return;
    }
    memset(mPlaneContexts, 0, mContextLength);

    // allocate primary plane pool
    if (mPrimaryPlaneCount) {
        mPrimaryPlanes = (IntelDisplayPlane**)calloc(mPrimaryPlaneCount,
                                                sizeof(IntelDisplayPlane*));
        if (!mPrimaryPlanes) {
            ALOGE("%s: failed to allocate primary plane pool\n", __func__);
            goto primary_alloc_err;
        }

        for (i = 0; i < mPrimaryPlaneCount; i++) {
            mPrimaryPlanes[i] =
                new MedfieldSpritePlane(mDrmFd, i, mGrallocBufferManager);
            if (!mPrimaryPlanes[i]) {
                ALOGE("%s: failed to allocate sprite plane %d\n", __func__, i);
                goto primary_init_err;
            }
            // set as primary plane
            mPrimaryPlanes[i]->mType = IntelDisplayPlane::DISPLAY_PLANE_PRIMARY;
            // reset overlay plane
            mPrimaryPlanes[i]->reset();
        }
    }

    // allocate sprite plane pool
    if (mSpritePlaneCount) {
        mSpritePlanes = (IntelDisplayPlane**)calloc(mSpritePlaneCount,
                                                sizeof(IntelDisplayPlane*));
        if (!mSpritePlanes) {
            ALOGE("%s: failed to allocate sprite plane pool\n", __func__);
            goto primary_init_err;
        }

        for (i = 0; i < mSpritePlaneCount; i++) {
            mSpritePlanes[i] =
                new MedfieldSpritePlane(mDrmFd, i, mGrallocBufferManager);
            if (!mSpritePlanes[i]) {
                ALOGE("%s: failed to allocate sprite plane %d\n", __func__, i);
                goto sprite_init_err;
            }
            // reset overlay plane
            mSpritePlanes[i]->reset();
        }
    }

    if (mOverlayPlaneCount) {
        // allocate overlay plane pool
        mOverlayPlanes = (IntelDisplayPlane**)calloc(mOverlayPlaneCount,
                                                sizeof(IntelDisplayPlane*));
        if (!mOverlayPlanes) {
            ALOGE("%s: failed to allocate overlay plane pool\n", __func__);
            goto sprite_init_err;
        }

        for (i = 0; i < mOverlayPlaneCount; i++) {
            mOverlayPlanes[i] =
                new IntelOverlayPlane(mDrmFd, i, mGrallocBufferManager);
            if (!mOverlayPlanes[i]) {
                ALOGE("%s: failed to allocate overlay plane %d\n", __func__, i);
                goto overlay_alloc_err;
            }
            // reset overlay plane
            mOverlayPlanes[i]->reset();
        }

        // allocate RGB overlay plane pool
        mRGBOverlayPlanes = (IntelDisplayPlane**)calloc(mOverlayPlaneCount,
                                                    sizeof(IntelDisplayPlane*));
        if (!mRGBOverlayPlanes) {
            LOGE("%s: failed to allocate RGB overlay plane pool\n", __func__);
            goto overlay_alloc_err;
        }

        for (i = 0; i < mOverlayPlaneCount; i++) {
            mRGBOverlayPlanes[i] =
                new IntelRGBOverlayPlane(mDrmFd, i, mGrallocBufferManager);
            if (!mRGBOverlayPlanes[i]) {
                LOGE("%s: failed to allocate RGB overlay plane %d\n",
                     __func__, i);
                goto rgb_alloc_err;
            }
            mRGBOverlayPlanes[i]->reset();
        }
    }

    // allocate zorder configs
    mZOrderConfigs = (int *)calloc(mPrimaryPlaneCount, sizeof(int));
    if (!mZOrderConfigs) {
        ALOGE("%s: failed to allocated ZOrderConfigs\n", __func__);
        goto rgb_alloc_err;
    }

    // allocate Widi plane
    mWidiPlane = new IntelWidiPlane(mDrmFd,mOverlayPlaneCount, mGrallocBufferManager);
    if (!mWidiPlane) {
        ALOGE("%s: failed to allocate widi plane %d\n", __func__, i);
        goto zorder_config_err;
    }
#ifdef INTEL_WIDI
    // Instantiate frame server and add it to service manager
    ret = defaultServiceManager()->addService(String16("hwc.widi"), new FrameServer((IntelWidiPlane*) mWidiPlane));
    if(ret != NO_ERROR) {
        ALOGE("%s: Could not register hwc.widi with service manager, error = %d", __func__, ret);
    }
    ProcessState::self()->startThreadPool();
#endif

    mInitialized = true;
    return;

zorder_config_err:
    free(mZOrderConfigs);
rgb_alloc_err:
    if (mRGBOverlayPlanes) {
        for (int i = 0; i < mOverlayPlaneCount; i++) {
            if (mRGBOverlayPlanes[i]) {
                mRGBOverlayPlanes[i]->reset();
                delete mRGBOverlayPlanes[i];
            }
        }
        free(mRGBOverlayPlanes);
        mRGBOverlayPlanes = 0;
    }
overlay_alloc_err:
    if (mOverlayPlanes) {
        for (int i = 0; i < mOverlayPlaneCount; i++) {
            if (mOverlayPlanes[i]) {
                mOverlayPlanes[i]->reset();
                delete mOverlayPlanes[i];
            }
        }
        free(mOverlayPlanes);
        mOverlayPlanes = 0;
    }
sprite_init_err:
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
primary_init_err:
    if (mPrimaryPlanes) {
        for (int i = 0; i < mPrimaryPlaneCount; i++) {
            if (mPrimaryPlanes[i]) {
                mPrimaryPlanes[i]->reset();
                delete mPrimaryPlanes[i];
            }
        }
        free(mPrimaryPlanes);
        mPrimaryPlanes = 0;
    }
primary_alloc_err:
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

    // delete primary planes
    if (mPrimaryPlanes) {
        for (int i = 0; i < mPrimaryPlaneCount; i++) {
            if (mPrimaryPlanes[i]) {
                mPrimaryPlanes[i]->reset();
                delete mPrimaryPlanes[i];
            }
        }
        free(mPrimaryPlanes);
        mPrimaryPlanes = 0;
    }

    // delete RGB overlay planes
    if (mRGBOverlayPlanes) {
        for (int i = 0; i < mOverlayPlaneCount; i++) {
            if (mRGBOverlayPlanes[i]) {
                mRGBOverlayPlanes[i]->reset();
                delete mRGBOverlayPlanes[i];
            }
        }
        free(mRGBOverlayPlanes);
        mRGBOverlayPlanes = 0;
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
        mOverlayPlanes = 0;
    }

    // delete zorder configs
    if (mZOrderConfigs)
        free(mZOrderConfigs);

    if (mWidiPlane)
        delete mWidiPlane;

    mInitialized = false;
}

void IntelDisplayPlaneManager::detect()
{
    mSpritePlaneCount = 0;
    mPrimaryPlaneCount = INTEL_SPRITE_PLANE_NUM;
    mOverlayPlaneCount = INTEL_OVERLAY_PLANE_NUM;
    mTotalPlaneCount = mSpritePlaneCount + mPrimaryPlaneCount + mOverlayPlaneCount;
    mMaxPlaneCount = mSpritePlaneCount + mOverlayPlaneCount + 1;
    // Platform dependent, no sprite plane on Medfield
    mFreeSpritePlanes = 0x0;
    // plane A, B & C
    mFreePrimaryPlanes = 0x7;
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
        ALOGW("%s: bit %d was set\n", __func__, index);
        return;
    }

    mask |= bit;
}

int IntelDisplayPlaneManager::getPlane(uint32_t& mask, int index)
{
    if (!mask || index < 0 || index > mTotalPlaneCount)
        return -1;

    int bit = (1 << index);
    if (bit & mask) {
        mask &= ~bit;
        return index;
    }

    return -1;
}

IntelDisplayPlane* IntelDisplayPlaneManager::getSpritePlane()
{
    if (!initCheck()) {
        ALOGE("%s: plane manager was not initialized\n", __func__);
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
    ALOGE("%s: failed to get a sprite plane\n", __func__);
    return 0;
}

IntelDisplayPlane* IntelDisplayPlaneManager::getPrimaryPlane(int pipe)
{
    if (!initCheck()) {
        ALOGE("%s: plane manager was not initialized\n", __func__);
        return 0;
    }

    int freePlaneIndex;

    // check reclaimed primary planes
    freePlaneIndex = getPlane(mReclaimedPrimaryPlanes, pipe);
    if (freePlaneIndex >= 0)
        return mPrimaryPlanes[freePlaneIndex];

    // check free overlay planes
    freePlaneIndex = getPlane(mFreePrimaryPlanes, pipe);
    if (freePlaneIndex >= 0)
        return mPrimaryPlanes[freePlaneIndex];
    ALOGE("%s: failed to get a primary plane\n", __func__);
    return 0;
}

IntelDisplayPlane* IntelDisplayPlaneManager::getOverlayPlane()
{
    if (!initCheck()) {
        ALOGE("%s: plane manager was not initialized\n", __func__);
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
       ALOGE("%s: failed to get a overlay plane\n", __func__);
       return 0;
    }

    if (isWidiActive()) {
       ((IntelOverlayPlane*)mOverlayPlanes[freePlaneIndex])->setWidiPlane(mWidiPlane);
    }

    return mOverlayPlanes[freePlaneIndex];
}

IntelDisplayPlane* IntelDisplayPlaneManager::getRGBOverlayPlane()
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
       LOGE("%s: failed to get a RGB overlay plane\n", __func__);
       return 0;
    }

    return mRGBOverlayPlanes[freePlaneIndex];
}

bool IntelDisplayPlaneManager::hasFreeSprites()
{
    if (!initCheck())
        return false;

    return (mFreeSpritePlanes || mReclaimedSpritePlanes) ? true : false;
}

bool IntelDisplayPlaneManager::hasFreeOverlays()
{
    if (!initCheck())
        return false;

    return (mFreeOverlayPlanes || mReclaimedOverlayPlanes) ? true : false;
}

bool IntelDisplayPlaneManager::hasReclaimedOverlays()
{
    if (!initCheck())
        return false;

    return (mReclaimedOverlayPlanes) ? true : false;
}

bool IntelDisplayPlaneManager::hasFreeRGBOverlays()
{
	return hasFreeOverlays();
}

bool IntelDisplayPlaneManager::primaryAvailable(int pipe)
{
    if (!initCheck())
        return false;

    return ((mFreePrimaryPlanes & (1 << pipe)) ||
            (mReclaimedPrimaryPlanes & (1 << pipe))) ? true : false;
}

void IntelDisplayPlaneManager::reclaimPlane(IntelDisplayPlane *plane)
{
    if (!plane)
        return;

    if (!initCheck()) {
        ALOGE("%s: plane manager is not initialized\n", __func__);
        return;
    }

    int index = plane->mIndex;

    ALOGD_IF(ALLOW_PLANE_PRINT, "%s: reclaimPlane %d\n", __func__, index);

    if (plane->mType == IntelDisplayPlane::DISPLAY_PLANE_OVERLAY ||
        plane->mType == IntelDisplayPlane::DISPLAY_PLANE_RGB_OVERLAY)
        putPlane(index, mReclaimedOverlayPlanes);
    else if (plane->mType == IntelDisplayPlane::DISPLAY_PLANE_SPRITE)
        putPlane(index, mReclaimedSpritePlanes);
    else if (plane->mType == IntelDisplayPlane::DISPLAY_PLANE_PRIMARY)
        putPlane(index, mReclaimedPrimaryPlanes);
    else
        ALOGE("%s: invalid plane type %d\n", __func__, plane->mType);
}

void IntelDisplayPlaneManager::disableReclaimedPlanes(int type)
{
    if (!initCheck()) {
        ALOGE("%s: plane manager is not initialized\n", __func__);
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
                    mSpritePlanes[i]->invalidateDataBuffer();
                }
            }
        }
        // merge into free sprite bitmap
        mFreeSpritePlanes |= mReclaimedSpritePlanes;
        mReclaimedSpritePlanes = 0;
    }

    // disable reclaimed primary planes
    if (type == IntelDisplayPlane::DISPLAY_PLANE_PRIMARY &&
        mPrimaryPlanes && mReclaimedPrimaryPlanes) {
        for (int i = 0; i < mPrimaryPlaneCount; i++) {
            int bit = (1 << i);
            if (mReclaimedPrimaryPlanes & bit) {
                if (mPrimaryPlanes[i]) {
                    // disable plane
                    mPrimaryPlanes[i]->disable();
                    // invalidate plane's data buffer
                    mPrimaryPlanes[i]->invalidateDataBuffer();
                }
            }
        }
        // merge into free sprite bitmap
        mFreePrimaryPlanes |= mReclaimedPrimaryPlanes;
        mReclaimedPrimaryPlanes = 0;
    }

    // disable reclaimed overlay planes
    if ((type == IntelDisplayPlane::DISPLAY_PLANE_OVERLAY ||
         type == IntelDisplayPlane::DISPLAY_PLANE_RGB_OVERLAY) &&
        mOverlayPlanes && mRGBOverlayPlanes && mReclaimedOverlayPlanes) {
        for (int i = 0; i < mOverlayPlaneCount; i++) {
            int bit = (1 << i);
            if (mReclaimedOverlayPlanes & bit) {
                if (mOverlayPlanes[i]) {
                    mOverlayPlanes[i]->disable();
                    mOverlayPlanes[i]->waitForFlipCompletion();

                    mOverlayPlanes[i]->invalidateDataBuffer();
                }
            }

            if (mRGBOverlayPlanes[i]) {
                mRGBOverlayPlanes[i]->disable();
                mRGBOverlayPlanes[i]->invalidateDataBuffer();
            }
        }
        // merge into free overlay bitmap
        mFreeOverlayPlanes |= mReclaimedOverlayPlanes;
        mReclaimedOverlayPlanes = 0;
    }
}

void IntelDisplayPlaneManager::resetPlaneContexts()
{
    memset(mPlaneContexts, 0, mContextLength);
}

void* IntelDisplayPlaneManager::getPlaneContexts() const
{
    return mPlaneContexts;
}

int IntelDisplayPlaneManager::getContextLength() const
{
    return mContextLength;
}

int IntelDisplayPlaneManager::setZOrderConfig(int config, int pipe)
{
    ALOGD_IF(ALLOW_PLANE_PRINT, "%s: %d", __func__, config);

    if (!initCheck()) {
        ALOGE("%s: plane manager is not initialized\n", __func__);
        return -1;
    }

    if (pipe > 2 || pipe < 0)
        return -1;

    if (mZOrderConfigs[pipe] == config)
        return -1;

    switch (config) {
    case ZORDER_POcOa:
        mPrimaryPlanes[pipe]->forceBottom(false);
        mOverlayPlanes[0]->forceBottom(true);
        break;
    case ZORDER_OaOcP:
        mPrimaryPlanes[pipe]->forceBottom(true);
        mOverlayPlanes[0]->forceBottom(false);
        break;
    case ZORDER_OcOaP:
        mPrimaryPlanes[pipe]->forceBottom(true);
        mOverlayPlanes[0]->forceBottom(true);
        break;
    case ZORDER_POaOc:
    default:
        config = ZORDER_POaOc;
        mPrimaryPlanes[pipe]->forceBottom(false);
        mOverlayPlanes[0]->forceBottom(false);
    }

    ALOGD("%s: set zorder: %d\n", __func__, config);
    mZOrderConfigs[pipe] = config;
    return 0;
}

int IntelDisplayPlaneManager::getZOrderConfig(int pipe)
{
    if (!initCheck()) {
        ALOGE("%s: plane manager is not initialized\n", __func__);
        return ZORDER_POaOc;
    }

    if (pipe > 2 || pipe < 0)
        return ZORDER_POaOc;

    return mZOrderConfigs[pipe];
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

    dumpPrintf("-------------- Plane Infos ---------------\n");
    dumpPrintf("     sprite plane count: %d\n", mSpritePlaneCount);
    dumpPrintf("     primary plane count: %d\n", mPrimaryPlaneCount);
    dumpPrintf("     overlay plane count: %d\n", mOverlayPlaneCount);
    dumpPrintf("     free sprite plane : 0x%x\n", mFreeSpritePlanes);
    dumpPrintf("     free primary plane : 0x%x\n", mFreePrimaryPlanes);
    dumpPrintf("     free overlay count : 0x%x\n", mFreeOverlayPlanes);
    dumpPrintf("     plane zOrder: %d\n", mZOrderConfigs[0]);
    dumpPrintf("-------------End of Plane Infos-----------\n");

    *cur_len = mDumpLen;

    return ret;
}


bool
 IntelDisplayPlaneManager::isWidiStatusChanged() {


     if(mWidiPlane)
         return ((IntelWidiPlane*)mWidiPlane)->isWidiStatusChanged();

     return false;
 }

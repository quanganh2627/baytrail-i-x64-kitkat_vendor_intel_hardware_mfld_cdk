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

#include <IntelHWComposerLayer.h>

IntelHWComposerLayer::IntelHWComposerLayer()
    : mHWCLayer(0), mPlane(0), mFlags(0)
{

}

IntelHWComposerLayer::IntelHWComposerLayer(hwc_layer_t *layer,
                                           IntelDisplayPlane *plane,
                                           int flags)
    : mHWCLayer(layer), mPlane(plane), mFlags(flags), mForceOverlay(false),
      mLayerType(0), mFormat(0), mIsProtected(false)
{

}

IntelHWComposerLayer::~IntelHWComposerLayer()
{

}

IntelHWComposerLayerList::IntelHWComposerLayerList(IntelDisplayPlaneManager *pm)
    : mLayerList(0),
      mPlaneManager(pm),
      mNumLayers(0),
      mNumRGBLayers(0),
      mNumYUVLayers(0),
      mAttachedSpritePlanes(0),
      mAttachedOverlayPlanes(0),
      mNumAttachedPlanes(0)
{
    if (!mPlaneManager)
        mInitialized = false;
    else
        mInitialized = true;
}

IntelHWComposerLayerList::~IntelHWComposerLayerList()
{
    if (!initCheck())
        return;

    // delete list
    mPlaneManager = 0;
    delete[] mLayerList;
    mNumLayers = 0;
    mNumRGBLayers = 0;
    mNumYUVLayers= 0;
    mAttachedSpritePlanes = 0;
    mAttachedOverlayPlanes = 0;
    mInitialized = false;
}

void IntelHWComposerLayerList::updateLayerList(hwc_layer_list_t *layerList)
{
    int numLayers = layerList->numHwLayers;
    int numRGBLayers = 0;
    int numYUVLayers = 0;

    if (numLayers <= 0 || !initCheck())
        return;

    if (mNumLayers < numLayers) {
        delete [] mLayerList;
        mLayerList = new IntelHWComposerLayer[numLayers];
        if (!mLayerList) {
            LOGE("%s: failed to create layer list\n", __func__);
            return;
        }
    }

    for (int i = 0; i < numLayers; i++) {
        mLayerList[i].mHWCLayer = &layerList->hwLayers[i];
        mLayerList[i].mPlane = 0;
        mLayerList[i].mFlags = 0;
        mLayerList[i].mForceOverlay = false;
        mLayerList[i].mNeedClearup = false;
        mLayerList[i].mLayerType = IntelHWComposerLayer::LAYER_TYPE_INVALID;
        mLayerList[i].mFormat = 0;
        mLayerList[i].mIsProtected = false;

        // update layer format
        intel_gralloc_buffer_handle_t *grallocHandle =
            (intel_gralloc_buffer_handle_t*)layerList->hwLayers[i].handle;

        if (!grallocHandle)
            continue;

        mLayerList[i].mFormat = grallocHandle->format;

        if (grallocHandle->format == HAL_PIXEL_FORMAT_YV12 ||
            grallocHandle->format == HAL_PIXEL_FORMAT_INTEL_HWC_NV12 ||
            grallocHandle->format == HAL_PIXEL_FORMAT_INTEL_HWC_YUY2 ||
            grallocHandle->format == HAL_PIXEL_FORMAT_INTEL_HWC_UYVY ||
            grallocHandle->format == HAL_PIXEL_FORMAT_INTEL_HWC_I420) {
            mLayerList[i].mLayerType = IntelHWComposerLayer::LAYER_TYPE_YUV;
            numYUVLayers++;
        } else if (grallocHandle->format == HAL_PIXEL_FORMAT_RGB_565 ||
            grallocHandle->format == HAL_PIXEL_FORMAT_BGRA_8888 ||
            grallocHandle->format == HAL_PIXEL_FORMAT_BGRX_8888 ||
            grallocHandle->format == HAL_PIXEL_FORMAT_RGBX_8888 ||
            grallocHandle->format == HAL_PIXEL_FORMAT_RGBA_8888) {
            mLayerList[i].mLayerType = IntelHWComposerLayer::LAYER_TYPE_RGB;
            numRGBLayers++;
        } else
            LOGW("updateLayerList: unknown format 0x%x", grallocHandle->format);

        // check if a protected layer
        if (grallocHandle->usage & GRALLOC_USAGE_PROTECTED)
            mLayerList[i].mIsProtected = true;
    }

    mNumLayers = numLayers;
    mNumRGBLayers = numRGBLayers;
    mNumYUVLayers = numYUVLayers;
    mNumAttachedPlanes = 0;
}

bool IntelHWComposerLayerList::invalidatePlanes()
{
    if (!initCheck())
        return false;

    for (int i = 0; i < mNumLayers; i++) {
        if (mLayerList[i].mPlane) {
            mPlaneManager->reclaimPlane(mLayerList[i].mPlane);
            mLayerList[i].mPlane = 0;
        }
    }

    mAttachedSpritePlanes = 0;
    mAttachedOverlayPlanes = 0;
    mNumAttachedPlanes = 0;
    return true;
}

void IntelHWComposerLayerList::attachPlane(int index,
                                           IntelDisplayPlane *plane,
                                           int flags)
{
    if (index < 0 || index >= mNumLayers || !plane) {
        LOGE("%s: Invalid parameters\n", __func__);
        return;
    }

    if (initCheck()) {
        mLayerList[index].mPlane = plane;
        mLayerList[index].mFlags = flags;
        if (plane->getPlaneType() == IntelDisplayPlane::DISPLAY_PLANE_SPRITE)
            mAttachedSpritePlanes++;
        else if (plane->getPlaneType() == IntelDisplayPlane::DISPLAY_PLANE_OVERLAY)
            mAttachedOverlayPlanes++;
        mNumAttachedPlanes++;
    }
}

void IntelHWComposerLayerList::detachPlane(int index, IntelDisplayPlane *plane)
{
    if (index < 0 || index >= mNumLayers || !plane) {
        LOGE("%s: Invalid parameters\n", __func__);
        return;
    }

    if (initCheck()) {
        mPlaneManager->reclaimPlane(plane);
        mLayerList[index].mPlane = 0;
        mLayerList[index].mFlags = 0;
        if (plane->getPlaneType() == IntelDisplayPlane::DISPLAY_PLANE_SPRITE)
            mAttachedSpritePlanes--;
        else if (plane->getPlaneType() == IntelDisplayPlane::DISPLAY_PLANE_OVERLAY)
            mAttachedOverlayPlanes--;
        mNumAttachedPlanes--;
    }
}

IntelDisplayPlane* IntelHWComposerLayerList::getPlane(int index)
{
    if (index < 0 || index >= mNumLayers) {
        LOGE("%s: Invalid parameters\n", __func__);
        return 0;
    }

    if (initCheck())
        return mLayerList[index].mPlane;

    return 0;
}

void IntelHWComposerLayerList::setFlags(int index, int flags)
{
    if (index < 0 || index >= mNumLayers) {
        LOGE("%s: Invalid parameters\n", __func__);
        return;
    }

    if (initCheck())
        mLayerList[index].mFlags = flags;
}

int IntelHWComposerLayerList::getFlags(int index)
{
    if (index < 0 || index >= mNumLayers) {
        LOGE("%s: Invalid parameters\n", __func__);
        return 0;
    }

    if (initCheck())
        return mLayerList[index].mFlags;

    return 0;
}

void IntelHWComposerLayerList::setForceOverlay(int index, bool isForceOverlay)
{
    if (index < 0 || index >= mNumLayers) {
        LOGE("%s: Invalid parameters\n", __func__);
        return;
    }

    if (initCheck())
        mLayerList[index].mForceOverlay = isForceOverlay;
}

bool IntelHWComposerLayerList::getForceOverlay(int index)
{
    if (index < 0 || index >= mNumLayers) {
        LOGE("%s: Invalid parameters\n", __func__);
        return false;
    }

    if (initCheck())
        return mLayerList[index].mForceOverlay;

    return false;
}

void IntelHWComposerLayerList::setNeedClearup(int index, bool needClearup)
{
    if (index < 0 || index >= mNumLayers) {
        LOGE("%s: Invalid parameters\n", __func__);
        return;
    }

    if (initCheck())
        mLayerList[index].mNeedClearup = needClearup;
}

bool IntelHWComposerLayerList::getNeedClearup(int index)
{
    if (index < 0 || index >= mNumLayers) {
        LOGE("%s: Invalid parameters\n", __func__);
        return false;
    }

    if (initCheck())
        return mLayerList[index].mNeedClearup;

    return false;
}

int IntelHWComposerLayerList::getLayerType(int index) const
{
    if (!initCheck() || index < 0 || index >= mNumLayers) {
        LOGE("%s: Invalid parameters\n", __func__);
        return IntelHWComposerLayer::LAYER_TYPE_INVALID;
    }

    return mLayerList[index].mLayerType;
}

int IntelHWComposerLayerList::getLayerFormat(int index) const
{
    if (!initCheck() || index < 0 || index >= mNumLayers) {
        LOGE("%s: Invalid parameters\n", __func__);
        return 0;
    }

    return mLayerList[index].mFormat;
}

bool IntelHWComposerLayerList::isProtectedLayer(int index) const
{
    if (!initCheck() || index < 0 || index >= mNumLayers) {
        LOGE("%s: Invalid parameters\n", __func__);
        return false;
    }

    return mLayerList[index].mIsProtected;
}

int IntelHWComposerLayerList::getRGBLayerCount() const
{
    if (!initCheck()) {
        LOGE("%s: Invalid parameters\n", __func__);
        return 0;
    }

    return mNumRGBLayers;
}

int IntelHWComposerLayerList::getYUVLayerCount() const
{
    if (!initCheck()) {
        LOGE("%s: Invalid parameters\n", __func__);
        return 0;
    }

    return mNumYUVLayers;
}

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
    : mHWCLayer(layer), mPlane(plane), mFlags(flags)
{

}

IntelHWComposerLayer::~IntelHWComposerLayer()
{

}

IntelHWComposerLayerList::IntelHWComposerLayerList(IntelDisplayPlaneManager *pm)
    : mLayerList(0), mPlaneManager(pm), mNumLayers(0),
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
    mInitialized = false;
}

void IntelHWComposerLayerList::updateLayerList(hwc_layer_list_t *layerList)
{
    int numLayers = layerList->numHwLayers;

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
    }

    mNumLayers = numLayers;
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

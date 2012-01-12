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

#include <cutils/ashmem.h>
#include <sys/mman.h>
#include <math.h>

#include <IntelHWComposerDrm.h>
#include <IntelWidiPlane.h>
#include <IntelOverlayUtil.h>


IntelWidiPlane::IntelWidiPlane(int fd, int index, IntelBufferManager *bm)
    : IntelDisplayPlane(fd, IntelDisplayPlane::DISPLAY_PLANE_OVERLAY, index, bm)
{
    bool ret;
    LOGV("%s\n", __func__);

    // initialized successfully
    mDataBuffer = NULL;
    mContext = NULL;
    mInitialized = true;
    return;

}

IntelWidiPlane::~IntelWidiPlane()
{
    if (initCheck()) {
        mInitialized = false;
    }
}

void IntelWidiPlane::setPosition(int left, int top, int right, int bottom)
{
    if (initCheck()) {
    }
}






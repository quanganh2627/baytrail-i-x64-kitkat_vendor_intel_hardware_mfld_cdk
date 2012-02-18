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

/*
 * INTEL CONFIDENTIAL
 * Copyright 2010-2011 Intel Corporation All Rights Reserved.

 * The source code, information and material ("Material") contained herein is owned
 * by Intel Corporation or its suppliers or licensors, and title to such Material
 * remains with Intel Corporation or its suppliers or licensors. The Material contains
 * proprietary information of Intel or its suppliers and licensors. The Material is
 * protected by worldwide copyright laws and treaty provisions. No part of the Material
 * may be used, copied, reproduced, modified, published, uploaded, posted, transmitted,
 * distributed or disclosed in any way without Intel's prior express written permission.
 * No license under any patent, copyright or other intellectual property rights in the
 * Material is granted to or conferred upon you, either expressly, by implication, inducement,
 * estoppel or otherwise. Any license under such intellectual property rights must be express
 * and approved by Intel in writing.

 * Unless otherwise agreed by Intel in writing, you may not remove or alter this notice or any
 * other notice embedded in Materials by Intel or Intel's suppliers or licensors in any way.
 */

#ifndef INTEL_HWWIDIPLANE_H
#define INTEL_HWWIDIPLANE_H

#include <utils/Errors.h>  // for status_t
#include <utils/KeyedVector.h>
#include <utils/RefBase.h>
#include <utils/String8.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include "IPageFlipListener.h"

namespace intel {
namespace widi {

class IWirelessDisplay;

// This is the interface for Widi HW Plane  service. This interface is used for
// Enabling the pseudo  HW-Composer plane in the HWC HAL Module

class IHwWidiPlane: public android::IInterface
{
public:
    DECLARE_META_INTERFACE(HwWidiPlane);

    virtual android::status_t  enablePlane(android::sp<android::IBinder> display) = 0;
    virtual void disablePlane() = 0;
    virtual void allowExtVideoMode(bool allow) = 0;
    virtual void setPlayerStatus(bool status) = 0;
    virtual android::status_t  registerFlipListener(android::sp<IPageFlipListener> listener) = 0;
};

// ----------------------------------------------------------------------------

class BnHwWidiPlane: public android::BnInterface<IHwWidiPlane>
{
public:
    virtual android::status_t    onTransact( uint32_t code,
                                    const android::Parcel& data,
                                    android::Parcel* reply,
                                    uint32_t flags = 0);
};

}; // namespace widi
}; // namespace intel

#endif // INTEL_HWWIDIPLANE_H

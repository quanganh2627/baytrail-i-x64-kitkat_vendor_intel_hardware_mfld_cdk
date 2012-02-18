/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
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

#include <stdint.h>
#include <sys/types.h>

#include <binder/Parcel.h>
#include <binder/IMemory.h>
#include <utils/Errors.h>  // for status_t

#include "IHwWidiPlane.h"

namespace intel {
namespace widi {

using namespace android;

enum {
    ENABLE_HW_WIDI_PLANE = IBinder::FIRST_CALL_TRANSACTION,
    DISABLE_HW_WIDI_PLANE,
    REGISTER_FLIP_LISTENER,
    ALLOW_EXT_VIDEO_MODE,
    SET_PLAYER_STATUS

};

class BpHwWidiPlane: public BpInterface<IHwWidiPlane>
{
public:
    BpHwWidiPlane(const sp<IBinder>& impl)
        : BpInterface<IHwWidiPlane>(impl)
    {
    }

    virtual status_t  enablePlane(sp<IBinder> widiClass)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IHwWidiPlane::getInterfaceDescriptor());
        data.writeStrongBinder(widiClass);
        remote()->transact(ENABLE_HW_WIDI_PLANE, data, &reply);
        return reply.readInt32();
    }
    virtual void  disablePlane()
    {
        Parcel data, reply;
        data.writeInterfaceToken(IHwWidiPlane::getInterfaceDescriptor());
        remote()->transact(DISABLE_HW_WIDI_PLANE, data, &reply);
        return;
    }
    virtual status_t  registerFlipListener(sp<IPageFlipListener> listener)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IHwWidiPlane::getInterfaceDescriptor());
        data.writeStrongBinder(listener->asBinder());
        remote()->transact(REGISTER_FLIP_LISTENER, data, &reply);
        return reply.readInt32();
    }
    virtual void allowExtVideoMode(bool allow) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwWidiPlane::getInterfaceDescriptor());
        data.writeInt32(((int32_t) allow));
        remote()->transact(ALLOW_EXT_VIDEO_MODE, data, &reply);
        return;
    }
    virtual void setPlayerStatus(bool status) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwWidiPlane::getInterfaceDescriptor());
        data.writeInt32(((int32_t) status));
        remote()->transact(SET_PLAYER_STATUS, data, &reply);
        return;
    }
};

IMPLEMENT_META_INTERFACE(HwWidiPlane, "android.widi.IHwWidiPlane");

// ----------------------------------------------------------------------

status_t BnHwWidiPlane::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case ENABLE_HW_WIDI_PLANE: {
            CHECK_INTERFACE(IHwWidiPlane, data, reply);
            sp<IBinder> widiClass = data.readStrongBinder();
            enablePlane(widiClass);
            reply->writeInt32(NO_ERROR);
            return NO_ERROR;
        } break;
        case DISABLE_HW_WIDI_PLANE: {
            CHECK_INTERFACE(IHwWidiPlane, data, reply);
            disablePlane();
            reply->writeInt32(NO_ERROR);
            return NO_ERROR;
        } break;
        case REGISTER_FLIP_LISTENER: {
            CHECK_INTERFACE(IHwWidiPlane, data, reply);
            sp<IBinder> listener = data.readStrongBinder();
            registerFlipListener(interface_cast<IPageFlipListener>(listener));
            reply->writeInt32(NO_ERROR);
            return NO_ERROR;
        } break;
        case ALLOW_EXT_VIDEO_MODE: {
            CHECK_INTERFACE(IHwWidiPlane, data, reply);
            int32_t allow = data.readInt32();
            allowExtVideoMode(allow);
            reply->writeInt32(NO_ERROR);
            return NO_ERROR;
        } break;
        case SET_PLAYER_STATUS: {
            CHECK_INTERFACE(IHwWidiPlane, data, reply);
            int32_t status = data.readInt32();
            setPlayerStatus(status);
            reply->writeInt32(NO_ERROR);
            return NO_ERROR;
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

// ----------------------------------------------------------------------------

}; // namespace widi
}; // namespace intel

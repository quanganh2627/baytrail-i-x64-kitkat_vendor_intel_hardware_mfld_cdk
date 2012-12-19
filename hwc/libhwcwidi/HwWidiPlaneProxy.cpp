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
    SET_BACKGROUND_VIDEO_MODE,
    RETURN_BUFFER
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
    virtual void  disablePlane(bool isConnected)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IHwWidiPlane::getInterfaceDescriptor());
        data.writeInt32((int32_t)(isConnected ? 1 : 0));
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
    virtual void setBackgroundVideoMode(bool value) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwWidiPlane::getInterfaceDescriptor());
        data.writeInt32(((int32_t) value));
        remote()->transact(SET_BACKGROUND_VIDEO_MODE, data, &reply);
        return;
    }
    virtual void returnBuffer(int index) {
        Parcel data, reply;
        data.writeInterfaceToken(IHwWidiPlane::getInterfaceDescriptor());
        data.writeInt32(((int32_t) index));
        remote()->transact(RETURN_BUFFER, data, &reply, IBinder::FLAG_ONEWAY);
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
            bool isConnected =false;
            CHECK_INTERFACE(IHwWidiPlane, data, reply);
            isConnected = data.readInt32() == 1;
            disablePlane(isConnected);
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
        case SET_BACKGROUND_VIDEO_MODE: {
            CHECK_INTERFACE(IHwWidiPlane, data, reply);
            int32_t value = data.readInt32();
            setBackgroundVideoMode(value);
            return NO_ERROR;
        } break;
        case RETURN_BUFFER: {
            CHECK_INTERFACE(IHwWidiPlane, data, reply);
            int32_t index = data.readInt32();
            returnBuffer(index);
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

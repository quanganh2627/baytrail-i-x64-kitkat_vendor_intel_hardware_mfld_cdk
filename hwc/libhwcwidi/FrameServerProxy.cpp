/*
 * Copyright (c) 2013, Intel Corporation. All rights reserved.
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

//#define LOG_NDEBUG 0
#define LOG_TAG "HWC-Widi-FrameServerProxy"

#include <stdint.h>
#include <sys/types.h>

#include <binder/Parcel.h>
#include <binder/IMemory.h>
#include <utils/Errors.h>  // for status_t
#include <binder/IPCThreadState.h>

#include "IFrameServer.h"

#define AID_MEDIA 1013 // for permission check

using namespace android;

enum {
    START = IBinder::FIRST_CALL_TRANSACTION,
    STOP,
    NOTIFY_BUFFER_RETURNED,
};

class BpFrameServer: public BpInterface<IFrameServer>
{
public:
    BpFrameServer(const sp<IBinder>& impl)
        : BpInterface<IFrameServer>(impl)
    {
    }

    virtual status_t start(sp<IFrameTypeChangeListener> frameTypeChangeListener)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IFrameServer::getInterfaceDescriptor());
        data.writeStrongBinder(frameTypeChangeListener->asBinder());
        status_t ret = remote()->transact(START, data, &reply);
        if(ret != NO_ERROR) {
            return ret;
        }
        return reply.readInt32();
    }
    virtual status_t stop(bool isConnected)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IFrameServer::getInterfaceDescriptor());
        data.writeInt32((int32_t)(isConnected ? 1 : 0));
        status_t ret = remote()->transact(STOP, data, &reply);
        if(ret != NO_ERROR) {
            return ret;
        }
        return reply.readInt32();
    }
    virtual status_t notifyBufferReturned(int index) {
        Parcel data, reply;
        data.writeInterfaceToken(IFrameServer::getInterfaceDescriptor());
        data.writeInt32(((int32_t) index));
        status_t ret = remote()->transact(NOTIFY_BUFFER_RETURNED, data, &reply, IBinder::FLAG_ONEWAY);
        if(ret != NO_ERROR) {
            return ret;
        }
        return reply.readInt32();
    }
};

IMPLEMENT_META_INTERFACE(FrameServer, "android.widi.IFrameServer");

// ----------------------------------------------------------------------

// Return true if the calling process is mediaserver, false if not
bool hasPermission() {
    const int uid = IPCThreadState::self()->getCallingUid();
    if(uid != AID_MEDIA) {
        ALOGE("%s: Permission denied! Calling process uid=%d is not authorized", __func__, uid);
        return false;
    }
    return true;
}

status_t BnFrameServer::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case START: {
            if(!hasPermission()) {
                return PERMISSION_DENIED;
            }
            CHECK_INTERFACE(IFrameServer, data, reply);
            sp<IFrameTypeChangeListener> frameTypeChangeListener = interface_cast<IFrameTypeChangeListener>(data.readStrongBinder());
            status_t ret = start(frameTypeChangeListener);
            reply->writeInt32(ret);
            return NO_ERROR;
        } break;
        case STOP: {
            if(!hasPermission()) {
                return PERMISSION_DENIED;
            }
            bool isConnected = false;
            CHECK_INTERFACE(IFrameServer, data, reply);
            isConnected = data.readInt32() == 1;
            status_t ret = stop(isConnected);
            reply->writeInt32(ret);
            return NO_ERROR;
        } break;
        case NOTIFY_BUFFER_RETURNED: {
            if(!hasPermission()) {
                return PERMISSION_DENIED;
            }
            CHECK_INTERFACE(IFrameServer, data, reply);
            int32_t index = data.readInt32();
            status_t ret = notifyBufferReturned(index);
            reply->writeInt32(ret);
            return NO_ERROR;
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

// ----------------------------------------------------------------------------

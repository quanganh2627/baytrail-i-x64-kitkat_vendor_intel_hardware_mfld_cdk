/*
 * Copyright (c) 2011 Intel Corporation.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <binder/Parcel.h>
#include <binder/IMemory.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>

#include <IPageFlipListener.h>

using namespace android;

class BpPageFlipListener : public BpInterface<IPageFlipListener>
{
public:
    BpPageFlipListener(const sp<IBinder>& impl)
        : BpInterface<IPageFlipListener>(impl)
    {
    }

    virtual void pageFlipped(int64_t time, uint32_t orientation)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IPageFlipListener::getInterfaceDescriptor());
        data.writeInt64(time);
        data.writeInt32(orientation);
        remote()->transact(BnPageFlipListener::PAGE_FLIPPED, data, &reply);
    }
};

IMPLEMENT_META_INTERFACE(PageFlipListener, "android.ui.IPageFlipListener");

status_t BnPageFlipListener::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case PAGE_FLIPPED: {
            CHECK_INTERFACE(IPageFlipListener, data, reply);
            int64_t time = data.readInt64();
            uint32_t orientation = data.readInt32();
            pageFlipped(time, orientation);
            reply->writeNoException();
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
    return NO_ERROR;
}



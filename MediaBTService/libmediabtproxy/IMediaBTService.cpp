/*
 **
 ** Copyright 2012 Intel Corporation
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **      http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#define LOG_TAG "IMediaBTService"
//#define LOG_NDEBUG 0

#include <binder/Parcel.h>
#include <binder/IServiceManager.h>

#include "IMediaBTService.h"

namespace android {

enum {
    ENABLE_BT_PORT = IBinder::FIRST_CALL_TRANSACTION
};


//=============================================================================
//
//                        BpMediaBTService Class
//
//=============================================================================
int BpMediaBTService::enableBluetoothPort(bool bEnable)
{
    Parcel data, reply;

    data.writeInterfaceToken(IMediaBTService::getInterfaceDescriptor());
    data.writeInt32(bEnable);

    remote()->transact(ENABLE_BT_PORT, data, &reply);
    return reply.readInt32();
}

IMPLEMENT_META_INTERFACE(MediaBTService, "android.media.IMediaBTService");


//=============================================================================
//
//                        BnMediaBTService Class
//
//=============================================================================
status_t BnMediaBTService::onTransact(uint32_t code, const Parcel &data, Parcel *reply, uint32_t flags)
{
    switch(code) {
    case ENABLE_BT_PORT: {
        CHECK_INTERFACE(IMediaBTService, data, reply);
        reply->writeInt32( enableBluetoothPort(data.readInt32()) );
        return NO_ERROR;
    }break;
    default:
        return BBinder::onTransact(code, data, reply, flags);
    }
}

//=============================================================================
//
//                        BpMediaBTInterface Class
//
//=============================================================================
sp<IMediaBTService> MediaBTInterface::gMediaBTService;
sp<MediaBTInterface::MediaBTDeathNotifier> MediaBTInterface::gMediaBTDeathNotifier;
Mutex MediaBTInterface::gLock;

const sp<IMediaBTService>& MediaBTInterface::getMediaBTService()
{
    Mutex::Autolock _l(gLock);
    if (gMediaBTService.get() == 0) {
        sp<IServiceManager> sm = defaultServiceManager();
        sp<IBinder> binder;
        do {
            binder = sm->getService(android::String16("media.bt_service"));
            if (binder != 0)
                break;
            LOGW("IMediaBTService not published, waiting...");
            usleep(MediaBTInterface::BIND_SERVICE_DELAY);
        } while(true);

        if (gMediaBTDeathNotifier == NULL) {
            gMediaBTDeathNotifier = new MediaBTDeathNotifier();
        }
        binder->linkToDeath(gMediaBTDeathNotifier);
        gMediaBTService = interface_cast<IMediaBTService>(binder);
    }
    return gMediaBTService;
}

int MediaBTInterface::enableBluetoothPort(bool bEnable)
{
    const sp<IMediaBTService>& mbts = MediaBTInterface::getMediaBTService();
    if (mbts == 0) {
        LOGE("Cannot get MediaBTService");
        return -1; // Returned values are not used in ctl_vpc...
    }
    return mbts->enableBluetoothPort(bEnable);
}

void MediaBTInterface::MediaBTDeathNotifier::binderDied(const wp<IBinder>& who)
{
    Mutex::Autolock _l(MediaBTInterface::gLock);
    MediaBTInterface::gMediaBTService.clear();

    LOGW("MediaBTService server died!");
}

}; // namespace android


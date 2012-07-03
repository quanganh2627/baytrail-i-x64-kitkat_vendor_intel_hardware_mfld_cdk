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

#pragma once

#include <binder/IInterface.h>
#include <utils/threads.h>

namespace android {

//=============================================================================
//
//                        IMediaBTService Class
//
//=============================================================================
class IMediaBTService : public IInterface
{
public:
    DECLARE_META_INTERFACE(MediaBTService);

    virtual int enableBluetoothPort(bool bEnable) = 0;
};

//=============================================================================
//
//                        BpMediaBTService Class
//
//=============================================================================
class BpMediaBTService : public BpInterface<IMediaBTService>
{
public:
    BpMediaBTService(const sp<IBinder>& impl)
        :BpInterface<IMediaBTService>(impl)
    {
    }

    virtual int enableBluetoothPort(bool bEnable);
};

//=============================================================================
//
//                        BnMediaBTService Class
//
//=============================================================================
class BnMediaBTService : public BnInterface<IMediaBTService>
{
public:
    virtual status_t onTransact(uint32_t code,
                                const Parcel& data,
                                Parcel* reply,
                                uint32_t flags = 0);
};

//=============================================================================
//
//                        BpMediaBTInterface Class
//
//=============================================================================
class MediaBTInterface
{
private:
    class MediaBTDeathNotifier: public IBinder::DeathRecipient
    {
    public:
        MediaBTDeathNotifier() {}
        // DeathRecipient
        virtual void binderDied(const wp<IBinder>& who);
    };
    static sp<IMediaBTService> gMediaBTService;
    static sp<MediaBTDeathNotifier> gMediaBTDeathNotifier;
    static Mutex gLock;
    static const sp<IMediaBTService>& getMediaBTService();
    static const int BIND_SERVICE_DELAY = 500000; // 0.5s: delay between each bind try

public:
    static int enableBluetoothPort(bool bEnable);
};

}; // namespace android

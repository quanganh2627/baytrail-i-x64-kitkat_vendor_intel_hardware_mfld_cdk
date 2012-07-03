/*
 *
 *  Copyright 2012 Intel Corporation
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include <binder/BinderService.h>
#include <binder/MemoryDealer.h>

#include "libmediabtproxy/IMediaBTService.h"

namespace android {

class MediaBTService :
        public BinderService<MediaBTService>,
        public BnMediaBTService,
        public IBinder::DeathRecipient
{
private:
    friend class BinderService<MediaBTService>;
    static const int HCI_CMD_TIMEOUT_MS = 5000;
    status_t addToServiceManager();

public:
    static const int BIND_SERVICE_DELAY = 500000; // 0.5s: delay between each bind try
    MediaBTService() {}
    status_t init();
    static char const* getServiceName() { return "media.bt_service"; }
    virtual void binderDied(const wp<IBinder>& who);
    virtual int enableBluetoothPort(bool bEnable);
};

}; //namespace android

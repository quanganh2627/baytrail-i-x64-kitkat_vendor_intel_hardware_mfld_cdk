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

#ifndef ANDROID_SF_IPAGE_FLIP_LISTENER_H
#define ANDROID_SF_IPAGE_FLIP_LISTENER_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>


class IPageFlipListener : public android::IInterface
{
public:
    DECLARE_META_INTERFACE(PageFlipListener);

    virtual void pageFlipped(int64_t time, uint32_t orientation) = 0;
};

class BnPageFlipListener : public android::BnInterface<IPageFlipListener>
{
public:
    enum {
        PAGE_FLIPPED = IBinder::FIRST_CALL_TRANSACTION,
    };

    virtual android::status_t onTransact(   uint32_t code,
                                            const android::Parcel& data,
                                            android::Parcel* reply,
                                            uint32_t flags = 0);
};


#endif // ANDROID_SF_IPAGE_FLIP_LISTENER_H

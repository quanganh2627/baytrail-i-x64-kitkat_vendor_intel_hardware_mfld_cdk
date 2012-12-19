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
    virtual void disablePlane(bool isConnected) = 0;
    virtual void allowExtVideoMode(bool allow) = 0;
    virtual void setBackgroundVideoMode(bool value) = 0;
    virtual android::status_t  registerFlipListener(android::sp<IPageFlipListener> listener) = 0;
    virtual void returnBuffer(int index) = 0;
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

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
#define LOG_TAG "HWC-Widi-FrameTypeChangeListenerProxy"

#include <stdint.h>
#include <sys/types.h>

#include <binder/Parcel.h>
#include <binder/IMemory.h>
#include <utils/Errors.h>  // for status_t

#include "IFrameTypeChangeListener.h"

using namespace android;

enum {
    FRAME_TYPE_CHANGED = IBinder::FIRST_CALL_TRANSACTION,
};

class BpFrameTypeChangeListener: public BpInterface<IFrameTypeChangeListener>
{
public:
    BpFrameTypeChangeListener(const sp<IBinder>& impl)
        : BpInterface<IFrameTypeChangeListener>(impl)
    {
    }

    virtual sp<IFrameListener> frameTypeChanged(const FrameInfo& frameInfo)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IFrameTypeChangeListener::getInterfaceDescriptor());
        data.writeInt32(frameInfo.frameType);
        data.writeInt32(frameInfo.contentWidth);
        data.writeInt32(frameInfo.contentHeight);
        data.writeInt32(frameInfo.bufferWidth);
        data.writeInt32(frameInfo.bufferHeight);
        data.writeInt32(frameInfo.bufferFormat);
        data.writeInt32(frameInfo.lumaUStride);
        data.writeInt32(frameInfo.chromaUStride);
        data.writeInt32(frameInfo.chromaVStride);
        data.writeInt32(frameInfo.contentFrameRateN);
        data.writeInt32(frameInfo.contentFrameRateD);
        status_t ret = remote()->transact(FRAME_TYPE_CHANGED, data, &reply);
        if(ret == NO_ERROR) {
            return interface_cast<IFrameListener>(reply.readStrongBinder());
        }
        return NULL;
    }
};

IMPLEMENT_META_INTERFACE(FrameTypeChangeListener, "android.widi.IFrameTypeChangeListener");

// ----------------------------------------------------------------------
status_t BnFrameTypeChangeListener::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
        case FRAME_TYPE_CHANGED: {
            CHECK_INTERFACE(IFrameTypeChangeListener, data, reply);
            FrameInfo frameInfo;
            frameInfo.frameType = (HWCFrameType) data.readInt32();
            frameInfo.contentWidth = (uint32_t) data.readInt32();
            frameInfo.contentHeight = (uint32_t) data.readInt32();
            frameInfo.bufferWidth = (uint32_t) data.readInt32();
            frameInfo.bufferHeight = (uint32_t) data.readInt32();
            frameInfo.bufferFormat = (uint32_t) data.readInt32();
            frameInfo.lumaUStride = (uint16_t) data.readInt32();
            frameInfo.chromaUStride = (uint16_t) data.readInt32();
            frameInfo.chromaVStride = (uint16_t) data.readInt32();
            frameInfo.contentFrameRateN = (uint32_t) data.readInt32();
            frameInfo.contentFrameRateD = (uint32_t) data.readInt32();
            sp<IFrameListener> fl = frameTypeChanged(frameInfo);
            if(fl != NULL) {
                status_t ret = reply->writeStrongBinder(fl->asBinder());
                return NO_ERROR;
            }
            return BAD_VALUE;
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}
// ----------------------------------------------------------------------------

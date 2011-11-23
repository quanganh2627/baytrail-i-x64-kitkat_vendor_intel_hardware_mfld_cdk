/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __INTEL_OVERLAY_UTIL_H__
#define __INTEL_OVERLAY_UTIL_H__

static inline uint32_t align_to(uint32_t arg, uint32_t align)
{
    return ((arg + (align - 1)) & (~(align - 1)));
}

/* Data device parameters*/
enum {
    MRST_OVERLAY_PIPE = 0,
    MRST_OVERLAY_USAGE,
    MRST_OVERLAY_DISPLAY_BUFFER,
    MRST_OVERLAY_Y_STRIDE,
    MRST_OVERLAY_RESET,
    MRST_OVERLAY_HDMI_STATUS,
};

/*Reset operations*/
enum {
    MRST_OVERLAY_OP_RESET = 0,
    MRST_OVERLAY_OP_DISABLE,
    MRST_OVERLAY_OP_ENABLE,
};

/*HDMI status for MDFLD_OVERLAY_HDMI_STATUS parameter setting*/
enum {
    MRST_HDMI_STATUS_DISCONNECTED = 0,
    MRST_HDMI_STATUS_CONNECTED_CLONE,
    MRST_HDMI_STATUS_CONNECTED_EXTEND,
};

/*struct for MDFLD_OVERLAY_DISPLAY_BUFFER parameter setting*/
struct overlay_display_buffer_param {
    /*BCD buffer device id*/
    uint32_t device;

    /*index*/
    uint32_t handle;

    /*rotated buffer handle*/
    uint32_t kBufferHandle;
};

#endif /*__INTEL_OVERLAY_UTIL_H__*/

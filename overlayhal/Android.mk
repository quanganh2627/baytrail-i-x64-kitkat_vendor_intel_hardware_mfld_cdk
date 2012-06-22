# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


LOCAL_PATH := $(call my-dir)

ifeq ($(BOARD_USES_OVERLAY_VIDEO), true)
# HAL module implemenation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_CFLAGS:= -Wno-unused-parameter
LOCAL_C_INCLUDES := $(addprefix $(LOCAL_PATH)/../../, $(SGX_INCLUDES)) \
            $(TARGET_OUT_HEADERS)/eurasia/pvr2d \
            hardware/intel/libdrm/libdrm \
            hardware/intel/libdrm/shared-core \
            hardware/intel/libwsbm/src

LOCAL_SRC_FILES := PVROverlayModule.cpp \
            PVROverlayHAL.cpp \
            PVRWsbmWrapper.c \
            PVRWsbm.cpp \
            PVROverlayControlDevice.cpp \
            PVROverlayDataDevice.cpp \
            PVROverlay.cpp

LOCAL_MODULE := overlay.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := eng
LOCAL_SHARED_LIBRARIES := liblog libcutils libdrm libpvr2d libdrm libwsbm
include $(BUILD_SHARED_LIBRARY)
endif

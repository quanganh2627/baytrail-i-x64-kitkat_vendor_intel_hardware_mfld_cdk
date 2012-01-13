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

# HAL module implemenation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libEGL libcutils libdrm libpvr2d libwsbm libsrv_um libui
LOCAL_SRC_FILES := IntelHWComposerModule.cpp \
                   IntelHWComposer.cpp \
                   IntelHWComposerLayer.cpp \
                   IntelBufferManager.cpp \
                   IntelDisplayPlaneManager.cpp \
                   IntelHWComposerDrm.cpp \
                   IntelOverlayPlane.cpp \
                   IntelSpritePlane.cpp \
                   MedfieldSpritePlane.cpp \
                   IntelWsbm.cpp \
                   IntelWsbmWrapper.c
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE := hwcomposer.$(TARGET_DEVICE)
LOCAL_CFLAGS:= -DLOG_TAG=\"hwcomposer\" -DLINUX
LOCAL_C_INCLUDES := $(addprefix $(LOCAL_PATH)/../../, $(SGX_INCLUDES)) \
            hardware/intel/include/eurasia/pvr2d \
            hardware/intel/include/eurasia/include4 \
            hardware/intel/libdrm/libdrm \
            hardware/intel/libdrm/shared-core \
            hardware/intel/libwsbm/src \
            hardware/intel/linux-2.6/drivers/staging/mrst/drv \
            hardware/intel/linux-2.6/include/drm \
            hardware/intel/linux-2.6/drivers/staging/mrst/bc_video \
            hardware/intel/linux-2.6/drivers/staging/mrst/imgv
include $(BUILD_SHARED_LIBRARY)
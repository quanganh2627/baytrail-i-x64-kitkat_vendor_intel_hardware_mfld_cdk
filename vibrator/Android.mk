# Copyright 2012 Intel Corporation - Vincent becker

LOCAL_PATH := $(call my-dir)

# Build only when BOARD_USE_VIBRATOR_ALSA is set
ifeq ($(BOARD_USES_ALSA_AUDIO), true)

include $(CLEAR_VARS)

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_CFLAGS += -fPIC -D_POSIX_SOURCE

LOCAL_LDFLAGS:= -lstdc++

LOCAL_C_INCLUDES += \
    $(TARGET_OUT_HEADERS)/parameter \
    $(TARGET_OUT_HEADERS)/hw \
    external/stlport/stlport/ \
    bionic/libstdc++ \
    bionic/ \
    external/alsa-lib/include \
    $(TARGET_OUT_HEADERS)/event-listener \
    $(TARGET_OUT_HEADERS)/property \
    hardware/libhardware_legacy

LOCAL_SRC_FILES += \
    vibrator_instance.cpp \
    Vibrator.cpp

LOCAL_SHARED_LIBRARIES += \
        libasound \
        libutils \
        libstlport \
        libc \
        libevent-listener \
        libxmlserializer \
        libparameter \
        libproperty

TARGET_ERROR_FLAGS += -Wno-non-virtual-dtor

LOCAL_MODULE:= vibrator.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS:= optional

include $(BUILD_SHARED_LIBRARY)
endif

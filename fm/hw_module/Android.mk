LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_COPY_HEADERS_TO := hw

LOCAL_COPY_HEADERS := \
	fm_module.h

LOCAL_CFLAGS := -D_POSIX_SOURCE -Wno-multichar

LOCAL_C_INCLUDES += \
	external/alsa-lib/include \
	external/bluetooth/bluez/lib/bluetooth

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libasound \
	libbluetooth

LOCAL_SRC_FILES:= \
	fm_module.cpp

LOCAL_MODULE:= fm.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
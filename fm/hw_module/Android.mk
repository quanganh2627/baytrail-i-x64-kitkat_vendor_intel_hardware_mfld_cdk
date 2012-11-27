LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

FM_STACK_PATH := device/intel/tiwl128x/fmradio/fm_stack

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_COPY_HEADERS_TO := hw

LOCAL_COPY_HEADERS := \
	fm_module.h

ifeq ($(BUILD_FM_RADIO),true)

LOCAL_CFLAGS := -D_POSIX_SOURCE -Wno-multichar

LOCAL_C_INCLUDES += \
	external/alsa-lib/include \
	$(FM_STACK_PATH)/HSW_FMStack/stack/inc \
	$(FM_STACK_PATH)/HSW_FMStack/stack/inc/int \
	$(FM_STACK_PATH)/MCP_Common/inc \
	$(FM_STACK_PATH)/MCP_Common/Platform/fmhal/LINUX/common/inc \
	$(FM_STACK_PATH)/MCP_Common/Platform/os/LINUX/common/inc \
	$(FM_STACK_PATH)/MCP_Common/Platform/os/LINUX/android_zoom2/inc \
	$(FM_STACK_PATH)/MCP_Common/Platform/inc \
	$(FM_STACK_PATH)/MCP_Common/Platform/fmhal/inc \
	$(FM_STACK_PATH)/MCP_Common/Platform/fmhal/inc/int \
	$(FM_STACK_PATH)/MCP_Common/Platform/fmhal/LINUX/android_zoom2/inc

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libasound \
	libfmstack

LOCAL_SRC_FILES:= \
	fm_module.cpp

endif

LOCAL_MODULE:= fm.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

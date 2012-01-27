LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS := \
        -DDEBUG

LOCAL_SRC_FILES := \
        ATManager.cpp \
        ATCommand.cpp \
        PeriodicATCommand.cpp \
        UnsollicitedATCommand.cpp \
        Tokenizer.cpp \
        CallStatUnsollicitedATCommand.cpp \
        ProgressUnsollicitedATCommand.cpp

LOCAL_C_INCLUDES += \
        hardware/intel/rapid_ril/CORE \
        system/core/include/cutils \
        hardware/intel/IFX-modem

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../at-parser \
        $(LOCAL_PATH)/../tty-handler \
        $(LOCAL_PATH)/../event-listener

LOCAL_C_INCLUDES += \
        external/stlport/stlport/ \
        bionic/libstdc++ \
        bionic/

LOCAL_SHARED_LIBRARIES := libstlport libcutils libtty-handler libat-parser libevent-listener

LOCAL_MODULE := libat-manager
LOCAL_MODULE_TAGS := optional

TARGET_ERROR_FLAGS += -Wno-non-virtual-dtor

include $(BUILD_SHARED_LIBRARY)

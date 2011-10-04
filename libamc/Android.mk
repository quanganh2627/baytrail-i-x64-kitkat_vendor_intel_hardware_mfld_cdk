LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS := \
        -DDEBUG

LOCAL_SRC_FILES := \
        ATmodemControl.c \
        AudioModemControl_IFX_XMM6160.c \
        AmcConfDev.c

LOCAL_C_INCLUDES += \
        hardware/intel/rapid_ril/CORE \
        system/core/include/cutils \
        hardware/intel/mfld_cdk/vpc \
        hardware/intel/IFX-modem

LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_MODULE := libamc
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)


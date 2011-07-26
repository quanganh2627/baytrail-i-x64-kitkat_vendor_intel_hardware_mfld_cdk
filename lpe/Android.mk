LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_CFLAGS := -D_POSIX_SOURCE -Wno-multichar
LOCAL_C_INCLUDES += \
        external/alsa-lib/include \
        hardware/alsa_sound \
        hardware/intel/include \
        hardware/intel/mfld_cdk/vpc

LOCAL_SRC_FILES += \
        parameter_tuning_lib.c \
        lpe_io_control.cpp

LOCAL_SHARED_LIBRARIES += \
        libasound \
        liblog \
        libaudio

LOCAL_MODULE:= lpe.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

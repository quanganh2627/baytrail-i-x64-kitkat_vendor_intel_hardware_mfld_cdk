LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS := \
        -fPIC -D_POSIX_SOURCE \
        -DALSA_CONFIG_DIR=\"/system/usr/share/alsa\" \
        -DALSA_PLUGIN_DIR=\"/system/usr/lib/alsa-lib\" \
        -DALSA_DEVICE_DIRECTORY=\"/dev/snd/\" \
        -UNDEBUG

LOCAL_C_INCLUDES:= \
        $(TOP)/external/alsa-utils/include \
        $(TOP)/external/alsa-utils/android \
        $(TOP)/external/alsa-lib/include

LOCAL_SRC_FILES := \
        ATmodemControl.c \
        AudioModemControl_base.c \
        AudioModemControl_IFX_XMM6160.c

LOCAL_SHARED_LIBRARIES := \
        libaudio \
        libc

LOCAL_MODULE := libamc
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

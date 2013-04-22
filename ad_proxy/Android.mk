LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

# Do not compile the Audience proxy if the board does not include Audience
ifeq ($(BOARD_HAVE_AUDIENCE),true)

LOCAL_C_INCLUDES += \
    $(TARGET_OUT_HEADERS)/full_rw

LOCAL_SRC_FILES := \
    proxy_main.c \
    ad_i2c.c \
    ad_usb_tty.c

LOCAL_SHARED_LIBRARIES := \
    libsysutils \
    libcutils

LOCAL_STATIC_LIBRARIES := \
    libfull_rw

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)

LOCAL_MODULE := ad_proxy
LOCAL_MODULE_TAGS := debug

include $(BUILD_EXECUTABLE)

endif


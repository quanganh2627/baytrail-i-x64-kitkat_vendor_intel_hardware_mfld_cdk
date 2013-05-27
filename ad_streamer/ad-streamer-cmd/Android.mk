LOCAL_PATH:= $(call my-dir)

#Do not compile the Audience proxy if the board does not include Audience
ifeq ($(BOARD_HAVE_AUDIENCE),true)

#######################################################################
# ad_streamer command

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    ad_streamer_cmd.c \
    ad_streamer_cmd_main.c

LOCAL_C_INCLUDES := \
    $(KERNEL_HEADERS) \
    $(TARGET_OUT_HEADERS)/full_rw

LOCAL_SHARED_LIBRARIES :=  \
    libsysutils \
    libcutils \
    libhardware_legacy

LOCAL_STATIC_LIBRARIES := \
    libfull_rw \
    libad-streamer

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)

LOCAL_ERROR_FLAGS += \
    -Wall \
    -Werror

LOCAL_MODULE := ad_streamer
LOCAL_MODULE_TAGS := debug

include $(BUILD_EXECUTABLE)

endif

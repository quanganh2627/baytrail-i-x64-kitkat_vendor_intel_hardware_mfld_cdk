LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked and stored in
# hw/<SENSORS_HARDWARE_MODULE_ID>.<ro.hardware>.so
ifneq (,$(findstring $(CUSTOM_BOARD),mfld_cdk mfld_pr1))

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libnetutils libwpa_client
LOCAL_SRC_FILES += wifi.c
LOCAL_C_INCLUDES += \
        $(call include-path-for, libhardware)/hardware

ifeq ($(BOARD_HAVE_TI12XX),true)
LOCAL_CFLAGS += -DTIWLAN
endif

LOCAL_MODULE := wifi.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
endif


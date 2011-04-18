LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked and stored in
# hw/<SENSORS_HARDWARE_MODULE_ID>.<ro.hardware>.so
ifneq (,$(findstring $(CUSTOM_BOARD),mfld_cdk))

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libnetutils libwpa_client
ifeq ($(BOARD_WLAN_DEVICE),wl1283)
LOCAL_SRC_FILES += wifi.c
endif
ifeq ($(BOARD_WLAN_DEVICE),wl12xx-compat)
LOCAL_SRC_FILES += wifi_nlcp.c
endif
LOCAL_C_INCLUDES += \
        $(call include-path-for, libhardware)/hardware


LOCAL_MODULE := wifi.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
endif


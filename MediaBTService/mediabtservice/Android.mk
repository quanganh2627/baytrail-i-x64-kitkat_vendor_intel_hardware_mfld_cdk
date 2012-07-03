LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= MediaBTService.cpp main_MediaBTService.cpp

LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES += \
     external/alsa-lib/include \
     external/bluetooth/bluez/lib/bluetooth \
     vendor/intel/common/bluetooth/include \
     $(TARGET_OUT_HEADERS)/libbluetooth_vs \
     hardware/intel/mfld_cdk/MediaBTService

LOCAL_SHARED_LIBRARIES := \
	    libmediabtproxy libasound libbluetooth libbluetooth_vs

LOCAL_MODULE:= mediabtservice

include $(BUILD_EXECUTABLE)

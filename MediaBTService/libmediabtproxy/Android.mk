LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= IMediaBTService.cpp

LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
	  libutils libbinder

LOCAL_MODULE:= libmediabtproxy

include $(BUILD_SHARED_LIBRARY)
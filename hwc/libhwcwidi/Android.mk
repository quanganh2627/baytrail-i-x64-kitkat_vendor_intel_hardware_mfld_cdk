LOCAL_PATH:= $(call my-dir)

#
# libhwcwidi - proxy side for HWC widi plane
#
include $(CLEAR_VARS)
LOCAL_SRC_FILES:=               \
    FrameServerProxy.cpp         \
    FrameListenerProxy.cpp       \
    FrameTypeChangeListenerProxy.cpp

LOCAL_SHARED_LIBRARIES :=           \
    libutils                        \
    libbinder


#LOCAL_C_INCLUDES :=                                                 \

LOCAL_COPY_HEADERS := IFrameServer.h \
                      IFrameTypeChangeListener.h \
                      IFrameListener.h

LOCAL_MODULE:= libhwcwidi
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)


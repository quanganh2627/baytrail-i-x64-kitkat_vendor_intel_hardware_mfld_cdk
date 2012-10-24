ifeq ($(BOARD_USES_ALSA_AUDIO),true)

LOCAL_PATH := $(call my-dir)

  include $(CLEAR_VARS)

  LOCAL_PRELINK_MODULE := false

  LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

  LOCAL_CFLAGS := -D_POSIX_SOURCE -Wno-multichar

  LOCAL_C_INCLUDES += \
        external/tinyalsa/include \
        hardware/alsa_sound/audio_hw_lpe_centric \
        $(TARGET_OUT_HEADERS)/hw

  LOCAL_SRC_FILES:= \
        tinyalsa_if.cpp \

  LOCAL_SHARED_LIBRARIES := \
        libtinyalsa \
        liblog


  LOCAL_MODULE:= tinyalsa.$(TARGET_DEVICE)
  LOCAL_MODULE_TAGS := optional

  include $(BUILD_SHARED_LIBRARY)
endif

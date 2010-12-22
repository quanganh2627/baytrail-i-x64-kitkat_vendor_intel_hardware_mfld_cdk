ifeq ($(CUSTOM_BOARD),mfld_cdk)

LOCAL_PATH := $(call my-dir)

  include $(CLEAR_VARS)

  LOCAL_PRELINK_MODULE := false

  LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

  LOCAL_CFLAGS := -D_POSIX_SOURCE -Wno-multichar

ifneq ($(ALSA_DEFAULT_SAMPLE_RATE),)
    LOCAL_CFLAGS += -DALSA_DEFAULT_SAMPLE_RATE=$(ALSA_DEFAULT_SAMPLE_RATE)
endif

  LOCAL_C_INCLUDES += external/alsa-lib/include hardware/alsa_sound

  LOCAL_SRC_FILES:= alsa_mfld_cdk.cpp

  LOCAL_SHARED_LIBRARIES := \
  	libasound \
  	liblog

  LOCAL_MODULE:= alsa.$(TARGET_DEVICE)

  include $(BUILD_SHARED_LIBRARY)

endif

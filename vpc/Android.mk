
LOCAL_PATH := $(call my-dir)

  include $(CLEAR_VARS)

  LOCAL_PRELINK_MODULE := false

  LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

  LOCAL_CFLAGS := -D_POSIX_SOURCE -Wno-multichar -fPIC

  ifeq ($(CUSTOM_BOARD),mfld_pr2)
     LOCAL_CFLAGS += -DCUSTOM_BOARD_WITH_AUDIENCE
  endif

ifneq ($(ALSA_DEFAULT_SAMPLE_RATE),)
    LOCAL_CFLAGS += -DALSA_DEFAULT_SAMPLE_RATE=$(ALSA_DEFAULT_SAMPLE_RATE)
endif

LOCAL_C_INCLUDES += external/alsa-lib/include hardware/alsa_sound hardware/intel/include hardware/intel/mfld_cdk/vpc hardware/intel/mfld_cdk/libamc system/core/include/cutils $(TOP)/external/bluetooth/bluez/lib/bluetooth



  LOCAL_SRC_FILES:= \
	ctl_vpc.cpp \
	bt.cpp

  LOCAL_STATIC_LIBRARIES := libamc

  LOCAL_SHARED_LIBRARIES := \
		libasound \
		liblog \
		libaudio \
		libbluetooth

  LOCAL_MODULE:= vpc.$(TARGET_DEVICE)
  LOCAL_MODULE_TAGS := optional

  include $(BUILD_SHARED_LIBRARY)



LOCAL_PATH := $(call my-dir)

  include $(CLEAR_VARS)

  LOCAL_PRELINK_MODULE := false

  LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

  LOCAL_COPY_HEADERS_TO := hw

  LOCAL_COPY_HEADERS := \
    vpc_hardware.h

  LOCAL_CFLAGS := -D_POSIX_SOURCE -Wno-multichar

ifeq ($(CUSTOM_BOARD),mfld_pr2)
    LOCAL_CFLAGS += -DCUSTOM_BOARD_WITH_AUDIENCE
endif

ifneq ($(ALSA_DEFAULT_SAMPLE_RATE),)
    LOCAL_CFLAGS += -DALSA_DEFAULT_SAMPLE_RATE=$(ALSA_DEFAULT_SAMPLE_RATE)
endif

  LOCAL_C_INCLUDES += \
        external/alsa-lib/include \
        hardware/intel/include \
        hardware/intel/mfld_cdk/vpc \
        hardware/intel/mfld_cdk/libamc \
        system/core/include/cutils \
        external/bluetooth/bluez/lib/bluetooth

  LOCAL_SRC_FILES:= \
        ctl_vpc.cpp \
        acoustic.cpp \
        bt.cpp \
        msic.cpp

  LOCAL_SHARED_LIBRARIES := \
        libasound \
        liblog \
        libbluetooth \
        libcutils \
        libamc

  LOCAL_MODULE:= vpc.$(TARGET_DEVICE)
  LOCAL_MODULE_TAGS := optional

  include $(BUILD_SHARED_LIBRARY)



LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_COPY_HEADERS_TO := hw

LOCAL_COPY_HEADERS := \
    vpc_hardware.h

LOCAL_CFLAGS := -D_POSIX_SOURCE -Wno-multichar

ifeq ($(CUSTOM_BOARD),mfld_pr2)
	LOCAL_CFLAGS += \
		-DHAL_VPC_PLUS_6DB_MODEM_UL
endif

LOCAL_SRC_FILES:= \
     ctl_vpc.cpp \
     bt.cpp \
     msic.cpp \
     volume_keys.cpp

LOCAL_SHARED_LIBRARIES := \
     libasound \
     liblog \
     libbluetooth \
     libcutils \
     libamc

ifeq ($(BOARD_HAVE_AUDIENCE),true)
    LOCAL_CFLAGS += -DCUSTOM_BOARD_WITH_AUDIENCE
    LOCAL_SRC_FILES += acoustic.cpp
    LOCAL_C_INCLUDES += \
        hardware/intel/mfld_cdk/utility/property \
        external/stlport/stlport/ \
        bionic/libstdc++ \
        bionic
    LOCAL_SHARED_LIBRARIES += \
        libstlport \
        libproperty
endif

ifneq ($(BOARD_HAVE_MODEM),true)
    LOCAL_CFLAGS += -DCUSTOM_BOARD_WITHOUT_MODEM
endif

ifeq ($(CUSTOM_BOARD),ctp_pr0)
     LOCAL_CFLAGS += -DCUSTOM_BOARD_WITH_VOICE_CODEC_SLAVE
endif

ifeq ($(CUSTOM_BOARD),ctp_pr1)
     LOCAL_CFLAGS += -DCUSTOM_BOARD_WITH_VOICE_CODEC_SLAVE
endif

ifeq ($(CUSTOM_BOARD),ctp_pr2)
     LOCAL_CFLAGS += -DCUSTOM_BOARD_WITH_VOICE_CODEC_SLAVE
endif

ifneq ($(ALSA_DEFAULT_SAMPLE_RATE),)
    LOCAL_CFLAGS += -DALSA_DEFAULT_SAMPLE_RATE=$(ALSA_DEFAULT_SAMPLE_RATE)
endif

LOCAL_C_INCLUDES += \
     external/alsa-lib/include \
     hardware/intel/include \
     hardware/intel/mfld_cdk/vpc \
     hardware/intel/mfld_cdk/amc/libamc \
     system/core/include/cutils \
     external/bluetooth/bluez/lib/bluetooth \
     hardware/intel/IFX-modem

LOCAL_MODULE:= vpc.$(TARGET_DEVICE)
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)


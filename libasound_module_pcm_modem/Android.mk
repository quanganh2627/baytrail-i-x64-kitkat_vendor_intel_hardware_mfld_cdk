ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    pcm_hook_ctl_modem.c \
    ctl_modem.c \
    pcm_modem.c

LOCAL_MODULE_PATH := $(TARGET_OUT)/usr/lib/alsa-lib

LOCAL_CFLAGS += -DPIC -UNDEBUG

LOCAL_C_INCLUDES:= \
	$(TOP)/external/alsa-lib/include \
	$(TOP)/hardware/intel/mfld_cdk/libamc

LOCAL_STATIC_LIBRARIES := libamc

LOCAL_SHARED_LIBRARIES :=       \
			liblog \
			libasound

LOCAL_MODULE := libasound_module_pcm_modem
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    ctl_modem.c

LOCAL_MODULE_PATH := $(TARGET_OUT)/usr/lib/alsa-lib

#enable log
LOCAL_CFLAGS += -DPIC #-DLOG_NDEBUG=0

LOCAL_C_INCLUDES:= \
        $(TOP)/external/alsa-lib/include \
	$(TOP)/hardware/intel/mfld_cdk/libamc


LOCAL_STATIC_LIBRARIES := libamc
LOCAL_SHARED_LIBRARIES :=       \
                        liblog \
                        libasound

LOCAL_MODULE := libasound_module_ctl_modem
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
endif

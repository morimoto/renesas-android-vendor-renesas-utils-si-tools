# Include only for Renesas ones.
ifneq (,$(filter $(TARGET_PRODUCT), salvator ulcb kingfisher))

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PROPRIETARY_MODULE    := true
LOCAL_SRC_FILES             := si_flash.c si46xx.c si46xx_props.c spi.c crc32.c i2c.c
LOCAL_MODULE                := si_flash
LOCAL_MODULE_TAGS           := optional
LOCAL_C_INCLUDES            := $(LOCAL_PATH)
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_PROPRIETARY_MODULE    := true
LOCAL_SRC_FILES             := si_ctl.c si46xx.c si46xx_props.c spi.c i2c.c
LOCAL_MODULE                := si_ctl
LOCAL_MODULE_TAGS           := optional
LOCAL_C_INCLUDES            := $(LOCAL_PATH)
include $(BUILD_EXECUTABLE)
endif # Include only for Renesas ones.

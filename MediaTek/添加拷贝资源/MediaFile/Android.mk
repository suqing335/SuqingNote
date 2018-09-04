
LOCAL_PATH:= $(call my-dir)

#first method 

#MY_IMAGE_LIST := $(wildcard $(LOCAL_PATH)/wallpaper/*.jpg)
#MY_VIDEO_LIST := $(wildcard $(LOCAL_PATH)/video/*.mp4)
#MY_MEDIA_LIST := $(MY_IMAGE_LIST) \
#		 $(MY_VIDEO_LIST)

####判断是否存在指定文件夹
HAVE_SASIN_CUST_FILE := $(shell test -d $(TARGET_OUT)/media/mediaSasin/ && echo yes)
####不存在就去创建
ifneq ($(HAVE_SASIN_CUST_FILE),yes)
$(shell mkdir -p $(TARGET_OUT)/media/mediaSasin/)
endif
####拷贝文件
$(shell cp -rf $(LOCAL_PATH)/resource/ $(TARGET_OUT)/media/mediaSasin/)


# second method

#include $(CLEAR_VARS)
#LOCAL_MODULE := Tesla_01
#LOCAL_SRC_FILES := wallpaper/$(LOCAL_MODULE).jpg
#LOCAL_MODULE_CLASS := ETC
#LOCAL_MODULE_SUFFIX := .jpg
#LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/media/0/Wallpapers
#$(info ===========$(LOCAL_SRC_FILES)===============)
#include $(BUILD_PREBUILT)
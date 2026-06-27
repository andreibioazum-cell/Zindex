LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := main
LOCAL_SRC_FILES := src/main.c src/game.c src/world.c src/math.c $(NDK_ROOT)/sources/android/native_app_glue/android_native_app_glue.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/src $(NDK_ROOT)/sources/android/native_app_glue
LOCAL_LDLIBS := -landroid -llog -lEGL -lGLES2v -lm
LOCAL_CFLAGS := -DANDROID -std=c99 -O2
include $(BUILD_SHARED_LIBRARY)

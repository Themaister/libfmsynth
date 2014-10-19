LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := fmsynth_neon
LOCAL_SRC_FILES := \
	../../fmsynth.c \
	../../fmsynth_test.c \
	../../arm/fmsynth_neon.S \
	fmsynth_jni.c

LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true

LOCAL_LDLIBS := -lm
LOCAL_CFLAGS := -std=c99 -Wall -Wextra -DFMSYNTH_SIMD -Ofast

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := fmsynth
LOCAL_SRC_FILES := \
	../../fmsynth.c \
	../../fmsynth_test.c \
	fmsynth_jni.c

LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := false

LOCAL_LDLIBS := -lm
LOCAL_CFLAGS := -std=c99 -Wall -Wextra -Ofast

include $(BUILD_SHARED_LIBRARY)

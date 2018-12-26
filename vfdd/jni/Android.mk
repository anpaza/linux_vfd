# Android makefile for building an bionic-linked binary of the vfdd

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := vfdd
LOCAL_SRC_FILES := $(addprefix ../,vfdd.c cfg_parse/cfg_parse.c cfg.c sysfs.c task.c \
	task-display.c task-suspend.c task-clock.c task-temp.c task-disk.c task-dot.c)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../cfg_parse

include $(BUILD_EXECUTABLE)

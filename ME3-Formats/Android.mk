LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cpp .cc
LOCAL_SRC_FILES = test.cpp pcc.cpp
LOCAL_MODULE := test
LOCAL_CFLAGS += -Wall -std=c++11
LOCAL_LDLIBS = -lz

ifeq ($(TARGET_ARCH_ABI),x86)
    LOCAL_CFLAGS += -ffast-math -mtune=atom -mssse3 -mfpmath=sse
endif

include $(BUILD_EXECUTABLE)

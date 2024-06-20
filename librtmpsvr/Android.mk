LOCAL_PATH := $(call my-dir)
 
include $(CLEAR_VARS)
 
LOCAL_MODULE := librtmpsvr
 
include $(LOCAL_PATH)/config-android.mak
 
SRC = librtmp/amf.c \
	librtmp/hashswf.c \
	librtmp/log.c \
	librtmp/parseurl.c \
	librtmp/rtmp.c \
	rtmp_stream.c \
	rtmpsrv.c \
	packet_fifo.c \
	rtmp_client.c \
	thread.c \
	utils.c

CFLAGS += -I$(LOCAL_PATH)/. 

LOCAL_CPP_EXTENSION := .c
LOCAL_SRC_FILES := $(SRC)

#NDK_DEBUG=1

LOCAL_CC := $(CC)
LOCAL_CXX := $(CXX)
TARGET_CC := $(CC)
TARGET_CXX := $(CXX)
LOCAL_CFLAGS := $(CFLAGS)
LOCAL_CPPFLAGS := $(CXXFLAGS)
LOCAL_LDFLAGS := $(LDFLAGS)
LOCAL_LDLIBS := $(LDLIBS)
#LOCAL_SHARED_LIBRARIES := $(SHARED_LIBS)
#LOCAL_STATIC_LIBRARIES := $(STATIC_LIBS)

include $(BUILD_SHARED_LIBRARY)


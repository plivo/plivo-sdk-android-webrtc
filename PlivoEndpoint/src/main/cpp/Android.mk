# A simple test for the minimal standard C++ library
#

LOCAL_PATH := $(call my-dir)


#include $(CLEAR_VARS)
#LOCAL_MODULE := local_gperf
#LOCAL_SRC_FILES := $(LOCAL_PATH)/../../../distribution/gperf/lib/x86/libgperf.so
#LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../distribution/gperf/include

#include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := local_ssl
LOCAL_SRC_FILES :=$(LOCAL_PATH)/../../../distribution/openssl/$(TARGET_ARCH_ABI)/lib/libssl.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../distribution/openssl/$(TARGET_ARCH_ABI)/include
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := local_crypto
LOCAL_SRC_FILES :=$(LOCAL_PATH)/../../../distribution/openssl/$(TARGET_ARCH_ABI)/lib/libcrypto.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../distribution/openssl/$(TARGET_ARCH_ABI)/include
include $(PREBUILT_STATIC_LIBRARY)

#include $(CLEAR_VARS)
#LOCAL_MODULE := local_ssl
#LOCAL_SRC_FILES := $(LOCAL_PATH)/../../../distribution/openssl/x86/lib/libssl.so
#LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../distribution/openssl/x86/include
#include $(PREBUILT_SHARED_LIBRARY)

#include $(CLEAR_VARS)
#LOCAL_MODULE := local_crypto
#LOCAL_SRC_FILES := $(LOCAL_PATH)/../../../distribution/openssl/x86/lib/libcrypto.so
#LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../distribution/openssl/x86/include
#include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := local_resip
LOCAL_SRC_FILES := $(LOCAL_PATH)/../../../distribution/resip/libs/$(TARGET_ARCH_ABI)/libresip-1.12.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../distribution/resip/include
#LOCAL_EXPORT_C_INCLUDES :=  $(LOCAL_PATH)/../../../../../../dev/new_plivo-resip-krishna/plivo-resip/resiprocate
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := local_rutil
LOCAL_SRC_FILES := $(LOCAL_PATH)/../../../distribution/resip/libs/$(TARGET_ARCH_ABI)/librutil-1.12.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../distribution/resip/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := local_ares
LOCAL_SRC_FILES := $(LOCAL_PATH)/../../../distribution/resip/libs/$(TARGET_ARCH_ABI)/libresipares-1.12.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../distribution/resip/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := local_dum
LOCAL_SRC_FILES := $(LOCAL_PATH)/../../../distribution/resip/libs/$(TARGET_ARCH_ABI)/libdum-1.12.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../../../distribution/resip/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := rtcsip_jni
LOCAL_SRC_FILES :=  $(LOCAL_PATH)/  ../cpp/rtcsip_jni.cpp ../cpp/SipControllerCore.cpp
LOCAL_LDLIBS    := -llog -landroid -std=c++11  -fexceptions -fstack-protector
LOCAL_CFLAGS := -std=c++11 -fexceptions -frtti -fstack-protector

LOCAL_STATIC_LIBRARIES := local_ssl local_crypto
LOCAL_SHARED_LIBRARIES := local_resip local_rutil local_ares local_dum

include $(BUILD_SHARED_LIBRARY)
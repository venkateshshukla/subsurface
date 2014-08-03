LOCAL_PATH := $(call my-dir)
LIB_PATH := $(PREFIX)/lib
INCLUDE_PATH := $(PREFIX)/include

include $(CLEAR_VARS)
LOCAL_MODULE    := libsqlite
LOCAL_SRC_FILES := $(LIB_PATH)/libsqlite3.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libz
LOCAL_SRC_FILES := $(LIB_PATH)/libz.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libzip
LOCAL_SRC_FILES := $(LIB_PATH)/libzip.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libexslt
LOCAL_SRC_FILES := $(LIB_PATH)/libexslt.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libxslt
LOCAL_SRC_FILES := $(LIB_PATH)/libxslt.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libxml
LOCAL_SRC_FILES := $(LIB_PATH)/libxml2.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libgit
LOCAL_SRC_FILES := $(LIB_PATH)/libgit2.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libexif
LOCAL_SRC_FILES := $(LIB_PATH)/libexif.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libusb
LOCAL_SRC_FILES := $(LIB_PATH)/libusb-1.0.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libftdi
LOCAL_SRC_FILES := $(LIB_PATH)/libftdi1.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libdivecomputer
LOCAL_SRC_FILES := $(LIB_PATH)/libdivecomputer.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libsubsurface
LOCAL_SRC_FILES := deco.c device.c dive.c divelist.c equipment.c file.c
LOCAL_SRC_FILES += libdivecomputer.c load-git.c membuffer.c parse-xml.c
LOCAL_SRC_FILES += planner.c profile.c worldmap-save.c save-git.c save-xml.c
LOCAL_SRC_FILES += save-html.c sha1.c statistics.c strtod.c getline.c gettext.c
LOCAL_SRC_FILES += subsurfacestartup.c time.c uemis.c uemis-downloader.c linux.c
LOCAL_SRC_FILES += androidhelper.c divecomputer_android.c
LOCAL_C_INCLUDES := $(INCLUDE_PATH)
LOCAL_C_INCLUDES += $(INCLUDE_PATH)/libxml2
LOCAL_C_INCLUDES += $(INCLUDE_PATH)/../lib/libzip/include
LOCAL_CFLAGS := -std=gnu99
LOCAL_WHOLE_STATIC_LIBRARIES := libsqlite libz libzip libexslt libxslt libxml
LOCAL_WHOLE_STATIC_LIBRARIES += libgit libexif libusb libftdi libdivecomputer
include $(BUILD_SHARED_LIBRARY)


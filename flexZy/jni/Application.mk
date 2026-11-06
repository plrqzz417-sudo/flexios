APP_ABI := arm64-v8a
APP_PLATFORM := android-24
APP_CPPFLAGS := -std=c++17 -fno-exceptions -fno-rtti -O2
APP_CFLAGS := -O2
APP_STL := c++_static
APP_CPPFLAGS += -I$(LOCAL_PATH)/imgui
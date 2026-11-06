#pragma once

#include <jni.h>
#include <android/log.h>

#define ZYGISK_EXPORT __attribute__((visibility("default")))

namespace zygisk {

struct Api;
struct AppSpecializeArgs;
struct ServerSpecializeArgs;

class ModuleBase {
public:
    virtual void onLoad(Api* api, JNIEnv* env) = 0;
    virtual void preAppSpecialize(AppSpecializeArgs* args) {}
    virtual void preServerSpecialize(ServerSpecializeArgs* args) {}
    virtual void postAppSpecialize(const AppSpecializeArgs* args) {}
    virtual void postServerSpecialize(const ServerSpecializeArgs* args) {}
    virtual ~ModuleBase() = default;
};

struct AppSpecializeArgs {
    jstring nice_name;
    jstring app_data_dir;
    jintArray rlimits;
};

struct ServerSpecializeArgs {
    jstring nice_name;
};

struct Api {
    void setOption(int opt) {}
};

enum Option {
    DLCLOSE_MODULE_LIBRARY = 1,
};

} // namespace zygisk

#define REGISTER_ZYGISK_MODULE(cls) \
    extern "C" ZYGISK_EXPORT void zygisk_module_entry(::zygisk::Api* api, JNIEnv* env) { \
        static cls module; \
        module.onLoad(api, env); \
    } \
    extern "C" ZYGISK_EXPORT void zygisk_pre_app_specialize(::zygisk::Api* api, JNIEnv* env, ::zygisk::AppSpecializeArgs* args) { \
        static cls module; \
        module.preAppSpecialize(args); \
    } \
    extern "C" ZYGISK_EXPORT void zygisk_post_app_specialize(::zygisk::Api* api, JNIEnv* env, const ::zygisk::AppSpecializeArgs* args) { \
        static cls module; \
        module.postAppSpecialize(args); \
    }
#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include "zygisk.hpp"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "FlexCity", __VA_ARGS__)

using zygisk::Api;
using zygisk::AppSpecializeArgs;

static const char* GAME_PACKAGE = "com.jarvigames.viceonline";

class FlexCityModule : public zygisk::ModuleBase {
public:
    void onLoad(Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs* args) override {
        auto package = env->GetStringUTFChars(args->nice_name, nullptr);
        if (package && strcmp(package, GAME_PACKAGE) == 0) {
            enable = true;
            LOGI("FlexCity: detectado %s", package);
        }
        env->ReleaseStringUTFChars(args->nice_name, package);
    }

    void postAppSpecialize(const AppSpecializeArgs* args) override {
        if (!enable) return;

        // caminho absoluto da lib dentro do módulo (KernelSU extrai pra /data/adb/modules/)
        const char* lib_path = "/data/adb/modules/flexcity_zygisk/zygisk/arm64-v8a/zygisk.so";

        void* handle = dlopen(lib_path, RTLD_NOW);
        if (!handle) {
            LOGI("FlexCity: dlopen %s falhou: %s", lib_path, dlerror());
            return;
        }

        auto jni_onload = (jint(*)(JavaVM*, void*))dlsym(handle, "JNI_OnLoad");
        if (!jni_onload) {
            LOGI("FlexCity: JNI_OnLoad nao encontrado");
            dlclose(handle);
            return;
        }

        JavaVM* vm = nullptr;
        env->GetJavaVM(&vm);
        jni_onload(vm, (void*)1337);
        LOGI("FlexCity: JNI_OnLoad chamado");
    }

private:
    Api* api = nullptr;
    JNIEnv* env = nullptr;
    bool enable = false;
};

REGISTER_ZYGISK_MODULE(FlexCityModule)
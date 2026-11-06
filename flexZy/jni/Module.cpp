#include <sys/types.h>
#include <jni.h>
#include <android/log.h>
#include <cstring>
#include "zygisk.hpp"
#include "Cheat.h"
#include "Hooks.h"
#include "Input.h"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class FlexCityModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        const char *process = env->GetStringUTFChars(args->nice_name, nullptr);
        LOGI("Zygisk preAppSpecialize: %s", process);

        bool is_target = (process && strcmp(process, "com.jarvigames.viceonline") == 0);
        env->ReleaseStringUTFChars(args->nice_name, process);

        if (!is_target) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        LOGI("Zygisk postAppSpecialize: iniciando...");
        Cheat_Init();
        InstallGraphicsHook(env);
        Input_Init();
        LOGI("FlexCity boot OK");
    }

private:
    Api *api;
    JNIEnv *env;
};

REGISTER_ZYGISK_MODULE(FlexCityModule)
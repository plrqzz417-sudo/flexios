#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <android/log.h>

#define LOG_TAG "FlexCity"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

struct Vector3 { float x, y, z; };
struct Matrix4x4 { float m[16]; };

struct TargetData {
    Vector3 pos;
    Vector3 head;
    int16_t hp;
    int16_t maxHp;
    float distance;
    char name[32];
    bool valid;
    bool is_local;
};

struct AimbotConfig {
    bool enabled = true;
    bool silent_aim = true;
    bool esp_box = true;
    bool esp_health = true;
    bool esp_name = true;
    bool esp_line = true;
    float fov_deg = 45.0f;
    float max_snap_deg = 25.0f;
};

extern AimbotConfig g_cfg;

void Cheat_Init();
int Cheat_GetTargets(TargetData* out, int max);
Matrix4x4 Cheat_GetViewMatrix();
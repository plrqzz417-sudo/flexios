#pragma once
#include <cstdint>
#include <jni.h>

bool InstallGraphicsHook(JNIEnv* env);
void SetOverlayVisible(bool v);
bool IsOverlayVisible();

struct RenderTargets {
    struct TargetData* data;
    int count;
};
void SetRenderTargets(const struct TargetData* src, int count);
RenderTargets GetRenderTargets();
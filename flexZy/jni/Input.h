#pragma once
#include <android/log.h>
#define LOG_TAG "FlexCity"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

void Input_Init();
void Input_HandleTouch();
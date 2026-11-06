#include "Input.h"
#include "imgui.h"
#include <android/input.h>

void Input_Init() {
    LOGI("Input init");
}

// Placeholder — o ImGui_ImplAndroid oficial precisa de AInputEvent.
// O touch é gerenciado pela SurfaceView (FLAG_NOT_TOUCHABLE deixa o jogo receber).
void Input_HandleTouch() {
}
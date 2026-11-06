// C++ / Menu.h / iOS ARM64 ImGui
#pragma once
bool Menu_IsOpen();
void Menu_GetBounds(float* x,float* y,float* w,float* h);
void Menu_Render();
bool Menu_AimbotEnabled();
bool Menu_QueueTouch(int action,float x,float y);
bool Menu_HitTest(float x,float y);
void Menu_DrainInput();

enum class StreamModeState { Off, Applying, Requested, Unavailable, Error };
void Menu_RequestStreamMode(bool enabled);
bool Menu_StreamModeRequested();
StreamModeState Menu_StreamModeState();
void Menu_SetStreamModeState(StreamModeState state);

int Menu_GetToggleKey();
int Menu_GetMenuToggleKey();
void Menu_ToggleRequested();

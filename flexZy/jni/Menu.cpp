#include "Menu.h"
#include "Cheat.h"
#include "imgui.h"
#include <cmath>

static float fov_radius_px(float fov_deg, float screen_h) {
    if (screen_h <= 0.f) return 0.f;
    float clamped = fmaxf(1.0f, fminf(fov_deg, 179.0f));
    float half_v = (70.f * 0.5f) * 3.14159265f / 180.f;
    float half_o = (clamped * 0.5f) * 3.14159265f / 180.f;
    float tan_v = tanf(half_v);
    if (fabsf(tan_v) < 0.00001f) return 0.f;
    return ((screen_h * 0.5f) / tan_v) * tanf(half_o);
}

void Menu_Render() {
    static bool menu_open = true;
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("FlexCity", &menu_open);

    ImGui::Text("ESP");
    ImGui::Checkbox("Box", &g_cfg.esp_box);
    ImGui::Checkbox("Health", &g_cfg.esp_health);
    ImGui::Checkbox("Name", &g_cfg.esp_name);
    ImGui::Checkbox("Line", &g_cfg.esp_line);

    ImGui::Separator();
    ImGui::Text("Aimbot");
    ImGui::Checkbox("Enabled", &g_cfg.enabled);
    ImGui::Checkbox("Silent Aim", &g_cfg.silent_aim);
    ImGui::SliderFloat("FOV", &g_cfg.fov_deg, 1.0f, 90.0f, "%.1f");
    ImGui::SliderFloat("Max Snap", &g_cfg.max_snap_deg, 1.0f, 90.0f, "%.1f");

    ImGui::Separator();
    if (ImGui::Button("Close Menu")) menu_open = false;

    ImGui::End();

    if (g_cfg.enabled) {
        ImVec2 screen = ImGui::GetIO().DisplaySize;
        float cx = screen.x * 0.5f;
        float cy = screen.y * 0.5f;
        float radius = fov_radius_px(g_cfg.fov_deg, screen.y);
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        dl->AddCircle(ImVec2(cx, cy), radius, IM_COL32(0, 0, 0, 130), 32, 2.f);
        dl->AddCircle(ImVec2(cx, cy), radius, IM_COL32(255, 255, 255, 200), 32, 1.f);
    }
}
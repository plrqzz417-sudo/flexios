// C++ / Cheat.h / iOS ARM64 Unity IL2CPP
#pragma once
#include "Version.h"
#include <atomic>
#include <cstdint>
#include <android/log.h>
#define LOG_TAG "FlexCity"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
struct Vector3 { float x, y, z; };
inline constexpr int SkeletonEdges[][2] = {
    {10,9},{9,8},{8,7},{7,0},
    {8,13},{13,15},{15,17},{8,14},{14,16},{16,18},
    {0,1},{1,3},{3,5},{0,2},{2,4},{4,6}
};
inline constexpr const char* AimBoneNames[]={"Cabeca","Pescoco","Peito","Coluna","Quadril"};
inline constexpr int AimBoneIds[]={10,9,8,7,0};
inline constexpr uint32_t ValidBonePool(uint32_t pool) { return (pool&31)?(pool&31):1; }
inline constexpr int PickAimBone(uint32_t pool,uint32_t random,int previous) {
    pool=ValidBonePool(pool);
    if(previous>=0&&previous<5&&(pool&(pool-1))) pool&=~(1u<<previous);
    int count=0; for(int i=0;i<5;++i) if(pool&(1u<<i)) ++count;
    unsigned pick=random%count;
    for(int i=0;i<5;++i) if(pool&(1u<<i)) { if(!pick--) return i; }
    return 0;
}
struct TargetData {
    Vector3 pos{}, head{}, screen_pos{0,0,-1}, screen_head{0,0,-1};
    int16_t hp = 0, maxHp = 0, armor = 0;
    float distance = 0;
    Vector3 bones[19] = {};
    Vector3 world_bones[19] = {};
    uint32_t bone_mask = 0;
    char name[192] = {};
    bool passive=false,invulnerable=false,friend_player=false,friend_known=false;
    bool valid = false, is_local = false;
};
struct SessionPlayer {uint32_t entity=0;char name[192]={},account[128]={};bool excluded=false,is_local=false;};
struct AimbotConfig { std::atomic<int> esp_hz{30}; std::atomic<float> esp_distance{500}; std::atomic<bool> esp_skeleton{true},hide_passive{false},hide_invulnerable{false},hide_friends{false}; bool esp_head=false,esp_fill=false,esp_hp_text=false; bool esp_enabled=true, esp_corner=true, esp_outline=true, show_fov=true; float esp_color[4]={0.97f,0.24f,0.32f,1}; float esp_thickness=1.5f,bar_width=6; bool esp_box=true, esp_health=true, esp_name=true, esp_line=false, esp_self=false; };
struct AimbotTune {
    bool vehicle_car=false,vehicle_bike=false,vehicle_boat=false;
    float vehicle_car_power=1.5f,vehicle_bike_power=1.5f,vehicle_boat_power=1.5f;
    bool enabled=false, require_aiming=true, silent=false;
    bool native_assist=false;
    float assist_x=2.f,assist_y=2.f,assist_distance=1.5f;
    float assist_radius_min=1.25f,assist_radius_max=1.5f;
    bool require_visible=true;
    bool multipoint=false;
    int target_priority=0; // 0: crosshair, 1: distance, 2: current HP
    bool ignore_passive=true,ignore_invulnerable=true,ignore_friends=true;
    uint64_t revision=0;
    bool cargo_farm=false;
    float cargo_step=2.f; // minimum real run distance after each delivery
    bool tp_marker=false,spin_visual=false,tiny_visual=false,weapon_tint=false;
    float spin_speed=360,model_scale=.08f,weapon_color[3]={1,.1f,.5f};
    bool auto_revive=false,speed=false,fly=false;
    float move_speed=12,fly_speed=10;
    float move_response=.12f;
    bool fps120=false,freeze_ammo=false,body_headshot=false;
    bool no_spread=false,no_recoil=false,auto_fire=false;
    bool rapid_fire=false;
    float rapid_multiplier=2;
    float silent_max_angle=6,hit_chance=100;
    float distanceMax=200, fov_deg=30, smoothing=4;
    int bone=0; bool random_bones=false; uint32_t bone_pool=31; float bone_interval=0.8f;
};
struct CheatStatus {
    unsigned vehicle_ready=0;int vehicle_active=0;float vehicle_power=1;
    unsigned cost_mean_us[5]{},cost_peak_us[5]{};
    bool pose_ready=false;unsigned pose_samples=0;
    unsigned spread_overrides=0,recoil_impulses=0,recoil_camera_impulses=0;
    bool local_fire_ready=false;
    bool ammo_ready=false,spread_ready=false,accuracy_ready=false,recoil_apply_ready=false;
    unsigned ammo_blocked=0,head_marks=0,hs_checked=0,hs_promoted=0,hs_missing=0,hs_steered=0;
    bool keybind_ready=false;
    bool auto_fire_ready=false,auto_firing=false,no_recoil_ready=false;
    bool camera_lock_ready=false, silent_ready=false;
    bool native_assist_ready=false,native_assist_active=false;
    unsigned native_assist_calls=0,native_assist_scaled=0;
    bool hook_ready=false, local_ready=false, camera_ready=false, locked=false;
    unsigned shots=0,rays=0,redirects=0;
    int protected_targets=0,filter_unknown=0;
    unsigned frames=0, callback_calls=0;
    unsigned callback_counts[5]={};
    char aim_state[80]="Disabled";
    int players=0, visible=0, invoke_errors=0, skeletons=0, aim_mode=-1;
    int dead_filtered=0, hidden_filtered=0, aim_point=0, aim_bone=0;
    float camera_fov=60;
    float aim_error_pixels=-1;
    float aim_rig_gap_cm=-1;
    char initialization[128]="Starting IL2CPP";
};
extern AimbotConfig g_cfg;
void Cheat_Init();
int Cheat_GetTargets(TargetData* out, int max);
void Cheat_SetScreenSize(int w, int h);
AimbotTune Cheat_GetAimbot();
void Cheat_SetAimbot(const AimbotTune& tune);
CheatStatus Cheat_GetStatus();

int Cheat_GetSessionPlayers(SessionPlayer* out,int max,bool* ready);
void Cheat_SetExcluded(const char* account,bool exclude);
void Cheat_ClearExclusions();

void Cheat_ToggleAimbot();

void Cheat_RequestMarkerTP();
void Cheat_SetFlyVertical(int direction);
void Cheat_GetMovementMessage(char* out,int size);

void Cheat_GetMovementDiagnostics(char* out,int size);
void Cheat_GetReviveMessage(char* out,int size);

void Cheat_GetFarmMessage(char* out,int size);
bool Cheat_FollowSearchEnabled();
void Cheat_SetFollowSearchEnabled(bool enabled);

// C++ / Cheat.cpp / iOS ARM64 Unity IL2CPP
#include "Cheat.h"
#include "Auth.h"
#include "Menu.h"
#include "il2cpp_api.h"
#include "InlineHook.h"
#include "HeadGeometry.h"
#include "PoseFreshness.h"
#include "Performance.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <string>
#include <type_traits>
#include <unistd.h>
#include <sys/uio.h>
#include "Platform.h"
using namespace il2cpp_api;
AimbotConfig g_cfg;
static std::mutex state_mutex;
static AimbotTune config;
static CheatStatus status;
static CostMeter cost_meters[5];
static void vehicle_status(CheatStatus&);
static std::atomic<uint32_t> pose_demand_mask{0},pose_priority_mask{0};
static std::atomic<int> pose_body_period_ms{33};
static std::atomic<bool> pose_geometry_needed{false};
static std::vector<TargetData> snapshot;
static std::chrono::steady_clock::time_point snapshot_at{};
static std::vector<SessionPlayer> roster;
static std::unordered_set<std::string> excluded_accounts;
static std::unordered_map<uint32_t,std::string> session_accounts;
static bool roster_ready=false;
static std::atomic<bool> roster_requested{false};
static MethodInfo* session_dictionary=nullptr;
static bool (*class_is_value)(Il2CppClass*)=nullptr;
static std::atomic<int> screen_w{0}, screen_h{0};
static std::atomic<bool> started{false}, installed{false};
static unsigned frame_counter=0;
static unsigned room_epoch=0;
static MethodInfo* room_session_method=nullptr;
using LateUpdateFn=void (*)(void*, const MethodInfo*);
static std::atomic<LateUpdateFn> originals[5];
static std::atomic<bool> callback_ready[5];
static std::atomic<unsigned> callback_counts[5];
static FieldInfo *local_field,*switch_instance,*switch_aim,*switch_rig;
static MethodInfo *is_active,*child_renderers,*renderer_visible,*renderer_enabled,*renderer_forced_off;
static MethodInfo *hit_classifier=nullptr;
static MethodInfo *hit_manager,*transform_point,*get_game_object,*components_internal;
static MethodInfo* skin_object_active=nullptr;
static MethodInfo *mesh_get_bones=nullptr,*object_get_name=nullptr;
static MethodInfo *mesh_shared=nullptr,*mesh_vertices=nullptr,*mesh_weights=nullptr,*mesh_bindposes=nullptr,*mesh_readable=nullptr;
static MethodInfo *head_mesh_ctor=nullptr,*head_bake=nullptr,*head_recalculate=nullptr,*head_bounds=nullptr,*head_inverse_point=nullptr;
static Il2CppClass* head_mesh_class=nullptr;
static GCHandle head_scratch_mesh=0;
static Il2CppObject *renderer_type,*hit_controller_type;
static Il2CppClass *sphere_class,*capsule_class;
struct Quaternion { float x,y,z,w; };
struct AimTracking {
    GCHandle handle=0;
    uintptr_t local=0,camera=0,rig=0;
    float yaw=0,pitch=0,goal_yaw=0,goal_pitch=0;
    int bone=-1; std::chrono::steady_clock::time_point bone_changed{};
    bool active=false;
    std::chrono::steady_clock::time_point sampled{},updated{};
};
// Accessed only from Unity callbacks; no managed calls on the overlay thread.
static AimTracking tracking,silent_tracking;
static uint64_t applied_revision=0;
static std::atomic<int> unity_thread{0};
static_assert(std::is_same_v<decltype(tracking.handle),GCHandle>, "Target storage must match the runtime handle ABI");
static_assert(std::is_same_v<gchandle_new_fn,GCHandle (*)(Il2CppObject*,bool)>);
static_assert(std::is_same_v<gchandle_get_target_fn,Il2CppObject* (*)(GCHandle)>);
static_assert(std::is_same_v<gchandle_free_fn,void (*)(GCHandle)>);
static_assert(sizeof(void*)!=8||GCHandle{0x00000079fc5aff28ULL}==0x00000079fc5aff28ULL,
              "Target handles must preserve the upper 32 address bits");
static void clear_target(AimTracking& target=tracking) {
    if(target.handle&&gchandle_free) gchandle_free(target.handle);
    target.handle=0; target.active=false; target.bone=-1;
}
static void clear_all_targets() {clear_target(tracking);clear_target(silent_tracking);}
static uintptr_t selected_player(const AimTracking& target=tracking) {
    return target.handle&&gchandle_get_target?(uintptr_t)gchandle_get_target(target.handle):0;
}
static constexpr bool camera_route(bool enabled,bool silent,uint64_t requested,uint64_t applied) {
    return enabled&&!silent&&requested==applied;
}
static constexpr bool primary_camera_output(uintptr_t output,uintptr_t active,uintptr_t main) {
    return output&&(active?output==active:output==main);
}
static constexpr bool chance_pass(float percent,uint32_t roll) {
    return percent>=100||(percent>0&&(roll%10000)<percent*100);
}
static MethodInfo *find_objects,*get_position,*get_forward,*get_transform,*world_to_screen;
static MethodInfo *camera_main,*pixel_width,*pixel_height,*camera_fov;
static MethodInfo *screen_width,*screen_height;
static Il2CppObject* linker_type;
static MethodInfo* shot_camera_rotation=nullptr;
static MethodInfo *nickname_text,*set_rotation,*brain_output;
static MethodInfo *get_bone,*is_human,*child_animator;
static Il2CppObject* animator_type;
static std::chrono::steady_clock::time_point last_brain{};
static uintptr_t observed_local=0;
static bool observed_alive=false;
static constexpr bool session_changed(uintptr_t before,bool was_alive,uintptr_t after,bool alive) {
    return before!=after||was_alive!=alive;
}
using AxisUpdateFn=bool (*)(void*,float,const MethodInfo*);
using AxisResetFn=void (*)(void*,const MethodInfo*);
static std::atomic<AxisUpdateFn> axis_original{nullptr};
static std::atomic<bool> axis_ready{false};
using ShootFn=void (*)(void*,Vector3,Vector3,const MethodInfo*);
static std::atomic<ShootFn> shoot_original{nullptr};
static std::atomic<bool> shoot_ready{false},cast_ready{false};
struct HitInfoResult {
    uint64_t damaged; Vector3 point; uint32_t padding;
    uintptr_t transform; uint32_t flags,end_padding;
};
static_assert(sizeof(HitInfoResult)==40&&offsetof(HitInfoResult,transform)==24&&offsetof(HitInfoResult,flags)==32);
using CastFn=HitInfoResult (*)(Vector3,Vector3,float,bool,bool,void*,const MethodInfo*);
static std::atomic<CastFn> cast_original{nullptr};
struct ShotContext {bool scope=false,active=false;Vector3 point{};float range=0;};
static std::atomic<unsigned> local_shots{0},local_rays{0},redirected_rays{0};
static FieldInfo* client_session_field=nullptr;
static MethodInfo *hit_status=nullptr,*player_api_get=nullptr,*player_userid=nullptr,*friend_contains=nullptr,*player_packet=nullptr;
// Only the Unity thread can open/read this scope. Other threads always forward.
static ShotContext shot_context;
static bool shot_no_spread=false;
static Vector3 shot_base_direction{};
using FireInputFn=void (*)(void*,bool,const MethodInfo*);
static std::atomic<FireInputFn> fire_input_original{nullptr};
static std::atomic<bool> fire_input_ready{false};
static FieldInfo *controls_instance=nullptr,*shooting_field=nullptr,*runtime_characteristics=nullptr,*characteristics_recoil=nullptr;
static MethodInfo* clone_object=nullptr;
static FieldInfo *characteristics_accuracy=nullptr,*characteristics_spread_time=nullptr,*characteristics_spread_cooldown=nullptr,*recoil_rotation_field=nullptr;
static MethodInfo *fps_get=nullptr,*fps_set=nullptr;
static MethodInfo* keyboard_key=nullptr;
static std::atomic<bool> keyboard_ready{false},recoil_ready{false};
using SetObjectFieldFn=void (*)(Il2CppObject*,FieldInfo*,Il2CppObject*);
static SetObjectFieldFn set_object_field=nullptr;
static GCHandle recoil_runtime=0;
static FieldInfo *scoped_weapon_field=nullptr,*scoped_angle_field=nullptr;
static FieldInfo* rapid_interval_field=nullptr;
static GCHandle rapid_runtime=0;
static float rapid_saved=0,rapid_applied=0;
static GCHandle scoped_weapon_original=0,scoped_weapon_clone=0;
static std::atomic<unsigned> spread_override_count{0},recoil_impulse_count{0};
using RecoilImpulseFn=void (*)(void*,float,const MethodInfo*);
static std::atomic<RecoilImpulseFn> recoil_impulse_original{nullptr};
static void update_weapon_mods(uintptr_t local,bool alive);
static GCHandle auto_controls=0,recoil_original_asset=0,recoil_clone_asset=0;
using ConsumeFn=bool (*)(void*,const MethodInfo*);
using SpreadFn=float (*)(void*,const MethodInfo*);
static std::atomic<ConsumeFn> consume_original{nullptr};
static std::atomic<SpreadFn> spread_original{nullptr},accuracy_original{nullptr};
using RecoilApplyFn=void (*)(void*,const MethodInfo*);
static std::atomic<RecoilApplyFn> recoil_apply_original{nullptr};
using FireVisualFn=void (*)(void*,bool,bool,bool,void*,const MethodInfo*);
using FireShakeFn=void* (*)(void*,float,float,float,float,const MethodInfo*);
using FireShakeInfluenceFn=void* (*)(void*,float,float,float,float,Vector3,Vector3,const MethodInfo*);
static std::atomic<FireVisualFn> fire_visual_original{nullptr};
static std::atomic<FireShakeFn> fire_shake_original{nullptr};
static std::atomic<FireShakeInfluenceFn> fire_shake_influence_original{nullptr};
struct SafeShotTransform {float origin_x,origin_y,origin_z,origin_check,direction_x,direction_y,direction_z,direction_check;};
static_assert(sizeof(SafeShotTransform)==32&&offsetof(SafeShotTransform,direction_x)==16,"SafeTransform ARM64 value ABI");
using SafeShootFn=void (*)(void*,SafeShotTransform,const MethodInfo*);
static std::atomic<SafeShootFn> safe_shoot_original{nullptr};
static unsigned local_visual_depth=0; // Unity thread only
static std::atomic<unsigned> recoil_camera_impulses{0};
static std::atomic<bool> ammo_ready{false},spread_ready{false};
static std::atomic<unsigned> ammo_blocked{0},head_marks{0},hs_checked{0},hs_promoted{0},hs_missing{0},hs_steered{0};
static std::atomic<bool> auto_held{false};
static bool manual_held=false;
static void set_auto_fire(bool held) {
    uintptr_t control=0;if(controls_instance)field_static_get(controls_instance,&control);
    auto old=auto_controls&&gchandle_get_target?(uintptr_t)gchandle_get_target(auto_controls):0;
    if(!held||control!=old) {
        if(auto_held&&old&&shooting_field&&field_set_value) {
            uintptr_t native=0;iovec l{&native,sizeof(native)},r{(void*)(old+0x10),sizeof(native)};
            if(syscall(SYS_process_vm_readv,getpid(),&l,1,&r,1,0)==sizeof(native)&&native)field_set_value((Il2CppObject*)old,shooting_field,&manual_held);
        }
        auto_held=false;
        if(control!=old) {if(auto_controls)gchandle_free(auto_controls);auto_controls=0;manual_held=false;}
    }
    uintptr_t native=0;
    if(control){iovec l{&native,sizeof(native)},r{(void*)(control+0x10),sizeof(native)};
        if(syscall(SYS_process_vm_readv,getpid(),&l,1,&r,1,0)!=sizeof(native))native=0;}
    if(held&&native&&control&&shooting_field&&field_set_value&&fire_input_ready.load()) {
        if(!auto_controls)auto_controls=gchandle_new((Il2CppObject*)control,false);
        bool pressed=true;field_set_value((Il2CppObject*)control,shooting_field,&pressed);auto_held=true;
    }
}
static MethodInfo *physics_ray=nullptr,*hit_transform=nullptr,*is_child=nullptr;
static Il2CppClass* ray_hit_class=nullptr;
static size_t ray_hit_size=0;
static GCHandle ray_buffer=0;
static Il2CppArray* (*array_new)(Il2CppClass*,uintptr_t)=nullptr;
static uint32_t shot_random=0;
static uint32_t next_shot_random() {
    if(!shot_random)shot_random=uint32_t(std::chrono::steady_clock::now().time_since_epoch().count())|1;
    shot_random^=shot_random<<13;shot_random^=shot_random>>17;shot_random^=shot_random<<5;return shot_random;
}
static AxisResetFn axis_reset=nullptr;
static MethodInfo* axis_reset_method=nullptr;
static size_t free_x=0,free_y=0,free_orbitals=0,orbital_x=0;
static MethodInfo* camera_projection;
static MethodInfo* camera_view=nullptr;
static MethodInfo *camera_reset_view=nullptr,*camera_reset_culling=nullptr;
struct Matrix4 { float m[16]; };
#include "EspProjection.h"
// Solve the yaw/pitch of a roll-free camera so its viewport-center ray, which
// can differ from transform.forward with lens shift, points at the target.
static bool projection_aim_goal(Vector3 direction,const Matrix4& projection,float& yaw,float& pitch) {
    const float* p=projection.m;
    if(!std::isfinite(p[0])||!std::isfinite(p[5])||!std::isfinite(p[8])||!std::isfinite(p[9])||
       std::fabs(p[0])<.0001f||std::fabs(p[5])<.0001f||std::fabs(p[11]+1.f)>.001f||std::fabs(p[15])>.001f)return false;
    float x=p[8]/p[0],y=p[9]/p[5],z=1.f;
    float ray_length=std::sqrt(x*x+y*y+z*z),distance=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
    if(!std::isfinite(ray_length)||!std::isfinite(distance)||distance<.1f)return false;
    x/=ray_length;y/=ray_length;z/=ray_length;
    float yz=std::hypot(y,z),sine=direction.y/(distance*yz);
    if(!std::isfinite(sine)||std::fabs(sine)>1.f)return false;
    float pitch_radians=std::atan2(y,z)-std::asin(sine);
    float yaw_radians=std::atan2(direction.x,direction.z)-std::atan2(x,y*std::sin(pitch_radians)+z*std::cos(pitch_radians));
    constexpr float degrees=57.295779513f;
    yaw=std::remainder(yaw_radians*degrees,360.f);pitch=pitch_radians*degrees;
    return std::isfinite(yaw)&&std::isfinite(pitch);
}
// All cached managed objects are rooted explicitly and used on the Unity thread.
// No GC calls from global destructors or the overlay render thread.
struct MeshBoneCache {
    GCHandle renderer=0,bones=0,info=0;
    GCHandle geometry_mesh=0;
    bool palette_dirty=true;
    uintptr_t geometry_renderer=0,geometry_info=0;
    bool geometry_attempted=false,geometry_ready=false;
    Vector3 head_center{};
    int indices[19];
    std::chrono::steady_clock::time_point bone_retry[19]{},palette_retry{};
    std::vector<std::string> names;
    std::vector<uintptr_t> named_bones;
    MeshBoneCache(){std::fill(std::begin(indices),std::end(indices),-2);}
};
struct ActorCache {
    GCHandle owner=0,renderers=0,animator=0,bones[19]={};
    MeshBoneCache mesh[2];
    Vector3 posed_bones[19]{},posed_center{};
    uint32_t posed_mask=0;
    bool posed_center_ready=false;
    unsigned posed_room=0;
    std::chrono::steady_clock::time_point posed_at{},posed_body_at{},posed_times[19]{};
    uintptr_t skin=0,skin_info=0;
    std::string display_name;std::chrono::steady_clock::time_point name_at{};
    bool friend_known=false,is_friend=false;
    uint32_t friend_entity=0;uintptr_t friend_client=0;
    std::chrono::steady_clock::time_point friend_checked{};
    std::chrono::steady_clock::time_point refreshed{};
    std::chrono::steady_clock::time_point renderers_retry{};
    uintptr_t pose_owner=0,pose_solvers[2]{};
    std::chrono::steady_clock::time_point pose_links_at{};
};
// Lives only during one read-only post-IK sampling callback, never between frames.
struct PoseRigReads {uintptr_t player=0,info=0,object=0,references=0;ActorCache* actor=nullptr;bool checked=false,active=false;};
static PoseRigReads* pose_rig_reads=nullptr;
static std::unordered_map<uintptr_t,ActorCache> actor_cache;
static std::unordered_map<uintptr_t,uintptr_t> pose_solver_owners;
static void release_pose_links(ActorCache& c) {
    for(auto solver:c.pose_solvers){
        auto it=pose_solver_owners.find(solver);
        if(it!=pose_solver_owners.end()&&it->second==c.pose_owner)pose_solver_owners.erase(it);
    }
    c.pose_owner=0;c.pose_solvers[0]=c.pose_solvers[1]=0;c.pose_links_at={};
}
using PoseUpdateFn=void (*)(void*,const MethodInfo*);
static std::atomic<PoseUpdateFn> pose_original{nullptr};
static std::atomic<unsigned> pose_sample_count{0};
static void release_handle(GCHandle& h) { if(h&&gchandle_free) gchandle_free(h); h=0; }
static void release_actor(ActorCache& c,bool keep_pose=false) {
    if(keep_pose){
        // Revalidate components without discarding a live rig's warm palette.
        release_handle(c.renderers);c.renderers_retry={};
        for(auto& mesh:c.mesh)mesh.palette_dirty=true;
        return;
    }
    c.display_name.clear();c.name_at={};c.posed_body_at={};
    if(!keep_pose)release_pose_links(c);
    if(!keep_pose){c.posed_mask=0;c.posed_center_ready=false;c.posed_at={};}
    c.friend_known=false;c.friend_checked={};
    release_handle(c.renderers); release_handle(c.animator);
    c.renderers_retry={};
    for(auto& h:c.bones) release_handle(h);
    for(auto& mesh:c.mesh){
        release_handle(mesh.renderer);release_handle(mesh.bones);release_handle(mesh.info);
        if(!keep_pose){release_handle(mesh.geometry_mesh);mesh.geometry_attempted=mesh.geometry_ready=false;mesh.geometry_renderer=mesh.geometry_info=0;}
        std::fill(std::begin(mesh.indices),std::end(mesh.indices),-2);mesh.names.clear();mesh.named_bones.clear();
        std::fill(std::begin(mesh.bone_retry),std::end(mesh.bone_retry),std::chrono::steady_clock::time_point{});mesh.palette_retry={};
    }
    release_handle(c.owner);
}
static void clear_actor_cache() { for(auto& entry:actor_cache) release_actor(entry.second); actor_cache.clear();pose_solver_owners.clear(); }
static uintptr_t rooted(GCHandle h) { return h&&gchandle_get_target?(uintptr_t)gchandle_get_target(h):0; }
static void root(GCHandle& h,uintptr_t object) {
    release_handle(h); if(object&&gchandle_new) h=gchandle_new((Il2CppObject*)object,false);
}

enum class TargetReject { None, Inactive, NoHealth, Dead, NotRendered };
static constexpr TargetReject target_rule(bool active,bool health_known,int hp,bool dead,bool rendered) {
    return !active?TargetReject::Inactive:!health_known?TargetReject::NoHealth:
           (dead||hp<=0)?TargetReject::Dead:!rendered?TargetReject::NotRendered:TargetReject::None;
}
static void phase(const char* message) {
    apk_trace::record(message);
    std::lock_guard<std::mutex> lock(state_mutex);
    std::snprintf(status.initialization,sizeof(status.initialization),"%s",message);
    LOGI(MOD_LABEL " init: %s",message);
}
template<class T> static bool read(uintptr_t address,T& result) {
    if(address<0x10000 || address>UINTPTR_MAX-sizeof(T)) return false;
    iovec local{&result,sizeof(T)}, remote{(void*)address,sizeof(T)};
    return syscall(SYS_process_vm_readv,getpid(),&local,1,&remote,1,0)==sizeof(T);
}
static uintptr_t pointer(uintptr_t object,size_t offset) {
    uintptr_t result=0; if(object) read(object+offset,result); return result;
}
static bool finite(Vector3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
static Vector3 sub(Vector3 a,Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
static float length(Vector3 v) { return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
static MethodInfo* method(const char* ns,const char* cls,const char* name,int args) {
    auto k=find_class_any(ns,cls); return k?class_get_method(k,name,args):nullptr;
}
static Il2CppObject* invoke(MethodInfo* m,void* object,void** args,CheatStatus& current) {
    if(!m) return nullptr;
    Il2CppException* exception=nullptr;
    auto result=runtime_invoke(m,object,args,&exception);
    if(exception) { ++current.invoke_errors; return nullptr; } return result;
}
template<class T> static bool value(MethodInfo* m,void* object,void** args,T& result,CheatStatus& current) {
    auto boxed=invoke(m,object,args,current);
    void* raw=boxed?object_unbox(boxed):nullptr;
    if(!raw) return false;
    std::memcpy(&result,raw,sizeof(T)); return true;
}
static bool position(uintptr_t transform,Vector3& result,CheatStatus& current) {
    return transform&&pointer(transform,0x10)&&value(get_position,(void*)transform,nullptr,result,current)&&finite(result);
}
static void refresh_pose_links(uintptr_t player,ActorCache& c) {
    const auto now=std::chrono::steady_clock::now();
    if(c.pose_owner==player&&c.pose_links_at.time_since_epoch().count()&&now-c.pose_links_at<std::chrono::milliseconds(50))return;
    const auto body=pointer(pointer(player,0xD8),0x38),aim=pointer(pointer(player,0xD0),0x68);
    const uintptr_t solvers[]={pointer(body,0x48),pointer(aim,0x40)};
    for(int i=0;i<2;++i){
        const auto previous=c.pose_solvers[i];
        if(previous&&(previous!=solvers[i]||c.pose_owner!=player)){
            auto it=pose_solver_owners.find(previous);
            if(it!=pose_solver_owners.end()&&it->second==c.pose_owner)pose_solver_owners.erase(it);
        }
        c.pose_solvers[i]=solvers[i];
        if(solvers[i]&&(pose_solver_owners.size()<2048||pose_solver_owners.count(solvers[i])))pose_solver_owners[solvers[i]]=player;
    }
    c.pose_owner=player;c.pose_links_at=now;
}
static ActorCache& cache_for(uintptr_t player) {
    if(pose_rig_reads&&pose_rig_reads->player==player)return *pose_rig_reads->actor;
    auto& c=actor_cache[player];
    auto now=std::chrono::steady_clock::now();
    uintptr_t skin=pointer(player,0x48);
    const auto info=pointer(skin,0x88);
    // Refresh component references when skins change, and periodically to catch
    // replacement renderers. This never removes a stationary actor from ESP.
    if(!c.owner||c.skin!=skin||c.skin_info!=info||std::chrono::duration<float>(now-c.refreshed).count()>1) {
        const bool keep_pose=rooted(c.owner)==player&&c.skin==skin&&c.skin_info==info&&c.posed_room==room_epoch;
        release_actor(c,keep_pose);root(c.owner,player);c.skin=skin;c.skin_info=info;c.refreshed=now;
    }
    refresh_pose_links(player,c);
    return c;
}
struct PoseRigScope {
    PoseRigReads reads;PoseRigReads* previous=pose_rig_reads;
    explicit PoseRigScope(uintptr_t player){
        auto& actor=cache_for(player);reads.player=player;reads.actor=&actor;reads.info=actor.skin_info;
        reads.object=pointer(reads.info,0x30);reads.references=pointer(reads.info,0xE0);pose_rig_reads=&reads;
    }
    ~PoseRigScope(){pose_rig_reads=previous;}
};
static std::string utf8(uintptr_t address) {
    if(!address||!string_length||!string_chars) return {};
    auto str=(Il2CppString*)address;
    int n=string_length(str); if(n<0||n>1024) return {};
    const uint16_t* chars=string_chars(str); if(!chars) return {};
    std::string result;
    for(int i=0;i<n;++i) {
        uint32_t c=chars[i];
        if(c>=0xD800&&c<=0xDBFF&&i+1<n&&chars[i+1]>=0xDC00&&chars[i+1]<=0xDFFF) c=0x10000+((c-0xD800)<<10)+(chars[++i]-0xDC00);
        else if(c>=0xD800&&c<=0xDFFF) c=0xFFFD;
        if(c<32) c=' ';
        if(c<0x80) result+=char(c);
        else if(c<0x800) { result+=char(0xC0|(c>>6)); result+=char(0x80|(c&63)); }
        else if(c<0x10000) { result+=char(0xE0|(c>>12)); result+=char(0x80|((c>>6)&63)); result+=char(0x80|(c&63)); }
        else { result+=char(0xF0|(c>>18)); result+=char(0x80|((c>>12)&63)); result+=char(0x80|((c>>6)&63)); result+=char(0x80|(c&63)); }
    }
    return result;
}
static std::string plain_text(const std::string& input) {
    std::string result; bool tag=false;
    for(char c:input) { if(c=='<') tag=true; else if(c=='>') tag=false; else if(!tag) result+=c; }
    return result;
}
static MethodInfo* instance_method(uintptr_t object,const char* name,int args=0) {
    for(auto k=(Il2CppClass*)pointer(object,0);k;k=class_get_parent?class_get_parent(k):nullptr)
        if(auto m=class_get_method(k,name,args))return m;
    return nullptr;
}
static void* instance_self(uintptr_t object) {
    auto k=(Il2CppClass*)pointer(object,0);
    return k&&class_is_value&&class_is_value(k)?object_unbox((Il2CppObject*)object):(void*)object;
}
static void update_roster(uintptr_t local,CheatStatus& current) {
    static std::chrono::steady_clock::time_point last{};
    auto now=std::chrono::steady_clock::now();
    if(std::chrono::duration<float>(now-last).count()<1)return;
    bool required=roster_requested.exchange(false,std::memory_order_relaxed)||Cheat_GetAimbot().body_headshot;
    {std::lock_guard<std::mutex> lock(state_mutex);required=required||!excluded_accounts.empty();}
    if(!required)return;
    CostScope roster_cost(cost_meters[4]);
    last=now;
    std::vector<SessionPlayer> fresh;
    std::unordered_map<uint32_t,std::string> ids;
    bool valid=false;
    auto dict=invoke(session_dictionary,nullptr,nullptr,current);
    GCHandle dict_root=dict&&gchandle_new?gchandle_new(dict,false):0;
    auto enumerate=dict?instance_method((uintptr_t)dict,"GetEnumerator"):nullptr;
    auto iterator=enumerate?invoke(enumerate,instance_self((uintptr_t)dict),nullptr,current):nullptr;
    GCHandle iterator_root=iterator&&gchandle_new?gchandle_new(iterator,false):0;
    auto move=iterator?instance_method((uintptr_t)iterator,"MoveNext"):nullptr;
    auto get_current=iterator?instance_method((uintptr_t)iterator,"get_Current"):nullptr;
    if(iterator&&move&&get_current&&class_is_value) {
        valid=true;uint32_t own=0;read(local+0x20,own);
        for(int i=0;i<1024;++i) {
            bool more=false;
            if(!value(move,instance_self((uintptr_t)iterator),nullptr,more,current)){valid=false;break;}
            if(!more)break;
            auto pair=invoke(get_current,instance_self((uintptr_t)iterator),nullptr,current);
            static Il2CppClass* pair_class=nullptr;static MethodInfo *key=nullptr,*val=nullptr;
            const auto klass=pair?(Il2CppClass*)pointer((uintptr_t)pair,0):nullptr;
            if(klass!=pair_class||!key||!val){
                pair_class=klass;key=pair?instance_method((uintptr_t)pair,"get_Key"):nullptr;
                val=pair?instance_method((uintptr_t)pair,"get_Value"):nullptr;
            }
            if(!pair){key=val=nullptr;}
            uint32_t entity=0;
            if(!key||!val||!value(key,instance_self((uintptr_t)pair),nullptr,entity,current)){valid=false;break;}
            auto packet=invoke(val,instance_self((uintptr_t)pair),nullptr,current);
            auto raw=packet?(uintptr_t)object_unbox(packet):0;
            if(!raw){valid=false;break;}
            auto name=plain_text(utf8(pointer(raw,8))),account=utf8(pointer(raw,0));
            SessionPlayer entry;entry.entity=entity;entry.is_local=entity==own;
            std::snprintf(entry.name,sizeof(entry.name),"%s",name.empty()?"Jogador":name.c_str());
            if(account.size()>=sizeof(entry.account))account.clear();
            std::snprintf(entry.account,sizeof(entry.account),"%s",account.c_str());
            if(!account.empty())ids[entity]=account;
            fresh.push_back(entry);
        }
        if(auto dispose=instance_method((uintptr_t)iterator,"Dispose"))invoke(dispose,instance_self((uintptr_t)iterator),nullptr,current);
    }
    if(iterator_root)gchandle_free(iterator_root);if(dict_root)gchandle_free(dict_root);
    std::sort(fresh.begin(),fresh.end(),[](const SessionPlayer& a,const SessionPlayer& b) {
        int order=std::strcmp(a.name,b.name);return order?order<0:a.entity<b.entity;
    });
    if(valid)session_accounts=std::move(ids);else session_accounts.clear();
    std::lock_guard<std::mutex> lock(state_mutex);
    roster_ready=valid;roster=valid?std::move(fresh):std::vector<SessionPlayer>{};
}
static bool manually_excluded(uintptr_t player,CheatStatus& current) {
    {std::lock_guard<std::mutex> lock(state_mutex);if(excluded_accounts.empty())return false;}
    uint32_t entity=0;if(!read(player+0x20,entity)||!entity)return false;
    std::string account;
    auto it=session_accounts.find(entity);
    if(it!=session_accounts.end())account=it->second;
    else {
        auto api=invoke(player_api_get,nullptr,nullptr,current);
        alignas(8) uint8_t packet[0x30]{};bool found=false;void* args[]={&entity,packet};
        if(api&&player_packet&&value(player_packet,api,args,found,current)&&found) {
            uintptr_t identity=0;std::memcpy(&identity,packet,sizeof(identity));
            GCHandle identity_root=0;root(identity_root,identity);
            account=utf8(rooted(identity_root));release_handle(identity_root);
        }
    }
    std::lock_guard<std::mutex> lock(state_mutex);
    // Unknown account with active exclusions must not accidentally acquire an excluded player.
    return account.empty()||excluded_accounts.count(account)!=0;
}
static void label(uintptr_t player,TargetData& target,CheatStatus& current) {
    auto& cached=cache_for(player);const auto now=std::chrono::steady_clock::now();
    if(cached.display_name.empty()||now-cached.name_at>=std::chrono::milliseconds(500)){
    uintptr_t canvas=pointer(player,0x80);
    std::string nick=plain_text(utf8(pointer(canvas,0x228)));
    if(nick.empty()) {
        uintptr_t tmp=pointer(canvas,0xD0);
        if(tmp&&nickname_text) nick=plain_text(utf8((uintptr_t)invoke(nickname_text,(void*)tmp,nullptr,current)));
    }
    if(nick.empty()) nick="Player";
    cached.display_name=std::move(nick);cached.name_at=now;
    }
    std::snprintf(target.name,sizeof(target.name),"%s | %.0fm",cached.display_name.c_str(),target.distance);
}
static bool active_object(uintptr_t object,CheatStatus& current) {
    bool active=false;
    return object&&pointer(object,0x10)&&is_active&&
           value(is_active,(void*)object,nullptr,active,current)&&active;
}
static bool mesh_rendered(uintptr_t player,CheatStatus& current,bool visible_required=true) {
    if((!child_renderers&&!components_internal)||!get_game_object||!renderer_type||!renderer_visible||!renderer_enabled) return false;
    auto& cached=cache_for(player);
    const auto now=std::chrono::steady_clock::now();
    auto array=(Il2CppArray*)rooted(cached.renderers);
    if(!array) {
    if(now<cached.renderers_retry)return false;
    cached.renderers_retry=now+std::chrono::milliseconds(50);
    bool include_inactive=false;
    uintptr_t game_object=(uintptr_t)invoke(get_game_object,(void*)player,nullptr,current);
    if(!game_object||!pointer(game_object,0x10)) return false;
    if(child_renderers) {
        void* args[]={renderer_type,&include_inactive};
        array=(Il2CppArray*)invoke(child_renderers,(void*)game_object,args,current);
    } else {
        bool typed_array=false,recursive=true,reverse=false;
        void* args[]={renderer_type,&typed_array,&recursive,&include_inactive,&reverse,nullptr};
        array=(Il2CppArray*)invoke(components_internal,(void*)game_object,args,current);
    }
    root(cached.renderers,(uintptr_t)array);
    }
    unsigned count=array?std::min<unsigned>(array_length(array),96):0;
    bool enabled_renderer=false;
    for(unsigned i=0;i<count;++i) {
        uintptr_t renderer=pointer((uintptr_t)array,0x20+i*sizeof(uintptr_t));
        if(!renderer||!pointer(renderer,0x10)) continue;
        bool enabled=false,visible=false,forced_off=false;
        if(!value(renderer_enabled,(void*)renderer,nullptr,enabled,current)||!enabled) continue;
        if(renderer_forced_off&&(!value(renderer_forced_off,(void*)renderer,nullptr,forced_off,current)||forced_off)) continue;
        enabled_renderer=true;
        if(!visible_required)return true;
        if(value(renderer_visible,(void*)renderer,nullptr,visible,current)&&visible) return true;
    }
    if(!enabled_renderer)release_handle(cached.renderers);
    return false;
}
static TargetReject player_state(uintptr_t player,TargetData* data,CheatStatus& current,bool check_mesh=true) {
    bool active=active_object(player,current);
    uintptr_t health=pointer(player,0x68);
    struct HealthFields { int16_t maximum,current; uint16_t unused; uint8_t dead,pad; } fields{};
    int16_t hp=0,max_hp=0; uint8_t dead=0;
    // Verified setters: 0x6A is current health; 0x68 is maximum health.
    // The 0x6E flag is set when current health falls below one.
    bool known=health&&pointer(health,0x10)&&read(health+0x68,fields);
    hp=fields.current; max_hp=fields.maximum; dead=fields.dead;
    if(data) { data->hp=hp; data->maxHp=max_hp>0&&hp<=max_hp?max_hp:0;
        int16_t armor=0;if(known&&read(health+0x6C,armor)&&armor>0)data->armor=armor;
    }
    auto initial=target_rule(active,known,hp,dead!=0,true);
    if(initial!=TargetReject::None) return initial;
    return target_rule(true,true,hp,false,!check_mesh||mesh_rendered(player,current));
}
enum class AimReject {None,Passive,Invulnerable,Friend,Excluded,Unknown};
static constexpr AimReject aim_filter(bool mask_known,uint32_t mask,bool friend_known,bool is_friend,bool passive,bool invulnerable,bool friends) {
    if((passive||invulnerable)&&!mask_known)return AimReject::Unknown;
    if(passive&&(mask&1))return AimReject::Passive;
    if(invulnerable&&(mask&2))return AimReject::Invulnerable;
    if(friends&&!friend_known)return AimReject::Unknown;
    return friends&&is_friend?AimReject::Friend:AimReject::None;
}
static AimReject aim_eligibility(uintptr_t player,const AimbotTune& tune,CheatStatus& current) {
    if(!player||!pointer(player,0x10))return AimReject::Unknown;
    uint32_t mask=0;bool mask_known=false;
    if(!tune.ignore_passive&&!tune.ignore_invulnerable)mask_known=true;
    else if(pointer(pointer(player,0x40),0x10)&&pointer(pointer(player,0x100),0x10))
        mask_known=value(hit_status,(void*)player,nullptr,mask,current);
    bool known=true,friend_value=false;
    if(tune.ignore_friends) {
        auto& c=cache_for(player);auto now=std::chrono::steady_clock::now();
        uintptr_t session=0;if(client_session_field)field_static_get(client_session_field,&session);
        const auto client=pointer(session,0x38);
        uint32_t entity=0;read(player+0x20,entity);
        if(c.friend_entity!=entity||c.friend_client!=client||c.friend_checked.time_since_epoch().count()==0||
           std::chrono::duration<float>(now-c.friend_checked).count()>=.5f) {
            c.friend_checked=now;c.friend_known=false;c.is_friend=false;
            c.friend_entity=entity;c.friend_client=client;
            const auto api=(uintptr_t)invoke(player_api_get,nullptr,nullptr,current);
            if(entity&&api&&client&&player_packet&&friend_contains) {
                alignas(8) uint8_t packet[0x30]{};bool found=false;
                void* args[]={&entity,packet};
                if(value(player_packet,(void*)api,args,found,current)&&found) {
                    uintptr_t account=0;std::memcpy(&account,packet,sizeof(account));
                    GCHandle account_root=0;root(account_root,account);
                    if(account_root&&string_length((Il2CppString*)rooted(account_root))>0) {
                        void* friend_args[]={(void*)rooted(account_root)};
                        c.friend_known=value(friend_contains,(void*)client,friend_args,c.is_friend,current);
                    }
                    release_handle(account_root);
                }
            } else if(!entity)c.friend_known=true;
        }
        known=c.friend_known;friend_value=c.is_friend;
    }
    if(manually_excluded(player,current))return AimReject::Excluded;
    return aim_filter(mask_known,mask,known,friend_value,tune.ignore_passive,tune.ignore_invulnerable,tune.ignore_friends);
}
static uintptr_t character_animator(uintptr_t player,CheatStatus& current) {
    // SkinChanger owns the visible skin. WeaponAnimations may keep another rig.
    uintptr_t animator=pointer(pointer(player,0x48),0x38);
    if(!animator) animator=pointer(pointer(player,0xD0),0x60);
    if(!animator&&child_animator&&animator_type) {
        bool inactive=false; void* args[]={animator_type,&inactive};
        animator=(uintptr_t)invoke(child_animator,(void*)player,args,current);
    }
    return animator&&pointer(animator,0x10)?animator:0;
}
static uintptr_t skin_reference_bone(uintptr_t player,int bone,CheatStatus& current) {
    auto batch=pose_rig_reads&&pose_rig_reads->player==player?pose_rig_reads:nullptr;
    auto info=batch?batch->info:pointer(pointer(player,0x48),0x88);
    auto object=batch?batch->object:pointer(info,0x30),references=batch?batch->references:pointer(info,0xE0);
    if(!references||!object||!pointer(object,0x10)||!skin_object_active)return 0;
    bool active=false;
    if(batch&&batch->checked)active=batch->active;
    else {
        const bool ok=value(skin_object_active,(void*)object,nullptr,active,current);
        if(batch){batch->checked=true;batch->active=ok&&active;}
        if(!ok)return 0;
    }
    if(!active)return 0;
    // HumanBodyBones -> the active skin's explicit BipedReferences fields.
    static constexpr size_t offsets[19]={
        0x18,0x20,0x38,0x28,0x40,0x30,0x48,0,0,0,0x80,0,0,
        0x50,0x68,0x58,0x70,0x60,0x78
    };
    if(bone<0||bone>=19||!offsets[bone])return 0;
    auto transform=pointer(references,offsets[bone]);
    return transform&&pointer(transform,0x10)?transform:0;
}
static uintptr_t visible_bone(uintptr_t player,int bone,CheatStatus& current) {
    if(bone<0||bone>=19)return 0;
    auto reference=skin_reference_bone(player,bone,current);
    if(!reference||!mesh_get_bones||!object_get_name)return 0;
    auto info=pose_rig_reads&&pose_rig_reads->player==player?pose_rig_reads->info:pointer(pointer(player,0x48),0x88);auto& cache=cache_for(player);
    const auto now=std::chrono::steady_clock::now();
    // Resolve the bone from the actual renderer palette. A BipedReferences or
    // Animator transform can belong to the driver's rig rather than the mesh.
    for(int pass=0;pass<2;++pass){
        int slot=bone==10?pass:1-pass; // head mesh first for head; body otherwise
        auto renderer=pointer(info,slot==0?0xA0:0x98);
        bool enabled=false;
        if(!renderer||!pointer(renderer,0x10)||!value(renderer_enabled,(void*)renderer,nullptr,enabled,current)||!enabled)continue;
        auto& mesh=cache.mesh[slot];
        if(rooted(mesh.renderer)!=renderer||rooted(mesh.info)!=info||!rooted(mesh.bones)||mesh.palette_dirty){
            const bool same=rooted(mesh.renderer)==renderer&&rooted(mesh.info)==info;
            if(same&&!mesh.palette_dirty&&now<mesh.palette_retry)continue;
            if(!same){mesh.names.clear();mesh.named_bones.clear();}
            mesh.palette_retry=now+std::chrono::milliseconds(50);
            auto bones=(uintptr_t)invoke(mesh_get_bones,(void*)renderer,nullptr,current);
            const auto previous=rooted(mesh.bones);
            bool changed=!same||!previous||!bones;
            const unsigned old_count=previous?array_length((Il2CppArray*)previous):0;
            const unsigned new_count=bones?array_length((Il2CppArray*)bones):0;
            if(old_count!=new_count||new_count>512)changed=true;
            if(!changed&&previous!=bones)for(unsigned i=0;i<new_count;++i)
                if(pointer(previous,0x20+i*sizeof(uintptr_t))!=pointer(bones,0x20+i*sizeof(uintptr_t))){changed=true;break;}
            mesh.palette_dirty=false;
            root(mesh.renderer,renderer);root(mesh.info,info);root(mesh.bones,bones);
            if(mesh.geometry_renderer!=renderer||mesh.geometry_info!=info) {
                release_handle(mesh.geometry_mesh);mesh.geometry_attempted=mesh.geometry_ready=false;
            }
            mesh.geometry_renderer=renderer;mesh.geometry_info=info;
            if(changed){
                std::fill(std::begin(mesh.indices),std::end(mesh.indices),-2);
                release_handle(mesh.geometry_mesh);mesh.geometry_attempted=mesh.geometry_ready=false;
                cache.posed_mask=0;cache.posed_center_ready=false;cache.posed_at={};
            }
            std::fill(std::begin(mesh.bone_retry),std::end(mesh.bone_retry),std::chrono::steady_clock::time_point{});
        }
        auto array=rooted(mesh.bones);unsigned count=array?array_length((Il2CppArray*)array):0;
        if(!count||count>512){release_handle(mesh.bones);continue;}
        int& index=mesh.indices[bone];
        if(index==-1&&now>=mesh.bone_retry[bone])index=-2;
        if(index==-2){
            index=-1;
            for(unsigned i=0;i<count;++i)if(pointer(array,0x20+i*sizeof(uintptr_t))==reference){index=int(i);break;}
            if(index<0){
                auto name=utf8((uintptr_t)invoke(object_get_name,(void*)reference,nullptr,current));
                if(!name.empty()){
                    if(mesh.names.size()!=count){mesh.names.resize(count);mesh.named_bones.resize(count);}
                    for(unsigned i=0;i<count;++i) {
                        auto t=pointer(array,0x20+i*sizeof(uintptr_t));
                        if(mesh.named_bones[i]!=t){mesh.named_bones[i]=t;mesh.names[i].clear();}
                        if(mesh.names[i].empty()) {
                            if(t&&pointer(t,0x10))mesh.names[i]=utf8((uintptr_t)invoke(object_get_name,(void*)t,nullptr,current));
                        }
                        if(mesh.names[i]==name){index=int(i);break;}
                    }
                }
            }
            if(index<0){
                mesh.bone_retry[bone]=now+std::chrono::milliseconds(50);
                // A skin can publish an empty/old palette before its head is
                // ready. Reacquire it on retry instead of caching that miss.
                if(bone==10)release_handle(mesh.bones);
            }
        }
        if(index>=0&&unsigned(index)<count){auto t=pointer(array,0x20+index*sizeof(uintptr_t));if(t&&pointer(t,0x10))return t;release_handle(mesh.bones);mesh.palette_retry=now+std::chrono::milliseconds(50);}
    }
    return 0;
}
static bool mesh_head_center(uintptr_t player,uintptr_t head,Vector3& result,CheatStatus& current) {
    if(!head||!mesh_shared||!mesh_vertices||!mesh_weights||!mesh_bindposes||!transform_point)return false;
    auto& actor=cache_for(player);
    for(auto& mesh:actor.mesh) {
        const auto renderer=rooted(mesh.renderer),bones=rooted(mesh.bones);
        const int index=mesh.indices[10];
        if(!renderer||!bones||index<0||unsigned(index)>=array_length((Il2CppArray*)bones)||
           pointer(bones,0x20+index*sizeof(uintptr_t))!=head)continue;
        const auto shared=(uintptr_t)invoke(mesh_shared,(void*)renderer,nullptr,current);
        if(!shared)continue;
        if(rooted(mesh.geometry_mesh)!=shared) {
            root(mesh.geometry_mesh,shared);mesh.geometry_attempted=mesh.geometry_ready=false;
        }
        if(!mesh.geometry_attempted) {
            mesh.geometry_attempted=true;
            GCHandle vertices=0,weights=0,bindposes=0;
            bool readable=true;
            if(mesh_readable&&!value(mesh_readable,(void*)shared,nullptr,readable,current))readable=false;
            if(readable) {
                root(vertices,(uintptr_t)invoke(mesh_vertices,(void*)shared,nullptr,current));
                root(weights,(uintptr_t)invoke(mesh_weights,(void*)shared,nullptr,current));
                root(bindposes,(uintptr_t)invoke(mesh_bindposes,(void*)shared,nullptr,current));
            }
            const auto v=rooted(vertices),w=rooted(weights),b=rooted(bindposes);
            const auto count=v?array_length((Il2CppArray*)v):0;
            Matrix4 bind{};
            if(count>=8&&count<=32768&&w&&array_length((Il2CppArray*)w)==count&&b&&
               unsigned(index)<array_length((Il2CppArray*)b)&&read(b+0x20+index*sizeof(Matrix4),bind)) {
                std::vector<Vector3> positions(count);
                std::vector<head_geometry::Weight> influences(count);
                auto copy=[](uintptr_t address,void* data,size_t bytes) {
                    iovec local{data,bytes},remote{(void*)address,bytes};
                    return syscall(SYS_process_vm_readv,getpid(),&local,1,&remote,1,0)==ssize_t(bytes);
                };
                if(copy(v+0x20,positions.data(),count*sizeof(Vector3))&&copy(w+0x20,influences.data(),count*sizeof(head_geometry::Weight))) {
                    head_geometry::Bounds bounds;
                    for(unsigned i=0;i<count;++i)head_geometry::add(bounds,positions[i],influences[i],index,bind.m);
                    mesh.geometry_ready=head_geometry::center(bounds,mesh.head_center);
                }
            }
            release_handle(vertices);release_handle(weights);release_handle(bindposes);
            // Non-readable meshes cannot expose CPU weights/vertices. Bake only
            // the dedicated head renderer into one reusable private mesh.
            const auto info=pointer(pointer(player,0x48),0x88);
            if(!mesh.geometry_ready&&renderer==pointer(info,0xA0)&&head_mesh_class&&head_mesh_ctor&&
               head_bake&&head_recalculate&&head_bounds&&head_inverse_point&&object_new) {
                if(!rooted(head_scratch_mesh)) {
                    auto scratch=object_new(head_mesh_class);
                    if(scratch) {
                        const int errors=current.invoke_errors;
                        invoke(head_mesh_ctor,scratch,nullptr,current);
                        if(current.invoke_errors==errors)root(head_scratch_mesh,(uintptr_t)scratch);
                    }
                }
                const auto scratch=rooted(head_scratch_mesh);
                if(scratch) {
                    const int errors=current.invoke_errors;bool scale=false;
                    void* args[]={(void*)scratch,&scale};
                    invoke(head_bake,(void*)renderer,args,current);
                    if(current.invoke_errors==errors)invoke(head_recalculate,(void*)scratch,nullptr,current);
                    struct MeshBounds {Vector3 center,extents;};MeshBounds bounds{};
                    const auto transform=(uintptr_t)invoke(get_transform,(void*)renderer,nullptr,current);
                    Vector3 world{},local{};void* world_args[]={&bounds.center};void* local_args[]={&world};
                    if(current.invoke_errors==errors&&transform&&value(head_bounds,(void*)scratch,nullptr,bounds,current)&&
                       finite(bounds.center)&&finite(bounds.extents)&&length(bounds.extents)>.001f&&
                       value(transform_point,(void*)transform,world_args,world,current)&&finite(world)&&
                       value(head_inverse_point,(void*)head,local_args,local,current)&&finite(local)) {
                        mesh.head_center=local;mesh.geometry_ready=true;
                    }
                }
            }
        }
        if(mesh.geometry_ready) {
            void* args[]={&mesh.head_center};
            Vector3 pivot{};
            if(value(transform_point,(void*)head,args,result,current)&&finite(result)&&position(head,pivot,current)&&
               length(sub(result,pivot))<=.4f)return true;
        }
    }
    return false;
}
static uintptr_t bone_transform(uintptr_t player,int bone,CheatStatus& current) {
    if(bone<0||bone>=19||!get_bone||!is_human) return 0;
    if(auto visible=visible_bone(player,bone,current))return visible;
    auto& c=cache_for(player);
    uintptr_t animator=character_animator(player,current);
    if(animator!=rooted(c.animator)) {
        release_handle(c.animator); for(auto& h:c.bones) release_handle(h);
        root(c.animator,animator);
    }
    if(!animator) return 0;
    uintptr_t transform=rooted(c.bones[bone]);
    if(transform&&pointer(transform,0x10)) return transform;
    bool human=false;
    if(!value(is_human,(void*)animator,nullptr,human,current)||!human) return 0;
    void* args[]={&bone}; transform=(uintptr_t)invoke(get_bone,(void*)animator,args,current);
    root(c.bones[bone],transform); return transform;
}
struct EspProjectionBatch {
    Matrix4 view{},projection{};uintptr_t camera=0;int width=0,height=0;bool valid=false;unsigned checked=0;
    void capture(uintptr_t next,int w,int h,CheatStatus& current){
        camera=next;width=w;height=h;checked=0;
        valid=camera_view&&value(camera_view,(void*)camera,nullptr,view,current)&&
              value(camera_projection,(void*)camera,nullptr,projection,current);
    }
};
static EspProjectionBatch* esp_projection_batch=nullptr;
struct EspProjectionScope {
    EspProjectionBatch batch;EspProjectionBatch* previous=esp_projection_batch;
    EspProjectionScope(){esp_projection_batch=&batch;}
    ~EspProjectionScope(){esp_projection_batch=previous;}
};
static bool camera_project(uintptr_t camera,Vector3 world,int width,int height,Vector3& screen,CheatStatus& current) {
    auto batch=esp_projection_batch;
    if(batch&&batch->valid&&batch->camera==camera&&batch->width==width&&batch->height==height){
        if(EspProjectMatrices(batch->view,batch->projection,world,width,height,screen)){
            // Validate against native projection at each phase. Unexpected
            // viewport/stereo/projection semantics fall back without persisting.
            if(batch->checked<3){
                Vector3 native{};void* args[]={&world};
                if(!value(world_to_screen,(void*)camera,args,native,current)||!finite(native)||
                   native.z<=.01f||std::hypot(native.x-screen.x,native.y-screen.y)>.5f||std::fabs(native.z-screen.z)>.02f)batch->valid=false;
                else ++batch->checked;
            }
            if(batch->valid){screen.x*=float(screen_w.load())/width;screen.y*=float(screen_h.load())/height;return true;}
        }
    }
    void* args[]={&world};
    if(!value(world_to_screen,(void*)camera,args,screen,current))return false;
    if(!finite(screen)||screen.z<=0.01f) return false;
    screen.x*=float(screen_w.load())/width; screen.y*=float(screen_h.load())/height;
    return true;
}
struct HeadSamples {Vector3 points[7]{};int count=0;};
static bool damage_head_point(uintptr_t player,Vector3& result,int& source,CheatStatus& current,HeadSamples* samples=nullptr) {
    source=0;
    // These are the actual damage shapes, including their local-space offsets.
    // raycastHeadPoint is a movement probe and does not follow the animated skull.
    if(child_animator&&hit_controller_type&&hit_manager&&transform_point) {
        bool inactive=false; void* component_args[]={hit_controller_type,&inactive};
        uintptr_t controller=(uintptr_t)invoke(child_animator,(void*)player,component_args,current);
        uintptr_t manager=controller&&pointer(controller,0x10)?(uintptr_t)invoke(hit_manager,(void*)controller,nullptr,current):0;
        if(manager&&pointer(manager,0x10)) {
            uintptr_t list=pointer(manager,0x20),items=pointer(list,0x10);
            int count=0; if(list) read(list+0x18,count);
            if(items&&count>0&&count<=64&&unsigned(count)<=array_length((Il2CppArray*)items)) {
                for(int i=0;i<count;++i) {
                    uintptr_t shape=pointer(items,0x20+i*sizeof(uintptr_t));
                    int damage_type=-1; if(!shape||!read(shape+0x10,damage_type)||damage_type!=0) continue;
                    auto klass=(Il2CppClass*)pointer(shape,0);
                    size_t offset=klass==sphere_class?0x24:klass==capsule_class?0x28:0;
                    float radius=0;
                    if(!read(shape+0x20,radius)||!std::isfinite(radius)||radius<=0)continue;
                    uintptr_t target=pointer(shape,0x18); Vector3 local_offset{};
                    if(!offset||!target||!pointer(target,0x10)||!read(shape+offset,local_offset)||!finite(local_offset)) continue;
                    // Use the HitShape target itself: offsetPos is expressed in
                    // this transform, not in a mesh bone with a similar name.
                    void* args[]={&local_offset};
                    if(value(transform_point,(void*)target,args,result,current)&&finite(result)) {
                        source=1;
                        if(samples) {
                            samples->count=1;samples->points[0]=result;
                            const float r=radius*.6f;
                            const Vector3 offsets[]={{r,0,0},{-r,0,0},{0,r,0},{0,-r,0},{0,0,r},{0,0,-r}};
                            for(auto offset:offsets) {
                                Vector3 local{local_offset.x+offset.x,local_offset.y+offset.y,local_offset.z+offset.z},world{};
                                void* sample_args[]={&local};
                                if(value(transform_point,(void*)target,sample_args,world,current)&&finite(world))samples->points[samples->count++]=world;
                            }
                        }
                        return true;
                    }
                }
            }
        }
    }
    return false;
}
static bool posed_point(uintptr_t player,int bone,Vector3& result,bool center=false) {
    auto it=actor_cache.find(player);if(it==actor_cache.end()||bone<0||bone>=19)return false;
    const auto& sample=it->second;
    const auto age=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-sample.posed_times[bone]).count();
    if(!PoseFresh(room_epoch,sample.posed_room,age,sample.posed_mask)||!(sample.posed_mask&(1u<<bone)))return false;
    if(center&&!sample.posed_center_ready)return false;
    result=center?sample.posed_center:sample.posed_bones[bone];return finite(result);
}
static bool animated_head(uintptr_t player,Vector3& result,int& source,CheatStatus& current) {
    // ESP follows the visible mesh; head aiming uses damage_head_point instead.
    source=0;
    if(posed_point(player,10,result)) {source=7;return true;}
    if(position(visible_bone(player,10,current),result,current)) { source=5; return true; }
    if(position(bone_transform(player,10,current),result,current)) { source=2; return true; }
    return damage_head_point(player,result,source,current);
}
static bool aim_point(uintptr_t player,int bone,Vector3& result,int& source,CheatStatus& current) {
    bone=std::clamp(bone,0,4);
    if(bone==0) return damage_head_point(player,result,source,current);
    source=0;
    if(posed_point(player,AimBoneIds[bone],result)) {source=7;return true;}
    if(position(visible_bone(player,AimBoneIds[bone],current),result,current)) { source=5; return true; }
    if(position(bone_transform(player,AimBoneIds[bone],current),result,current)) { source=3; return true; }
    if((bone==1||bone==2)&&position(bone_transform(player,7,current),result,current)) { source=3; return true; }
    return false;
}
static uintptr_t active_switch(uintptr_t camera,CheatStatus& current) {
    uintptr_t instance=0;
    if(switch_instance) field_static_get(switch_instance,&instance);
    if(!instance||pointer(instance,0x28)!=camera||!pointer(instance,0x10)) return 0;
    return instance;
}
static constexpr bool fire_weapon_allowed(int type) {return type==0||type==1||type==4||type==5||type==6||type==9;}
static constexpr bool automatic_gate(bool authenticated,bool enabled,bool auto_fire,bool alive,bool valid_target,bool visible,bool menu_closed,bool weapon) {
    return authenticated&&enabled&&auto_fire&&alive&&valid_target&&visible&&menu_closed&&weapon;
}
static bool line_visible(uintptr_t local,uintptr_t target,Vector3 origin,Vector3 point) {
    if(!physics_ray||!hit_transform||!is_child||!array_new||!ray_hit_class||!ray_hit_size)return false;
    CheatStatus scratch;
    if(!ray_buffer)root(ray_buffer,(uintptr_t)array_new(ray_hit_class,64));
    auto array=(Il2CppArray*)rooted(ray_buffer);if(!array)return false;
    auto delta=sub(point,origin);float distance=length(delta);
    if(!finite(delta)||distance<.05f)return false;
    auto local_root=invoke(get_transform,(void*)local,nullptr,scratch);
    auto target_root=invoke(get_transform,(void*)target,nullptr,scratch);
    if(!local_root||!target_root)return false;
    Vector3 direction{delta.x/distance,delta.y/distance,delta.z/distance};
    float max_distance=distance-.03f;int mask=-5,trigger_ignore=1,count=0;
    void* args[]={&origin,&direction,array,&max_distance,&mask,&trigger_ignore};
    if(!value(physics_ray,nullptr,args,count,scratch)||count<0||count>=64)return false;
    for(int i=0;i<count;++i) {
        auto raw=(void*)((uintptr_t)array+0x20+i*ray_hit_size);
        auto transform=invoke(hit_transform,raw,nullptr,scratch);
        if(!transform)return false;
        bool own=false,target_hit=false;void* own_args[]={local_root};void* target_args[]={target_root};
        if(!value(is_child,transform,own_args,own,scratch)||!value(is_child,transform,target_args,target_hit,scratch))return false;
        if(!own&&!target_hit)return false;
    }
    return true;
}
static bool live_head_point(uintptr_t player,Vector3& result,int& source,CheatStatus& current) {
    source=0;
    // Read the active renderer, never an Animator/driver rig or a damage shape.
    // This is a live mesh read, not proof that this frame's IK has completed.
    const auto head=visible_bone(player,10,current);
    if(!position(head,result,current))return false;
    source=5;
    Vector3 center{};
    if(mesh_head_center(player,head,center,current)){result=center;source=6;}
    return true;
}
static bool mesh_aim_point(uintptr_t player,int bone,Vector3& result,int& source,CheatStatus& current,
                             Vector3 origin,Vector3 forward,const AimbotTune& tune) {
    if(bone!=0)return aim_point(player,bone,result,source,current);
    source=0;
    if(posed_point(player,10,result)) {
        source=7;
        Vector3 geometric{};
        if(posed_point(player,10,geometric,true)){result=geometric;source=8;}
    } else if(!live_head_point(player,result,source,current))return false;
    if(!tune.multipoint||!tune.require_visible||line_visible(observed_local,player,origin,result))return true;
    Vector3 neck{};float radius=.035f;
    if(posed_point(player,9,neck))radius=std::clamp(length(sub(result,neck))*.2f,.02f,.06f);
    const auto center=result;
    const Vector3 offsets[]={{radius,0,0},{-radius,0,0},{0,radius,0},{0,-radius,0},{0,0,radius},{0,0,-radius}};
    bool found=false;float best=-2;
    for(auto offset:offsets) {
        const Vector3 point{center.x+offset.x,center.y+offset.y,center.z+offset.z};
        const auto d=sub(point,origin);const float distance=length(d),f=length(forward);
        if(distance<.1f||distance>tune.distanceMax||f<.001f)continue;
        const float score=(d.x*forward.x+d.y*forward.y+d.z*forward.z)/(distance*f);
        if(score>best&&line_visible(observed_local,player,origin,point)){best=score;result=point;found=true;}
    }
    return found;
}

#include "HeadAim.inl"
static bool select_aim_point(uintptr_t player,int bone,Vector3& result,int& source,CheatStatus& current,
                             Vector3 origin,Vector3 forward,const AimbotTune& tune) {
    if(bone!=0)return aim_point(player,bone,result,source,current);
    const bool mesh_clear=mesh_aim_point(player,bone,result,source,current,origin,forward,tune);
    if(!source)return false; // Keep active-mesh readiness; never invent a head from the root.
    return verified_head_point(player,result,source,current,origin,tune,mesh_clear);
}

static constexpr bool prefer_target(int priority,float distance,int hp,float angle,float best_distance,int best_hp,float best_angle) {
    if(priority==1&&distance!=best_distance)return distance<best_distance;
    if(priority==2&&hp!=best_hp)return hp<best_hp;
    return angle<best_angle;
}
static void update_auto_fire(uintptr_t local,uintptr_t camera,Vector3 origin,Vector3 forward,const AimbotTune& tune,CheatStatus& current) {
    bool valid=false,visible=false;int type=-1;
    auto runtime=pointer(pointer(local,0x70),0x48);auto data=pointer(runtime,0x20);
    if(data)read(data+0x10,type);
    if(tune.enabled&&tune.auto_fire&&observed_alive&&!Menu_IsOpen()) {
        auto& selected=tune.silent?silent_tracking:tracking;auto player=selected_player(selected);
        Vector3 point{};int source=0;
        if(player_state(player,nullptr,current,false)==TargetReject::None&&aim_eligibility(player,tune,current)==AimReject::None&&
           select_aim_point(player,selected.bone<0?tune.bone:selected.bone,point,source,current,origin,forward,tune)) {
            auto d=sub(point,origin);float n=length(d),f=length(forward);
            float dot=(d.x*forward.x+d.y*forward.y+d.z*forward.z)/std::max(.0001f,n*f);
            valid=finite(d)&&n>.1f&&n<=tune.distanceMax&&std::acos(std::clamp(dot,-1.f,1.f))*57.295779513f<=tune.fov_deg*.5f;
            if(valid)visible=line_visible(local,player,origin,point);
        }
    }
    set_auto_fire(automatic_gate(Auth_Allowed(),tune.enabled,tune.auto_fire,observed_alive,valid,visible,!Menu_IsOpen(),fire_weapon_allowed(type)));
}
static constexpr float aim_blend_angle(float from,float to,float alpha) {
    float delta=to-from;
    if(delta>180.f)delta-=360.f;
    if(delta< -180.f)delta+=360.f;
    return from+delta*alpha;
}
static void aim_frame(AimTracking& tracking,uintptr_t local,uintptr_t camera,uintptr_t transform,
                      Vector3 origin,Vector3& forward,const AimbotTune& tune,CheatStatus& current) {
    current.locked=false;
    current.aim_error_pixels=-1;
    auto reason=[&](const char* text) { std::snprintf(current.aim_state,sizeof(current.aim_state),"%s",text); };
    auto now=std::chrono::steady_clock::now();
    float dt=tracking.updated.time_since_epoch().count()?std::chrono::duration<float>(now-tracking.updated).count():1.0f/60;
    if(tracking.local!=local) { clear_target(tracking); tracking.local=local; tracking.camera=tracking.rig=0; }
    if(!observed_alive) { clear_target(tracking); reason("Waiting for respawn"); return; }
    if(!tune.enabled) { clear_target(tracking); reason("Disabled"); return; }
    uintptr_t sw=active_switch(camera,current),rig=0;
    int aim_type=-1;
    if(sw&&switch_aim&&field_get_value) field_get_value((Il2CppObject*)sw,switch_aim,&aim_type);
    if(sw&&switch_rig&&field_get_value) field_get_value((Il2CppObject*)sw,switch_rig,&rig);
    current.aim_mode=aim_type;
    if(!sw||!rig||!set_rotation||!camera_reset_view||!camera_reset_culling) { tracking.active=false; reason("Active virtual camera unavailable"); return; }
    if(tune.require_aiming&&(aim_type<0||aim_type>2)) { tracking.active=false; reason("Aim state unavailable"); return; }
    if(tune.require_aiming&&aim_type==0) { tracking.active=false; reason("Waiting for aim input"); return; }
    uintptr_t player=selected_player(tracking);
    if(!player){tracking.active=false;return;} // Keep the acquisition-stage reason from tick.
    if(std::chrono::duration<float>(now-tracking.sampled).count()>0.15f) { clear_target(tracking); reason("Waiting for target refresh"); return; }
    Vector3 head{};
    if(player_state(player,nullptr,current,false)!=TargetReject::None) {
        clear_target(tracking); reason("Target dead, inactive or not rendered"); return;
    }
    auto filter=aim_eligibility(player,tune,current);
    if(filter!=AimReject::None) {clear_target(tracking);reason(filter==AimReject::Unknown?"Aim filter data unavailable":"Target excluded by aim filters");return;}
    int point_source=0;
    if(!tune.random_bones) tracking.bone=tune.bone;
    else if(tracking.bone<0||!(tune.bone_pool&(1u<<tracking.bone))||std::chrono::duration<float>(now-tracking.bone_changed).count()>=tune.bone_interval) {
        static uint32_t random=0;
        if(!random) random=uint32_t(now.time_since_epoch().count())|1u;
        random^=random<<13; random^=random>>17; random^=random<<5;
        tracking.bone=PickAimBone(tune.bone_pool,random,tracking.bone); tracking.bone_changed=now;
    }
    current.aim_bone=tracking.bone;
    if(!select_aim_point(player,tracking.bone,head,point_source,current,origin,forward,tune)) {
        bool head_selected=tracking.bone==0;
        clear_target(tracking); reason(point_source<0?"No clear native head ray":point_source?"Target ray blocked":head_selected?"Active head mesh unavailable":"Selected animated bone unavailable"); return;
    }
    current.aim_point=point_source;
    static uintptr_t gap_player=0;static unsigned gap_room=0;
    static std::chrono::steady_clock::time_point gap_at{};static float gap_cm=-1;
    if(gap_player!=player||gap_room!=room_epoch||now-gap_at>=std::chrono::milliseconds(250)){
        gap_player=player;gap_room=room_epoch;gap_at=now;gap_cm=-1;
        Vector3 mesh_position{},hs_reference{};int hs_source=0;
        if(tracking.bone==0&&(posed_point(player,10,mesh_position)||position(visible_bone(player,10,current),mesh_position,current))&&
           damage_head_point(player,hs_reference,hs_source,current))gap_cm=100.f*length(sub(hs_reference,mesh_position));
    }
    current.aim_rig_gap_cm=tracking.bone==0?gap_cm:-1;
    Vector3 direction=sub(head,origin); float distance=length(direction);
    if(!finite(direction)||distance<0.1f||distance>tune.distanceMax) { clear_target(tracking); reason("Target out of range"); return; }
    if(tune.require_visible&&!line_visible(local,player,origin,head)) {
        clear_target(tracking);reason("Target occluded or visibility unavailable");return;
    }
    if(tune.silent) {
        tracking.active=false; tracking.local=local; tracking.camera=camera; tracking.rig=rig;
        current.locked=(shoot_ready.load()&&cast_ready.load()); reason(current.locked?"Silent aim: target ready":"Silent aim hook unavailable"); return;
    }
    constexpr float degrees=57.295779513f;
    float yaw_goal=std::atan2(direction.x,direction.z)*degrees;
    float pitch_goal=-std::atan2(direction.y,std::hypot(direction.x,direction.z))*degrees;
    Matrix4 live_projection{};
    if(value(camera_projection,(void*)camera,nullptr,live_projection,current))
        projection_aim_goal(direction,live_projection,yaw_goal,pitch_goal);
    // Each update starts from the current camera pose, including native camera movement.
    const float current_yaw=std::atan2(forward.x,forward.z)*degrees;
    const float current_pitch=-std::atan2(forward.y,std::hypot(forward.x,forward.z))*degrees;
    const float alpha=tune.smoothing<=0.f?1.f:-std::expm1(-20.f*std::clamp(dt,0.f,.05f)/tune.smoothing);
    const bool continuous=tracking.active&&tracking.local==local&&tracking.camera==camera&&
                          tracking.rig==rig&&dt>0.f&&dt<=.15f;
    float base_yaw=current_yaw,base_pitch=current_pitch;
    if(continuous) {
        base_yaw=tracking.yaw+std::remainder(yaw_goal-tracking.goal_yaw,360.f);
        base_pitch=tracking.pitch+pitch_goal-tracking.goal_pitch;
    }
    tracking.yaw=aim_blend_angle(std::remainder(base_yaw,360.f),yaw_goal,alpha);
    tracking.pitch=base_pitch+(pitch_goal-base_pitch)*alpha;
    tracking.goal_yaw=yaw_goal;
    tracking.goal_pitch=pitch_goal;
    float yaw=tracking.yaw/degrees*0.5f,pitch=tracking.pitch/degrees*0.5f;
    Quaternion rotation{std::sin(pitch)*std::cos(yaw),std::cos(pitch)*std::sin(yaw),-std::sin(pitch)*std::sin(yaw),std::cos(pitch)*std::cos(yaw)};
    int before=current.invoke_errors;
    // Aim only changes orientation after the final camera callback. The game
    // retains control of the camera orbit, follow offset and collision position.
    // Resetting FreeLook's pose here recalculates its orbit axes and can move
    // the camera through nearby geometry on the next frame.
    void* rotation_args[]={&rotation};
    invoke(set_rotation,(void*)transform,rotation_args,current);
    if(current.invoke_errors==before) {
        invoke(camera_reset_view,(void*)camera,nullptr,current);
        invoke(camera_reset_culling,(void*)camera,nullptr,current);
    }
    tracking.active=current.invoke_errors==before;
    tracking.local=local; tracking.camera=camera; tracking.rig=rig;
    current.locked=tracking.active;
    if(tracking.active) {
        tracking.updated=now;
        forward={std::sin(2*yaw)*std::cos(2*pitch),-std::sin(2*pitch),std::cos(2*yaw)*std::cos(2*pitch)};
        Vector3 screen{};int width=0,height=0;void* point_args[]={&head};
        if(value(world_to_screen,(void*)camera,point_args,screen,current)&&screen.z>0&&
           value(screen_width,nullptr,nullptr,width,current)&&value(screen_height,nullptr,nullptr,height,current)&&width>0&&height>0)
            current.aim_error_pixels=std::hypot(screen.x-width*.5f,screen.y-height*.5f);
        reason("Target acquired; tracking");
    } else reason("Camera update invocation failed");
}
static void skeleton(uintptr_t player,TargetData& target,CheatStatus& current) {
    if(!g_cfg.esp_skeleton.load(std::memory_order_relaxed)||!get_bone||!is_human) return;
    for(int bone=0;bone<19;++bone) {
        if(bone==11||bone==12) continue; // shoulder joints are optional; connect upper arms to chest.
        Vector3 world{};
        if(!posed_point(player,bone,world)&&!position(bone_transform(player,bone,current),world,current))continue;
        target.world_bones[bone]=world;target.bone_mask|=1u<<bone;
    }
    // Chest and neck are optional on humanoid rigs. Use the nearest available torso joint.
    if(!(target.bone_mask&(1u<<8))&&(target.bone_mask&(1u<<7))) { target.world_bones[8]=target.world_bones[7]; target.bone_mask|=1u<<8; }
    if(!(target.bone_mask&(1u<<9))&&(target.bone_mask&(1u<<8))) { target.world_bones[9]=target.world_bones[8]; target.bone_mask|=1u<<9; }
    if(target.bone_mask) ++current.skeletons;
}
#include "NativeAimAssist.inl"
void Cheat_SetScreenSize(int w,int h) { if(w>0&&h>0) { screen_w=w; screen_h=h; } }
AimbotTune Cheat_GetAimbot() { std::lock_guard<std::mutex> lock(state_mutex); auto result=config;if(!Auth_Allowed())result.enabled=false;return result; }
CheatStatus Cheat_GetStatus() {
    std::lock_guard<std::mutex> lock(state_mutex);
    auto result=status;vehicle_status(result);
    for(int i=0;i<5;++i){result.cost_mean_us[i]=cost_meters[i].mean.load();result.cost_peak_us[i]=cost_meters[i].peak.load();}
    result.accuracy_ready=accuracy_original.load()!=nullptr;result.recoil_apply_ready=recoil_apply_original.load()!=nullptr;result.recoil_camera_impulses=recoil_camera_impulses.load();result.local_fire_ready=safe_shoot_original.load()!=nullptr;result.spread_overrides=spread_override_count.load();result.recoil_impulses=recoil_impulse_count.load();result.ammo_ready=ammo_ready.load();result.spread_ready=spread_ready.load();result.ammo_blocked=ammo_blocked.load();result.head_marks=head_marks.load();result.hs_checked=hs_checked.load();result.hs_promoted=hs_promoted.load();result.hs_missing=hs_missing.load();result.hs_steered=hs_steered.load();result.auto_fire_ready=fire_input_ready.load();result.auto_firing=auto_held;result.no_recoil_ready=recoil_ready.load();result.keybind_ready=keyboard_ready.load();result.camera_lock_ready=axis_ready.load(); result.shots=local_shots.load(); result.rays=local_rays.load(); result.redirects=redirected_rays.load(); result.silent_ready=shoot_ready.load()&&cast_ready.load();
    result.pose_ready=pose_original.load()!=nullptr;result.pose_samples=pose_sample_count.load();
#if !defined(JM_APK) && !defined(JM_HYBRID)
    result.native_assist_ready=native_assist_ready.load(std::memory_order_acquire);
    result.native_assist_active=native_assist_active.load(std::memory_order_acquire);
    result.native_assist_calls=native_assist_calls.load(std::memory_order_relaxed);
    result.native_assist_scaled=native_assist_scaled.load(std::memory_order_relaxed);
#endif
    for(int i=0;i<5;++i) { result.callback_counts[i]=callback_counts[i].load(); result.callback_calls+=result.callback_counts[i]; }
    return result;
}
void Cheat_SetAimbot(const AimbotTune& tune) {
    if(!std::isfinite(tune.cargo_step)||!std::isfinite(tune.rapid_multiplier))return;
    if(!std::isfinite(tune.spin_speed)||!std::isfinite(tune.model_scale))return;
    for(float color:tune.weapon_color)if(!std::isfinite(color))return;
    if(!std::isfinite(tune.move_speed)||!std::isfinite(tune.fly_speed)||!std::isfinite(tune.move_response))return;
    if(!std::isfinite(tune.vehicle_car_power)||!std::isfinite(tune.vehicle_bike_power)||!std::isfinite(tune.vehicle_boat_power))return;
    if(!std::isfinite(tune.silent_max_angle)||!std::isfinite(tune.hit_chance)||!std::isfinite(tune.bone_interval)||!std::isfinite(tune.distanceMax)||!std::isfinite(tune.fov_deg)||!std::isfinite(tune.smoothing)) return;
    if(!std::isfinite(tune.assist_x)||!std::isfinite(tune.assist_y)||!std::isfinite(tune.assist_distance)||
       !std::isfinite(tune.assist_radius_min)||!std::isfinite(tune.assist_radius_max))return;
    std::lock_guard<std::mutex> lock(state_mutex);
    auto revision=config.revision+uint64_t(config.enabled!=tune.enabled||config.silent!=tune.silent||config.multipoint!=tune.multipoint||config.target_priority!=tune.target_priority);
    config=tune;config.revision=revision;
    config.cargo_step=std::clamp(tune.cargo_step,2.f,4.f);
    config.spin_speed=std::clamp(tune.spin_speed,30.f,1080.f);config.model_scale=std::clamp(tune.model_scale,.05f,1.f);
    for(int i=0;i<3;++i)config.weapon_color[i]=std::clamp(tune.weapon_color[i],0.f,1.f);
    config.vehicle_car_power=std::clamp(tune.vehicle_car_power,1.f,3.f);
    config.vehicle_bike_power=std::clamp(tune.vehicle_bike_power,1.f,3.f);
    config.vehicle_boat_power=std::clamp(tune.vehicle_boat_power,1.f,3.f);
    config.move_response=std::clamp(tune.move_response,0.f,.4f);
    config.move_speed=std::clamp(tune.move_speed,2.f,40.f);config.fly_speed=std::clamp(tune.fly_speed,2.f,30.f);
    config.distanceMax=std::clamp(tune.distanceMax,10.0f,500.0f);
    config.fov_deg=std::clamp(tune.fov_deg,1.0f,360.0f);
    config.smoothing=std::clamp(tune.smoothing,0.0f,10.0f);
    config.assist_x=std::clamp(tune.assist_x,1.f,6.f);
    config.assist_y=std::clamp(tune.assist_y,1.f,6.f);
    config.assist_distance=std::clamp(tune.assist_distance,1.f,4.f);
    config.assist_radius_min=std::clamp(tune.assist_radius_min,1.f,3.f);
    config.assist_radius_max=std::clamp(tune.assist_radius_max,1.f,3.f);
    config.bone=std::clamp(tune.bone,0,4);
    config.target_priority=std::clamp(tune.target_priority,0,2);
    config.rapid_multiplier=std::clamp(tune.rapid_multiplier,1.f,5.f);
    config.hit_chance=std::clamp(tune.hit_chance,0.f,100.f);config.silent_max_angle=std::clamp(tune.silent_max_angle,1.f,15.f);
    config.bone_pool=ValidBonePool(tune.bone_pool);
    config.bone_interval=std::clamp(tune.bone_interval,0.2f,3.0f);
}
void Cheat_ToggleAimbot() {
    if(!Auth_Allowed())return;
    std::lock_guard<std::mutex> lock(state_mutex);
    config.enabled=!config.enabled;++config.revision;
}
// Poll only on Unity's thread. A held key produces exactly one toggle.
static constexpr bool toggle_key_edge(int previous_key,int key,bool previous_down,bool down) {
    return key>0&&previous_key==key&&down&&!previous_down;
}
static void poll_toggle_key(CheatStatus& scratch) {
    static int previous_keys[2]={-1,-1};static bool previous_down[2]={false,false};
    if(!keyboard_ready.load(std::memory_order_acquire))return;
    const int menu_key=Menu_GetMenuToggleKey();
    for(int i=0;i<2;++i) {
        int key=i==0?menu_key:Menu_GetToggleKey();
        if(key<=0||(i==1&&key==menu_key)){previous_keys[i]=key;previous_down[i]=false;continue;}
        bool down=false;void* args[]={&key};
        if(!value(keyboard_key,nullptr,args,down,scratch))continue;
        if(toggle_key_edge(previous_keys[i],key,previous_down[i],down)) {
            if(i==0)Menu_ToggleRequested();else Cheat_ToggleAimbot();
        }
        previous_keys[i]=key;previous_down[i]=down;
    }
}
int Cheat_GetSessionPlayers(SessionPlayer* out,int max,bool* ready) {
    roster_requested.store(true,std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(state_mutex);
    if(ready)*ready=roster_ready;
    if(!out||max<=0||!Auth_Allowed())return 0;
    int count=std::min<int>(max,roster.size());
    for(int i=0;i<count;++i){out[i]=roster[i];out[i].excluded=excluded_accounts.count(out[i].account)!=0;}
    return count;
}
void Cheat_SetExcluded(const char* account,bool exclude) {
    if(!account||!*account||std::strlen(account)>=128)return;
    std::lock_guard<std::mutex> lock(state_mutex);
    if(exclude)excluded_accounts.insert(account);else excluded_accounts.erase(account);
}
void Cheat_ClearExclusions() {std::lock_guard<std::mutex> lock(state_mutex);excluded_accounts.clear();}
int Cheat_GetTargets(TargetData* out,int max) {
    if(!out||max<=0||!Auth_Allowed()) return 0;
    std::lock_guard<std::mutex> lock(state_mutex);
    if(std::chrono::steady_clock::now()-snapshot_at>std::chrono::milliseconds(100))return 0;
    int count=std::min<int>(max,snapshot.size()); std::copy_n(snapshot.begin(),count,out); return count;
}
#include "Movement.inl"
#include "VehicleBoost.inl"
#include "CargoFarm.inl"
#include "Visuals.inl"
#include "ApkIntegrity.inl"
#ifndef JM_IOS
#include "InternalIntegrityProbe.inl"
#endif
static void tick(uintptr_t source,int kind) {
    CostScope total_cost(cost_meters[0]);
    CheatStatus key_scratch;poll_toggle_key(key_scratch);
#if !defined(JM_APK) && !defined(JM_HYBRID)
    native_assist_tick(Auth_Allowed(),Cheat_GetAimbot());
#endif
    if(!Auth_Allowed()) { CheatStatus movement_scratch;movement_tick(0,false,movement_scratch);vehicle_tick(0,false,movement_scratch);farm_tick(0,false,movement_scratch);visuals_tick(0,false,movement_scratch);update_weapon_mods(0,false);set_auto_fire(false); clear_all_targets(); std::lock_guard<std::mutex> lock(state_mutex); snapshot.clear(); status.locked=false; return; }
    auto now=std::chrono::steady_clock::now();
    static GCHandle room_session=0;static auto room_checked=std::chrono::steady_clock::time_point{};
    if(room_session_method&&std::chrono::duration<float>(now-room_checked).count()>.5f){
        room_checked=now;CheatStatus scratch;auto session=invoke(room_session_method,nullptr,nullptr,scratch);
        if(!scratch.invoke_errors&&(uintptr_t)session!=rooted(room_session)){
            root(room_session,(uintptr_t)session);++room_epoch;clear_all_targets();clear_actor_cache();last_brain={};
        }
    }
    // A destroyed/replaced Brain must not suppress the fallback forever.
    if(kind!=2&&std::chrono::duration<float>(now-last_brain).count()<0.1f) return;
    bool character=kind==0;
    CheatStatus current; current.hook_ready=true;
    std::snprintf(current.initialization,sizeof(current.initialization),"Update running");
    uintptr_t local=0; field_static_get(local_field,&local);
    current.local_ready=local!=0;
#ifdef JM_APK
    apk_integrity_observe(local);
#endif
    uintptr_t third=pointer(local,0x90);
    uintptr_t own_controller=pointer(third,0x70);
    uintptr_t controller=(character||kind==2)?own_controller:source;
    uintptr_t output=kind==2?(uintptr_t)invoke(brain_output,(void*)source,nullptr,current):0;
    uintptr_t main_camera=kind==2?(uintptr_t)invoke(camera_main,nullptr,nullptr,current):0;
    uintptr_t sw=0; if(switch_instance) field_static_get(switch_instance,&sw);
    uintptr_t switched_camera=pointer(sw,0x28);
    bool is_local=local&&(kind==2?primary_camera_output(output,switched_camera,main_camera):(character?source==third:own_controller==source));
    if(kind==2&&is_local) last_brain=now;
    if(!is_local) {
        if(!local) { movement_tick(0,false,current);vehicle_tick(0,false,current);farm_tick(0,false,current);visuals_tick(0,false,current);update_weapon_mods(0,false);set_auto_fire(false); clear_all_targets(); observed_local=0; observed_alive=false; clear_actor_cache(); session_accounts.clear(); }
        std::lock_guard<std::mutex> lock(state_mutex);
        if(!local || status.frames==0) {
            if(!local){roster.clear();roster_ready=false;}
            current.frames=frame_counter;
            std::snprintf(current.initialization,sizeof(current.initialization),"%s",!local?"Callback active; local static is null":"Callback active; other player/camera");
            snapshot.clear(); status=current;
        }
        return;
    }
    int expected=0;unity_thread.compare_exchange_strong(expected,int(syscall(SYS_gettid)));
    static bool fps_applied=false;static int saved_fps=-1;
    bool requested=Cheat_GetAimbot().fps120;
    if(fps_get&&fps_set&&requested!=fps_applied) {
        if(!requested||value(fps_get,nullptr,nullptr,saved_fps,current)) {
            int desired=requested?120:saved_fps;void* args[]={&desired};int errors=current.invoke_errors;
            invoke(fps_set,nullptr,args,current);if(errors==current.invoke_errors)fps_applied=requested;
        }
    }
    static auto last_sample=std::chrono::steady_clock::now()-std::chrono::seconds(1);
    bool alive=player_state(local,nullptr,current,false)==TargetReject::None;
    movement_tick(local,alive,current);
    vehicle_tick(local,alive,current);
    farm_tick(local,alive,current);
    visuals_tick(local,alive,current);
    update_weapon_mods(local,alive);
    bool reset_session=session_changed(observed_local,observed_alive,local,alive);
    if(reset_session) {
        set_auto_fire(false);clear_all_targets(); tracking=AimTracking{}; silent_tracking=AimTracking{}; tracking.local=silent_tracking.local=local;
        observed_local=local; observed_alive=alive; clear_actor_cache();
    }
    bool sample=reset_session||std::chrono::duration<float>(now-last_sample).count()>=1.0f/30;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        current.players=status.players; current.visible=status.visible;
        current.skeletons=status.skeletons; current.camera_fov=status.camera_fov;
        current.dead_filtered=status.dead_filtered; current.hidden_filtered=status.hidden_filtered;
        current.invoke_errors=status.invoke_errors;
        current.protected_targets=status.protected_targets;current.filter_unknown=status.filter_unknown;
    }
    current.frames=++frame_counter;
    uintptr_t camera=output?output:switched_camera?switched_camera:pointer(controller,0x110);
    if(!camera) camera=(uintptr_t)invoke(camera_main,nullptr,nullptr,current);
    current.camera_ready=camera!=0;
    auto publish=[&](const std::vector<TargetData>& targets) {
        std::lock_guard<std::mutex> lock(state_mutex); snapshot=targets;snapshot_at=std::chrono::steady_clock::now();
        status=current;
    };
    if(!camera) { set_auto_fire(false);clear_all_targets(); publish({}); return; }
    uintptr_t transform=(uintptr_t)invoke(get_transform,(void*)camera,nullptr,current);
    Vector3 origin{},forward{};
    if(!position(transform,origin,current)||!value(get_forward,(void*)transform,nullptr,forward,current)||!finite(forward)) { set_auto_fire(false);clear_all_targets(); publish({}); return; }
    static unsigned applied_room_epoch=0;bool room_changed=applied_room_epoch!=room_epoch;
    if(room_changed){applied_room_epoch=room_epoch;sample=true;clear_actor_cache();clear_all_targets();}
    AimbotTune tune=Cheat_GetAimbot();
    pose_demand_mask.store(PoseDemand(tune.enabled,g_cfg.esp_enabled,g_cfg.esp_skeleton.load(),tune.random_bones,tune.bone,ValidBonePool(tune.bone_pool)),std::memory_order_relaxed);
    pose_priority_mask.store(PoseDemand(tune.enabled,g_cfg.esp_enabled,false,tune.random_bones,tune.bone,ValidBonePool(tune.bone_pool)),std::memory_order_relaxed);
    pose_geometry_needed.store(tune.enabled&&(tune.random_bones?(tune.bone_pool&1u)!=0:tune.bone==0),std::memory_order_relaxed);
    pose_body_period_ms.store(1000/std::clamp(g_cfg.esp_hz.load(),30,60),std::memory_order_relaxed);
    static std::chrono::steady_clock::time_point last_esp{};
    const bool decorate=g_cfg.esp_enabled&&now-last_esp>=std::chrono::microseconds(1000000/std::clamp(g_cfg.esp_hz.load(),30,60));
    if(applied_revision!=tune.revision) {
        clear_all_targets();tracking=AimTracking{};silent_tracking=AimTracking{};
        applied_revision=tune.revision;sample=true;
    }
    auto& target_tracking=tune.silent?silent_tracking:tracking;
    if(!tune.enabled&&!decorate){
        set_auto_fire(false);clear_all_targets();update_roster(local,current);
        if(!g_cfg.esp_enabled){std::lock_guard<std::mutex> lock(state_mutex);snapshot.clear();}
        std::lock_guard<std::mutex> lock(state_mutex);status=current;return;
    }
    if(decorate||tune.enabled)sample=true;
    if(!sample) { update_auto_fire(local,camera,origin,forward,tune,current);std::lock_guard<std::mutex> lock(state_mutex); status=current; return; }
    last_sample=now;
    current.players=current.dead_filtered=current.hidden_filtered=0;
    if(decorate)current.visible=current.skeletons=0;
    current.protected_targets=current.filter_unknown=0;
    int width=0,height=0;
    value(screen_width,nullptr,nullptr,width,current); value(screen_height,nullptr,nullptr,height,current);
    if(width<=0) value(pixel_width,(void*)camera,nullptr,width,current);
    if(height<=0) value(pixel_height,(void*)camera,nullptr,height,current);
    value(camera_fov,(void*)camera,nullptr,current.camera_fov,current);
    if(width<=0||height<=0||screen_w<=0||screen_h<=0) { publish({}); return; }
    // Object discovery is shared across scans; eligibility and positions remain live.
    static GCHandle discovered=0;
    static std::chrono::steady_clock::time_point discovery_at{};
    if(reset_session||room_changed||std::chrono::duration<float>(now-discovery_at).count()>=0.125f||!discovered) {
        CostScope discovery_cost(cost_meters[2]);
        void* args[]={linker_type};
        auto found=invoke(find_objects,nullptr,args,current);
        root(discovered,(uintptr_t)found); discovery_at=now;
        // Prune by actual discovery membership, never by motion or age.
        auto live=(Il2CppArray*)rooted(discovered);
        unsigned n=live?std::min<unsigned>(array_length(live),512):0;
        uintptr_t members[512]{};
        iovec local_read{members,n*sizeof(uintptr_t)},remote_read{(void*)((uintptr_t)live+0x20),n*sizeof(uintptr_t)};
        if(live&&(!n||syscall(SYS_process_vm_readv,getpid(),&local_read,1,&remote_read,1,0)==ssize_t(n*sizeof(uintptr_t)))){
            std::unordered_set<uintptr_t> membership;membership.reserve(n);
            for(unsigned j=0;j<n;++j)membership.insert(members[j]);
            for(auto it=actor_cache.begin();it!=actor_cache.end();){
                if(!membership.count(it->first)){release_actor(it->second);it=actor_cache.erase(it);}else ++it;
            }
        }
    }
    EspProjectionScope projection_scope;
    if(decorate)projection_scope.batch.capture(camera,width,height,current);
    auto array=(Il2CppArray*)rooted(discovered);
    unsigned count=array?std::min<unsigned>(array_length(array),512):0;
    static std::vector<TargetData> targets;static std::vector<uintptr_t> target_owners;
    targets.clear();target_owners.clear();if(decorate){targets.reserve(64);target_owners.reserve(64);}
    unsigned pose_pending=0,renderer_pending=0,occluded=0;
    float best_angle=tune.fov_deg>=359?181.f:tune.fov_deg*.5f,best_distance=1e30f;int best_hp=32768;
    uintptr_t best_player=0,held=selected_player(target_tracking),retained=0;
    constexpr float degrees=57.295779513f;
    Vector3 retained_forward=forward;
    const bool use_applied_view=target_tracking.active&&target_tracking.local==local&&
        target_tracking.camera==camera&&now-target_tracking.updated<std::chrono::milliseconds(150);
    if(use_applied_view) {
        const float yaw=target_tracking.yaw/degrees,pitch=target_tracking.pitch/degrees;
        retained_forward={std::sin(yaw)*std::cos(pitch),-std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
    }
    for(unsigned i=0;i<count;++i) {
        uintptr_t player=pointer((uintptr_t)array,0x20+i*sizeof(uintptr_t));
        if(!player||player==local) continue;
        ++current.players; uintptr_t ct=pointer(player,0x90); TargetData target;
        auto eligibility=player_state(player,&target,current,false);
        if(eligibility!=TargetReject::None) {
            if(eligibility==TargetReject::Dead) ++current.dead_filtered;
            else ++current.hidden_filtered;
            continue;
        }
        int point_source=0;
        if(!animated_head(player,target.head,point_source,current)) {
            const auto transform=(uintptr_t)invoke(get_transform,(void*)player,nullptr,current);
            if(!position(transform,target.pos,current))continue;
            target.head={target.pos.x,target.pos.y+1.7f,target.pos.z};
        }
        if(!position(pointer(ct,0x90),target.pos,current)) target.pos={target.head.x,target.head.y-1.7f,target.head.z};
        auto direction=sub(target.head,origin); target.distance=length(direction);
        const float esp_distance=std::clamp(g_cfg.esp_distance.load(),10.f,1000.f);
        if(!std::isfinite(target.distance)||target.distance<0.1f||target.distance>std::max(esp_distance,tune.distanceMax))continue;
        bool on_screen=false;
        if(decorate){
            const bool head_projected=camera_project(camera,target.head,width,height,target.screen_head,current);
            const bool feet_projected=camera_project(camera,target.pos,width,height,target.screen_pos,current);
            on_screen=head_projected||feet_projected;
        }
        target.valid=on_screen;
        auto query=tune;
        query.ignore_friends=(tune.enabled&&tune.ignore_friends)||g_cfg.hide_friends.load();
        query.ignore_passive=(tune.enabled&&tune.ignore_passive)||g_cfg.hide_passive.load();
        query.ignore_invulnerable=(tune.enabled&&tune.ignore_invulnerable)||g_cfg.hide_invulnerable.load();
        auto filter=aim_eligibility(player,query,current);
        if(tune.enabled&&query.ignore_friends!=tune.ignore_friends)filter=aim_eligibility(player,tune,current);
        uint32_t mask=0;
        if((g_cfg.hide_passive.load()||g_cfg.hide_invulnerable.load())&&pointer(player,0x40)&&pointer(player,0x100))value(hit_status,(void*)player,nullptr,mask,current);
        target.passive=mask&1;target.invulnerable=mask&2;
        auto& cached=cache_for(player);target.friend_player=cached.friend_known&&cached.is_friend;target.friend_known=cached.friend_known;
        if(decorate&&on_screen&&target.distance<=esp_distance&&mesh_rendered(player,current,false)&&targets.size()<64){targets.push_back(target);target_owners.push_back(player);}
        if(!tune.enabled)continue;
        if(target.distance>tune.distanceMax)continue;
        // Unity renderer visibility and foot projection can lag the real posed
        // aim point. The live FOV/ray checks below decide acquisition instead.
        if(!mesh_rendered(player,current,false)){++renderer_pending;continue;}
        if(filter!=AimReject::None) {
            if(filter==AimReject::Unknown)++current.filter_unknown;else ++current.protected_targets;
            continue;
        }
        int selection_bone=tune.random_bones?(player==held&&target_tracking.bone>=0?target_tracking.bone:PickAimBone(tune.bone_pool,0,-1)):tune.bone;
        Vector3 selected_point{};int source=0;
        if(!mesh_aim_point(player,selection_bone,selected_point,source,current,origin,forward,tune)){
            if(source)++occluded;else ++pose_pending;
            continue;
        }
        direction=sub(selected_point,origin);
        const auto view=player==held&&use_applied_view?retained_forward:forward;
        float dot=(direction.x*view.x+direction.y*view.y+direction.z*view.z)/
            (std::max(0.001f,length(direction))*std::max(0.001f,length(view)));
        float angle=std::acos(std::clamp(dot,-1.0f,1.0f))*degrees;
        if(tune.fov_deg<359&&angle>=tune.fov_deg*.5f)continue;
        if(tune.require_visible&&(tune.target_priority!=0||player==held||angle<best_angle)) {
            if(!line_visible(local,player,origin,selected_point)){++occluded;continue;}
        }
        if((tune.fov_deg>=359||angle<tune.fov_deg*0.5f)&&player==held) retained=player;
        if(prefer_target(tune.target_priority,target.distance,target.hp,angle,best_distance,best_hp,best_angle)) {
            best_angle=angle;best_distance=target.distance;best_hp=target.hp;best_player=player;
        }

    }
    if(decorate)current.visible=targets.size();
    if(retained&&tune.target_priority==0) best_player=retained;
    if(observed_alive&&tune.enabled&&best_player&&gchandle_new&&gchandle_get_target&&gchandle_free) {
        if(best_player!=held) {
            clear_target(target_tracking);
            target_tracking.handle=gchandle_new((Il2CppObject*)best_player,false);
        }
        target_tracking.sampled=std::chrono::steady_clock::now();
    } else clear_target(target_tracking);
    if(!target_tracking.handle&&tune.enabled) {
        current.locked=false;
        std::snprintf(current.aim_state,sizeof(current.aim_state),"%s",current.filter_unknown?"Waiting for friend/passive filter data":pose_pending?"Waiting for active target head mesh":renderer_pending?"Waiting for enabled target mesh":occluded?"Target ray blocked":current.protected_targets?"No eligible target (protected/friend)":"No target in FOV");
    }
    aim_frame(target_tracking,local,camera,transform,origin,forward,tune,current);
    update_auto_fire(local,camera,origin,forward,tune,current);
    // ESP decoration and roster enumeration must not delay using a fresh pose.
    if(decorate){
    CostScope esp_cost(cost_meters[3]);
    projection_scope.batch.capture(camera,width,height,current); // After aim rotation/reset; never reuse pre-aim matrices.
    for(size_t i=0;i<targets.size();++i) {
        auto& target=targets[i];if(g_cfg.esp_name)label(target_owners[i],target,current);skeleton(target_owners[i],target,current);
        const bool head_projected=camera_project(camera,target.head,width,height,target.screen_head,current);
        const bool feet_projected=camera_project(camera,target.pos,width,height,target.screen_pos,current);
        target.valid=head_projected||feet_projected;
        if(!target.valid)continue;
        for(int bone=0;bone<19;++bone)if(target.bone_mask&(1u<<bone)) {
            if(!camera_project(camera,target.world_bones[bone],width,height,target.bones[bone],current))
                target.bone_mask&=~(1u<<bone);
        }
    }
    }
    update_roster(local,current);
    if(decorate){publish(targets);last_esp=now;}
    else {std::lock_guard<std::mutex> lock(state_mutex);status=current;}
    if(current.frames%300==0) LOGI("V2 players=%d projected=%d errors=%d lock=%d",current.players,current.visible,current.invoke_errors,current.locked);
}
static constexpr bool camera_lock_gate(bool active,bool enabled,bool alive,bool target_alive,bool require_aiming,int aim) {
    return active&&enabled&&alive&&target_alive&&(!require_aiming||(aim>0&&aim<=2));
}
// IL2CPP can expose a boxed value-type adjustor thunk as methodPointer. Native
// Cinemachine calls the unboxed body: hook/call that body, never add 16 twice.
static void* unboxed_body(MethodInfo* m,uint32_t expected_first) {
    uintptr_t p=m?(uintptr_t)m->methodPointer:0;
    uint32_t first=0,branch=0;
    if(!p||!read(p,first)) return nullptr;
    if(first==0x91004000u&&read(p+4,branch)&&(branch&0xFC000000u)==0x14000000u) {
        int64_t displacement=int64_t(branch&0x03FFFFFFu);
        if(displacement&(1LL<<25)) displacement-=1LL<<26;
        p=p+4+displacement*4;
        if(!read(p,first)) return nullptr;
    }
    return jm_ios_unity_address((void*)p)?(void*)p:nullptr;
}
static bool axis_update_callback(void* self,float delta,const MethodInfo* info) {
    AxisUpdateFn original=nullptr;
    while(!(original=axis_original.load(std::memory_order_acquire))) {}
    if(!axis_ready.load(std::memory_order_acquire)||!tracking.active||!tracking.rig)
        return original(self,delta,info);
    CheatStatus scratch;
    uintptr_t sw=active_switch(tracking.camera,scratch),current_rig=0;
    if(sw&&switch_rig) field_get_value((Il2CppObject*)sw,switch_rig,&current_rig);
    // Validate the current owner's identity before dereferencing its children.
    if(!current_rig||current_rig!=tracking.rig||!pointer(current_rig,0x10))
        return original(self,delta,info);
    uintptr_t rig=tracking.rig,axis=(uintptr_t)self;
    bool ours=axis==rig+free_x||axis==rig+free_y;
    if(!ours) {
        uintptr_t orbitals=pointer(rig,free_orbitals);
        unsigned n=orbitals?std::min<unsigned>(array_length((Il2CppArray*)orbitals),3):0;
        for(unsigned i=0;i<n&&!ours;++i) {
            uintptr_t orbital=pointer(orbitals,0x20+i*sizeof(uintptr_t));
            ours=orbital&&axis==orbital+orbital_x;
        }
    }
    if(!ours) return original(self,delta,info);
    uintptr_t local=0; field_static_get(local_field,&local);
    int aim=-1;
    if(sw&&switch_aim) field_get_value((Il2CppObject*)sw,switch_aim,&aim);
    auto tune=Cheat_GetAimbot();
    bool alive=local==tracking.local&&player_state(local,nullptr,scratch,false)==TargetReject::None;
    bool target_alive=player_state(selected_player(),nullptr,scratch,false)==TargetReject::None&&aim_eligibility(selected_player(),tune,scratch)==AimReject::None;
    if(camera_route(tune.enabled,tune.silent,tune.revision,applied_revision)&&current_rig==rig&&camera_lock_gate(tracking.active,tune.enabled,alive,target_alive,tune.require_aiming,aim)) {
        // Reset speed/input only, preserving axis Value and the follow position.
        axis_reset(self,axis_reset_method);
        return false;
    }
    return original(self,delta,info);
}
static bool redirected_direction(Vector3 origin,Vector3 original,Vector3 point,float fov,float range,Vector3& redirected) {
    auto delta=sub(point,origin);float distance=length(delta),magnitude=length(original);
    if(!finite(delta)||!finite(original)||distance<.1f||distance>range||magnitude<.0001f) return false;
    float dot=(delta.x*original.x+delta.y*original.y+delta.z*original.z)/(distance*magnitude);
    if(std::acos(std::clamp(dot,-1.f,1.f))*57.295779513f>fov*.5f) return false;
    redirected={delta.x/distance,delta.y/distance,delta.z/distance};return true;
}
static constexpr bool silent_shot_gate(bool enabled,bool silent,uintptr_t local,uintptr_t shooter) {
    return enabled&&silent&&local&&shooter==local;
}
static ShotContext prepare_shot(const AimbotTune& tune) {
    ShotContext result;result.scope=true;
    if(!tune.enabled||!tune.silent||tune.revision!=applied_revision||
       !chance_pass(tune.hit_chance,next_shot_random())||
       std::chrono::duration<float>(std::chrono::steady_clock::now()-silent_tracking.sampled).count()>.15f)return result;
    uintptr_t local=0;field_static_get(local_field,&local);
    CheatStatus scratch;uintptr_t sw=active_switch(silent_tracking.camera,scratch);int aim=-1;
    if(sw&&switch_aim)field_get_value((Il2CppObject*)sw,switch_aim,&aim);
    if(!sw||(tune.require_aiming&&!(aim>0&&aim<=2)))return result;
    auto target=selected_player(silent_tracking);Vector3 point{},forward{},origin{},normalized{};int source=0;
    auto transform=(uintptr_t)invoke(get_transform,(void*)silent_tracking.camera,nullptr,scratch);
    if(player_state(local,nullptr,scratch,false)!=TargetReject::None||
       player_state(target,nullptr,scratch)!=TargetReject::None||
       aim_eligibility(target,tune,scratch)!=AimReject::None||
       !position(transform,origin,scratch)||!value(get_forward,(void*)transform,nullptr,forward,scratch)||
       !select_aim_point(target,silent_tracking.bone<0?tune.bone:silent_tracking.bone,point,source,scratch,origin,forward,tune))return result;
    // Selection and shot gating use the same camera FOV. The barrel ray can be
    // offset by third-person aiming and must not independently reject the target.
    if(redirected_direction(origin,forward,point,tune.fov_deg,tune.distanceMax,normalized))
        if(!tune.require_visible||line_visible(local,target,origin,point))result={true,true,point,tune.distanceMax};
    return result;
}
static constexpr uint64_t mark_head(uint64_t damaged) {
    unsigned part=(damaged>>32)&255;
    return part>=2&&part<=4?(damaged&~(uint64_t(255)<<32))|(uint64_t(1)<<32):damaged;
}
static bool mark_local_head_hit(HitInfoResult& hit,bool local_scope,bool enabled,bool known_player) {
    // HitParts is a byte at +4 in DamagedObjectData; PlayerDmgType is a different enum.
    // Only change an actual, damageable player hit returned by the native cast.
    if(!local_scope||!enabled||!known_player||!uint32_t(hit.damaged)||hit.flags)return false;
    const auto marked=mark_head(hit.damaged);
    if(marked==hit.damaged)return false;
    hit.damaged=marked;return true;
}
#include "ShotHead.inl"
static HitInfoResult cast_callback(Vector3 origin,Vector3 direction,float range,bool shotgun,bool local_check,void* ignore,const MethodInfo* info) {
    CastFn original=nullptr;while(!(original=cast_original.load(std::memory_order_acquire))) {}
    if(cast_ready.load(std::memory_order_acquire)&&unity_thread.load()==int(syscall(SYS_gettid))&&Auth_Allowed()) {
        auto context=shot_context;
        // Alternative firing paths still identify the local shooter by the
        // ignored WeaponSync GameObject. Never redirect an unidentified ray.
        if(!context.scope&&ignore) {
            uintptr_t local=0;field_static_get(local_field,&local);auto sync=pointer(local,0x60);
            CheatStatus scratch;
            auto game_object=sync&&pointer(sync,0x10)?invoke(get_game_object,(void*)sync,nullptr,scratch):nullptr;
            if(game_object==ignore)context=prepare_shot(Cheat_GetAimbot());
        }
        if(context.scope)++local_rays;
        if(shot_context.scope&&shot_no_spread)direction=shot_base_direction;
        if(context.active) {
            Vector3 redirected{};auto tune=Cheat_GetAimbot();
            if(redirected_direction(origin,direction,context.point,tune.silent_max_angle*2,std::min(range,context.range),redirected)) {
                direction=redirected;++redirected_rays;
            }
        }
        if(shot_context.scope&&steer_selected_head_ray(origin,direction,range,Cheat_GetAimbot()))++hs_steered;
    }
    auto hit=original(origin,direction,range,shotgun,local_check,ignore,info);
    if(shot_context.scope&&unity_thread.load()==int(syscall(SYS_gettid))&&Auth_Allowed())
        promote_selected_head(hit,origin,direction,range,Cheat_GetAimbot());
    if(shot_context.scope&&unity_thread.load()==int(syscall(SYS_gettid))&&Auth_Allowed()&&
       mark_local_head_hit(hit,true,Cheat_GetAimbot().body_headshot,
                           session_accounts.count(uint32_t(hit.damaged))!=0))++head_marks;
    return hit;
}
static void fire_input_callback(void* self,bool pressed,const MethodInfo* info) {
    FireInputFn original=nullptr;while(!(original=fire_input_original.load(std::memory_order_acquire))){}
    uintptr_t current=0;if(controls_instance)field_static_get(controls_instance,&current);
    if((uintptr_t)self==current)manual_held=pressed;
    original(self,pressed,info);
    if((uintptr_t)self==current&&auto_held&&Auth_Allowed()&&Cheat_GetAimbot().auto_fire) {
        bool combined=true;field_set_value((Il2CppObject*)self,shooting_field,&combined);
    }
}
static uintptr_t local_runtime() {
    uintptr_t local=0;if(local_field)field_static_get(local_field,&local);
    return pointer(pointer(local,0x70),0x48);
}
static constexpr bool freeze_consume(bool own,bool enabled,int clip) {return own&&enabled&&clip>0;}
static bool consume_callback(void* self,const MethodInfo* info) {
    ConsumeFn original=nullptr;while(!(original=consume_original.load(std::memory_order_acquire))){}
    if(unity_thread.load()==int(syscall(SYS_gettid))&&Auth_Allowed()) {
        auto tune=Cheat_GetAimbot();int clip=0;read((uintptr_t)self+0x64,clip);
        if(freeze_consume((uintptr_t)self==local_runtime(),tune.freeze_ammo,clip)) {++ammo_blocked;return true;}
    }
    return original(self,info);
}
static float spread_callback(void* self,const MethodInfo* info) {
    SpreadFn original=nullptr;while(!(original=spread_original.load(std::memory_order_acquire))){}
    float original_value=original(self,info);
    if(unity_thread.load()==int(syscall(SYS_gettid))&&Auth_Allowed()&&Cheat_GetAimbot().no_spread&&(uintptr_t)self==local_runtime())return 0.f;
    return original_value;
}
static float accuracy_callback(void* self,const MethodInfo* info){
    SpreadFn original=nullptr;while(!(original=accuracy_original.load(std::memory_order_acquire))){}
    if(unity_thread.load()==int(syscall(SYS_gettid))&&Auth_Allowed()&&Cheat_GetAimbot().no_spread&&(uintptr_t)self==local_runtime()){++spread_override_count;return 0.f;}
    return original(self,info);
}
static constexpr float rapid_delay(float original,float multiplier) {
    const float divided=original/multiplier;
    const float bounded=divided<.02f?.02f:divided;
    return bounded<original?bounded:original;
}
static void update_rapid_fire(uintptr_t local,bool alive) {
    if(!rapid_interval_field||!field_get_value||!field_set_value)return;
    const auto tune=Cheat_GetAimbot();
    uintptr_t runtime=alive&&local&&Auth_Allowed()&&tune.rapid_fire?pointer(pointer(local,0x70),0x48):0;
    int type=-1;const auto data=pointer(runtime,0x20);
    if(!data||!read(data+0x10,type)||!fire_weapon_allowed(type))runtime=0;
    const auto previous=rooted(rapid_runtime);
    if(previous&&previous!=runtime) {
        float current=0;field_get_value((Il2CppObject*)previous,rapid_interval_field,&current);
        if(current==rapid_applied)field_set_value((Il2CppObject*)previous,rapid_interval_field,&rapid_saved);
        release_handle(rapid_runtime);rapid_saved=rapid_applied=0;
    }
    if(!runtime)return;
    float current=0;field_get_value((Il2CppObject*)runtime,rapid_interval_field,&current);
    if(!std::isfinite(current)||current<=0||current>10)return;
    if(!rooted(rapid_runtime)){root(rapid_runtime,runtime);rapid_saved=current;}
    else if(current!=rapid_applied)rapid_saved=current;
    if(!rooted(rapid_runtime))return;
    float desired=rapid_delay(rapid_saved,tune.rapid_multiplier);
    if(current!=desired)field_set_value((Il2CppObject*)runtime,rapid_interval_field,&desired);
    rapid_applied=desired;
}
static void update_weapon_mods(uintptr_t local,bool alive) {
    update_rapid_fire(local,alive);
    if(!clone_object||!runtime_characteristics||!set_object_field)return;
    auto tune=Cheat_GetAimbot();
    auto runtime=alive&&local&&Auth_Allowed()&&(tune.no_recoil||tune.no_spread)?pointer(pointer(local,0x70),0x48):0;
    auto old=rooted(recoil_runtime),copy=rooted(recoil_clone_asset),asset=rooted(recoil_original_asset);
    if(old&&(runtime!=old||pointer(old,0x20)!=copy)) {
        if(copy&&asset&&pointer(old,0x20)==copy)set_object_field((Il2CppObject*)old,runtime_characteristics,(Il2CppObject*)asset);
        release_handle(recoil_runtime);release_handle(recoil_clone_asset);release_handle(recoil_original_asset);old=0;copy=0;asset=0;
    }
    if(runtime&&!old) {
        auto original_asset=pointer(runtime,0x20);if(!original_asset)return;
        CheatStatus scratch;auto clone=invoke(clone_object,(void*)original_asset,nullptr,scratch);if(!clone)return;
        root(recoil_original_asset,original_asset);root(recoil_clone_asset,(uintptr_t)clone);root(recoil_runtime,runtime);
        copy=rooted(recoil_clone_asset);asset=rooted(recoil_original_asset);
        if(rooted(recoil_runtime)&&copy&&asset)set_object_field((Il2CppObject*)runtime,runtime_characteristics,(Il2CppObject*)copy);
        else {release_handle(recoil_runtime);release_handle(recoil_clone_asset);release_handle(recoil_original_asset);return;}
    }
    if(!runtime||!copy||!asset)return;
    // Restore each original value when just one toggle is disabled. The clone
    // keeps every unrelated characteristic and is detached on death/weapon swap.
    auto apply=[&](FieldInfo* field,bool zero){if(!field)return;float v=0;if(!zero)field_get_value((Il2CppObject*)asset,field,&v);field_set_value((Il2CppObject*)copy,field,&v);};
    apply(characteristics_recoil,tune.no_recoil);
    apply(characteristics_accuracy,tune.no_spread);
    apply(characteristics_spread_time,tune.no_spread);
    apply(characteristics_spread_cooldown,tune.no_spread);
}
static bool own_recoil(void* self){
    uintptr_t local=0;field_static_get(local_field,&local);if(!local)return false;
    CheatStatus scratch;if(player_state(local,nullptr,scratch,false)!=TargetReject::None)return false;
    auto transform=invoke(get_transform,self,nullptr,scratch),root=invoke(get_transform,(void*)local,nullptr,scratch);
    bool own=false;void* args[]={root};return transform&&root&&value(is_child,transform,args,own,scratch)&&own;
}
static void recoil_apply_callback(void* self,const MethodInfo* info){
    RecoilApplyFn original=nullptr;while(!(original=recoil_apply_original.load(std::memory_order_acquire))){}
    if(unity_thread.load()==int(syscall(SYS_gettid))&&Auth_Allowed()&&Cheat_GetAimbot().no_recoil&&own_recoil(self)){
        Quaternion identity{0,0,0,1};if(recoil_rotation_field)field_set_value((Il2CppObject*)self,recoil_rotation_field,&identity);
        ++recoil_impulse_count;return;
    }original(self,info);
}
static void fire_visual_callback(void* self,bool first,bool second,bool third,void* runtime,const MethodInfo* info){
    FireVisualFn original=nullptr;while(!(original=fire_visual_original.load(std::memory_order_acquire))){}
    if(unity_thread.load()!=int(syscall(SYS_gettid))){original(self,first,second,third,runtime,info);return;}
    uintptr_t local=0;field_static_get(local_field,&local);
    bool own=local&&pointer((uintptr_t)self,0x20)==local&&(uintptr_t)runtime==local_runtime();
    if(own)++local_visual_depth;
    original(self,first,second,third,runtime,info);
    if(own)--local_visual_depth;
}
static constexpr bool recoil_shake_gate(bool unity,bool own_shot,bool authenticated,bool enabled){return unity&&own_shot&&authenticated&&enabled;}
static bool suppress_fire_shake(){
    if(unity_thread.load()!=int(syscall(SYS_gettid)))return false;
    return recoil_shake_gate(true,local_visual_depth>0||shot_context.scope,Auth_Allowed(),Cheat_GetAimbot().no_recoil);
}
static void* fire_shake_callback(void* self,float magnitude,float roughness,float fade_in,float fade_out,const MethodInfo* info){
    FireShakeFn original=nullptr;while(!(original=fire_shake_original.load(std::memory_order_acquire))){}
    if(suppress_fire_shake()){magnitude=0;++recoil_camera_impulses;}
    return original(self,magnitude,roughness,fade_in,fade_out,info);
}
static void* fire_shake_influence_callback(void* self,float magnitude,float roughness,float fade_in,float fade_out,Vector3 position,Vector3 rotation,const MethodInfo* info){
    FireShakeInfluenceFn original=nullptr;while(!(original=fire_shake_influence_original.load(std::memory_order_acquire))){}
    if(suppress_fire_shake()){magnitude=0;++recoil_camera_impulses;}
    return original(self,magnitude,roughness,fade_in,fade_out,position,rotation,info);
}
static void recoil_impulse_callback(void* self,float magnitude,const MethodInfo* info){
    RecoilImpulseFn original=nullptr;while(!(original=recoil_impulse_original.load(std::memory_order_acquire))){}
    if(unity_thread.load()==int(syscall(SYS_gettid))&&Auth_Allowed()&&Cheat_GetAimbot().no_recoil){
        uintptr_t local=0;field_static_get(local_field,&local);CheatStatus scratch;
        auto transform=invoke(get_transform,self,nullptr,scratch);auto root_transform=local?invoke(get_transform,(void*)local,nullptr,scratch):nullptr;
        bool owned=false;void* args[]={root_transform};
        if(transform&&root_transform&&value(is_child,transform,args,owned,scratch)&&owned&&player_state(local,nullptr,scratch,false)==TargetReject::None){++recoil_impulse_count;return;}
    }
    original(self,magnitude,info);
}
static uintptr_t scoped_spread_copy(uintptr_t runtime,CheatStatus& scratch){
    auto original=pointer(runtime,0x18);
    if(!original||!clone_object||!scoped_angle_field||!scoped_weapon_field||!set_object_field)return 0;
    if(original==rooted(scoped_weapon_clone))return original;
    if(original!=rooted(scoped_weapon_original)||!rooted(scoped_weapon_clone)){
        root(scoped_weapon_original,original);release_handle(scoped_weapon_clone);
        auto copy=invoke(clone_object,(void*)original,nullptr,scratch);
        if(copy){float zero=0;field_set_value(copy,scoped_angle_field,&zero);root(scoped_weapon_clone,(uintptr_t)copy);}
    }
    return rooted(scoped_weapon_clone);
}
static void shoot_callback(void* self,Vector3 origin,Vector3 direction,const MethodInfo* info) {
    ShootFn original=nullptr;while(!(original=shoot_original.load(std::memory_order_acquire))) {}
    if(unity_thread.load()!=int(syscall(SYS_gettid))) {original(self,origin,direction,info);return;}
    auto previous=shot_context;auto previous_spread=shot_no_spread;auto previous_direction=shot_base_direction;shot_context={};shot_no_spread=false;
    if(shoot_ready.load(std::memory_order_acquire)&&cast_ready.load(std::memory_order_acquire)&&Auth_Allowed()) {
        uintptr_t local=0;field_static_get(local_field,&local);
        if(local&&pointer((uintptr_t)self,0x20)==local) {
            if(!previous.scope)++local_shots;auto tune=Cheat_GetAimbot();shot_context=previous.scope?previous:prepare_shot(tune);
            float magnitude=length(direction);
            shot_no_spread=tune.no_spread&&finite(direction)&&magnitude>.0001f;
            if(shot_no_spread){
                shot_base_direction={direction.x/magnitude,direction.y/magnitude,direction.z/magnitude};

            }
        }
    }
    uintptr_t runtime=0,original_weapon=0,copy=0;
    if(Auth_Allowed()&&Cheat_GetAimbot().no_spread){
        uintptr_t local=0;field_static_get(local_field,&local);
        if(local&&pointer((uintptr_t)self,0x20)==local){
            runtime=local_runtime();original_weapon=pointer(runtime,0x18);CheatStatus scratch;copy=scoped_spread_copy(runtime,scratch);
            if(copy){set_object_field((Il2CppObject*)runtime,scoped_weapon_field,(Il2CppObject*)copy);if(pointer(runtime,0x18)==copy)++spread_override_count;}
        }
    }
    uintptr_t camera_transform=0;Quaternion camera_before{};bool restore_camera=false;CheatStatus camera_scratch;
    if(Auth_Allowed()&&Cheat_GetAimbot().no_recoil){
        uintptr_t local=0;field_static_get(local_field,&local);
        if(local&&pointer((uintptr_t)self,0x20)==local){auto camera=invoke(camera_main,nullptr,nullptr,camera_scratch);camera_transform=(uintptr_t)invoke(get_transform,camera,nullptr,camera_scratch);
            restore_camera=camera_transform&&value(shot_camera_rotation,(void*)camera_transform,nullptr,camera_before,camera_scratch);}
    }
    original(self,origin,direction,info);
    if(restore_camera&&pointer(camera_transform,0x10)){void* args[]={&camera_before};invoke(set_rotation,(void*)camera_transform,args,camera_scratch);}
    if(copy&&pointer(runtime,0x18)==copy)set_object_field((Il2CppObject*)runtime,scoped_weapon_field,(Il2CppObject*)original_weapon);
    shot_context=previous;shot_no_spread=previous_spread;shot_base_direction=previous_direction;
}

static void safe_shoot_callback(void* self,SafeShotTransform input,const MethodInfo* info){
    SafeShootFn original=nullptr;while(!(original=safe_shoot_original.load(std::memory_order_acquire))){}
    if(unity_thread.load()!=int(syscall(SYS_gettid))||!Auth_Allowed()){original(self,input,info);return;}
    uintptr_t local=0;field_static_get(local_field,&local);
    Vector3 origin{input.origin_x,input.origin_y,input.origin_z},direction{input.direction_x,input.direction_y,input.direction_z};
    float magnitude=length(direction);
    if(!local||pointer((uintptr_t)self,0x20)!=local||!finite(origin)||!finite(direction)||magnitude<.0001f){original(self,input,info);return;}
    auto tune=Cheat_GetAimbot();auto previous=shot_context;bool previous_spread=shot_no_spread;auto previous_direction=shot_base_direction;
    shot_context=prepare_shot(tune);shot_no_spread=tune.no_spread;
    shot_base_direction={direction.x/magnitude,direction.y/magnitude,direction.z/magnitude};++local_shots;++local_visual_depth;
    auto runtime=local_runtime();auto original_weapon=pointer(runtime,0x18);uintptr_t copy=0;CheatStatus scratch;
    if(tune.no_spread){copy=scoped_spread_copy(runtime,scratch);if(copy){set_object_field((Il2CppObject*)runtime,scoped_weapon_field,(Il2CppObject*)copy);if(pointer(runtime,0x18)==copy)++spread_override_count;}}
    uintptr_t camera_transform=0;Quaternion camera_before{};bool restore=false;
    if(tune.no_recoil){auto camera=invoke(camera_main,nullptr,nullptr,scratch);camera_transform=camera?(uintptr_t)invoke(get_transform,camera,nullptr,scratch):0;
        restore=camera_transform&&value(shot_camera_rotation,(void*)camera_transform,nullptr,camera_before,scratch);}
    // Preserve the entire SafeTransform and its validation fields unchanged.
    // Override only identified local ray directions after the game builds them.
    original(self,input,info);
    if(restore&&pointer(camera_transform,0x10)){void* args[]={&camera_before};invoke(set_rotation,(void*)camera_transform,args,scratch);}
    if(copy&&pointer(runtime,0x18)==copy)set_object_field((Il2CppObject*)runtime,scoped_weapon_field,(Il2CppObject*)original_weapon);
    --local_visual_depth;shot_context=previous;shot_no_spread=previous_spread;shot_base_direction=previous_direction;
}

template<int Slot> static void update_callback(void* self,const MethodInfo* info) {
    LateUpdateFn original=nullptr;
    while(!(original=originals[Slot].load(std::memory_order_acquire))) {}
    original(self,info);
    if(callback_ready[Slot].load(std::memory_order_acquire)) {
        ++callback_counts[Slot];
        tick((uintptr_t)self,Slot==4?2:Slot<2?0:1);
    }
}
static void pose_update_callback(void* self,const MethodInfo* info) {
    PoseUpdateFn original=nullptr;while(!(original=pose_original.load(std::memory_order_acquire))) {}
    original(self,info);
    // Read only after the solver and its OnPostUpdate delegate have completed.
    // This callback never writes a camera, transform, solver or overlay snapshot.
    static bool sampling=false;
    if(unity_thread.load()!=int(syscall(SYS_gettid))||!Auth_Allowed()||sampling)return;
    auto owner=pose_solver_owners.find((uintptr_t)self);
    if(owner==pose_solver_owners.end())return;
    const auto player=owner->second;auto cached=actor_cache.find(player);
    if(cached==actor_cache.end()||rooted(cached->second.owner)!=player)return;
    const auto body=pointer(pointer(player,0xD8),0x38),aim=pointer(pointer(player,0xD0),0x68);
    if(pointer(body,0x48)!=(uintptr_t)self&&pointer(aim,0x40)!=(uintptr_t)self)return;
    auto demand=pose_demand_mask.load(std::memory_order_relaxed);if(!demand)return;
    const auto sampled_at=std::chrono::steady_clock::now();
    const bool body_due=sampled_at-cached->second.posed_body_at>=std::chrono::milliseconds(pose_body_period_ms.load(std::memory_order_relaxed));
    if(!body_due)demand&=pose_priority_mask.load(std::memory_order_relaxed);
    CostScope pose_cost(cost_meters[1]);
    sampling=true;PoseRigScope rig_reads(player);CheatStatus scratch;const unsigned sampled_room=room_epoch;
    Vector3 bones[19]{};uint32_t mask=0;
    for(int bone=0;bone<19;++bone) {
        if(!(demand&(1u<<bone)))continue;
        const auto transform=visible_bone(player,bone,scratch);
        if(position(transform,bones[bone],scratch))mask|=1u<<bone;
    }
    Vector3 center{};
    bool center_ready=pose_geometry_needed.load(std::memory_order_relaxed)&&(mask&(1u<<10))&&mesh_head_center(player,visible_bone(player,10,scratch),center,scratch);
    cached=actor_cache.find(player);
    if(cached==actor_cache.end()||room_epoch!=sampled_room||rooted(cached->second.owner)!=player){sampling=false;return;}
    auto& sample=cached->second;
    const auto committed=std::chrono::steady_clock::now();
    if(sample.posed_room!=room_epoch)sample.posed_mask=0;
    for(int bone=0;bone<19;++bone)if(demand&(1u<<bone)){
        sample.posed_mask&=~(1u<<bone);
        if(mask&(1u<<bone)){sample.posed_bones[bone]=bones[bone];sample.posed_times[bone]=committed;sample.posed_mask|=1u<<bone;}
    }
    if(body_due)sample.posed_body_at=committed;
    sample.posed_center=center;sample.posed_center_ready=center_ready;
    sample.posed_room=room_epoch;sample.posed_at=std::chrono::steady_clock::now();
    if(mask)++pose_sample_count;
    sampling=false;
}
#include "FollowSearch.inl"
static void initialize() {
    phase("Waiting for libil2cpp/domain");
    Il2CppClass *linker_class=nullptr,*controller_class=nullptr;
    for(int attempt=0;attempt<600;++attempt) {
        il2cpp_api::init();
        if(attached_domain&&class_get_method&&field_static_get&&class_get_field&&type_get_object&&class_get_type&&runtime_invoke&&object_unbox&&array_length) {
            apk_trace::record("metadata.CharacterLinker.begin");
            linker_class=find_class("JarviGames.Character","CharacterLinker");
            apk_trace::record("metadata.CharacterLinker.returned");
            controller_class=find_class("RootMotion","CameraController");
            apk_trace::record("metadata.CameraController.returned");
            if(linker_class&&controller_class) break;
        }
        usleep(100000);
    }
    if(!attached_domain) { phase("Failed: libil2cpp/domain unavailable"); return; }
    if(!class_get_method||!field_static_get||!class_get_field||!type_get_object||!class_get_type||!runtime_invoke||!object_unbox||!array_length) { phase("Failed: required IL2CPP exports unavailable"); return; }
    if(!linker_class) { phase("Failed: CharacterLinker class unavailable"); return; }
    if(!controller_class) { phase("Failed: RootMotion.CameraController unavailable"); return; }
    phase("Resolving metadata");
#ifdef JM_APK
    apk_trace::record("integrity.early_install.begin");
    apk_integrity_init();
    apk_trace::record("integrity.early_install.end");
#elif !defined(JM_HYBRID) && !defined(JM_IOS)
    internal_probe_init();
#endif
    local_field=class_get_field(linker_class,"<ADBAABCBCCDCCADAACDDADD>k__BackingField");
    linker_type=type_get_object(class_get_type(linker_class));
    // Resolve the Type overload explicitly; generic FindObjectsOfType<T>(bool)
    // also has one argument and must never receive a System.Type object.
    auto object_class=find_class_any("UnityEngine","Object");
    if(object_class&&class_get_methods&&method_get_param&&method_get_name&&method_get_param_count&&type_get_object) {
        auto type_class=find_class_any("System","Type");
        auto expected=type_class?type_get_object(class_get_type(type_class)):nullptr;
        void* iterator=nullptr;
        while(auto candidate=class_get_methods(object_class,&iterator)) {
            const char* name=method_get_name(candidate);
            if(name&&std::strcmp(name,"FindObjectsOfType")==0&&method_get_param_count(candidate)==1) {
                auto parameter=method_get_param(candidate,0);
                if(parameter&&expected&&type_get_object(parameter)==expected) { find_objects=(MethodInfo*)candidate; break; }
            }
        }
    }
    get_bone=method("UnityEngine","Animator","GetBoneTransform",1);
    is_human=method("UnityEngine","Animator","get_isHuman",0);
    child_animator=method("UnityEngine","Component","GetComponentInChildren",2);
    auto animator_class=find_class_any("UnityEngine","Animator");
    animator_type=animator_class?type_get_object(class_get_type(animator_class)):nullptr;
    nickname_text=method("TMPro","TMP_Text","get_text",0);
    shot_camera_rotation=method("UnityEngine","Transform","get_rotation",0);
    set_rotation=method("UnityEngine","Transform","set_rotation",1);
    brain_output=method("Cinemachine","CinemachineBrain","get_OutputCamera",0);
    get_position=method("UnityEngine","Transform","get_position",0);
    get_forward=method("UnityEngine","Transform","get_forward",0);
    get_transform=method("UnityEngine","Component","get_transform",0);
    world_to_screen=method("UnityEngine","Camera","WorldToScreenPoint",1);
    camera_main=method("UnityEngine","Camera","get_main",0);
    camera_fov=method("UnityEngine","Camera","get_fieldOfView",0);
    camera_reset_view=method("UnityEngine","Camera","ResetWorldToCameraMatrix",0);
    camera_reset_culling=method("UnityEngine","Camera","ResetCullingMatrix",0);
    camera_view=method("UnityEngine","Camera","get_worldToCameraMatrix",0);
    camera_projection=method("UnityEngine","Camera","get_projectionMatrix",0);
    pixel_width=method("UnityEngine","Camera","get_pixelWidth",0);
    pixel_height=method("UnityEngine","Camera","get_pixelHeight",0);
    screen_width=method("UnityEngine","Screen","get_width",0);
    screen_height=method("UnityEngine","Screen","get_height",0);
    auto switch_class=find_class_any("","SwitchCamera");
    if(switch_class) {
        for(auto k=switch_class;k&&!switch_instance;k=class_get_parent?class_get_parent(k):nullptr)
            switch_instance=class_get_field(k,"<Instance>k__BackingField");
        switch_aim=class_get_field(switch_class,"CBABCBDABADDCDCABCABAAC");
        switch_rig=class_get_field(switch_class,"BCABDDCCCBDBCBCCBBAAACC");
    }
    is_active=method("UnityEngine","Behaviour","get_isActiveAndEnabled",0);
    skin_object_active=method("UnityEngine","GameObject","get_activeInHierarchy",0);
    mesh_get_bones=method("UnityEngine","SkinnedMeshRenderer","get_bones",0);
    mesh_shared=method("UnityEngine","SkinnedMeshRenderer","get_sharedMesh",0);
    mesh_vertices=method("UnityEngine","Mesh","get_vertices",0);
    mesh_weights=method("UnityEngine","Mesh","get_boneWeights",0);
    mesh_bindposes=method("UnityEngine","Mesh","get_bindposes",0);
    mesh_readable=method("UnityEngine","Mesh","get_isReadable",0);
    head_mesh_class=find_class_any("UnityEngine","Mesh");
    head_mesh_ctor=head_mesh_class?class_get_method(head_mesh_class,".ctor",0):nullptr;
    head_bake=method("UnityEngine","SkinnedMeshRenderer","BakeMesh",2);
    head_recalculate=method("UnityEngine","Mesh","RecalculateBounds",0);
    head_bounds=method("UnityEngine","Mesh","get_bounds",0);
    head_inverse_point=method("UnityEngine","Transform","InverseTransformPoint",1);
    object_get_name=method("UnityEngine","Object","get_name",0);
    renderer_visible=method("UnityEngine","Renderer","get_isVisible",0);
    renderer_enabled=method("UnityEngine","Renderer","get_enabled",0);
    renderer_forced_off=method("UnityEngine","Renderer","get_forceRenderingOff",0);
    transform_point=method("UnityEngine","Transform","TransformPoint",1);
    hit_manager=method("JarviGames.GamePlay.Character.HitSystem","HitBoxController","ACAABAABBDCDDDDDABDACDA",0);
    auto hit_controller_class=find_class_any("JarviGames.GamePlay.Character.HitSystem","HitBoxController");
    hit_controller_type=hit_controller_class?type_get_object(class_get_type(hit_controller_class)):nullptr;
    hit_classifier=method("JarviGames.HitBoxSystem","HitBoxObject","DCCDADCCDBBACDBADBCBDCC",4);
    uint32_t classifier_first=0;
    if(!hit_classifier||!jm_ios_unity_address((void*)hit_classifier->methodPointer))hit_classifier=nullptr;
    sphere_class=find_class_any("JarviGames.GamePlay.Character.HitSystem","HitVirtualSphere");
    capsule_class=find_class_any("JarviGames.GamePlay.Character.HitSystem","HitVirtualCapsule");
    auto renderer_class=find_class_any("UnityEngine","SkinnedMeshRenderer");
    renderer_type=renderer_class?type_get_object(class_get_type(renderer_class)):nullptr;
    // Select the non-generic (System.Type, bool) overload; another overload
    // accepts a bool and List<T>, with the same number of parameters.
    auto component_class=find_class_any("UnityEngine","GameObject");
    get_game_object=method("UnityEngine","Component","get_gameObject",0);
    components_internal=method("UnityEngine","GameObject","GetComponentsInternal",6);
    auto system_type=find_class_any("System","Type");
    auto expected_type=system_type?type_get_object(class_get_type(system_type)):nullptr;
    if(component_class&&expected_type&&class_get_methods&&method_get_name&&method_get_param_count&&method_get_param) {
        void* iterator=nullptr;
        while(auto candidate=class_get_methods(component_class,&iterator)) {
            const char* name=method_get_name(candidate);
            if(name&&std::strcmp(name,"GetComponentsInChildren")==0&&method_get_param_count(candidate)==2) {
                auto first=method_get_param(candidate,0);
                if(first&&type_get_object(first)==expected_type) { child_renderers=(MethodInfo*)candidate; break; }
            }
        }
    }
    physics_ray=method("UnityEngine","Physics","RaycastNonAlloc",6);
    hit_transform=method("UnityEngine","RaycastHit","get_transform",0);
    is_child=method("UnityEngine","Transform","IsChildOf",1);
    ray_hit_class=find_class_any("UnityEngine","RaycastHit");
    array_new=(decltype(array_new))dlsym(libil2cpp_handle,"il2cpp_array_new");
    auto value_size=(int32_t (*)(Il2CppClass*,uint32_t*))dlsym(libil2cpp_handle,"il2cpp_class_value_size");
    uint32_t alignment=0;if(value_size&&ray_hit_class)ray_hit_size=value_size(ray_hit_class,&alignment);
    if(ray_hit_size<40||ray_hit_size>128)ray_hit_size=0;
    auto hit_point_class=find_class_any("JarviGames","HitPoint");
    if(!value_size||!hit_point_class||value_size(hit_point_class,&alignment)!=28)hit_classifier=nullptr;
    session_dictionary=method("JarviGames.Utilities","RoomUtility","BCAABDABADBBBDCBACBDDAA",0);
    class_is_value=(bool (*)(Il2CppClass*))dlsym(libil2cpp_handle,"il2cpp_class_is_valuetype");
    hit_status=method("JarviGames.Character","CharacterLinker","BDDCDCBBCDBDBABACAAAABC",0);
    player_api_get=method("JarviGames.Utilities","RoomUtility","DDBDDABABCDCCCDCBBAAADA",0);
    player_userid=method("JarviGames.Multiplayer.Player","PlayerApi","BCBBDDCACBADDACBABBCDAD",1);
    player_packet=method("JarviGames.Multiplayer.Player","PlayerApi","AABDACCDCBBCBAADACDCDDC",2);
    friend_contains=method("JarviGames","ViceFriendsClient","BDADCDDBACBAADCCBBADCCC",1);
    auto network_class=find_class_any("JarviGames","ViceNetwork");
    if(network_class)client_session_field=class_get_field(network_class,"<CDDDCDCADACBBAAAADAABDB>k__BackingField");
    struct Required { const void* value; const char* name; };
    Required required[]={
        {local_field,"CharacterLinker local static field"},{linker_type,"CharacterLinker System.Type"},
        {find_objects,"Object.FindObjectsOfType(System.Type)"},{get_position,"Transform.get_position"},
        {get_forward,"Transform.get_forward"},{get_transform,"Component.get_transform"},
        {world_to_screen,"Camera.WorldToScreenPoint"},{set_rotation,"Transform.set_rotation"},
        {is_active,"Behaviour.get_isActiveAndEnabled"},
        {renderer_type,"SkinnedMeshRenderer System.Type"},{renderer_visible,"Renderer.get_isVisible"},
        {renderer_enabled,"Renderer.get_enabled"}
    };
    for(const auto& item:required) if(!item.value) {
        char message[128]; std::snprintf(message,sizeof(message),"Missing: %s",item.name); phase(message); return;
    }
    if(!get_game_object||(!child_renderers&&!components_internal))
        LOGI("Render query unavailable; callbacks will still connect");
    // Install before the frame callbacks so no later hook installation changes
    // the protection of a page containing a running trampoline.
    auto free_class=find_class_any("Cinemachine","CinemachineFreeLook");
    auto orbital_class=find_class_any("Cinemachine","CinemachineOrbitalTransposer");
    auto offset=[](Il2CppClass* k,const char* name)->size_t {
        auto f=k?class_get_field(k,name):nullptr;
        return f&&field_get_offset?field_get_offset(f):0;
    };
    free_x=offset(free_class,"m_XAxis"); free_y=offset(free_class,"m_YAxis");
    free_orbitals=offset(free_class,"mOrbitals"); orbital_x=offset(orbital_class,"m_XAxis");
    auto axis_update=method("Cinemachine","AxisState","Update",1);
    axis_reset_method=method("Cinemachine","AxisState","Reset",0);
    if(free_x&&free_y&&free_orbitals&&orbital_x&&axis_update&&axis_update->methodPointer&&axis_reset_method&&axis_reset_method->methodPointer) {
        auto update_body=unboxed_body(axis_update,0);
        axis_reset=(AxisResetFn)unboxed_body(axis_reset_method,0);
        auto trampoline=update_body&&axis_reset?inline_hook::install<AxisUpdateFn>(update_body,(void*)axis_update_callback):nullptr;
        if(trampoline) { axis_original.store(trampoline,std::memory_order_release); axis_ready.store(true,std::memory_order_release); }
        else LOGI("Camera axis lock unavailable: %s",inline_hook::last_error);
    }
    auto cast=method("JarviGames.Utilities","GameShootUtility","DCCBCCBCDDABBAADBCADCCA",6);
    // Fail closed unless the return-value ABI and prologue match the supplied build.
    uint32_t cast_first=0;
    if(cast&&jm_ios_unity_address((void*)cast->methodPointer)) {
        auto trampoline=inline_hook::install<CastFn>((void*)cast->methodPointer,(void*)cast_callback);
        if(trampoline) {cast_original.store(trampoline,std::memory_order_release);cast_ready.store(true,std::memory_order_release);}
    }
    auto shoot=method("JarviGames.Character","WeaponSync","TryShoot",2);
    if(shoot&&shoot->methodPointer) {
        auto trampoline=inline_hook::install<ShootFn>((void*)shoot->methodPointer,(void*)shoot_callback);
        if(trampoline) {shoot_original.store(trampoline,std::memory_order_release);shoot_ready.store(true,std::memory_order_release);}
        else LOGI("Silent aim unavailable: %s",inline_hook::last_error);
    }
    auto local_fire=method("JarviGames.Character","WeaponSync","CABBDBBCCADAAACCBADAADD",1);
    if(local_fire&&local_fire->methodPointer){auto fn=inline_hook::install<SafeShootFn>((void*)local_fire->methodPointer,(void*)safe_shoot_callback);if(fn)safe_shoot_original.store(fn,std::memory_order_release);}
    auto controls_class=find_class_any("","CharController");
    for(auto k=controls_class;k&&!controls_instance;k=class_get_parent?class_get_parent(k):nullptr)controls_instance=class_get_field(k,"<Instance>k__BackingField");
    shooting_field=controls_class?class_get_field(controls_class,"shooting"):nullptr;
    auto start_fire=controls_class?class_get_method(controls_class,"StartShoot",1):nullptr;
    if(start_fire&&start_fire->methodPointer&&shooting_field&&controls_instance&&field_set_value) {
        auto trampoline=inline_hook::install<FireInputFn>((void*)start_fire->methodPointer,(void*)fire_input_callback);
        if(trampoline){fire_input_original.store(trampoline,std::memory_order_release);fire_input_ready.store(true,std::memory_order_release);}
    }
    clone_object=method("System","Object","MemberwiseClone",0);
    auto runtime_class=find_class_any("JarviGames.Weapons","WeaponRuntimeData");
    rapid_interval_field=runtime_class?class_get_field(runtime_class,"<CCADACDAACABADCDDACAACC>k__BackingField"):nullptr;
    if(rapid_interval_field&&(!field_get_offset||field_get_offset(rapid_interval_field)!=0x74))rapid_interval_field=nullptr;
    auto characteristics_class=find_class_any("JarviGames","WeaponCharacteristicsData");
    runtime_characteristics=runtime_class?class_get_field(runtime_class,"DACDAACDABCCBCCCABCCCDB"):nullptr;
    characteristics_recoil=characteristics_class?class_get_field(characteristics_class,"_recoil"):nullptr;
    characteristics_accuracy=characteristics_class?class_get_field(characteristics_class,"_accuracy"):nullptr;
    characteristics_spread_time=characteristics_class?class_get_field(characteristics_class,"_timeToSpreadFactor"):nullptr;
    characteristics_spread_cooldown=characteristics_class?class_get_field(characteristics_class,"_cooldownSpreadFactor"):nullptr;
    set_object_field=(SetObjectFieldFn)dlsym(libil2cpp_handle,"il2cpp_field_set_value_object");
    recoil_ready.store(clone_object&&runtime_characteristics&&characteristics_recoil&&set_object_field,std::memory_order_release);
    auto consume=runtime_class?class_get_method(runtime_class,"CABCACDBBDAACCACBCDBBCA",0):nullptr;
    uint32_t first=0;
    if(consume&&jm_ios_unity_address((void*)consume->methodPointer)) {
        auto trampoline=inline_hook::install<ConsumeFn>((void*)consume->methodPointer,(void*)consume_callback);
        if(trampoline){consume_original.store(trampoline,std::memory_order_release);ammo_ready.store(true,std::memory_order_release);}
    }
    auto spread=runtime_class?class_get_method(runtime_class,"ACBBBCDCBCACABBDDBCADDB",0):nullptr;
    if(spread&&jm_ios_unity_address((void*)spread->methodPointer)) {
        auto trampoline=inline_hook::install<SpreadFn>((void*)spread->methodPointer,(void*)spread_callback);
        if(trampoline){spread_original.store(trampoline,std::memory_order_release);spread_ready.store(true,std::memory_order_release);}
    }
    auto accuracy=runtime_class?class_get_method(runtime_class,"ACCDDBABABBBDADCAAADAAD",0):nullptr;
    if(accuracy&&accuracy->methodPointer){auto fn=inline_hook::install<SpreadFn>((void*)accuracy->methodPointer,(void*)accuracy_callback);if(fn)accuracy_original.store(fn,std::memory_order_release);}
    auto recoil_class=find_class_any("RootMotion.FinalIK","Recoil");
    recoil_rotation_field=recoil_class?class_get_field(recoil_class,"rotationOffset"):nullptr;
    auto recoil_apply=recoil_class?class_get_method(recoil_class,"BDAADBADDBBDCCDBBDCDCAC",0):nullptr;
    if(recoil_apply&&recoil_apply->methodPointer){auto fn=inline_hook::install<RecoilApplyFn>((void*)recoil_apply->methodPointer,(void*)recoil_apply_callback);if(fn)recoil_apply_original.store(fn,std::memory_order_release);}
    auto weapon_class=find_class_any("JarviGames.Multiplayer.Common","Weapon");
    scoped_weapon_field=runtime_class?class_get_field(runtime_class,"DDCACADDCBDBBBBDABBBCCB"):nullptr;
    scoped_angle_field=weapon_class?class_get_field(weapon_class,"<SpreadAngle>k__BackingField"):nullptr;
    auto impulse=method("RootMotion.FinalIK","Recoil","CDBDDCCBCCBADDCBDDCDACB",1);
    if(impulse&&impulse->methodPointer){auto fn=inline_hook::install<RecoilImpulseFn>((void*)impulse->methodPointer,(void*)recoil_impulse_callback);if(fn)recoil_impulse_original.store(fn,std::memory_order_release);}
    auto visual_fire=method("JarviGames.Character","WeaponAnimations","BBABDCAACDCAAACCBACDBDD",4);
    if(visual_fire&&visual_fire->methodPointer){auto fn=inline_hook::install<FireVisualFn>((void*)visual_fire->methodPointer,(void*)fire_visual_callback);if(fn)fire_visual_original.store(fn,std::memory_order_release);}
    auto shake=method("","CameraShaker","DBDDBBADBDDCACDCDDABDDD",4);
    if(shake&&shake->methodPointer){auto fn=inline_hook::install<FireShakeFn>((void*)shake->methodPointer,(void*)fire_shake_callback);if(fn)fire_shake_original.store(fn,std::memory_order_release);}
    auto influenced_shake=method("","CameraShaker","DBDDBBADBDDCACDCDDABDDD",6);
    if(influenced_shake&&influenced_shake->methodPointer){auto fn=inline_hook::install<FireShakeInfluenceFn>((void*)influenced_shake->methodPointer,(void*)fire_shake_influence_callback);if(fn)fire_shake_influence_original.store(fn,std::memory_order_release);}
    fps_get=method("UnityEngine","Application","get_targetFrameRate",0);
    fps_set=method("UnityEngine","Application","set_targetFrameRate",1);
    auto input_class=find_class_any("UnityEngine","Input");
    if(input_class&&class_get_methods&&method_get_name&&method_get_param_count&&method_get_param&&type_get_object) {
        auto key_class=find_class_any("UnityEngine","KeyCode");
        auto key_type=key_class?type_get_object(class_get_type(key_class)):nullptr;
        void* iter=nullptr;
        while(key_type) {
            auto m=class_get_methods(input_class,&iter);if(!m)break;
            if(std::strcmp(method_get_name(m),"GetKey")||method_get_param_count(m)!=1)continue;
            auto param=method_get_param(m,0);
            if(param&&type_get_object(param)==key_type){keyboard_key=const_cast<MethodInfo*>(m);break;}
        }
    }
    keyboard_ready.store(keyboard_key!=nullptr,std::memory_order_release);
    room_session_method=method("JarviGames.Utilities","RoomUtility","ADBBCABCBBCCBBCADCCACAC",0);
#if !defined(JM_APK) && !defined(JM_HYBRID)
    native_assist_init();
#endif
    movement_init();vehicle_init();
    farm_init();
    visuals_init();
    init_follow();
    auto pose_update=method("RootMotion.FinalIK","IKSolver","DACDACBCACBCCADBBCBACCC",0);
    uint32_t pose_first=0;
    if(pose_update&&jm_ios_unity_address((void*)pose_update->methodPointer)) {
        auto trampoline=inline_hook::install<PoseUpdateFn>((void*)pose_update->methodPointer,(void*)pose_update_callback);
        if(trampoline)pose_original.store(trampoline,std::memory_order_release);
    }
    struct Candidate { Il2CppClass* klass; const char* name; bool character; };
    auto character_class=find_class("RootMotion.Demos","CharacterThirdPerson");
    Candidate candidates[]={
        {character_class,"Update",true},
        {character_class,"LateUpdate",true},
        {controller_class,"Update",false},
        {controller_class,"LateUpdate",false},
        {find_class_any("Cinemachine","CinemachineBrain"),"LateUpdate",false}
    };
    void* detours[]={(void*)update_callback<0>,(void*)update_callback<1>,(void*)update_callback<2>,(void*)update_callback<3>,(void*)update_callback<4>};
    bool any_connected=false;
    void* connected_addresses[5]={};
    for(int slot=0;slot<5;++slot) {
        const auto& candidate=candidates[slot];
        if(!candidate.klass) continue;
        auto update=class_get_method(candidate.klass,candidate.name,0);
        if(!update||!update->methodPointer) continue;
        char message[128];
        std::snprintf(message,sizeof(message),"Connecting %s.%s",candidate.character?"CharacterThirdPerson":"CameraController",candidate.name);
        phase(message);
        void* address=(void*)update->methodPointer;
        bool duplicate=false;
        for(int previous=0;previous<slot;++previous) if(connected_addresses[previous]==address) duplicate=true;
        if(duplicate) continue;
        auto trampoline=inline_hook::install<LateUpdateFn>(address,detours[slot]);
        if(!trampoline) { phase(inline_hook::last_error); continue; }
        originals[slot].store(trampoline,std::memory_order_release);
        callback_ready[slot].store(true,std::memory_order_release);
        connected_addresses[slot]=address;
        any_connected=true;
        { std::lock_guard<std::mutex> lock(state_mutex); status.hook_ready=true; }

    }
    if(any_connected) {
        std::lock_guard<std::mutex> lock(state_mutex);
        if(!status.frames) std::snprintf(status.initialization,sizeof(status.initialization),"Callbacks installed; waiting for execution");
    } else LOGI(MOD_LABEL " no supported Unity callback could be installed");

}
void Cheat_Init() { if(!started.exchange(true)) std::thread(initialize).detach(); }

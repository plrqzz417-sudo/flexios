#include "MovementControl.h"

// Unity-thread-only movement experiments. Included after IL2CPP helpers.
static MethodInfo *move_native_teleport=nullptr,*move_free_handler=nullptr,*move_hit_normal=nullptr;
struct MoveOptionalRotation {bool has_value=false;uint8_t padding[3]{};Quaternion value{};};
static_assert(sizeof(MoveOptionalRotation)==20&&offsetof(MoveOptionalRotation,value)==4,"IL2CPP Nullable Quaternion layout");
static std::atomic<unsigned> move_input_calls{0};
static std::atomic<unsigned> move_reapplies{0};
static MethodInfo* move_canvas_alpha=nullptr;
static MethodInfo* move_passive_remove=nullptr;
static MethodInfo* move_respawn_interactable=nullptr;
static MethodInfo* move_popup_active=nullptr;
static MethodInfo *move_direct_respawn=nullptr,*move_euler=nullptr,*move_punch=nullptr;
static bool move_direct_sent=false,move_spawn_punched=false;
static Vector3 move_last_euler{};
static GCHandle move_respawn_pending=0;
static unsigned move_revive_room=0;
static std::chrono::steady_clock::time_point move_passive_until{},move_passive_request{};
static MethodInfo *move_set_pos,*move_set_velocity,*move_get_gravity,*move_set_gravity;
static MethodInfo *move_is_kinematic=nullptr,*move_position=nullptr,*move_fixed_delta=nullptr;
static MethodInfo *move_vehicle,*move_gps_active,*move_hit_point,*move_button_event,*move_event_invoke;
static Il2CppObject* move_popup_type=nullptr;
static std::atomic<unsigned> move_tp_requests{0};
static std::atomic<int> move_vertical{0};
static std::mutex move_mutex;
static char move_message[160]="Aguardando jogo";
static GCHandle move_body=0;
static Vector3 move_fly_heading{0,0,1},move_fly_command{};
static std::chrono::steady_clock::time_point move_restore_next{},move_speed_at{};
static float move_speed_factor=1;static uintptr_t move_speed_owner=0;static unsigned move_speed_room=0;
static MethodInfo* move_delta=nullptr;
static std::atomic<bool> move_physics_ready{false};
static bool move_saved_gravity=true,move_had_alive=false,move_death_position=false;
static Vector3 move_last{},move_death{};
static Vector3 move_tp_goal{};
static GCHandle move_tp_body=0;
static MethodInfo* move_get_pos=nullptr;
static std::chrono::steady_clock::time_point move_tp_check{};

void Cheat_RequestMarkerTP(){++move_tp_requests;}
void Cheat_SetFlyVertical(int direction){move_vertical=std::clamp(direction,-1,1);}
void Cheat_GetMovementMessage(char* out,int size){if(!out||size<=0)return;std::lock_guard<std::mutex> lock(move_mutex);std::snprintf(out,size,"%s",move_message);}
static void move_note(const char* text){std::lock_guard<std::mutex> lock(move_mutex);std::snprintf(move_message,sizeof(move_message),"%s",text);}
static bool move_set(MethodInfo* method,uintptr_t self,void* arg,CheatStatus& scratch){
    if(!method||!self||!pointer(self,0x10))return false;
    unsigned errors=scratch.invoke_errors;void* args[]={arg};invoke(method,(void*)self,args,scratch);return errors==unsigned(scratch.invoke_errors);
}
static void move_restore(CheatStatus& scratch){
    const auto now=std::chrono::steady_clock::now();if(now<move_restore_next)return;
    const auto body=rooted(move_body);
    if(body&&pointer(body,0x10)){
        bool kinematic=false;
        if(move_is_kinematic&&value(move_is_kinematic,(void*)body,nullptr,kinematic,scratch)&&!kinematic){
            Vector3 stopped{};move_set(move_set_velocity,body,&stopped,scratch);
        }
        if(!move_set(move_set_gravity,body,&move_saved_gravity,scratch)){
            move_restore_next=now+std::chrono::milliseconds(250);move_note("Fly: aguardando restaurar gravidade");return;
        }
    }
    release_handle(move_body);move_fly_command={};move_restore_next={};
}
static bool move_ground(Vector3 destination,Vector3& out,CheatStatus& scratch){
    if(!finite(destination)||!physics_ray||!ray_hit_class||!array_new||!move_hit_point||!ray_hit_size)return false;
    if(!ray_buffer){auto array=array_new(ray_hit_class,64);if(array)ray_buffer=gchandle_new((Il2CppObject*)array,false);}
    auto array=rooted(ray_buffer);if(!array)return false;
    Vector3 start{destination.x,destination.y+500,destination.z},down{0,-1,0};float distance=1500;int mask=-5,triggers=1,count=0;
    void* args[]={&start,&down,(void*)array,&distance,&mask,&triggers};
    if(!value(physics_ray,nullptr,args,count,scratch)||count<=0||count>=64)return false;
    bool found=false;float best=1e9f;
    for(int i=0;i<count;++i){Vector3 point{};void* raw=(void*)(array+0x20+i*ray_hit_size);
        Vector3 normal{};if(move_hit_normal&&(!value(move_hit_normal,raw,nullptr,normal,scratch)||!finite(normal)||normal.y<.6f))continue;
        if(value(move_hit_point,raw,nullptr,point,scratch)&&finite(point)){
            float error=std::fabs(point.y-destination.y);if(error<best){best=error;out=point;found=true;}
        }
    }
    if(found)out.y+=.5f;return found;
}
static bool move_teleport(uintptr_t local,Vector3 destination,CheatStatus& scratch){
    if(!move_native_teleport||!finite(destination)||!local||!pointer(local,0x10))return false;
    auto vehicles=pointer(local,0x150);uint8_t seated=0,driver=0;
    if(vehicles){read(vehicles+0x10,seated);read(vehicles+0x11,driver);}
    uintptr_t vehicle=0;
    if(seated){
        if(!driver)return false;
        bool found=false;void* args[]={&vehicle};auto controller=pointer(local,0xB0);
        if(!controller||!value(move_vehicle,(void*)controller,args,found,scratch)||!found||!vehicle)return false;
    }
    uintptr_t body=seated?pointer(vehicle,0x78):pointer(pointer(local,0xF8),0x20);
    if(!body||!pointer(body,0x10))return false;
    // This is the same local teleport routine used by TeleportRoomService.
    // It synchronizes puppet/physics/vehicle state. MoveToPoint is a walking
    // coroutine, not teleportation, and must never be used as a TP fallback.
    MoveOptionalRotation rotation{};bool option=false;
    void* args[]={(void*)local,&destination,&rotation,&option};int errors=scratch.invoke_errors;
    invoke(move_native_teleport,nullptr,args,scratch);
    if(scratch.invoke_errors!=errors)return false;
    Vector3 stopped{};move_set(move_set_velocity,body,&stopped,scratch);
    root(move_tp_body,body);move_tp_goal=destination;move_tp_check=std::chrono::steady_clock::now();return true;
}
static bool move_free_popup_ready(uintptr_t popup,CheatStatus& scratch){
    bool active=false,opened=false,button_active=false,available=false;
    if(!popup||!pointer(popup,0x10)||!move_popup_active||!move_respawn_interactable||
       !value(is_active,(void*)popup,nullptr,active,scratch)||!active||
       !value(move_popup_active,(void*)popup,nullptr,opened,scratch)||!opened)return false;
    // Fragment.IsActive is the native open-state, independent of canvas fade.
    // Only the free button +0x58 is inspected; no paid button access.
    const auto button=pointer(popup,0x58);
    return button&&pointer(button,0x10)&&value(is_active,(void*)button,nullptr,button_active,scratch)&&button_active&&
           value(move_respawn_interactable,(void*)button,nullptr,available,scratch)&&available;
}
static GCHandle move_find_free_popup(CheatStatus& scratch){
    if(move_free_popup_ready(rooted(move_respawn_pending),scratch)){
        GCHandle result=0;root(result,rooted(move_respawn_pending));return result;
    }
    if(!move_popup_type||!find_objects||!move_popup_active)return 0;
    void* args[]={move_popup_type};auto array=(Il2CppArray*)invoke(find_objects,nullptr,args,scratch);if(!array)return 0;
    GCHandle matches=0;root(matches,(uintptr_t)array);array=(Il2CppArray*)rooted(matches);if(!array){release_handle(matches);return 0;}
    GCHandle result=0;const auto count=std::min<unsigned>(array_length(array),8);
    for(unsigned i=0;i<count;++i){
        const auto popup=pointer((uintptr_t)array,0x20+i*sizeof(uintptr_t));
        if(move_free_popup_ready(popup,scratch)){root(result,popup);break;}
    }
    release_handle(matches);return result;
}
static bool move_respawn_request(CheatStatus& scratch){
    GCHandle found=move_find_free_popup(scratch);const auto popup=rooted(found);if(!popup){release_handle(found);return false;}
    const auto button=pointer(popup,0x58);const int errors=scratch.invoke_errors;
    auto event=move_button_event?(uintptr_t)invoke(move_button_event,(void*)button,nullptr,scratch):0;
    bool attempted=false;
    if(event&&move_event_invoke){invoke(move_event_invoke,(void*)event,nullptr,scratch);attempted=true;}
    else if(move_free_handler&&scratch.invoke_errors==errors){int free_type=0,zero=0;void* free_args[]={&free_type,&zero};invoke(move_free_handler,(void*)popup,free_args,scratch);attempted=true;}
    const bool sent=attempted&&errors==scratch.invoke_errors;
    if(sent)root(move_respawn_pending,popup);
    release_handle(found);return sent;
}
struct MoveRespawnTransform {Vector3 position,rotation;};
static_assert(sizeof(MoveRespawnTransform)==24,"EntityTransform native layout");
static bool move_request_early(uintptr_t local,CheatStatus& scratch) {
    if(move_direct_sent||!move_direct_respawn||!move_death_position)return false;
    const auto health=pointer(local,0x68);
    int16_t hp=0,max_hp=0;
    // CharacterHpSync: maximum at 0x68, current at 0x6A (native getters).
    if(!health||!pointer(health,0x10)||!read(health+0x6A,hp)||!read(health+0x68,max_hp)||
       hp>0||max_hp<=0||!pointer(health,0x80))return false;
    MoveRespawnTransform destination{move_death,move_last_euler};
    if(!finite(destination.position)||!finite(destination.rotation))return false;
    bool option=false;
    void* args[]={&destination,&option};
    const int errors=scratch.invoke_errors;
    invoke(move_direct_respawn,(void*)health,args,scratch);
    move_direct_sent=scratch.invoke_errors==errors;
    if(move_direct_sent)move_note("Reaparecimento nativo solicitado antes do popup");
    return move_direct_sent;
}
static void movement_drive(uintptr_t local,CheatStatus& scratch,float fixed_step=0){
    const auto tune=Cheat_GetAimbot();
    if(!tune.fly&&!move_body){move_fly_command={};move_vertical=0;return;}
    const auto body=pointer(pointer(local,0xF8),0x20);
    uint32_t vehicle=0;const bool vehicle_known=local&&read(pointer(local,0xB0)+0x3c,vehicle);
    const bool ready=move_physics_ready.load()&&move_set_velocity&&move_get_gravity&&move_set_gravity&&move_is_kinematic;
    const bool active=Auth_Allowed()&&local&&body&&pointer(body,0x10)&&ready&&vehicle_known&&!vehicle&&!tune.cargo_farm&&tune.fly;
    if(move_body&&(!active||rooted(move_body)!=body)){move_restore(scratch);if(move_body)return;}
    if(!active){move_fly_command={};move_vertical=0;return;}
    // All flight integration is physics-only. Render/root callbacks only restore lifecycle state.
    if(!std::isfinite(fixed_step)||fixed_step<=0||fixed_step>.1f)return;
    const auto input=pointer(pointer(local,0x90),0x68);float h=0,v=0;uint8_t locked=1;
    const bool input_known=input&&read(input+0x25,locked)&&read(input+0x28,h)&&read(input+0x2c,v)&&std::isfinite(h)&&std::isfinite(v);
    const bool paused=Menu_IsOpen()||!input_known||locked;
    Vector3 direction{};
    if(!paused){
        // BADDCD... receives animation displacement/delta time, not joystick input.
        // Its displacement is deliberately zero during flight and cannot veto controls.
        Vector3 world{};
        const auto controls=pointer(local,0x140),fallback=pointer(local,0x148);
        bool world_input=controls&&read(controls+0x14,world)&&finite(world);
        if(!world_input)world_input=fallback&&read(fallback+0x10,world)&&finite(world);
        const float n=world_input?std::hypot(world.x,world.z):0;
        if(n>.03f){const float strength=std::min(1.f,n);direction={world.x/n*strength,0,world.z/n*strength};}
        else if(std::hypot(h,v)>.03f){
            uintptr_t sw=0;if(switch_instance)field_static_get(switch_instance,&sw);
            auto camera=pointer(sw,0x28);if(!camera)camera=(uintptr_t)invoke(camera_main,nullptr,nullptr,scratch);
            const auto transform=camera?(uintptr_t)invoke(get_transform,(void*)camera,nullptr,scratch):0;
            Vector3 forward{};
            if(transform&&value(get_forward,(void*)transform,nullptr,forward,scratch)&&finite(forward)){
                const float f=std::hypot(forward.x,forward.z);if(f>.01f)move_fly_heading={forward.x/f,0,forward.z/f};
            }
            direction={move_fly_heading.x*v+move_fly_heading.z*h,0,move_fly_heading.z*v-move_fly_heading.x*h};
        }
    }
    bool kinematic=false;
    if(!value(move_is_kinematic,(void*)body,nullptr,kinematic,scratch)){move_restore(scratch);return;}
    if(!move_body){
        if(!value(move_get_gravity,(void*)body,nullptr,move_saved_gravity,scratch))return;
        root(move_body,body);if(rooted(move_body)!=body){release_handle(move_body);return;}
        move_fly_command={};
    }
    bool gravity=false;if(!move_set(move_set_gravity,body,&gravity,scratch)){move_restore(scratch);return;}
    const float vertical=!paused?float(move_vertical.load()):0;
    const auto desired=movement_control::velocity(direction,tune.fly_speed,vertical,true,0);
    const bool stopped=paused||(desired.x==0&&desired.y==0&&desired.z==0);
    move_fly_command=stopped?Vector3{}:movement_control::approach(move_fly_command,desired,tune.fly_speed,fixed_step,tune.move_response);
    if(kinematic){
        Vector3 pos{};
        if(move_position&&move_get_pos&&value(move_get_pos,(void*)body,nullptr,pos,scratch)&&finite(pos)){
            Vector3 next{pos.x+move_fly_command.x*fixed_step,pos.y+move_fly_command.y*fixed_step,pos.z+move_fly_command.z*fixed_step};
            if(!move_set(move_position,body,&next,scratch))move_restore(scratch);
        }else move_restore(scratch);
    }else if(!move_set(move_set_velocity,body,&move_fly_command,scratch))move_restore(scratch);
}
static void movement_reassert(uintptr_t local,CheatStatus& scratch){
    // Native BADDCD... may run in Update/LateUpdate and overwrite the velocity
    // after FixedUpdate. Reapply the last physics command without integrating it.
    if(!move_body)return;
    const auto tune=Cheat_GetAimbot();const auto body=rooted(move_body);
    uint32_t vehicle=0;
    if(!Auth_Allowed()||!local||!tune.fly||tune.cargo_farm||!body||body!=pointer(pointer(local,0xF8),0x20)||
       !read(pointer(local,0xB0)+0x3c,vehicle)||vehicle){move_restore(scratch);return;}
    bool kinematic=false;
    if(!move_is_kinematic||!value(move_is_kinematic,(void*)body,nullptr,kinematic,scratch)){move_restore(scratch);return;}
    if(kinematic)return; // MovePosition remains exclusive to the physics callback.
    uint8_t locked=1;const auto input=pointer(pointer(local,0x90),0x68);
    if(Menu_IsOpen()||!input||!read(input+0x25,locked)||locked)move_fly_command={};
    const float magnitude=length(move_fly_command);
    if(!finite(move_fly_command)||!std::isfinite(tune.fly_speed)||tune.fly_speed<=0){move_restore(scratch);return;}
    if(magnitude>tune.fly_speed){const float scale=tune.fly_speed/magnitude;move_fly_command.x*=scale;move_fly_command.y*=scale;move_fly_command.z*=scale;}
    if(!move_set(move_set_velocity,body,&move_fly_command,scratch))move_restore(scratch);
    else ++move_reapplies;
}

using MarkerFn=void (*)(void*,int,Vector3,const MethodInfo*);
using MarkerClearFn=void (*)(void*,int,const MethodInfo*);
static std::atomic<MarkerFn> move_marker_original{nullptr};
static std::atomic<MarkerClearFn> move_marker_clear_original{nullptr};
static Vector3 move_user_marker{};
static uintptr_t move_marker_owner=0;
static unsigned move_marker_epoch=0,move_marker_serial=0,move_marker_consumed=0;
static std::chrono::steady_clock::time_point move_marker_at{};
static std::chrono::steady_clock::time_point move_marker_next{};
static unsigned move_marker_attempts=0;static bool move_marker_manual=false,move_marker_ground_ready=false;
static bool move_marker_dispatching=false;
static Vector3 move_marker_ground{};
static constexpr bool user_marker_priority(int priority){return priority==1;}
static void marker_callback(void* self,int priority,Vector3 destination,const MethodInfo* info){
    MarkerFn original=nullptr;while(!(original=move_marker_original.load(std::memory_order_acquire))){}
    original(self,priority,destination,info);
    // GpsPriority.UserInput=1. FromJob=2 must NEVER initiate a teleport.
    if(user_marker_priority(priority)&&finite(destination)){
        uintptr_t local=0;if(local_field)field_static_get(local_field,&local);
        if(local){move_user_marker=destination;move_marker_owner=local;move_marker_epoch=room_epoch;move_marker_at=std::chrono::steady_clock::now();++move_marker_serial;move_marker_next={};move_marker_attempts=0;move_marker_ground_ready=false;}
    }
}
static void marker_clear_callback(void* self,int priority,const MethodInfo* info){
    MarkerClearFn original=nullptr;while(!(original=move_marker_clear_original.load(std::memory_order_acquire))){}
    original(self,priority,info);if(user_marker_priority(priority)&&!move_marker_dispatching)move_marker_consumed=move_marker_serial;
}
static void move_marker_tick(uintptr_t local,const AimbotTune& tune,CheatStatus& scratch){
    const auto now=std::chrono::steady_clock::now();
    if(move_tp_requests.exchange(0)>0)move_marker_manual=true;
    if(!tune.tp_marker&&!move_marker_manual){move_marker_consumed=move_marker_serial;return;}
    if(tune.cargo_farm){move_marker_consumed=move_marker_serial;move_marker_manual=false;return;}
    if(move_marker_serial==move_marker_consumed){move_marker_manual=false;return;}
    if(move_marker_owner!=local||move_marker_epoch!=room_epoch){move_marker_consumed=move_marker_serial;move_marker_manual=false;move_note("Marque novamente apos trocar de sessao/personagem");return;}
    Vector3 actual{};const auto body=pointer(pointer(local,0xF8),0x20);
    if(move_marker_attempts&&move_marker_ground_ready&&value(move_get_pos,(void*)body,nullptr,actual,scratch)&&movement_control::reached(actual,move_marker_ground,1.5f)){
        move_marker_consumed=move_marker_serial;move_marker_manual=false;move_note("TP do marcador confirmado localmente");return;
    }
    uint8_t locked=1;read(pointer(pointer(local,0x90),0x68)+0x25,locked);
    if(Menu_IsOpen()||locked){move_marker_at=now;move_note("TP pendente: aguardando liberar o controle");return;}
    if(now-move_marker_at>std::chrono::seconds(8)){
        move_marker_consumed=move_marker_serial;move_marker_manual=false;move_note("TP expirou sem terreno/chegada confirmada; marque novamente");return;
    }
    if(now<move_marker_next)return;
    if(move_marker_attempts>=3){move_note("TP enviado: aguardando confirmacao final");return;}
    move_marker_next=now+std::chrono::milliseconds(500);
    if(!move_marker_ground_ready){
        if(!move_ground(move_user_marker,move_marker_ground,scratch)){move_note("TP pendente: aguardando terreno carregado");return;}
        move_marker_ground_ready=true;
    }
    if(!move_marker_attempts)move_marker_at=now;
    ++move_marker_attempts;
    move_marker_dispatching=true;const bool sent=move_teleport(local,move_marker_ground,scratch);move_marker_dispatching=false;
    move_note(sent?"TP enviado; aguardando confirmar chegada":"TP pendente: estado nativo ainda indisponivel");
}
#include "AutoRevive.inl"
static void movement_tick(uintptr_t local,bool alive,CheatStatus& scratch){
    const auto now=std::chrono::steady_clock::now();const auto tune=Cheat_GetAimbot();
    auto_revive_tick(local,tune,scratch);
    if(!Auth_Allowed()||!local||!alive){
        move_restore(scratch);release_handle(move_tp_body);move_tp_requests=0;move_vertical=0;
        move_marker_consumed=move_marker_serial;move_marker_manual=false;
        return;
    }
    if(move_tp_body&&now-move_tp_check>std::chrono::seconds(4)){
        Vector3 actual{};const auto body=rooted(move_tp_body);
        if(body&&value(move_get_pos,(void*)body,nullptr,actual,scratch)){
            const float error=length(sub(actual,move_tp_goal));
            move_note(error>8?"TP nao se manteve: posicao corrigida pelo jogo/rede":"TP se manteve na verificacao local de 4 s");
        }
        release_handle(move_tp_body);
    }
    if(revive_cycle.phase==revive_control::Controller::Idle)move_marker_tick(local,tune,scratch);
    movement_drive(local,scratch);
}

using MovementFixedFn=void (*)(void*,const MethodInfo*);
using RootMotionFn=void (*)(void*,Vector3,Quaternion,const MethodInfo*);
static std::atomic<MovementFixedFn> movement_fixed_original{nullptr};
static std::atomic<RootMotionFn> movement_root_original{nullptr};
static std::atomic<unsigned> movement_fixed_calls{0},movement_root_calls{0};
static std::atomic<bool> movement_metadata_ready{false};
static char movement_fixed_error[96]="aguardando",movement_root_error[96]="aguardando";
void Cheat_GetMovementDiagnostics(char* out,int size){
    if(!out||size<=0)return;
    if(!movement_metadata_ready.load(std::memory_order_acquire)){std::snprintf(out,size,"Movimento: inicializando");return;}
    std::snprintf(out,size,"Fisica: %s (%u) | Animacao: %s (%u) | Movimento nativo: %u | Fly pos-nativo: %u | TP nativo: %s",movement_fixed_original.load()?"conectada":movement_fixed_error,movement_fixed_calls.load(),movement_root_original.load()?"conectada":movement_root_error,movement_root_calls.load(),move_input_calls.load(),move_reapplies.load(),move_native_teleport?"conectado":"indisponivel");
}
static bool movement_owned(void* self){uintptr_t local=0;field_static_get(local_field,&local);return local&&pointer(local,0x90)==(uintptr_t)self;}
static void movement_fixed(void* self,const MethodInfo* info){
    MovementFixedFn original=nullptr;while(!(original=movement_fixed_original.load(std::memory_order_acquire))){}
    original(self,info);++movement_fixed_calls;
    if(movement_owned(self)){
        uintptr_t local=0;field_static_get(local_field,&local);CheatStatus scratch;
        if(Auth_Allowed()&&player_state(local,nullptr,scratch,false)==TargetReject::None){
            float dt=0;if(move_fixed_delta)value(move_fixed_delta,nullptr,nullptr,dt,scratch);
            movement_drive(local,scratch,dt);
        }else move_restore(scratch);
    }
}
static void movement_root(void* self,Vector3 delta,Quaternion rotation,const MethodInfo* info){
    RootMotionFn original=nullptr;while(!(original=movement_root_original.load(std::memory_order_acquire))){}
    ++movement_root_calls;bool speed_active=false;
    if(Auth_Allowed()&&movement_owned(self)){
        const auto tune=Cheat_GetAimbot();uintptr_t local=0;field_static_get(local_field,&local);
        uint32_t vehicle=0;CheatStatus scratch;uint8_t locked=1;
        const auto input=pointer(pointer(local,0x90),0x68);
        const bool vehicle_known=read(pointer(local,0xB0)+0x3c,vehicle);
        const bool controls=vehicle_known&&input&&read(input+0x25,locked)&&!locked&&!vehicle;
        const bool alive=player_state(local,nullptr,scratch,false)==TargetReject::None;
        const bool fly_ready=move_physics_ready.load()&&move_set_velocity&&move_get_gravity&&move_set_gravity&&move_is_kinematic&&pointer(pointer(local,0xF8),0x20);
        if(alive&&vehicle_known&&!vehicle&&!tune.cargo_farm&&tune.fly&&fly_ready)delta={};
        speed_active=alive&&controls&&!tune.cargo_farm&&tune.speed&&!tune.fly&&!Menu_IsOpen()&&finite(delta);
        if(speed_active){
            const auto now=std::chrono::steady_clock::now();float dt=0;
            if(move_delta)value(move_delta,nullptr,nullptr,dt,scratch);
            if(move_speed_owner==local&&move_speed_room==room_epoch)dt=std::chrono::duration<float>(now-move_speed_at).count();
            else move_speed_factor=1;
            move_speed_owner=local;move_speed_room=room_epoch;move_speed_at=now;
            move_speed_factor=movement_control::speed_factor(move_speed_factor,std::clamp(tune.move_speed/5.f,.4f,8.f),dt,tune.move_response);
            delta.x*=move_speed_factor;delta.z*=move_speed_factor;
        }
    }
    if(!speed_active){move_speed_factor=1;move_speed_owner=0;move_speed_at={};}
    original(self,delta,rotation,info);
}

using MoveInputFn=void (*)(void*,Vector3,float,bool,const MethodInfo*);
static std::atomic<MoveInputFn> move_input_original{nullptr};
static void move_input_callback(void* self,Vector3 displacement,float delta_time,bool use_body,const MethodInfo* info){
    MoveInputFn original=nullptr;while(!(original=move_input_original.load(std::memory_order_acquire))){}
    if(movement_owned(self)){
        ++move_input_calls;
    }
    original(self,displacement,delta_time,use_body,info);
    if(movement_owned(self)){
        uintptr_t local=0;field_static_get(local_field,&local);CheatStatus scratch;
        if(Auth_Allowed()&&player_state(local,nullptr,scratch,false)==TargetReject::None)movement_reassert(local,scratch);
        else move_restore(scratch);
    }
}
static void movement_init(){
    auto marker=method("JarviGames.Navigation","GlobalPositioningSystem","AACAADACDBBCBCADAADCBBC",2);
    auto clear=method("JarviGames.Navigation","GlobalPositioningSystem","CBDCACAACDDCCBAAABBADDD",1);
    if(marker&&marker->methodPointer){auto fn=inline_hook::install<MarkerFn>((void*)marker->methodPointer,(void*)marker_callback);if(fn)move_marker_original.store(fn,std::memory_order_release);}
    if(clear&&clear->methodPointer){auto fn=inline_hook::install<MarkerClearFn>((void*)clear->methodPointer,(void*)marker_clear_callback);if(fn)move_marker_clear_original.store(fn,std::memory_order_release);}

    move_free_handler=method("JarviGames.Fragments.RespawnPopup","RespawnPopupView","DABACDCBACBACDCCBCCBDDD",2);
    move_native_teleport=method("JarviGames.Services","TeleportRoomService","CBAADDABBCCCDCCDBDABCBA",4);
    auto input=method("RootMotion.Demos","CharacterThirdPerson","BADDCDABCCBBDCBDCDCABAC",3);
    if(input&&input->methodPointer){auto fn=inline_hook::install<MoveInputFn>((void*)input->methodPointer,(void*)move_input_callback);if(fn)move_input_original.store(fn,std::memory_order_release);}

    move_canvas_alpha=method("UnityEngine","CanvasGroup","get_alpha",0);
    move_passive_remove=method("JarviGames.PassiveMode","PlayerPassiveMode","DABCDAACCCBBDACCCDADACC",1);
    move_respawn_interactable=method("UnityEngine.UI","Selectable","get_interactable",0);
    move_popup_active=method("JarviGames.Fragments","FragmentBehaviour","get_IsActive",0);
    move_direct_respawn=method("JarviGames.Character","CharacterHpSync","CDCABACDBADBAABDBCCAADA",2);
    move_euler=method("UnityEngine","Transform","get_eulerAngles",0);
    move_punch=method("","CharController","SetPunch",0);
    move_set_pos=method("UnityEngine","Rigidbody","set_position",1);
    move_get_pos=method("UnityEngine","Rigidbody","get_position",0);
    move_set_velocity=method("UnityEngine","Rigidbody","set_linearVelocity",1);if(!move_set_velocity)move_set_velocity=method("UnityEngine","Rigidbody","set_velocity",1);
    move_get_gravity=method("UnityEngine","Rigidbody","get_useGravity",0);move_set_gravity=method("UnityEngine","Rigidbody","set_useGravity",1);
    move_is_kinematic=method("UnityEngine","Rigidbody","get_isKinematic",0);
    move_position=method("UnityEngine","Rigidbody","MovePosition",1);
    move_fixed_delta=method("UnityEngine","Time","get_fixedDeltaTime",0);
    move_delta=method("UnityEngine","Time","get_deltaTime",0);
    move_vehicle=method("JarviGames.Character","VehicleController","AACCBBDCCDADCBCCABACBDC",1);
    auto gps=find_class_any("","CharacterGps");if(!gps)gps=find_class_any("JarviGames.Gps","CharacterGps");
    move_gps_active=gps?class_get_method(gps,"ADAAAACCDCACBBDDACBDDCC",0):nullptr;
    move_hit_point=method("UnityEngine","RaycastHit","get_point",0);
    move_hit_normal=method("UnityEngine","RaycastHit","get_normal",0);
    auto popup=find_class_any("JarviGames.Fragments.RespawnPopup","RespawnPopupView");
    if(!popup)popup=find_class_any("","RespawnPopupView");
    if(popup)move_popup_type=type_get_object(class_get_type(popup));
    move_button_event=method("UnityEngine.UI","Button","get_onClick",0);
    move_event_invoke=method("UnityEngine.Events","UnityEvent","Invoke",0);
    auto fixed=method("RootMotion.Demos","CharacterThirdPerson","FixedUpdate",0);
    if(fixed&&fixed->methodPointer){auto fn=inline_hook::install<MovementFixedFn>((void*)fixed->methodPointer,(void*)movement_fixed);if(fn)movement_fixed_original.store(fn,std::memory_order_release);else std::snprintf(movement_fixed_error,sizeof(movement_fixed_error),"%s",inline_hook::last_error);}
    auto motion=method("RootMotion.Demos","CharacterThirdPerson","BADCBBCCBDADBBADBADACAC",2);
    if(motion&&motion->methodPointer){auto fn=inline_hook::install<RootMotionFn>((void*)motion->methodPointer,(void*)movement_root);if(fn)movement_root_original.store(fn,std::memory_order_release);else std::snprintf(movement_root_error,sizeof(movement_root_error),"%s",inline_hook::last_error);}

    move_physics_ready.store(movement_fixed_original.load()!=nullptr,std::memory_order_release);
    movement_metadata_ready.store(true,std::memory_order_release);
}

// Local driver only. No new native hook, shared preset mutation or velocity injection.
#include "VehiclePower.h"
struct VehicleBinding {Il2CppClass *sync=nullptr,*physics=nullptr;FieldInfo *controller=nullptr,*power=nullptr;};
static VehicleBinding vehicle_bindings[3];
static MethodInfo* vehicle_resolve=nullptr;
static std::atomic<unsigned> vehicle_ready{0};
static std::atomic<int> vehicle_active{0};
static std::atomic<float> vehicle_multiplier{1};
static GCHandle vehicle_local=0,vehicle_sync=0,vehicle_controller=0;
static int vehicle_kind=-1;static unsigned vehicle_room=0;
static vehicle_power::Value vehicle_value;
static std::chrono::steady_clock::time_point vehicle_checked{};
static void vehicle_status(CheatStatus& result){
    result.vehicle_ready=vehicle_ready.load();result.vehicle_active=vehicle_active.load();result.vehicle_power=vehicle_multiplier.load();
}
static bool vehicle_class(uintptr_t object,Il2CppClass* expected){
    if(!object||!expected||!pointer(object,0x10))return false;
    auto klass=(Il2CppClass*)pointer(object,0);
    for(int depth=0;klass&&depth<16;++depth){
        if(klass==expected)return true;
        klass=class_get_parent?class_get_parent(klass):nullptr;
    }
    return false;
}
static void vehicle_stop(){
    const auto controller=rooted(vehicle_controller);
    if(vehicle_kind>=0&&vehicle_kind<3&&vehicle_class(controller,vehicle_bindings[vehicle_kind].physics)){
        auto field=vehicle_bindings[vehicle_kind].power;float current=0,restore=0;
        if(field&&field_get_value&&field_set_value){
            field_get_value((Il2CppObject*)controller,field,&current);
            if(vehicle_value.restore(current,restore))field_set_value((Il2CppObject*)controller,field,&restore);
        }
    }
    release_handle(vehicle_local);release_handle(vehicle_sync);release_handle(vehicle_controller);
    vehicle_value={};vehicle_kind=-1;vehicle_active=0;vehicle_multiplier=1;vehicle_checked={};
}
static void vehicle_tick(uintptr_t local,bool alive,CheatStatus& scratch){
    const auto tune=Cheat_GetAimbot();
    const bool enabled[]={tune.vehicle_car,tune.vehicle_bike,tune.vehicle_boat};
    const float factors[]={tune.vehicle_car_power,tune.vehicle_bike_power,tune.vehicle_boat_power};
    const bool requested=enabled[0]||enabled[1]||enabled[2];
    if(vehicle_room!=room_epoch){vehicle_stop();vehicle_room=room_epoch;return;}
    if(!requested||!Auth_Allowed()||!local||!alive||
       (rooted(vehicle_local)&&rooted(vehicle_local)!=local)){
        vehicle_stop();vehicle_room=room_epoch;
        if(!requested||!Auth_Allowed()||!local||!alive)return;
    }
    if(vehicle_kind>=0&&(!enabled[vehicle_kind]||factors[vehicle_kind]<=1))vehicle_stop();
    uint8_t seated=0,driver=0;auto state=pointer(local,0x150);
    if(!state||!read(state+0x10,seated)||!read(state+0x11,driver)||!seated||!driver){vehicle_stop();return;}
    if(!vehicle_resolve||!field_get_value||!field_set_value){vehicle_stop();return;}
    const auto now=std::chrono::steady_clock::now();
    if(vehicle_checked.time_since_epoch().count()&&now-vehicle_checked<std::chrono::milliseconds(100))return;
    vehicle_checked=now;
    uintptr_t sync=0;bool found=false;void* args[]={&sync};const auto character=pointer(local,0xB0);
    if(!character||!value(vehicle_resolve,(void*)character,args,found,scratch)||!found||!sync){vehicle_stop();return;}
    int kind=-1;for(int i=0;i<3;++i)if(vehicle_class(sync,vehicle_bindings[i].sync)){kind=i;break;}
    if(kind<0||!enabled[kind]||!std::isfinite(factors[kind])||factors[kind]<=1||factors[kind]>3){vehicle_stop();return;}
    auto& binding=vehicle_bindings[kind];uintptr_t controller=0;
    if(!binding.controller||!binding.power){vehicle_stop();return;}
    field_get_value((Il2CppObject*)sync,binding.controller,&controller);
    if(!vehicle_class(controller,binding.physics)){vehicle_stop();return;}
    if(kind!=vehicle_kind||sync!=rooted(vehicle_sync)||controller!=rooted(vehicle_controller)){
        vehicle_stop();root(vehicle_local,local);root(vehicle_sync,sync);root(vehicle_controller,controller);
        if(!rooted(vehicle_local)||!rooted(vehicle_sync)||!rooted(vehicle_controller)){vehicle_stop();return;}
        vehicle_kind=kind;vehicle_room=room_epoch;vehicle_checked=now;
    }
    float current=0,desired=0;field_get_value((Il2CppObject*)controller,binding.power,&current);
    const bool write=vehicle_value.update(current,factors[kind],desired);
    if(!vehicle_power::Value::valid(desired)){vehicle_stop();return;}
    if(write){
        field_set_value((Il2CppObject*)controller,binding.power,&desired);
        float actual=0;field_get_value((Il2CppObject*)controller,binding.power,&actual);
        if(!vehicle_power::Value::same(actual,desired)){vehicle_stop();return;}
        vehicle_value.committed(desired);
    }
    vehicle_active=vehicle_value.applied?kind+1:0;vehicle_multiplier=vehicle_value.applied?factors[kind]:1;
}
static void vehicle_init(){
    if(!class_get_field||!field_get_offset||!field_get_value||!field_set_value)return;
    vehicle_resolve=method("JarviGames.Character","VehicleController","AACCBBDCCDADCBCCABACBDC",1);
    const char* sync_names[]={"CarSync","BikeSync","BoatSync"};
    const char* physics_names[]={"VehicleController","BikeController","Boat"};
    const char* power_names[]={"maxDriveForce","EngineTorque","enginePower"};
    const size_t offsets[]={0x78,0x10C,0x28};unsigned ready=0;
    for(int i=0;i<3;++i){
        auto& b=vehicle_bindings[i];b.sync=find_class_any("JarviGames",sync_names[i]);
        b.physics=find_class_any(i==0?"EVP":"",physics_names[i]);
        b.controller=b.sync?class_get_field(b.sync,i==2?"boat":"controller"):nullptr;
        b.power=b.physics?class_get_field(b.physics,power_names[i]):nullptr;
        if(!b.controller||!b.power||field_get_offset(b.controller)!=0x378||field_get_offset(b.power)!=offsets[i]){b={};continue;}
        if(vehicle_resolve)ready|=1u<<i;
    }
    vehicle_ready.store(ready,std::memory_order_release);
}

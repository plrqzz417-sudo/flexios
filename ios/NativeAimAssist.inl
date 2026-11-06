#if !defined(JM_APK) && !defined(JM_HYBRID)
// iOS method ownership is checked against the loaded UnityFramework image.
// All access stays on the Unity callback thread; no new native detours.
static MethodInfo* native_assist_getter=nullptr;
static Il2CppClass* native_assist_class=nullptr;
static FieldInfo* native_assist_enabled_field=nullptr;
static FieldInfo* native_assist_fields[5]{};
static GCHandle native_assist_service=0;
struct NativeAssistValues {bool enabled=false;float values[5]{};};
static NativeAssistValues native_assist_saved{},native_assist_applied{};
static bool native_assist_has_applied=false;
static std::atomic<bool> native_assist_ready{false},native_assist_active{false};
using NativeAssistScalarFn=float(*)(const MethodInfo*);
static std::atomic<NativeAssistScalarFn> native_assist_originals[5];
static std::atomic<float> native_assist_gains[5];
static std::atomic<unsigned> native_assist_calls{0},native_assist_scaled{0};

template<int Index> static float native_assist_scalar(const MethodInfo* info) {
    NativeAssistScalarFn original=nullptr;
    while(!(original=native_assist_originals[Index].load(std::memory_order_acquire))){}
    const float base=original(info);
    if(!native_assist_active.load(std::memory_order_acquire)||!Auth_Allowed()||
       !std::isfinite(base)||base<0)return base;
    native_assist_calls.fetch_add(1,std::memory_order_relaxed);
    constexpr float caps[5]={480.f,50.f,1200.f,.012f,.0036f};
    const float gain=native_assist_gains[Index].load(std::memory_order_relaxed);
    const float result=std::max(base,std::min(caps[Index],base*gain));
    if(result!=base)native_assist_scaled.fetch_add(1,std::memory_order_relaxed);
    return result;
}

static bool native_assist_read_raw(uintptr_t service,NativeAssistValues& result) {
    if(!service||pointer(service,0)!=(uintptr_t)native_assist_class||!field_get_value)return false;
    field_get_value((Il2CppObject*)service,native_assist_enabled_field,&result.enabled);
    for(int i=0;i<5;++i)field_get_value((Il2CppObject*)service,native_assist_fields[i],&result.values[i]);
    return true;
}
static bool native_assist_read(uintptr_t service,NativeAssistValues& result) {
    if(!native_assist_read_raw(service,result))return false;
    // A stale or uninitialized service must never receive writes.
    constexpr float limits[5]={5000.f,1000.f,1000.f,1000.f,1000.f};
    for(int i=0;i<5;++i)if(!std::isfinite(result.values[i])||result.values[i]<0||result.values[i]>limits[i])return false;
    return result.values[1]<=result.values[2];
}

static void native_assist_restore() {
    const auto service=rooted(native_assist_service);
    NativeAssistValues now{};
    if(native_assist_has_applied&&native_assist_read_raw(service,now)&&field_set_value) {
        if(now.enabled==native_assist_applied.enabled)
            field_set_value((Il2CppObject*)service,native_assist_enabled_field,&native_assist_saved.enabled);
        for(int i=0;i<5;++i)if(now.values[i]==native_assist_applied.values[i])
            field_set_value((Il2CppObject*)service,native_assist_fields[i],&native_assist_saved.values[i]);
    }
    release_handle(native_assist_service);
    native_assist_has_applied=false;
    native_assist_active.store(false,std::memory_order_release);
}

static void native_assist_tick(bool allowed,const AimbotTune& tune) {
    if(!allowed||!tune.native_assist||!native_assist_ready.load(std::memory_order_acquire)){
        if(native_assist_service)native_assist_restore();
        return;
    }
    static auto next=std::chrono::steady_clock::time_point{};
    const auto now=std::chrono::steady_clock::now();
    if(now<next)return;
    next=now+std::chrono::milliseconds(100);
    CheatStatus scratch{};
    const auto service=(uintptr_t)invoke(native_assist_getter,nullptr,nullptr,scratch);
    if(scratch.invoke_errors||!service||pointer(service,0)!=(uintptr_t)native_assist_class) {
        if(native_assist_service)native_assist_restore();
        return;
    }
    if(rooted(native_assist_service)!=service){
        if(native_assist_service)native_assist_restore();
        root(native_assist_service,service);
        native_assist_has_applied=false;
    }
    if(!rooted(native_assist_service))return;
    NativeAssistValues current{};
    if(!native_assist_read(service,current)){
        native_assist_active.store(false,std::memory_order_release);
        return;
    }
    if(!native_assist_has_applied)native_assist_saved=current;
    else {
        if(current.enabled!=native_assist_applied.enabled)native_assist_saved.enabled=current.enabled;
        for(int i=0;i<5;++i)if(current.values[i]!=native_assist_applied.values[i])
            native_assist_saved.values[i]=current.values[i];
    }
    NativeAssistValues desired=native_assist_saved;
    desired.enabled=true;
    const float gains[5]={tune.assist_distance,tune.assist_radius_min,tune.assist_radius_max,
                          tune.assist_x,tune.assist_y};
    for(int i=0;i<5;++i)native_assist_gains[i].store(gains[i],std::memory_order_relaxed);
    // WeaponAnimations clamps SettingsService values before using them. Keep
    // the service's original floats and scale the verified post-clamp helpers.
    if(current.enabled!=desired.enabled)
        field_set_value((Il2CppObject*)service,native_assist_enabled_field,&desired.enabled);
    for(int i=0;i<5;++i)if(current.values[i]!=desired.values[i])
        field_set_value((Il2CppObject*)service,native_assist_fields[i],&desired.values[i]);
    native_assist_applied=desired;
    native_assist_has_applied=true;
    native_assist_active.store(true,std::memory_order_release);
}

static void native_assist_init() {
    native_assist_class=find_class_any("JarviGames.Services","SettingsService");
    native_assist_getter=method("JarviGames.Utilities","SettingsUtility","ADAACDCBDBCCDBDADBCDDAD",0);
    if(!native_assist_class||!native_assist_getter||!native_assist_getter->methodPointer||
       !field_get_offset||!field_get_value||!field_set_value||!gchandle_new||!gchandle_get_target||!gchandle_free)return;
    Dl_info image{};
    const auto address=(uintptr_t)native_assist_getter->methodPointer;
    if(!jm_ios_unity_address((void*)address))return;
    native_assist_enabled_field=class_get_field(native_assist_class,"_autoAim");
    constexpr const char* names[5]={"_aimAssistDistanceMax","_aimAssistRadiusMin","_aimAssistRadiusMax",
                                    "_aimAssistXSpeed","_aimAssistYSpeed"};
    constexpr size_t offsets[5]={0x9c,0xa0,0xa4,0xa8,0xac};
    if(!native_assist_enabled_field||field_get_offset(native_assist_enabled_field)!=0x98)return;
    for(int i=0;i<5;++i){
        native_assist_fields[i]=class_get_field(native_assist_class,names[i]);
        if(!native_assist_fields[i]||field_get_offset(native_assist_fields[i])!=offsets[i])return;
    }
    auto weapon=find_class_any("JarviGames.Character","WeaponAnimations");
    if(!weapon)return;
    constexpr const char* helper_names[5]={
        "CBDABCBDAACBACADACABDBD", // distance, native cap 120
        "ACACCDBABCBDBDCCABDDDBB", // minimum radius, native cap 20
        "BACBADDBCBBADBDADCBBACC", // maximum radius, native range 50..400
        "DDCBACADCABDABCBBBBDDCB", // X speed, native range .0002..002
        "CDBBCBBACBBDABADDCDACAA"  // Y speed, native range .00005..0006
    };
    void* detours[5]={(void*)native_assist_scalar<0>,(void*)native_assist_scalar<1>,
                      (void*)native_assist_scalar<2>,(void*)native_assist_scalar<3>,
                      (void*)native_assist_scalar<4>};
    for(int i=0;i<5;++i){
        auto helper=class_get_method(weapon,helper_names[i],0);
        if(!helper||!helper->methodPointer)return;
        const auto target=(uintptr_t)helper->methodPointer;
        if(!jm_ios_unity_address((void*)target))return;
        auto original=inline_hook::install<NativeAssistScalarFn>((void*)target,detours[i]);
        if(!original)return;
        native_assist_originals[i].store(original,std::memory_order_release);
    }
    native_assist_ready.store(true,std::memory_order_release);
}
#endif

// Local model transforms and renderer property blocks; no shared material writes.
static MethodInfo* visual_property_id=nullptr;
static int visual_color_ids[3]={};
static bool visual_ids_ready=false;
static MethodInfo *visual_scale_get,*visual_scale_set,*visual_rot_get,*visual_rot_set;
static MethodInfo *visual_block_ctor,*visual_block_get,*visual_block_set,*visual_color_set;
static Il2CppClass* visual_block_class=nullptr;
static Il2CppObject* visual_renderer_type=nullptr;
static Il2CppObject* (*visual_string_new)(const char*)=nullptr;
static GCHandle visual_model=0,visual_weapon=0,visual_work_block=0;
static Vector3 visual_saved_scale{};static Quaternion visual_saved_rotation{};
static float visual_angle=0;static std::chrono::steady_clock::time_point visual_time{},visual_refresh{};
struct VisualMaterial {GCHandle renderer=0,original=0;};
static std::vector<VisualMaterial> visual_materials;
struct VisualColor{float r,g,b,a;};
static MethodInfo* visual_method(const char* ns,const char* cls,const char* name,int count,const char* first_type){
    auto k=find_class_any(ns,cls);if(!k||!class_get_methods||!method_get_param_count||!method_get_name||!type_get_name)return nullptr;
    void* it=nullptr;while(auto m=class_get_methods(k,&it)){
        if(std::strcmp(method_get_name(m),name)||int(method_get_param_count(m))!=count)continue;
        auto type=method_get_param(m,0);auto text=type?type_get_name(type):nullptr;
        if(text&&!std::strcmp(text,first_type))return const_cast<MethodInfo*>(m);
    }return nullptr;
}
static void visual_restore_model(CheatStatus& scratch){
    auto model=rooted(visual_model);
    if(model&&pointer(model,0x10)){move_set(visual_scale_set,model,&visual_saved_scale,scratch);move_set(visual_rot_set,model,&visual_saved_rotation,scratch);}
    release_handle(visual_model);visual_angle=0;
}
static void visual_restore_weapon(CheatStatus& scratch){
    for(auto& item:visual_materials){auto renderer=rooted(item.renderer),block=rooted(item.original);
        if(renderer&&block&&pointer(renderer,0x10)){void* args[]={(void*)block};invoke(visual_block_set,(void*)renderer,args,scratch);}
        release_handle(item.renderer);release_handle(item.original);
    }
    visual_materials.clear();release_handle(visual_weapon);release_handle(visual_work_block);
}
static Il2CppObject* visual_new_block(CheatStatus& scratch){
    if(!object_new||!visual_block_class||!visual_block_ctor)return nullptr;
    auto block=object_new(visual_block_class);if(block)invoke(visual_block_ctor,block,nullptr,scratch);return block;
}
static void visuals_tick(uintptr_t local,bool alive,CheatStatus& scratch){
    auto tune=Cheat_GetAimbot();auto now=std::chrono::steady_clock::now();
    if(!local||!alive||!Auth_Allowed()){visual_restore_model(scratch);visual_restore_weapon(scratch);visual_time=now;return;}
    if(!tune.spin_visual&&!tune.tiny_visual&&!tune.weapon_tint&&!visual_model&&!visual_weapon&&!visual_work_block&&visual_materials.empty()){visual_time=now;return;}
    auto animator=pointer(pointer(local,0x48),0x38);
    auto candidate=animator?(uintptr_t)invoke(get_transform,(void*)animator,nullptr,scratch):0;
    auto root_transform=(uintptr_t)invoke(get_transform,(void*)local,nullptr,scratch);
    // Never shrink/rotate the physics root or the camera.
    if(candidate==root_transform){int hips=0;void* args[]={&hips};candidate=(uintptr_t)invoke(get_bone,(void*)animator,args,scratch);}
    if(candidate==root_transform)candidate=0;
    if(rooted(visual_model)&&((!tune.spin_visual&&!tune.tiny_visual)||rooted(visual_model)!=candidate))visual_restore_model(scratch);
    if(candidate&&(tune.spin_visual||tune.tiny_visual)&&visual_scale_get&&visual_rot_get&&visual_scale_set&&visual_rot_set){
        if(!visual_model){
            if(value(visual_scale_get,(void*)candidate,nullptr,visual_saved_scale,scratch)&&value(visual_rot_get,(void*)candidate,nullptr,visual_saved_rotation,scratch))root(visual_model,candidate);
        }
        if(rooted(visual_model)){
            float dt=std::clamp(std::chrono::duration<float>(now-visual_time).count(),0.f,.05f);
            visual_angle=std::fmod(visual_angle+dt*tune.spin_speed,360.f);
            Vector3 scale=visual_saved_scale;
            if(tune.tiny_visual){scale.x*=tune.model_scale;scale.y*=tune.model_scale;scale.z*=tune.model_scale;}
            move_set(visual_scale_set,candidate,&scale,scratch);
            if(tune.spin_visual){
                float half=visual_angle*.00872664626f;Quaternion a{0,std::sin(half),0,std::cos(half)},b=visual_saved_rotation;
                Quaternion q{a.w*b.x+a.y*b.z,a.w*b.y+a.y*b.w,a.w*b.z-a.y*b.x,a.w*b.w-a.y*b.y};move_set(visual_rot_set,candidate,&q,scratch);
            }else move_set(visual_rot_set,candidate,&visual_saved_rotation,scratch);
        }
    }
    visual_time=now;
    auto weapon=pointer(pointer(local,0xD0),0xF8);
    if(rooted(visual_weapon)&&(!tune.weapon_tint||rooted(visual_weapon)!=weapon))visual_restore_weapon(scratch);
    if(!tune.weapon_tint||!weapon||!visual_color_set||!visual_string_new||!visual_property_id||!visual_renderer_type||!visual_block_get||!visual_block_set)return;
    if(!visual_ids_ready){
        const char* names[]={"_BaseColor","_Color","_TintColor"};bool valid=true;
        for(int i=0;i<3;++i){auto text=visual_string_new(names[i]);void* args[]={text};valid=value(visual_property_id,nullptr,args,visual_color_ids[i],scratch)&&valid;}
        visual_ids_ready=valid;if(!valid)return;
    }
    if(!visual_weapon){
        auto go=invoke(get_game_object,(void*)weapon,nullptr,scratch);bool inactive=false;void* args[]={visual_renderer_type,&inactive};
        auto renderers=go?(Il2CppArray*)invoke(child_renderers,go,args,scratch):nullptr;if(!renderers)return;
        root(visual_weapon,weapon);
        for(unsigned i=0;i<std::min<unsigned>(array_length(renderers),64);++i){
            auto renderer=pointer((uintptr_t)renderers,0x20+i*sizeof(uintptr_t));if(!renderer||!pointer(renderer,0x10))continue;
            auto block=visual_new_block(scratch);if(!block)continue;
            void* args[]={block};unsigned errors=scratch.invoke_errors;invoke(visual_block_get,(void*)renderer,args,scratch);
            if(errors!=unsigned(scratch.invoke_errors))continue;
            VisualMaterial item;root(item.renderer,renderer);root(item.original,(uintptr_t)block);visual_materials.push_back(item);
        }
        auto working=visual_new_block(scratch);if(working)root(visual_work_block,(uintptr_t)working);
    }
    if(std::chrono::duration<float>(now-visual_refresh).count()<.15f)return;visual_refresh=now;
    auto block=rooted(visual_work_block);if(!block)return;
    VisualColor color{tune.weapon_color[0],tune.weapon_color[1],tune.weapon_color[2],1};
    for(auto& item:visual_materials){
        auto renderer=rooted(item.renderer);if(!renderer||!pointer(renderer,0x10))continue;
        void* get_args[]={(void*)block};invoke(visual_block_get,(void*)renderer,get_args,scratch);
        for(int id:visual_color_ids){void* args[]={&id,&color};invoke(visual_color_set,(void*)block,args,scratch);}
        invoke(visual_block_set,(void*)renderer,get_args,scratch);
    }
}
static void visuals_init(){
    auto renderer_class=find_class_any("UnityEngine","Renderer");
    if(renderer_class)visual_renderer_type=type_get_object(class_get_type(renderer_class));
    visual_scale_get=method("UnityEngine","Transform","get_localScale",0);visual_scale_set=method("UnityEngine","Transform","set_localScale",1);
    visual_rot_get=method("UnityEngine","Transform","get_localRotation",0);visual_rot_set=method("UnityEngine","Transform","set_localRotation",1);
    visual_block_class=find_class_any("UnityEngine","MaterialPropertyBlock");
    visual_block_ctor=method("UnityEngine","MaterialPropertyBlock",".ctor",0);
    visual_block_get=method("UnityEngine","Renderer","GetPropertyBlock",1);visual_block_set=method("UnityEngine","Renderer","SetPropertyBlock",1);
    visual_color_set=visual_method("UnityEngine","MaterialPropertyBlock","SetColor",2,"System.Int32");
    visual_string_new=(decltype(visual_string_new))dlsym(libil2cpp_handle,"il2cpp_string_new");
    visual_property_id=method("UnityEngine","Shader","PropertyToID",1);
    if(!visual_string_new||!visual_property_id)visual_color_set=nullptr;
}

// Prefer a genuine head intersection behind the selected player's body. This
// runs in the EXISTING cast hook after original; no classifier/IK detour.
struct NullableHeadHit {uint32_t has_value=0;NativeHeadHit value{};};
static_assert(sizeof(NullableHeadHit)==32&&offsetof(NullableHeadHit,value)==4,"Nullable<HitPoint> ABI");
struct HeadShapeQuery {Il2CppClass* klass=nullptr;MethodInfo* method=nullptr;};
static HeadShapeQuery head_shape_queries[2];
static MethodInfo* head_shape_query(uintptr_t shape,Il2CppClass* klass,uint32_t expected){
    // Only these two exact shape classes are admitted. This caches successful
    // method/ABI validation, not objects, intersections or game class discovery.
    for(const auto& entry:head_shape_queries)if(entry.klass==klass&&entry.method)return entry.method;
    using ClassFromType=Il2CppClass* (*)(const Il2CppType*);
    using ValueSize=int32_t (*)(Il2CppClass*,uint32_t*);
    static auto from_type=(ClassFromType)dlsym(libil2cpp_handle,"il2cpp_class_from_type");
    static auto value_size=(ValueSize)dlsym(libil2cpp_handle,"il2cpp_class_value_size");
    if(!from_type||!value_size||!method_get_param||!field_get_offset)return nullptr;
    auto query=instance_method(shape,"DCCDADCCDBBACDBADBCBDCC",3);uint32_t first=0;
    if(!query||!jm_ios_unity_address((void*)query->methodPointer))return nullptr;
    auto parameter=method_get_param(query,2);auto nullable=parameter?from_type(parameter):nullptr;uint32_t alignment=0;
    auto has_value=nullable?class_get_field(nullable,"hasValue"):nullptr;
    auto value_field=nullable?class_get_field(nullable,"value"):nullptr;
    if(!has_value||!value_field||value_size(nullable,&alignment)!=32||
       field_get_offset(has_value)!=16||field_get_offset(value_field)!=20)return nullptr;
    for(auto& entry:head_shape_queries)if(!entry.method){entry={klass,query};break;}
    return query;
}
static bool shot_head_intersection(uintptr_t player,Vector3 origin,Vector3 direction,float range,
                                   NativeHeadHit& closest,uintptr_t& transform,CheatStatus& current) {
    const float magnitude=length(direction);
    if(!finite(origin)||!finite(direction)||magnitude<.0001f||!std::isfinite(range)||range<=0)return false;
    direction={direction.x/magnitude,direction.y/magnitude,direction.z/magnitude};
    HeadManagerScope manager(player,current);const auto object=rooted(manager.handle);if(!object)return false;
    auto list=pointer(object,0x20),items=pointer(list,0x10);int count=0;
    if(!list||!items||!read(list+0x18,count)||count<1||count>64||unsigned(count)>array_length((Il2CppArray*)items))return false;
    bool found=false;closest.distance=range;
    for(int i=0;i<count;++i) {
        auto shape=pointer(items,0x20+i*sizeof(uintptr_t));int type=-1;
        if(!shape||!read(shape+0x10,type)||type!=0)continue;
        const auto klass=(Il2CppClass*)pointer(shape,0);
        const auto expected=klass==sphere_class?0xD10483FFu:klass==capsule_class?0xD10583FFu:0u;
        auto target=pointer(shape,0x18);if(!expected||!target||!pointer(target,0x10))continue;
        auto query=head_shape_query(shape,klass,expected);if(!query)continue;
        NullableHeadHit result{};bool hit=false;GCHandle rooted_shape=0;root(rooted_shape,shape);
        shape=rooted(rooted_shape);
        void* args[]={&origin,&direction,&result};
        const bool invoked=shape&&value(query,(void*)shape,args,hit,current);
        release_handle(rooted_shape);
        const auto& point=result.value;
        if(!invoked||!hit||(result.has_value&255)!=1||!finite(point.point)||!finite(point.normal)||
           !std::isfinite(point.distance)||point.distance<0||point.distance>closest.distance)continue;
        const Vector3 expected_point{origin.x+direction.x*point.distance,origin.y+direction.y*point.distance,origin.z+direction.z*point.distance};
        if(length(sub(point.point,expected_point))>.02f)continue;
        closest=point;transform=target;found=true;
    }
    return found;
}
// Correct only an identified local head shot whose barrel ray misses the
// native head shape by a small pose/third-person offset. The replacement ray
// must itself intersect that shape; otherwise leave the game's ray intact.
static bool steer_selected_head_ray(Vector3 origin,Vector3& direction,float range,const AimbotTune& tune) {
    if(!shot_context.scope||!tune.enabled||tune.revision!=applied_revision||
       !finite(origin)||!finite(direction)||!std::isfinite(range)||range<.1f)return false;
    auto& selected=tune.silent?silent_tracking:tracking;
    const float age=std::chrono::duration<float>(std::chrono::steady_clock::now()-selected.sampled).count();
    if((selected.bone<0?tune.bone:selected.bone)!=0||(!tune.silent&&!selected.active)||
       (tune.silent&&!shot_context.active)||age<0||age>.15f)return false;
    const auto player=selected_player(selected);CheatStatus scratch;uintptr_t local=0;
    field_static_get(local_field,&local);
    if(!player||!local||player==local||
       player_state(local,nullptr,scratch,false)!=TargetReject::None||
       player_state(player,nullptr,scratch,false)!=TargetReject::None||
       aim_eligibility(player,tune,scratch)!=AimReject::None)return false;
    const float limit=std::min(range,tune.distanceMax);
    NativeHeadHit intersection{};uintptr_t transform=0;
    if(shot_head_intersection(player,origin,direction,limit,intersection,transform,scratch))return false;
    Vector3 center{};int source=0;
    if(!damage_head_point(player,center,source,scratch)||source!=1)return false;
    Vector3 mesh{};int mesh_source=0;
    if(tune.silent&&shot_context.active)mesh=shot_context.point;
    else if(!posed_point(player,10,mesh,true)&&!posed_point(player,10,mesh)&&
            !live_head_point(player,mesh,mesh_source,scratch))return false;
    const auto gap=sub(center,mesh);
    if(!finite(gap)||length(gap)>.12f||std::fabs(gap.y)>.06f)return false;
    Vector3 candidate{};
    if(!redirected_direction(origin,direction,center,std::min(12.f,2.f*tune.silent_max_angle),limit,candidate)||
       !line_visible(local,player,origin,center)||
       !shot_head_intersection(player,origin,candidate,limit,intersection,transform,scratch))return false;
    direction=candidate;
    return true;
}
static bool promote_selected_head(HitInfoResult& hit,Vector3 origin,Vector3 direction,float range,const AimbotTune& tune) {
    const auto part=unsigned((hit.damaged>>32)&255);
    if(!tune.enabled||tune.revision!=applied_revision||hit.flags||part<2||part>4||!uint32_t(hit.damaged))return false;
    auto& selected=tune.silent?silent_tracking:tracking;
    const float age=std::chrono::duration<float>(std::chrono::steady_clock::now()-selected.sampled).count();
    if((selected.bone<0?tune.bone:selected.bone)!=0||(!tune.silent&&!selected.active)||
       (tune.silent&&!shot_context.active)||age<0||age>.15f)return false;
    auto player=selected_player(selected);uint32_t entity=0;
    if(!player||!read(player+0x20,entity)||entity!=uint32_t(hit.damaged))return false;
    CheatStatus scratch;uintptr_t local=0;field_static_get(local_field,&local);
    if(!local||local==player||player_state(local,nullptr,scratch,false)!=TargetReject::None||
       player_state(player,nullptr,scratch,false)!=TargetReject::None||aim_eligibility(player,tune,scratch)!=AimReject::None)return false;
    ++hs_checked;NativeHeadHit head{};uintptr_t transform=0;
    if(!shot_head_intersection(player,origin,direction,std::min(range,tune.distanceMax),head,transform,scratch)){++hs_missing;return false;}
    // Original has already hit this exact player. Still check walls between the
    // nearer body and head; never promote a head through world geometry.
    if(!line_visible(local,player,origin,head.point))return false;
    hit.damaged=(hit.damaged&~(uint64_t(255)<<32))|(uint64_t(1)<<32);
    hit.point=head.point;hit.transform=transform;++hs_promoted;return true;
}

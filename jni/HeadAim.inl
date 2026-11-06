// Read-only use of HitBoxObject's native closest-shape classifier. No detour,
// damage rewrite, collider mutation or mesh/native-offset substitution.
struct NativeHeadHit {Vector3 point{},normal{};float distance=0;};
static_assert(sizeof(NativeHeadHit)==28,"HitPoint ABI");
struct HeadManagerScope {
    GCHandle handle=0;
    HeadManagerScope(uintptr_t player,CheatStatus& current) {
        if(!hit_classifier||!child_animator||!hit_controller_type||!hit_manager)return;
        bool inactive=false;void* args[]={hit_controller_type,&inactive};
        auto controller=(uintptr_t)invoke(child_animator,(void*)player,args,current);
        if(!controller||!pointer(controller,0x10))return;
        GCHandle owner=0;root(owner,controller);
        controller=rooted(owner);
        const auto manager=controller?(uintptr_t)invoke(hit_manager,(void*)controller,nullptr,current):0;
        if(manager&&pointer(manager,0x10)&&pointer(manager,0x28)==player)root(handle,manager);
        release_handle(owner);
    }
    ~HeadManagerScope(){release_handle(handle);}
    HeadManagerScope(const HeadManagerScope&)=delete;
    HeadManagerScope& operator=(const HeadManagerScope&)=delete;
};
static bool native_head_ray(uintptr_t manager,Vector3 origin,Vector3 point,float range,CheatStatus& current) {
    const auto delta=sub(point,origin);const float distance=length(delta);
    if(!manager||!hit_classifier||!pointer(manager,0x10)||!finite(origin)||!finite(delta)||
       !std::isfinite(range)||distance<.1f||distance>range)return false;
    // The second Vector3 is a NORMALIZED DIRECTION, not an endpoint. Native
    // shapes reconstruct their hit point as origin + direction * hit.distance.
    Vector3 direction{delta.x/distance,delta.y/distance,delta.z/distance};
    NativeHeadHit hit{};int damage_type=-1;bool found=false;
    void* args[]={&origin,&direction,&hit,&damage_type};
    if(!value(hit_classifier,(void*)manager,args,found,current)||!found||damage_type!=0||
       !finite(hit.point)||!finite(hit.normal)||!std::isfinite(hit.distance)||hit.distance<0||hit.distance>range)return false;
    const Vector3 expected{origin.x+direction.x*hit.distance,origin.y+direction.y*hit.distance,origin.z+direction.z*hit.distance};
    return length(sub(expected,hit.point))<=.02f;
}
static bool verified_head_point(uintptr_t player,Vector3& point,int& source,CheatStatus& current,
                                Vector3 origin,const AimbotTune& tune,bool mesh_clear) {
    if(!finite(point)||!finite(origin)||!std::isfinite(tune.distanceMax)||tune.distanceMax<.1f){source=-1;return false;}
    const auto preferred=point;
    const auto preferred_source=source;
    // Pose is authoritative. Native geometry may lag a bent/aiming mesh: never
    // pull the target back to an upright head or label an unverified pose as HS.
    const auto retain_pose=[&]() {
        point=preferred;source=preferred_source;
        if(length(sub(point,origin))<.1f||length(sub(point,origin))>tune.distanceMax||
           (tune.require_visible&&!line_visible(observed_local,player,origin,point))){source=-1;return false;}
        return true;
    };
    HeadManagerScope scope(player,current);const auto manager=rooted(scope.handle);
    if(!manager)return retain_pose();
    if(native_head_ray(manager,origin,point,tune.distanceMax,current)&&
       (!tune.require_visible||line_visible(observed_local,player,origin,point)))return true;
    // Native refinement is bounded to 3.5cm around the selected animated point.
    // A disconnected/static damage head is not a fallback aiming location.
    (void)mesh_clear;
    auto list=pointer(manager,0x20),items=pointer(list,0x10);int count=0;
    if(!list||!items||!read(list+0x18,count)||count<1||count>64||
       unsigned(count)>array_length((Il2CppArray*)items))return retain_pose();
    struct Candidate{Vector3 point;float score;};Candidate candidates[64];int used=0,budget=0;
    for(int i=0;i<count&&budget<64;++i) {
        const auto shape=pointer(items,0x20+i*sizeof(uintptr_t));int type=-1;
        if(!shape||!read(shape+0x10,type)||type!=0)continue;
        const auto klass=(Il2CppClass*)pointer(shape,0);
        const size_t offset=klass==sphere_class?0x24:klass==capsule_class?0x28:0;
        float radius=0;Vector3 local{},center{};const auto target=pointer(shape,0x18);
        if(!offset||!target||!pointer(target,0x10)||!read(shape+0x20,radius)||
           !std::isfinite(radius)||radius<=0||radius>1||!read(shape+offset,local)||!finite(local))continue;
        void* args[]={&local};
        if(!value(transform_point,(void*)target,args,center,current)||!finite(center))continue;
        ++budget;
        const float center_gap=length(sub(center,preferred));
        if(center_gap<=.035f)candidates[used++]={center,center_gap};
        if(center_gap>.75f*radius+.0351f){budget=budget+26>64?64:budget+26;continue;}
        // Native radii are world-space: only the center uses TransformPoint.
        // Every shell point is inside the head sphere / capsule's center ball.
        for(int x=-1;x<=1&&budget<64;++x)for(int y=-1;y<=1&&budget<64;++y)for(int z=-1;z<=1&&budget<64;++z) {
            if(!x&&!y&&!z)continue;
            ++budget;
            const float r=.75f*radius/std::sqrt(float(x*x+y*y+z*z));
            Vector3 sample{center.x+x*r,center.y+y*r,center.z+z*r};
            const float score=length(sub(sample,preferred));
            if(score<=.035f)candidates[used++]={sample,score};
        }
    }
    std::sort(candidates,candidates+used,[](const Candidate& a,const Candidate& b){return a.score<b.score;});
    for(int i=0;i<used;++i) {
        const auto candidate=candidates[i].point;
        if(candidates[i].score>.035f)continue;
        if(!native_head_ray(manager,origin,candidate,tune.distanceMax,current))continue;
        if(tune.require_visible&&!line_visible(observed_local,player,origin,candidate))continue;
        point=candidate;source=1;return true;
    }
    return retain_pose(); // Mesh source stays explicit; this does not certify native HS damage.
}

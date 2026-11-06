// Restored from work/v501/jni/Cheat.cpp; search-row UI only.
using SearchResultBindFn=void (*)(void*,void*,int,const MethodInfo*);
static std::atomic<SearchResultBindFn> follow_search_original{nullptr};
static Il2CppClass* follow_result_class=nullptr;
static MethodInfo *follow_interactable=nullptr,*follow_button_active=nullptr;
static std::atomic<bool> follow_search_enabled{false};
static MethodInfo* follow_active_in_hierarchy=nullptr;
bool Cheat_FollowSearchEnabled(){return follow_search_enabled.load();}
void Cheat_SetFollowSearchEnabled(bool enabled){
    follow_search_enabled.store(enabled);
}
static bool follow_has_room(uintptr_t profile) {
    const auto room=pointer(profile,0x190);
    const auto id=(Il2CppString*)pointer(room,0x10);
    return id&&string_length&&string_length(id)>0;
}
static gchandle_new_fn follow_weak_handle=nullptr;
static MethodInfo *follow_parent=nullptr,*follow_set_parent=nullptr;
struct FollowPlacement { GCHandle button,parent,add; };
static std::vector<FollowPlacement> follow_placements;
static void follow_restore_row(uintptr_t button,CheatStatus& scratch) {
    for(auto it=follow_placements.begin();it!=follow_placements.end();) {
        auto old=rooted(it->button);
        if(old&&old!=button){++it;continue;}
        auto parent=rooted(it->parent);
        if(old&&parent&&pointer(old,0x10)&&pointer(parent,0x10)) {
            auto transform=invoke(get_transform,(void*)old,nullptr,scratch);
            bool keep_world=false;
            void* args[]={(void*)parent,&keep_world};
            if(transform)invoke(follow_set_parent,transform,args,scratch);
            auto add=rooted(it->add);
            bool show=true;
            void* active[]={(void*)add,&show};
            if(add&&pointer(add,0x10))invoke(follow_button_active,nullptr,active,scratch);
        }
        release_handle(it->button);release_handle(it->parent);release_handle(it->add);
        it=follow_placements.erase(it);
    }
}
static void follow_place_in_search(uintptr_t item,uintptr_t button,CheatStatus& scratch) {
    if(follow_placements.size()>=64)return;
    auto buttons=pointer(item,0x80);
    auto count=buttons?array_length((Il2CppArray*)buttons):0;
    uintptr_t add=0;
    for(unsigned i=0;i<std::min<unsigned>(count,16);++i) {
        auto candidate=pointer(buttons,0x20+i*sizeof(uintptr_t));
        auto object=candidate?invoke(get_game_object,(void*)candidate,nullptr,scratch):nullptr;
        bool visible=false;
        if(object&&value(follow_active_in_hierarchy,object,nullptr,visible,scratch)&&visible){add=candidate;break;}
    }
    if(!add)return;
    auto transform=invoke(get_transform,(void*)button,nullptr,scratch);
    auto add_transform=invoke(get_transform,(void*)add,nullptr,scratch);
    auto parent=transform?invoke(follow_parent,transform,nullptr,scratch):nullptr;
    auto visible_parent=add_transform?invoke(follow_parent,add_transform,nullptr,scratch):nullptr;
    if(!parent||!visible_parent||parent==visible_parent)return;
    FollowPlacement placement{
        follow_weak_handle((Il2CppObject*)button,false),
        follow_weak_handle((Il2CppObject*)parent,false),
        follow_weak_handle((Il2CppObject*)add,false)
    };
    if(!placement.button||!placement.parent||!placement.add) {
        release_handle(placement.button);release_handle(placement.parent);release_handle(placement.add);return;
    }
    follow_placements.push_back(placement);
    bool keep_world=false;
    void* args[]={visible_parent,&keep_world};
    const auto errors=scratch.invoke_errors;
    invoke(follow_set_parent,transform,args,scratch);
    if(scratch.invoke_errors==errors) {
        bool show=false;
        void* active[]={(void*)add,&show};
        invoke(follow_button_active,nullptr,active,scratch);
    }
}
static void follow_search_callback(void* self,void* item,int index,const MethodInfo* info) {
    SearchResultBindFn original=nullptr;
    while(!(original=follow_search_original.load(std::memory_order_acquire))) {}
    CheatStatus restore;
    if(item&&pointer((uintptr_t)item,0)==(uintptr_t)follow_result_class)
        follow_restore_row(pointer((uintptr_t)item,0x78),restore);
    original(self,item,index,info);
    if(!Cheat_FollowSearchEnabled()||!Auth_Allowed())return;
    if(!item||pointer((uintptr_t)item,0)!=(uintptr_t)follow_result_class)return;
    const auto profile=pointer((uintptr_t)item,0xA8);
    const auto button=pointer((uintptr_t)item,0x78);
    CheatStatus scratch;
    if(!profile||!follow_has_room(profile))return;
    if(!button||!pointer(button,0x10))return;
    bool enabled=true;
    void* active_args[]={(void*)button,&enabled};
    void* button_args[]={&enabled};
    invoke(follow_button_active,nullptr,active_args,scratch);
    invoke(follow_interactable,(void*)button,button_args,scratch);
    if(scratch.invoke_errors)return;
    auto object=invoke(get_game_object,(void*)button,nullptr,scratch);
    bool visible=false;
    if(object&&value(follow_active_in_hierarchy,object,nullptr,visible,scratch)&&!visible)
        follow_place_in_search((uintptr_t)item,button,scratch);
}
static void init_follow() {
    follow_weak_handle=(gchandle_new_fn)dlsym(libil2cpp_handle,"il2cpp_gchandle_new_weakref");
    follow_interactable=method("UnityEngine.UI","Selectable","set_interactable",1);
    follow_parent=method("UnityEngine","Transform","get_parent",0);
    follow_set_parent=method("UnityEngine","Transform","SetParent",2);
    follow_active_in_hierarchy=method("UnityEngine","GameObject","get_activeInHierarchy",0);
    follow_button_active=method("JarviGames.Extensions","ButtonExtensions","SetActive",2);
    follow_result_class=find_class_any("","FriendBlock");
    auto bind=method("JarviGames.Fragments.FriendsScreen","SearchTabPanelBehaviour","BDABACDBDDAABACDBBACCDB",2);
    if(!bind||!bind->methodPointer||!follow_result_class||
       !follow_interactable||!follow_button_active||!follow_parent||!follow_set_parent||!follow_weak_handle||
       !follow_active_in_hierarchy||!get_transform||!get_game_object)return;
    auto trampoline=inline_hook::install<SearchResultBindFn>((void*)bind->methodPointer,(void*)follow_search_callback);
    if(trampoline)follow_search_original.store(trampoline,std::memory_order_release);
}

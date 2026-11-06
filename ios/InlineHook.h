#pragma once
#include <atomic>
#include "Platform.h"
// Link Dobby statically into the iOS dylib. A missing/failed hook leaves the
// original method unmodified and the feature's existing readiness flag false.
extern "C" int DobbyHook(void* target,void* replacement,void** original);
namespace inline_hook {
inline const char* last_error="Hook not attempted";
template<class Fn> Fn install(void* target,void* replacement){
    if(!target||!replacement){last_error="Null hook target";return nullptr;}
    if(!jm_ios_unity_address(target)){last_error="Hook target outside UnityFramework";return nullptr;}
    void* original=nullptr;
    if(DobbyHook(target,replacement,&original)!=0||!original){last_error="DobbyHook failed";return nullptr;}
    last_error="";return reinterpret_cast<Fn>(original);
}
}

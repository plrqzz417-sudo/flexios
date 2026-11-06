#ifdef JM_APK
#include "ClientCheckPolicy.h"
using ApkCheckFn=void(*)(void*,int,const MethodInfo*);
using ApkIntegrityFn=void(*)(void*,uint8_t,uint8_t,const MethodInfo*);
struct ApkChallengeResult { uint64_t words[3]; };
static_assert(sizeof(ApkChallengeResult)==24);
using ApkChallengeFn=ApkChallengeResult(*)(Il2CppString*,uint64_t,int,const MethodInfo*);
static std::atomic<ApkCheckFn> apk_check_original{nullptr};
static std::atomic<ApkIntegrityFn> apk_integrity_original{nullptr};
static std::atomic<ApkChallengeFn> apk_challenge_original{nullptr};
static std::atomic<unsigned> apk_check_suppressed{0};

static ApkChallengeResult apk_challenge_observe(Il2CppString* nonce,uint64_t cancellation_token,int retry_delay,const MethodInfo* info) {
    ApkChallengeFn original=nullptr;
    while(!(original=apk_challenge_original.load(std::memory_order_acquire))){}
    int length=nonce&&string_length?string_length(nonce):-1;
    char text[96];std::snprintf(text,sizeof(text),"integrity.challenge length=%d",length);
    apk_trace::record(text);
    return original(nonce,cancellation_token,retry_delay,info);
}

static void apk_check_report(void* self,int flags,const MethodInfo* info) {
    ApkCheckFn original=nullptr;
    while(!(original=apk_check_original.load(std::memory_order_acquire))){}
    if(client_check::suppress(Auth_Allowed(),self!=nullptr,flags)) {
        unsigned count=apk_check_suppressed.fetch_add(1,std::memory_order_relaxed)+1;
        if(count==1||count%32==0){char text[128];std::snprintf(text,sizeof(text),"client_checker.suppressed flags=%d count=%u",flags,count);apk_trace::record(text);}
        return;
    }
    original(self,flags,info);
}

static void apk_integrity_result(void* self,uint8_t result,uint8_t error,const MethodInfo* info) {
    ApkIntegrityFn original=nullptr;
    while(!(original=apk_integrity_original.load(std::memory_order_acquire))){}
    char text[128];std::snprintf(text,sizeof(text),"integrity.server_result status=%u error=%u",unsigned(result),unsigned(error));
    apk_trace::record(text);
    // Observe the genuine callback. Never manufacture Passed or a token.
    original(self,result,error,info);
}

static void apk_integrity_observe(uintptr_t local) {
    static auto next=std::chrono::steady_clock::time_point{};
    static uintptr_t owner=0;static unsigned previous=~0u;
    auto now=std::chrono::steady_clock::now();if(now<next)return;
    next=now+std::chrono::milliseconds(500);
    if(!local){owner=0;previous=~0u;return;}
    auto passive=pointer(local,0x40);uint16_t value=0;
    if(!passive||!read(passive+0x28,value))return;
    if(owner!=local||previous!=value){
        owner=local;previous=value;char text[128];
        std::snprintf(text,sizeof(text),"passive.local source=%u enabled=%u",unsigned(value&255),unsigned((value>>8)&255));
        apk_trace::record(text);
    }
}

static void apk_integrity_init() {
    auto checker=find_class_any("JarviGames.Services.CheckerService","CheckerService");
    if(!checker){apk_trace::record("client_checker.class_missing");return;}
    auto guarded=[](const MethodInfo* info,uintptr_t rva,const uint32_t* expected)->void* {
        if(!info||!info->methodPointer)return nullptr;
        Dl_info image{};void* address=(void*)info->methodPointer;
        if(!dladdr(address,&image)||(uintptr_t)address!=(uintptr_t)image.dli_fbase+rva)return nullptr;
        for(unsigned i=0;i<4;++i){uint32_t word=0;if(!read((uintptr_t)address+i*4,word)||word!=expected[i])return nullptr;}
        return address;
    };
    const uint32_t report_signature[]={0xd10143ffu,0xf90013feu,0xa90357f6u,0xa9044ff4u};
    auto report=class_get_method(checker,"CCDCDCBBCBACBABABBCDCBD",1);
    if(auto address=guarded(report,0x1ff7648,report_signature)) {
        auto original=inline_hook::install<ApkCheckFn>(address,(void*)apk_check_report);
        if(original)apk_check_original.store(original,std::memory_order_release);
    }
    const uint32_t result_signature[]={0xa9bd5ffeu,0xa90157f6u,0xa9024ff4u,0x9000e717u};
    auto result=class_get_method(checker,"DCCDDDDDAADABBBCBBADBCA",2);
    if(auto address=guarded(result,0x1ff7c38,result_signature)) {
        auto original=inline_hook::install<ApkIntegrityFn>(address,(void*)apk_integrity_result);
        if(original)apk_integrity_original.store(original,std::memory_order_release);
    }
    auto helper=find_class_any("JarviGames.Services.CheckerService","PlayIntegrityHelper");
    const uint32_t challenge_signature[]={0xd10303ffu,0xa90867feu,0xa9095ff8u,0xa90a57f6u};
    auto challenge=helper?class_get_method(helper,"DBDCACDDCAAABDABAACBADB",3):nullptr;
    if(auto address=guarded(challenge,0x1ff9710,challenge_signature)) {
        auto original=inline_hook::install<ApkChallengeFn>(address,(void*)apk_challenge_observe);
        if(original)apk_challenge_original.store(original,std::memory_order_release);
    }
    char text[128];std::snprintf(text,sizeof(text),"client_checker.ready=%u integrity.observer.ready=%u server_bypass=unverified",
        unsigned(apk_check_original.load()!=nullptr),unsigned(apk_integrity_original.load()!=nullptr));
    apk_trace::record(text);
    apk_trace::record(apk_challenge_original.load()?"integrity.challenge.ready=1":"integrity.challenge.ready=0");
}
#endif

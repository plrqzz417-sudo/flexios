#include "Auth.h"
#include "AuthRules.h"
#include "AuthConfig.h"
#include "vendor/monocypher/monocypher-ed25519.h"
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <CommonCrypto/CommonDigest.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <ctime>

namespace {
std::mutex state_mutex;
std::atomic<bool> allowed{false},busy{false},ready{false};
std::atomic<int64_t> deadline{0};
std::string message="Copie sua key e toque em Colar key.",session;
constexpr const char* saved_key_name="jmflex-key";
int64_t monotonic(){return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
std::string str(NSString* s){return s?std::string(s.UTF8String):std::string{};}
NSString* ns(const std::string& s){return [NSString stringWithUTF8String:s.c_str()];}
void report(std::string s){std::lock_guard<std::mutex> lock(state_mutex);message=s.substr(0,512);}
std::string encode(const std::string& s){
    std::string out;const char* h="0123456789ABCDEF";
    for(unsigned char c:s){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=char(c);
        else {out+='%';out+=h[c>>4];out+=h[c&15];}}
    return out;
}
std::string unhex(const std::string& s){
    if(s.size()%2)return {};
    auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
    std::string out;for(size_t i=0;i<s.size();i+=2){int a=digit(s[i]),b=digit(s[i+1]);if(a<0||b<0)return {};out+=char(a*16+b);}return out;
}
std::string common(){return "&name="+encode(auth_config::name)+"&ownerid="+encode(auth_config::owner_id);}
std::string header(NSHTTPURLResponse* response,NSString* wanted){
    for(id key in response.allHeaderFields)if([[key description] caseInsensitiveCompare:wanted]==NSOrderedSame)
        return str([response.allHeaderFields[key] description]);
    return {};
}
bool verify(NSHTTPURLResponse* response,const std::string& body){
    const auto signature=unhex(header(response,@"x-signature-ed25519"));
    const auto stamp=header(response,@"x-signature-timestamp");
    if(signature.size()!=64||stamp.empty()||stamp.size()>12||stamp.find_first_not_of("0123456789")!=std::string::npos)return false;
    const auto seconds=std::strtoll(stamp.c_str(),nullptr,10);
    if(!auth_rules::fresh(std::time(nullptr),seconds))return false;
    static const auto key=unhex("5586b4bc69c7a4b487e4563a4cd96afd39140f919bd31cea7d1c6a1e8439422b");
    if(key.size()!=32)return false;
    const auto signed_body=stamp+body;
    return crypto_ed25519_check(reinterpret_cast<const uint8_t*>(signature.data()),
        reinterpret_cast<const uint8_t*>(key.data()),reinterpret_cast<const uint8_t*>(signed_body.data()),signed_body.size())==0;
}
struct Response{bool success=false;std::string session,message;int64_t expiry=0;};
Response request(const std::string& body){
    Response result;result.message="Falha de conexao com o KeyAuth. Tente novamente.";
    auto url=[NSURL URLWithString:@"https://keyauth.win/api/1.3/"];
    auto req=[NSMutableURLRequest requestWithURL:url];req.HTTPMethod=@"POST";
    req.HTTPBody=[ns(body) dataUsingEncoding:NSUTF8StringEncoding];
    [req setValue:@"application/x-www-form-urlencoded; charset=UTF-8" forHTTPHeaderField:@"Content-Type"];
    req.timeoutInterval=10;
    auto config=[NSURLSessionConfiguration ephemeralSessionConfiguration];config.timeoutIntervalForRequest=10;
    auto client=[NSURLSession sessionWithConfiguration:config];
    dispatch_semaphore_t done=dispatch_semaphore_create(0);
    __block NSData* payload=nil;__block NSHTTPURLResponse* response=nil;__block NSError* error=nil;
    auto task=[client dataTaskWithRequest:req completionHandler:^(NSData* data,NSURLResponse* raw,NSError* failure){
        payload=data;response=[raw isKindOfClass:NSHTTPURLResponse.class]?(NSHTTPURLResponse*)raw:nil;
        error=failure;dispatch_semaphore_signal(done);
    }];
    [task resume];dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,15*NSEC_PER_SEC));
    [client invalidateAndCancel];
    if(error||!response||response.statusCode!=200||!payload||payload.length>1024*1024)return result;
    const std::string raw(static_cast<const char*>(payload.bytes),payload.length);
    if(!verify(response,raw)){result.message="Resposta KeyAuth sem assinatura valida.";return result;}
    auto json=[NSJSONSerialization JSONObjectWithData:payload options:0 error:&error];
    if(![json isKindOfClass:NSDictionary.class]){result.message="Resposta KeyAuth invalida.";return result;}
    NSDictionary* object=(NSDictionary*)json;
    result.success=[object[@"success"] boolValue];result.message=str(object[@"message"]);
    result.session=str(object[@"sessionid"]);
    if(result.success&&body.rfind("type=license&",0)==0){
        id info=object[@"info"];id subs=[info isKindOfClass:NSDictionary.class]?info[@"subscriptions"]:nil;
        if([subs isKindOfClass:NSArray.class])for(id entry in [(NSArray*)subs subarrayWithRange:NSMakeRange(0,std::min((NSUInteger)128,[subs count]))])
            if([entry isKindOfClass:NSDictionary.class])result.expiry=std::max(result.expiry,int64_t([entry[@"expiry"] longLongValue]));
    }
    return result;
}
std::string device(){
    NSString* vendor_identifier=UIDevice.currentDevice.identifierForVendor.UUIDString;
    if(!vendor_identifier.length)return {};
    unsigned char hash[CC_SHA256_DIGEST_LENGTH];const auto bytes=str(vendor_identifier);
    CC_SHA256(bytes.data(),CC_LONG(bytes.size()),hash);
    const char* hex="0123456789abcdef";std::string out;out.reserve(64);
    for(auto byte:hash){out+=hex[byte>>4];out+=hex[byte&15];}return out;
}
}
void Auth_Init(){ready=true;}
bool Auth_Allowed(){return auth_rules::active(allowed.load(),monotonic(),deadline.load());}
AuthState Auth_Get(){std::lock_guard<std::mutex> lock(state_mutex);return {Auth_Allowed(),busy.load(),ready.load(),message};}
std::string Auth_Clipboard(){auto text=str(UIPasteboard.generalPasteboard.string);return text.substr(0,256);}
std::string Auth_SavedKey(){return str([NSUserDefaults.standardUserDefaults stringForKey:@"jmflex-key"]);}
void Auth_ForgetKey(){[NSUserDefaults.standardUserDefaults removeObjectForKey:@"jmflex-key"];}
void Auth_Login(std::string key){
    if(!ready||allowed||busy.exchange(true))return;
    if(key.empty()||key.size()>256){busy=false;report("Cole uma key valida.");return;}
    report("Validando a licenca...");
    std::thread([key=std::move(key)]() mutable {@autoreleasepool {
        bool ok=true;
        if(session.empty()){auto init=request("type=init&ver="+encode(auth_config::version)+common());
            if(init.success&&!init.session.empty())session=init.session;else{report(init.message);ok=false;}}
        auto hwid=ok?device():std::string{};
        if(ok&&hwid.empty()){report("Identificador do dispositivo indisponivel.");ok=false;}
        if(ok){auto login=request("type=license&key="+encode(key)+"&hwid="+encode(hwid)+"&sessionid="+encode(session)+common());
            ok=login.success&&login.expiry>std::time(nullptr);
            if(ok)deadline=monotonic()+login.expiry-std::time(nullptr);
            report(ok?"Licenca validada.":login.success?"Nenhuma assinatura ativa nesta licenca.":login.message);}
        if(ok)[NSUserDefaults.standardUserDefaults setObject:ns(key) forKey:@"jmflex-key"];
        else session.clear();std::fill(key.begin(),key.end(),'\0');allowed=ok;busy=false;
        while(ok){std::this_thread::sleep_for(std::chrono::seconds(60));
            auto check=request("type=check&sessionid="+encode(session)+common());
            if(!check.success||!Auth_Allowed()){busy=true;session.clear();report("Sessao encerrada. Valide a key novamente.");allowed=false;busy=false;break;}}
    }}).detach();
}

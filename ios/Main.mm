#include "Cheat.h"
#include "Auth.h"
#include "Hooks.h"
#include <atomic>
#include <thread>
#include <chrono>
#import <Foundation/Foundation.h>

static std::atomic<bool> started{false};
static void install_later(int attempt){
    dispatch_async(dispatch_get_main_queue(),^{
        if(!InstallGraphicsOverlay()&&attempt<60)
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW,NSEC_PER_SEC),dispatch_get_main_queue(),^{install_later(attempt+1);});
    });
}
__attribute__((constructor)) static void jm_ios_load(){
    if(started.exchange(true))return;
    Auth_Init();
    std::thread([]{
        // IL2CPP domain and Unity's view appear after dyld loads our dylib.
        std::this_thread::sleep_for(std::chrono::seconds(2));
        Cheat_Init();
        install_later(0);
    }).detach();
}

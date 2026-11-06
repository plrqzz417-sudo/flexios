#pragma once
#ifdef JM_APK
#include <atomic>
#include <cstdio>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
namespace apk_trace {
inline std::atomic<int> fd{-1};
inline std::atomic<unsigned> count{0};
inline void set_fd(int value) { int old=fd.exchange(value);if(old>=0&&old!=value)close(old); }
inline void record(const char* text) {
    int target=fd.load();if(target<0||count.fetch_add(1)>=4096)return;
    timespec now{};clock_gettime(CLOCK_REALTIME,&now);
    char line[384];int size=std::snprintf(line,sizeof(line),"%lld NATIVE tid=%ld %s\n",
        (long long)now.tv_sec*1000+now.tv_nsec/1000000,(long)syscall(SYS_gettid),text?text:"");
    if(size>0){size=size<int(sizeof(line))?size:int(sizeof(line))-1;(void)write(target,line,size);}
}
}
#else
namespace apk_trace { inline void record(const char*){} }
#endif

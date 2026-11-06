#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
// One writer per meter (Unity callbacks), atomic aggregated output for the UI.
struct CostMeter {
    std::atomic<unsigned> mean{0},peak{0};
    uint64_t sum=0;unsigned count=0,maximum=0;
    std::chrono::steady_clock::time_point published{};
    void record(std::chrono::steady_clock::time_point start){
        const auto now=std::chrono::steady_clock::now();
        const auto us=std::chrono::duration_cast<std::chrono::microseconds>(now-start).count();
        const unsigned elapsed=unsigned(us>0?us:0);sum+=elapsed;++count;
        if(elapsed>maximum)maximum=elapsed;
        if(now-published>=std::chrono::seconds(1)){
            mean.store(unsigned(sum/count),std::memory_order_relaxed);peak.store(maximum,std::memory_order_relaxed);
            published=now;sum=0;count=maximum=0;
        }
    }
};
struct CostScope {
    CostMeter& meter;std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
    explicit CostScope(CostMeter& m):meter(m){}
    ~CostScope(){meter.record(start);}
};
inline constexpr uint32_t PoseDemand(bool aim,bool esp,bool skeleton,bool random,int bone,uint32_t pool){
    if(esp&&skeleton)return ((1u<<19)-1)&~((1u<<11)|(1u<<12));
    if(!aim&&!esp)return 0;
    uint32_t mask=(1u<<10)|(1u<<9);
    constexpr int ids[]={10,9,8,7,0};
    for(int i=0;i<5;++i)if(aim&&(random?(pool&(1u<<i))!=0:bone==i))mask|=1u<<ids[i];
    return mask;
}
inline constexpr int OverlayPeriodUs(bool menu,int esp_hz){return menu||esp_hz>=60?16667:33333;}
inline constexpr int OverlayWaitUs(int64_t elapsed,int period=16667){
    return elapsed<=0?period:elapsed<period?int(period-elapsed):0;
}

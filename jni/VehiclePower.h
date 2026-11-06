#pragma once
#include <cmath>
// The baseline follows genuine game updates. Repeated polling never compounds.
namespace vehicle_power {
struct Value {
    float original=0,last=0;bool applied=false;
    static bool valid(float x){return std::isfinite(x)&&x>0&&x<=10000000.f;}
    static bool same(float a,float b){return std::fabs(a-b)<=std::fmax(.00001f,std::fabs(b)*.000001f);}
    bool update(float current,float multiplier,float& desired){
        if(!valid(current)||!std::isfinite(multiplier)||multiplier<1||multiplier>3)return false;
        if(!applied||!same(current,last))original=current;
        const float next=original*multiplier;
        if(!valid(next))return false;
        desired=next;return !same(current,next);
    }
    void committed(float value){last=value;applied=true;}
    bool restore(float current,float& desired){
        const bool owned=applied&&valid(original)&&same(current,last);
        desired=original;applied=false;return owned&&!same(current,desired);
    }
};
}

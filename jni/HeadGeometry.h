#pragma once
#include <cmath>
#include <cstddef>
namespace head_geometry {
struct Weight {float weights[4];int bones[4];};
static_assert(sizeof(Weight)==32,"Unity BoneWeight ABI");
struct Bounds {Vector3 minimum{},maximum{};unsigned count=0;};
inline bool add(Bounds& bounds,Vector3 vertex,const Weight& weight,int head,const float* bind) {
    float influence=0;
    for(int i=0;i<4;++i) {
        if(!std::isfinite(weight.weights[i])||weight.weights[i]<0||weight.weights[i]>1)return false;
        if(weight.bones[i]==head)influence+=weight.weights[i];
    }
    if(influence<.85f)return false;
    Vector3 local{bind[0]*vertex.x+bind[4]*vertex.y+bind[8]*vertex.z+bind[12],
                  bind[1]*vertex.x+bind[5]*vertex.y+bind[9]*vertex.z+bind[13],
                  bind[2]*vertex.x+bind[6]*vertex.y+bind[10]*vertex.z+bind[14]};
    if(!std::isfinite(local.x)||!std::isfinite(local.y)||!std::isfinite(local.z))return false;
    if(!bounds.count)bounds.minimum=bounds.maximum=local;
    else {
        bounds.minimum={std::fmin(bounds.minimum.x,local.x),std::fmin(bounds.minimum.y,local.y),std::fmin(bounds.minimum.z,local.z)};
        bounds.maximum={std::fmax(bounds.maximum.x,local.x),std::fmax(bounds.maximum.y,local.y),std::fmax(bounds.maximum.z,local.z)};
    }
    ++bounds.count;return true;
}
inline bool center(const Bounds& bounds,Vector3& out) {
    if(bounds.count<8)return false;
    out={(bounds.minimum.x+bounds.maximum.x)*.5f,(bounds.minimum.y+bounds.maximum.y)*.5f,(bounds.minimum.z+bounds.maximum.z)*.5f};
    return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.z);
}
}

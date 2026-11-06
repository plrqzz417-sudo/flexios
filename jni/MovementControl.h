#pragma once
#include <cmath>
namespace movement_control {
inline Vector3 approach(Vector3 current,Vector3 desired,float speed,float dt,float response){
    if(!std::isfinite(speed)||speed<=0||!std::isfinite(desired.x)||!std::isfinite(desired.y)||!std::isfinite(desired.z))return {};
    if(!std::isfinite(current.x)||!std::isfinite(current.y)||!std::isfinite(current.z))current={};
    const float magnitude=std::sqrt(current.x*current.x+current.y*current.y+current.z*current.z);
    if(magnitude>speed){current.x*=speed/magnitude;current.y*=speed/magnitude;current.z*=speed/magnitude;}
    if(!std::isfinite(dt)||dt<=0)return current;
    if(!std::isfinite(response)||response<=0)return desired;
    if(dt>.1f)dt=.1f;
    Vector3 delta{desired.x-current.x,desired.y-current.y,desired.z-current.z};
    const float distance=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
    const float step=speed*dt/response;
    if(distance<=step||distance<.00001f)return desired;
    return {current.x+delta.x*step/distance,current.y+delta.y*step/distance,current.z+delta.z*step/distance};
}
inline float speed_factor(float current,float desired,float dt,float response){
    if(!std::isfinite(desired)||desired<.4f||desired>8)return 1;
    if(!std::isfinite(current)||current<.4f||current>8)current=1;
    if(!std::isfinite(response))return 1;
    if(response<=0)return desired;
    if(!std::isfinite(dt)||dt<=0)return current;
    if(dt>.1f)dt=.1f;
    const float difference=desired-current;
    const float rate=std::fabs(desired-1)>1?std::fabs(desired-1):1;
    const float step=rate*dt/response;
    if(std::fabs(difference)<=step)return desired;
    return current+(difference>0?step:-step);
}
inline Vector3 velocity(Vector3 direction,float speed,float vertical,bool fly,float current_y) {
    if(!std::isfinite(speed)||speed<=0||!std::isfinite(direction.x)||!std::isfinite(direction.z)||!std::isfinite(vertical))return {0,fly?0:current_y,0};
    float length=std::sqrt(direction.x*direction.x+direction.z*direction.z);
    if(length>1){direction.x/=length;direction.z/=length;}
    Vector3 out{direction.x*speed,fly?vertical*speed:current_y,direction.z*speed};
    if(fly){float magnitude=std::sqrt(out.x*out.x+out.y*out.y+out.z*out.z);if(magnitude>speed){out.x*=speed/magnitude;out.y*=speed/magnitude;out.z*=speed/magnitude;}}
    return out;
}
inline constexpr bool reached(Vector3 position,Vector3 goal,float tolerance) {
    const float x=position.x-goal.x,y=position.y-goal.y,z=position.z-goal.z;
    return x*x+y*y+z*z<=tolerance*tolerance;
}
}

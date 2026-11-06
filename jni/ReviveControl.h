#pragma once
// Pure lifecycle. Unknown/missing HP never starts a death or confirms a respawn.
namespace revive_control {
enum class Health {Unknown,Alive,Dead};
enum class Action {None,Request,Return,Complete,Expired};
struct Controller {
    enum Phase {Idle,Dead,Returning} phase=Idle;
    double died=0,returning=0,next_request=0,next_return=0;
    unsigned room=0,attempts=0;
    void reset(){*this=Controller{};}
    void begin(double now,unsigned epoch){reset();phase=Dead;died=now;room=epoch;next_request=now;}
    Action step(double now,unsigned epoch,bool enabled,Health health,bool blocked,bool arrived){
        if(phase==Idle)return Action::None;
        if(!enabled||epoch!=room||now<died||now-died>=90000){reset();return Action::Expired;}
        if(phase==Dead){
            if(health==Health::Alive){phase=Returning;returning=now;next_return=now;}
            else if(now>=next_request){next_request=now+500;return Action::Request;}
            else return Action::None;
        }
        if(health==Health::Dead){reset();return Action::Expired;}
        if(health==Health::Alive&&arrived){reset();return Action::Complete;}
        // Keep the final confirmation window even after the third request.
        if(now-returning>=10000){reset();return Action::Expired;}
        if(health!=Health::Alive||blocked)return Action::None;
        if(attempts<3&&now>=next_return){++attempts;next_return=now+1000;return Action::Return;}
        return Action::None;
    }
};
inline constexpr Health health(bool known,int hp,int maximum){
    return !known||maximum<=0||hp>maximum?Health::Unknown:hp>0?Health::Alive:Health::Dead;
}
}

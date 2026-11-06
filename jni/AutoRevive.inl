#include "ReviveControl.h"
static revive_control::Controller revive_cycle;
static uintptr_t revive_local=0;
static bool revive_death_latched=false,revive_early_attempted=false;
static std::chrono::steady_clock::time_point revive_pose_at{};
static std::chrono::steady_clock::time_point revive_popup_check{};
static char revive_message[160]="Auto-revive desligado";
static void revive_note(const char* text){std::lock_guard<std::mutex> lock(move_mutex);std::snprintf(revive_message,sizeof(revive_message),"%s",text);}
void Cheat_GetReviveMessage(char* out,int size){if(!out||size<=0)return;std::lock_guard<std::mutex> lock(move_mutex);std::snprintf(out,size,"%s",revive_message);}
static void revive_cancel(){
    revive_cycle.reset();revive_local=0;revive_death_latched=revive_early_attempted=false;
    move_had_alive=move_death_position=move_direct_sent=move_spawn_punched=false;
    revive_pose_at={};revive_popup_check={};move_passive_until={};move_passive_request={};release_handle(move_respawn_pending);
}
static revive_control::Health revive_health(uintptr_t local){
    const auto object=pointer(local,0x68);int16_t hp=0,maximum=0;
    const bool known=object&&pointer(object,0x10)&&read(object+0x6A,hp)&&read(object+0x68,maximum);
    return revive_control::health(known,hp,maximum);
}
static void revive_passive_tick(uintptr_t local,CheatStatus& scratch){
    const auto now=std::chrono::steady_clock::now();
    if(now>=move_passive_until||now-move_passive_request<std::chrono::milliseconds(250))return;
    move_passive_request=now;
    const auto passive=pointer(local,0x40);uint8_t source=0,enabled=0;
    if(!passive||!pointer(passive,0x10)||!read(passive+0x28,source)||!read(passive+0x29,enabled)||!enabled||
       (source!=0&&source!=5&&source!=255))return;
    if(!move_spawn_punched&&move_punch&&controls_instance){
        uintptr_t controls=0;field_static_get(controls_instance,&controls);
        if(controls&&pointer(controls,0x10)){
            const int errors=scratch.invoke_errors;invoke(move_punch,(void*)controls,nullptr,scratch);
            move_spawn_punched=errors==scratch.invoke_errors;
        }
    }
    if(move_passive_remove){void* args[]={&source};invoke(move_passive_remove,(void*)passive,args,scratch);}
}
static void auto_revive_tick(uintptr_t local,const AimbotTune& tune,CheatStatus& scratch){
    using namespace revive_control;
    const auto now=std::chrono::steady_clock::now();
    const double ms=std::chrono::duration<double,std::milli>(now.time_since_epoch()).count();
    if(!tune.auto_revive||!Auth_Allowed()||move_revive_room!=room_epoch){
        revive_cancel();move_revive_room=room_epoch;
        revive_note(!tune.auto_revive?"Auto-revive desligado":"Auto-revive: aguardando personagem da sessao");
        return;
    }
    const auto observed_health=local?revive_health(local):Health::Unknown;
    auto health=observed_health;
    if(health==Health::Alive&&!active_object(local,scratch))health=Health::Unknown;
    if(local&&revive_local!=local){
        if(revive_cycle.phase==Controller::Idle){move_had_alive=false;revive_pose_at={};revive_death_latched=false;}
        revive_local=local;
    }
    Vector3 actual{};bool have_position=false;
    if(health==Health::Alive){
        const auto body=pointer(pointer(local,0xF8),0x20);
        have_position=body&&move_get_pos&&value(move_get_pos,(void*)body,nullptr,actual,scratch)&&finite(actual);
        const auto transform=(uintptr_t)invoke(get_transform,(void*)local,nullptr,scratch);
        if(!have_position)have_position=position(transform,actual,scratch);
        if(have_position){move_last=actual;move_had_alive=true;revive_pose_at=now;}
        if(transform&&move_euler){Vector3 angles{};if(value(move_euler,(void*)transform,nullptr,angles,scratch)&&finite(angles))move_last_euler=angles;}
        revive_death_latched=false;revive_passive_tick(local,scratch);
    }
    bool popup_death=false;
    if(health==Health::Unknown&&observed_health!=Health::Alive&&!revive_death_latched&&revive_cycle.phase==Controller::Idle&&now>=revive_popup_check){
        revive_popup_check=now+std::chrono::milliseconds(500);
        GCHandle popup=move_find_free_popup(scratch);
        popup_death=rooted(popup)!=0;
        if(popup_death){release_handle(move_respawn_pending);move_respawn_pending=popup;}
        else release_handle(popup);
    }
    if((health==Health::Dead||popup_death)&&!revive_death_latched&&revive_cycle.phase==Controller::Idle){
        revive_death_latched=true;revive_early_attempted=move_direct_sent=false;
        move_death=move_last;move_death_position=move_had_alive&&finite(move_last)&&now-revive_pose_at<=std::chrono::seconds(popup_death?90:2);
        revive_cycle.begin(ms,room_epoch);if(!popup_death)release_handle(move_respawn_pending);
        revive_note(popup_death?"Reaparecimento gratuito confirmado pela tela do jogo":"Morte confirmada; aguardando reaparecimento gratuito");
    }
    uint8_t locked=1;
    if(local)read(pointer(pointer(local,0x90),0x68)+0x25,locked);
    const bool arrived=health==Health::Alive&&(!move_death_position||(have_position&&movement_control::reached(actual,move_death,1.5f)));
    const auto before=revive_cycle.phase;
    const auto action=revive_cycle.step(ms,room_epoch,true,health,locked!=0,arrived);
    if(before==Controller::Dead&&health==Health::Alive){
        release_handle(move_respawn_pending);move_passive_until=now+std::chrono::seconds(3);move_passive_request={};move_spawn_punched=false;
    }
    if(action==Action::Request){
        if(observed_health==Health::Alive){revive_note("Vida detectada; aguardando personagem ativo");return;}
        if(local&&health==Health::Dead&&!revive_early_attempted&&move_death_position&&move_direct_respawn){
            revive_early_attempted=true;
            if(move_request_early(local,scratch)){revive_note("Reaparecimento nativo gratuito solicitado; aguardando vida");return;}
        }
        revive_note(move_respawn_request(scratch)?"Reaparecimento gratuito solicitado; aguardando vida":"Aguardando botao gratuito ativo e liberado");
    }else if(action==Action::Return){
        const bool sent=local&&move_death_position&&move_teleport(local,move_death,scratch);
        revive_note(sent?"Retorno enviado; aguardando confirmar chegada":"Retorno pendente: estado nativo indisponivel");
    }else if(action==Action::Complete){
        revive_note(move_death_position?"Reaparecimento e retorno confirmados localmente":"Reaparecimento confirmado; sem posicao anterior para retornar");move_death_position=false;
    }else if(action==Action::Expired){
        move_death_position=false;release_handle(move_respawn_pending);revive_note("Auto-revive encerrado: prazo ou estado mudou");
    }
}

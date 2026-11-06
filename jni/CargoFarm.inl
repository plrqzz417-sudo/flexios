// Unity thread only; exclusively CarMeetCargoActivity, never arbitrary jobs.
static Il2CppObject* farm_activity_type=nullptr;
static MethodInfo *farm_interact=nullptr,*farm_deliver=nullptr;
static FieldInfo* farm_delay_fields[2]{};
static GCHandle farm_delay_owner=0;
static float farm_saved_delays[2]{};
static void farm_restore_delays(){
    auto cargo=rooted(farm_delay_owner);
    if(cargo&&pointer(cargo,0x10)&&field_set_value)
        for(int i=0;i<2;++i)if(farm_delay_fields[i])field_set_value((Il2CppObject*)cargo,farm_delay_fields[i],&farm_saved_delays[i]);
    release_handle(farm_delay_owner);
}
static void farm_skip_delays(uintptr_t cargo){
    if(!cargo||!pointer(cargo,0x10)||!field_get_value||!field_set_value)return;
    if(rooted(farm_delay_owner)!=cargo){
        farm_restore_delays();root(farm_delay_owner,cargo);
        for(int i=0;i<2;++i)if(farm_delay_fields[i])field_get_value((Il2CppObject*)cargo,farm_delay_fields[i],&farm_saved_delays[i]);
    }
    float immediate=.0001f; // Progress UI can divide elapsed time by this delay.
    for(int i=0;i<2;++i)if(farm_delay_fields[i])field_set_value((Il2CppObject*)cargo,farm_delay_fields[i],&immediate);
}

static GCHandle farm_activity=0,farm_box=0;
static uintptr_t farm_owner=0;static unsigned farm_epoch=0;
enum class CargoStage {Pickup,ToDelivery,Deliver,WaitDelivery,Return,StepOut,StepBack,Blocked};
static CargoStage farm_stage=CargoStage::Pickup;
static Vector3 farm_anchor{},farm_destination{},farm_step{};
static bool farm_anchor_ready=false,farm_tp_sent=false,farm_pickup_pending=false;
static unsigned farm_pickup_attempts=0;
static std::chrono::steady_clock::time_point farm_scan{},farm_stage_at{},farm_interact_at{};
static std::mutex farm_mutex;static char farm_message[240]="Desligado";
static std::atomic<unsigned> farm_requests{0},farm_cycles{0},farm_teleports{0};
void Cheat_GetFarmMessage(char* out,int size){if(!out||size<=0)return;std::lock_guard<std::mutex> lock(farm_mutex);std::snprintf(out,size,"%s | pedidos %u / ciclos %u / TP %u",farm_message,farm_requests.load(),farm_cycles.load(),farm_teleports.load());}
static void farm_note(const char* text){std::lock_guard<std::mutex> lock(farm_mutex);std::snprintf(farm_message,sizeof(farm_message),"%s",text);}
static void farm_transition(CargoStage next){farm_stage=next;farm_tp_sent=false;farm_stage_at=std::chrono::steady_clock::now();if(next==CargoStage::Blocked)farm_restore_delays();}
static void farm_reset(){
    farm_restore_delays();release_handle(farm_activity);release_handle(farm_box);
    farm_owner=0;farm_anchor_ready=farm_tp_sent=farm_pickup_pending=false;farm_pickup_attempts=0;
    farm_stage=CargoStage::Pickup;farm_scan={};farm_stage_at={};farm_interact_at={};
}
static bool farm_goto(uintptr_t local,Vector3 position,Vector3 goal,CargoStage next,CheatStatus& scratch){
    if(movement_control::reached(position,goal,.75f)){farm_transition(next);return true;}
    if(!farm_tp_sent){
        if(!move_teleport(local,goal,scratch)){farm_transition(CargoStage::Blocked);farm_note("TP indisponivel: desligue e reposicione");return false;}
        farm_tp_sent=true;farm_stage_at=std::chrono::steady_clock::now();++farm_teleports;
    }else if(std::chrono::steady_clock::now()-farm_stage_at>std::chrono::seconds(2)){
        farm_transition(CargoStage::Blocked);farm_note("TP nao confirmado pelo jogo; farm interrompido");
    }
    return false;
}
static bool farm_step_point(float distance,CheatStatus& scratch){
    static uint32_t random=0x319a67u;
    for(int i=0;i<8;++i){
        random^=random<<13;random^=random>>17;random^=random<<5;
        const float angle=float(random%62832)*.0001f;
        Vector3 candidate{farm_anchor.x+std::cos(angle)*distance,farm_anchor.y,farm_anchor.z+std::sin(angle)*distance};
        if(move_ground(candidate,farm_step,scratch)&&std::fabs(farm_step.y-farm_anchor.y)<=1.5f)return true;
    }
    return false;
}
static void farm_tick(uintptr_t local,bool alive,CheatStatus& scratch){
    const auto tune=Cheat_GetAimbot();const auto now=std::chrono::steady_clock::now();
    if(!tune.cargo_farm||!Auth_Allowed()||!alive||!local){farm_reset();farm_note(tune.cargo_farm?"Pausado: aguardando personagem vivo":"Desligado");return;}
    if(farm_owner!=local||farm_epoch!=room_epoch){farm_reset();farm_owner=local;farm_epoch=room_epoch;}
    if(Menu_IsOpen()){farm_restore_delays();farm_stage_at=now;farm_interact_at=now;farm_note("Pausado com o menu aberto");return;}
    uint32_t seated=0;read(pointer(local,0xB0)+0x3c,seated);
    if(seated){farm_restore_delays();farm_stage_at=now;farm_note("Saia do veiculo para carregar a caixa");return;}
    if(!farm_activity_type||!find_objects||!farm_interact||!move_native_teleport){farm_note("Metodos nativos de coleta/TP indisponiveis");return;}
    Vector3 pos{};auto transform=(uintptr_t)invoke(get_transform,(void*)local,nullptr,scratch);
    if(!position(transform,pos,scratch))return;
    auto activity=rooted(farm_activity);bool active=false;
    if(activity&&(!pointer(activity,0x10)||!value(is_active,(void*)activity,nullptr,active,scratch)||!active)){farm_reset();farm_note("Atividade encerrada");return;}
    if(!activity&&now-farm_scan>=std::chrono::milliseconds(150)){
        farm_scan=now;void* args[]={farm_activity_type};auto list=(Il2CppArray*)invoke(find_objects,nullptr,args,scratch);float best=60;
        if(list)for(unsigned i=0;i<std::min<unsigned>(array_length(list),32);++i){
            auto candidate=pointer((uintptr_t)list,0x20+i*sizeof(uintptr_t));bool enabled=false;
            if(!candidate||!pointer(candidate,0x10)||!value(is_active,(void*)candidate,nullptr,enabled,scratch)||!enabled)continue;
            Vector3 pickup{};auto t=(uintptr_t)invoke(get_transform,(void*)candidate,nullptr,scratch);
            if(position(t,pickup,scratch)){const float d=length(sub(pickup,pos));if(d<best){best=d;activity=candidate;}}
        }
        root(farm_activity,activity);
    }
    if(!activity){farm_note("Aproxime-se do ponto Carregamento");return;}
    auto cargo=pointer(activity,0x40);bool cargo_active=false;
    if(!cargo||!pointer(cargo,0x10)||!value(is_active,(void*)cargo,nullptr,cargo_active,scratch)||!cargo_active){farm_note("Atividade de carga nao esta ativa");return;}
    if(!farm_anchor_ready){
        Vector3 pickup{};auto t=(uintptr_t)invoke(get_transform,(void*)activity,nullptr,scratch);
        if(!position(t,pickup,scratch)||length(sub(pickup,pos))>4){farm_note("Inicie a ate 4 m da coleta");return;}
        farm_anchor=pos;farm_anchor_ready=true;farm_stage_at=now;
    }
    if(farm_stage==CargoStage::Blocked)return;
    farm_skip_delays(cargo);
    uint8_t started=0;read(cargo+0x78,started);const auto box=pointer(cargo,0xA8),target=pointer(cargo,0x80);
    if(farm_stage==CargoStage::Return){
        farm_note("Voltando a coleta por TP");
        if(farm_goto(local,pos,farm_anchor,CargoStage::StepOut,scratch)&&!farm_step_point(tune.cargo_step+.8f,scratch)){
            farm_transition(CargoStage::Blocked);farm_note("Sem terreno livre para o passo TP");
        }
        return;
    }
    if(farm_stage==CargoStage::StepOut){farm_note("Passo TP para ponto livre");farm_goto(local,pos,farm_step,CargoStage::StepBack,scratch);return;}
    if(farm_stage==CargoStage::StepBack){
        farm_note("Retorno do passo TP");
        if(farm_goto(local,pos,farm_anchor,CargoStage::Pickup,scratch)){farm_pickup_pending=false;farm_pickup_attempts=0;release_handle(farm_box);}
        return;
    }
    if(farm_stage==CargoStage::WaitDelivery){
        if(!started){++farm_cycles;farm_transition(CargoStage::Return);return;}
        if(now-farm_stage_at>std::chrono::seconds(8)){farm_transition(CargoStage::Blocked);farm_note("Entrega sem confirmacao; pedido nao repetido");}
        else farm_note("Aguardando confirmacao da entrega");return;
    }
    if(farm_stage==CargoStage::Pickup){
        if(started){
            if(!box||!pointer(box,0x10)||!target||!pointer(target,0x10)){farm_note("Aguardando caixa e destino");return;}
            if(!position(target,farm_destination,scratch))return;
            root(farm_box,box);farm_pickup_pending=false;farm_transition(CargoStage::ToDelivery);return;
        }
        uint8_t available=0;read(activity+0x58,available);
        if(!available){farm_note("Aguardando coleta ficar disponivel");return;}
        if(!farm_pickup_pending||now-farm_interact_at>=std::chrono::milliseconds(700)){
            if(farm_pickup_attempts>=3){farm_transition(CargoStage::Blocked);farm_note("Coleta sem confirmacao; reposicione e reative");return;}
            farm_pickup_pending=true;farm_interact_at=now;++farm_pickup_attempts;
            invoke(farm_interact,(void*)activity,nullptr,scratch);farm_note("Coleta nativa solicitada");
        }
        return;
    }
    if(!started){farm_transition(CargoStage::Return);return;}
    if(rooted(farm_box)!=box){farm_transition(CargoStage::Blocked);farm_note("Caixa mudou durante a rota; reative o farm");return;}
    if(farm_stage==CargoStage::ToDelivery){farm_note("Indo ao destino da carga por TP");farm_goto(local,pos,farm_destination,CargoStage::Deliver,scratch);return;}
    if(farm_stage==CargoStage::Deliver){
        if(!movement_control::reached(pos,farm_destination,1.f)){farm_transition(CargoStage::ToDelivery);return;}
        uint8_t locked=1;read(pointer(pointer(local,0x90),0x68)+0x25,locked);
        if(locked){
            if(now-farm_stage_at>std::chrono::seconds(8)){farm_transition(CargoStage::Blocked);farm_note("Controle nao liberou; farm interrompido");}
            else farm_note("Aguardando liberacao nativa da coleta");return;
        }
        const auto trigger=pointer(cargo,0x58);
        if(!farm_deliver||!trigger||!pointer(trigger,0x10)){farm_transition(CargoStage::Blocked);farm_note("Trigger de entrega indisponivel");return;}
        void* args[]={(void*)trigger};const int errors=scratch.invoke_errors;
        invoke(farm_deliver,(void*)cargo,args,scratch);++farm_requests;
        farm_transition(scratch.invoke_errors==errors?CargoStage::WaitDelivery:CargoStage::Blocked);
        farm_note(scratch.invoke_errors==errors?"Entrega solicitada uma vez":"Falha na entrega nativa");
    }
}
static void farm_init(){
    auto cargo=find_class_any("JarviGames.GamePlay","CarMeetCargoBehaviour");
    if(cargo){farm_delay_fields[0]=class_get_field(cargo,"_getUpCargoDelay");farm_delay_fields[1]=class_get_field(cargo,"_defaultTriggerDelay");}
    auto activity=find_class_any("JarviGames.GamePlay","CarMeetCargoActivity");
    if(activity){farm_activity_type=type_get_object(class_get_type(activity));farm_interact=class_get_method(activity,"AACABDCCCDCADBADCDBCACD",0);}
    farm_deliver=method("JarviGames.GamePlay","CarMeetCargoBehaviour","CCAACABBCBDAABBBAACBCDB",1);
}

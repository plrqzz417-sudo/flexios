// C++ / Menu.cpp / iOS ARM64 ImGui 1.90.4
#include "Menu.h"
#include "Cheat.h"
#include "Auth.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>
#include <cstdio>

#include "MenuLanguage.inl"


static bool opened=true;
static int page=0;
static bool color_editor=false;
static ImVec2 menu_pos(60,60),menu_size(960,620),bubble_pos(0,0);
static ImVec2 bubble_size(240,96);
static bool first_layout=true,bubble_moved=false;
static bool aim_button_enabled=true;
static ImVec2 aim_button_pos(0,0),aim_button_size(160,64);
static float aim_bounds[4]={};static bool aim_button_published=false;
// Unity KeyCode values, not Android or Windows virtual-key codes.
static const int toggle_keys[]={0,282,283,284,285,286,287,288,289,290,291,292,293,
    97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,
    48,49,50,51,52,53,54,55,56,57,9,308,306};
static const char* toggle_names[]={"Sem tecla","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12",
    "A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
    "0","1","2","3","4","5","6","7","8","9","Tab","Alt esquerdo","Ctrl esquerdo"};
static_assert(sizeof(toggle_keys)/sizeof(*toggle_keys)==sizeof(toggle_names)/sizeof(*toggle_names));
static std::atomic<int> toggle_key{287};
static std::atomic<int> menu_toggle_key{285}; // F4
static std::atomic<unsigned> menu_toggle_requests{0};
int Menu_GetToggleKey(){return toggle_key.load();}
int Menu_GetMenuToggleKey(){return menu_toggle_key.load();}
void Menu_ToggleRequested(){menu_toggle_requests.fetch_add(1,std::memory_order_relaxed);}
static std::mutex input_mutex;
static std::atomic<bool> published_open{true};
static bool captured=false;
static float bounds[4]={60,60,880,570};
struct Touch { int action; float x,y; };
static std::vector<Touch> events;
static const ImVec4 Red(.90f,.91f,.93f,1),Muted(.48f,.49f,.52f,1),White(.94f,.94f,.95f,1);
static float scale=1;
bool Menu_IsOpen() { return published_open.load(std::memory_order_relaxed); }
bool Menu_AimbotEnabled() { return Cheat_GetAimbot().enabled; }
void Menu_GetBounds(float* x,float* y,float* w,float* h) {
    std::lock_guard<std::mutex> lock(input_mutex);
    if(x) *x=bounds[0]; if(y) *y=bounds[1]; if(w) *w=bounds[2]; if(h) *h=bounds[3];
}
bool Menu_HitTest(float x,float y){
    std::lock_guard<std::mutex> lock(input_mutex);
    const bool main=x>=bounds[0]&&y>=bounds[1]&&x<=bounds[0]+bounds[2]&&y<=bounds[1]+bounds[3];
    const bool aim=aim_button_published&&x>=aim_bounds[0]&&y>=aim_bounds[1]&&
                   x<=aim_bounds[0]+aim_bounds[2]&&y<=aim_bounds[1]+aim_bounds[3];
    return main||aim;
}
bool Menu_QueueTouch(int action,float x,float y) {
    std::lock_guard<std::mutex> lock(input_mutex);
    if(action==0) {
        bool main=x>=bounds[0]&&y>=bounds[1]&&x<=bounds[0]+bounds[2]&&y<=bounds[1]+bounds[3];
        bool aim=aim_button_published&&x>=aim_bounds[0]&&y>=aim_bounds[1]&&x<=aim_bounds[0]+aim_bounds[2]&&y<=aim_bounds[1]+aim_bounds[3];
        captured=main||aim;
    }
    bool handled=captured;
    if(handled) {
        if(action==2&&!events.empty()&&events.back().action==2) events.back()={action,x,y};
        else events.push_back({action,x,y});
    }
    if(action==1||action==3||action==4||action==6) captured=false;
    return handled;
}
void Menu_DrainInput() {
    static std::vector<Touch> pending;pending.clear();
    { std::lock_guard<std::mutex> lock(input_mutex); pending.swap(events); }
    auto& io=ImGui::GetIO();
    for(const auto& event:pending) {
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        io.AddMousePosEvent(event.x,event.y);
        if(event.action==0) io.AddMouseButtonEvent(0,true);
        else if(event.action!=2) io.AddMouseButtonEvent(0,false);
    }
    pending.clear();
}
static void publish() {
    const bool button_visible=(aim_button_enabled||Cheat_GetAimbot().fly)&&Auth_Allowed();
    std::lock_guard<std::mutex> lock(input_mutex);
    published_open=opened;
    aim_button_published=button_visible;
    aim_bounds[0]=aim_button_pos.x;aim_bounds[1]=aim_button_pos.y;
    aim_bounds[2]=aim_button_size.x;aim_bounds[3]=aim_button_size.y;
    bounds[0]=opened?menu_pos.x:bubble_pos.x; bounds[1]=opened?menu_pos.y:bubble_pos.y;
    bounds[2]=opened?menu_size.x:bubble_size.x; bounds[3]=opened?menu_size.y:bubble_size.y;
}
static std::atomic<bool> stream_requested{false};
static std::atomic<StreamModeState> stream_state{StreamModeState::Off};
void Menu_RequestStreamMode(bool enabled) { stream_requested=enabled; stream_state=StreamModeState::Applying; }
bool Menu_StreamModeRequested() { return stream_requested.load(); }
StreamModeState Menu_StreamModeState() { return stream_state.load(); }
void Menu_SetStreamModeState(StreamModeState state) { stream_state=state; }
static void theme() {
    static float applied=-1;if(applied==scale)return;applied=scale;
    auto& style=ImGui::GetStyle();
    style.WindowPadding={20*scale,18*scale};style.FramePadding={12*scale,9*scale};
    style.ItemSpacing={14*scale,9*scale};style.ItemInnerSpacing={8*scale,5*scale};
    style.WindowRounding=12*scale;style.ChildRounding=9*scale;style.FrameRounding=6*scale;
    style.GrabRounding=5*scale;style.GrabMinSize=20*scale;style.ScrollbarSize=15*scale;
    style.WindowBorderSize=1;style.ChildBorderSize=1;style.FrameBorderSize=1;
    style.Colors[ImGuiCol_Text]=White;style.Colors[ImGuiCol_TextDisabled]=Muted;
    style.Colors[ImGuiCol_WindowBg]={.035f,.035f,.04f,.99f};
    style.Colors[ImGuiCol_ChildBg]={.055f,.055f,.062f,1};
    style.Colors[ImGuiCol_FrameBg]={.09f,.09f,.10f,1};
    style.Colors[ImGuiCol_FrameBgHovered]={.14f,.14f,.15f,1};
    style.Colors[ImGuiCol_FrameBgActive]={.18f,.18f,.19f,1};
    style.Colors[ImGuiCol_Button]={.11f,.11f,.12f,1};
    style.Colors[ImGuiCol_ButtonHovered]={.19f,.19f,.20f,1};style.Colors[ImGuiCol_ButtonActive]={.25f,.25f,.26f,1};
    style.Colors[ImGuiCol_CheckMark]=White;style.Colors[ImGuiCol_SliderGrab]=White;style.Colors[ImGuiCol_SliderGrabActive]=White;
    style.Colors[ImGuiCol_Header]={.14f,.14f,.15f,1};style.Colors[ImGuiCol_HeaderHovered]={.20f,.20f,.21f,1};style.Colors[ImGuiCol_HeaderActive]={.25f,.25f,.26f,1};
    style.Colors[ImGuiCol_PopupBg]={.06f,.06f,.07f,1};
    style.Colors[ImGuiCol_Border]={.18f,.18f,.20f,1};style.Colors[ImGuiCol_Separator]=style.Colors[ImGuiCol_Border];
    style.Colors[ImGuiCol_ScrollbarBg]={0,0,0,0};style.Colors[ImGuiCol_ScrollbarGrab]={.26f,.26f,.28f,1};
}
static void heading(const char* title,const char* detail=nullptr) {
    ImGui::Spacing();ImGui::TextColored(White,"%s",title);ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
    if(detail&&ImGui::IsItemHovered())ImGui::SetTooltip("%s",detail);
}
static bool toggle(const char* label,bool& value,const char* detail=nullptr) {
    const auto p=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x;
    const float sw=36*scale,sh=18*scale,available=std::max(1.f,width-sw-18*scale);
    const float font=ImGui::GetFontSize();
    const auto text=ImGui::GetFont()->CalcTextSizeA(font,10000,available,label);
    const float height=std::max(std::max(32.f,39*scale),text.y+12*scale);
    ImGui::PushID(label);const bool clicked=ImGui::InvisibleButton("##switch",{width,height});if(clicked)value=!value;
    auto draw=ImGui::GetWindowDrawList();
    if(ImGui::IsItemHovered())draw->AddRectFilled(p,{p.x+width,p.y+height},IM_COL32(32,32,36,255),5*scale);
    draw->AddText(ImGui::GetFont(),font,{p.x+2*scale,p.y+(height-text.y)*.5f},ImGui::GetColorU32(White),label,nullptr,available);
    const ImVec2 a(p.x+width-sw,p.y+(height-sh)*.5f);
    draw->AddRectFilled(a,{a.x+sw,a.y+sh},value?IM_COL32(220,222,229,255):IM_COL32(51,51,58,255),sh*.5f);
    draw->AddCircleFilled({a.x+(value?sw-sh*.5f:sh*.5f),a.y+sh*.5f},sh*.5f-3*scale,value?IM_COL32(20,20,23,255):IM_COL32(151,151,160,255),20);
    if(detail&&ImGui::IsItemHovered())ImGui::SetTooltip("%s",detail);
    ImGui::PopID();return clicked;
}
static bool range(const char* label,float& v,float lo,float hi,const char* format) {
    ImGui::PushID(label);float width=ImGui::GetContentRegionAvail().x;
    char text[40];std::snprintf(text,sizeof(text),format,v);
    const auto header=ImGui::GetCursorScreenPos();const auto value_size=ImGui::CalcTextSize(text);
    const float label_width=std::max(1.f,width-value_size.x-24*scale);
    const auto label_size=ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(),10000,label_width,label);
    const float header_height=std::max(label_size.y,value_size.y);
    auto header_draw=ImGui::GetWindowDrawList();
    header_draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),header,ImGui::GetColorU32(Muted),label,nullptr,label_width);
    const ImVec2 badge(header.x+width-value_size.x-7*scale,header.y-2*scale);
    header_draw->AddRectFilled(badge,{header.x+width+3*scale,header.y+value_size.y+2*scale},IM_COL32(36,37,43,255),4*scale);
    header_draw->AddText({badge.x+7*scale,header.y},ImGui::GetColorU32(White),text);
    ImGui::Dummy({width,header_height});
    auto p=ImGui::GetCursorScreenPos();float h=std::max(30.f,30*scale),r=9*scale;
    ImGui::InvisibleButton("##slider",ImVec2(width,h));bool changed=false;
    if(ImGui::IsItemActive()&&ImGui::IsMouseDown(0)) {
        float t=std::clamp((ImGui::GetIO().MousePos.x-p.x-r)/std::max(1.f,width-2*r),0.f,1.f);
        float next=lo+(hi-lo)*t;changed=next!=v;v=next;
    }
    auto d=ImGui::GetWindowDrawList();float x=p.x+r+(width-2*r)*std::clamp((v-lo)/(hi-lo),0.f,1.f),y=p.y+h*.5f;
    d->AddLine(ImVec2(p.x+r,y),ImVec2(p.x+width-r,y),IM_COL32(50,50,57,255),5*scale);
    d->AddLine(ImVec2(p.x+r,y),ImVec2(x,y),ImGui::GetColorU32(Red),5*scale);
    d->AddCircleFilled(ImVec2(x,y),r+3*scale,IM_COL32(255,255,255,20),24);
    d->AddCircleFilled(ImVec2(x,y),r,ImGui::GetColorU32(Red),24);
    ImGui::PopID();return changed;
}
static bool choice(const char* label,int* selected,const char* const* values,int count) {
    ImGui::TextDisabled("%s",label);ImGui::SetNextItemWidth(-1);
    ImGui::PushID(label);const bool changed=ImGui::Combo("##choice",selected,values,count);ImGui::PopID();return changed;
}
static void metric(const char* label,const char* value,bool highlight=false) {
    ImGui::TextDisabled("%s",label); ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),ImGui::GetWindowContentRegionMax().x-ImGui::CalcTextSize(value).x));
    ImGui::TextColored(highlight?Red:White,"%s",value);
}
static void number(const char* label,int n) { char b[32];std::snprintf(b,sizeof(b),"%d",n);metric(label,b); }
static void nav(int id,const char* title) {
    auto p=ImGui::GetCursorScreenPos();float width=ImGui::GetContentRegionAvail().x;
    const float height=std::max(32.f,42*scale);
    ImGui::PushID(id);if(ImGui::InvisibleButton("##nav",ImVec2(width,height)))page=id;
    auto d=ImGui::GetWindowDrawList();auto color=ImGui::GetColorU32(page==id?Red:Muted);
    if(page==id||ImGui::IsItemHovered()) {
        d->AddRectFilled(p,ImVec2(p.x+width,p.y+height),page==id?IM_COL32(42,43,49,255):IM_COL32(29,30,35,255),6*scale);
        if(page==id)d->AddRectFilled(ImVec2(p.x,p.y+height*.25f),ImVec2(p.x+3*scale,p.y+height*.75f),color,2*scale);
    }
    ImVec2 center(p.x+20*scale,p.y+height*.5f);
    if(id==0) { d->AddCircle(center,8*scale,color,24,1.5f*scale);d->AddLine(ImVec2(center.x-13*scale,center.y),ImVec2(center.x+13*scale,center.y),color,scale);d->AddLine(ImVec2(center.x,center.y-13*scale),ImVec2(center.x,center.y+13*scale),color,scale); }
    else if(id==1) { d->AddEllipse(center,12*scale,7*scale,color,0,24,1.5f*scale);d->AddCircleFilled(center,3*scale,color); }
    else if(id==2) { d->AddRect(ImVec2(center.x-11*scale,center.y-8*scale),ImVec2(center.x+11*scale,center.y+7*scale),color,2*scale,0,1.5f*scale);d->AddLine(ImVec2(center.x,center.y+8*scale),ImVec2(center.x,center.y+13*scale),color,1.5f*scale); }
    else if(id==4){d->AddCircle({center.x,center.y-5*scale},5*scale,color,16,1.5f*scale);d->AddLine({center.x-9*scale,center.y+10*scale},{center.x-6*scale,center.y+3*scale},color,1.5f*scale);d->AddLine({center.x-6*scale,center.y+3*scale},{center.x+6*scale,center.y+3*scale},color,1.5f*scale);d->AddLine({center.x+6*scale,center.y+3*scale},{center.x+9*scale,center.y+10*scale},color,1.5f*scale);}
    else if(id==5){d->AddLine({center.x,center.y+11*scale},{center.x,center.y-11*scale},color,1.6f*scale);d->AddLine({center.x,center.y-11*scale},{center.x-7*scale,center.y-3*scale},color,1.6f*scale);d->AddLine({center.x,center.y-11*scale},{center.x+7*scale,center.y-3*scale},color,1.6f*scale);}
    else if(id==7){d->AddRect({center.x-9*scale,center.y-9*scale},{center.x+9*scale,center.y+9*scale},color,2*scale,0,1.5f*scale);d->AddLine({center.x-9*scale,center.y-3*scale},{center.x+9*scale,center.y-3*scale},color,1.5f*scale);d->AddLine({center.x,center.y-9*scale},{center.x,center.y+2*scale},color,1.5f*scale);}
    else { for(int j=0;j<3;++j) { float y=center.y+(j-1)*7*scale;d->AddLine(ImVec2(center.x-9*scale,y),ImVec2(center.x+9*scale,y),color,scale);d->AddCircleFilled(ImVec2(center.x+(j==1?4:-4)*scale,y),2.5f*scale,color); } }
    d->AddText(ImVec2(p.x+42*scale,p.y+(height-ImGui::GetFontSize())*.5f),ImGui::GetColorU32(page==id?White:Muted),title);
    ImGui::PopID();
}
static void preview() {
    heading(tr("Preview do ESP"),"Aparencia dos elementos ativados");
    auto p=ImGui::GetCursorScreenPos();auto size=ImGui::GetContentRegionAvail();size.y=std::max(80.f*scale,size.y-18*scale);
    ImGui::InvisibleButton("##preview",size);auto d=ImGui::GetWindowDrawList();float cx=p.x+size.x*.5f,top=p.y+32*scale,h=size.y-65*scale,w=h*.42f;
    auto c=ImGui::GetColorU32(ImVec4(g_cfg.esp_color[0],g_cfg.esp_color[1],g_cfg.esp_color[2],g_cfg.esp_color[3]));
    if(!g_cfg.esp_enabled) { d->AddText(ImVec2(p.x+16*scale,p.y+35*scale),ImGui::GetColorU32(Muted),tr("ESP desativado"));return; }
    for(int i=1;i<4;++i) d->AddLine(ImVec2(p.x+i*size.x/4,p.y),ImVec2(p.x+i*size.x/4,p.y+size.y),IM_COL32(29,29,35,255));
    if(g_cfg.esp_skeleton) {
        d->AddCircle(ImVec2(cx,top+h*.10f),h*.065f,c,24,1.5f*scale);
        d->AddLine(ImVec2(cx,top+h*.17f),ImVec2(cx,top+h*.58f),c,1.5f*scale);
        for(float dir:{-1.f,1.f}) { d->AddLine(ImVec2(cx,top+h*.28f),ImVec2(cx+dir*w*.48f,top+h*.53f),c,1.5f*scale);d->AddLine(ImVec2(cx,top+h*.58f),ImVec2(cx+dir*w*.4f,top+h*.96f),c,1.5f*scale); }
    }
    if(g_cfg.esp_box) {
        if(g_cfg.esp_corner) { for(int x:{-1,1})for(int y:{0,1}) { ImVec2 a(cx+x*w*.6f,top+y*h);d->AddLine(a,ImVec2(a.x-x*w*.3f,a.y),c,2*scale);d->AddLine(a,ImVec2(a.x,a.y+(y?-1:1)*h*.18f),c,2*scale); } }
        else d->AddRect(ImVec2(cx-w*.6f,top),ImVec2(cx+w*.6f,top+h),c,0,0,g_cfg.esp_thickness*scale);
    }
    if(g_cfg.esp_health)d->AddRectFilled(ImVec2(cx-w*.6f-(g_cfg.bar_width+3)*scale,top+h*.22f),ImVec2(cx-w*.6f-3*scale,top+h),IM_COL32(90,215,145,255));
    if(g_cfg.esp_line)d->AddLine(ImVec2(cx,p.y+size.y),ImVec2(cx,top+h),c,scale);
    if(g_cfg.esp_name) { const char* name=tr("Player | 35m");d->AddText(ImVec2(cx-ImGui::CalcTextSize(name).x*.5f,top-26*scale),ImGui::GetColorU32(White),name); }
}
static const char* stream_label() {
    switch(Menu_StreamModeState()) {
        case StreamModeState::Off:return tr("Desligado");
        case StreamModeState::Applying:return tr("Aplicando...");
        case StreamModeState::Requested:return tr("Solicitado - confira na gravacao");
        case StreamModeState::Unavailable:return tr("Indisponivel neste sistema");
        default:return tr("Falha ao aplicar");
    }
}
static void center_bubble(const ImVec2& display) {
    const float ui=std::max(.8f,scale);
    bubble_size=ImVec2(146*ui,56*ui);
    bubble_pos=ImVec2(std::max(0.f,(display.x-bubble_size.x)*.5f),std::max(0.f,display.y*.12f));
}
// One touch target for both tap and drag; releasing a drag never toggles it.
static bool floating_control(const char* id,ImVec2& pos,ImVec2 size,bool aim,bool enabled,bool& dragged) {
    const auto& io=ImGui::GetIO();const float ui=std::max(.8f,scale);
    const ImVec2 p=ImGui::GetCursorScreenPos();
    const bool clicked=ImGui::InvisibleButton(id,size);
    if(ImGui::IsItemActivated())dragged=false;
    if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(0,6.f*ui)) {
        dragged=true;pos.x+=io.MouseDelta.x;pos.y+=io.MouseDelta.y;
    }
    auto d=ImGui::GetWindowDrawList();
    const ImVec2 end(p.x+size.x,p.y+size.y),center(p.x+29*ui,p.y+size.y*.5f);
    d->AddRectFilled(ImVec2(p.x,p.y+3*ui),ImVec2(end.x,end.y+3*ui),IM_COL32(0,0,0,65),9*ui);
    d->AddRectFilled(p,end,ImGui::IsItemActive()?IM_COL32(38,38,42,250):IM_COL32(14,14,17,248),9*ui);
    d->AddRect(p,end,enabled?IM_COL32(202,204,214,230):IM_COL32(79,79,89,200),9*ui,0,1.4f*ui);
    d->AddCircleFilled(center,18*ui,enabled?IM_COL32(224,225,230,255):IM_COL32(37,37,43,255),32);
    if(aim) {
        const ImU32 ink=enabled?IM_COL32(18,18,22,255):IM_COL32(155,155,167,255);
        d->AddCircle(center,8*ui,ink,24,1.6f*ui);
        d->AddLine(ImVec2(center.x-12*ui,center.y),ImVec2(center.x+12*ui,center.y),ink,1.4f*ui);
        d->AddLine(ImVec2(center.x,center.y-12*ui),ImVec2(center.x,center.y+12*ui),ink,1.4f*ui);
        d->AddCircleFilled(ImVec2(end.x-15*ui,center.y),3*ui,enabled?IM_COL32(235,235,242,255):ink,16);
    } else {
        const float font=15*ui;const ImVec2 text=ImGui::GetFont()->CalcTextSizeA(font,1000,0,"JM");
        d->AddText(ImGui::GetFont(),font,ImVec2(center.x-text.x*.5f,center.y-text.y*.5f),IM_COL32(19,19,24,255),"JM");
    }
    const char* label=aim?tr(enabled?"MIRA ON":"MIRA OFF"):tr("MENU");
    const float available=size.x-54*ui-(aim?25.f:12.f)*ui;
    const float natural=ImGui::GetFont()->CalcTextSizeA(13*ui,1000,0,label).x;
    const float font=13*ui*std::min(1.f,available/std::max(1.f,natural));
    const ImVec2 text=ImGui::GetFont()->CalcTextSizeA(font,1000,0,label);
    d->AddText(ImGui::GetFont(),font,ImVec2(p.x+54*ui,center.y-text.y*.5f),IM_COL32(232,239,246,255),label);
    return clicked&&!dragged;
}
static void render_aim_button(const ImVec2& display) {
    int fly_vertical=0;
    bool fly=Cheat_GetAimbot().fly;
    if((!aim_button_enabled&&!fly)||!Auth_Allowed()){Cheat_SetFlyVertical(0);return;}
    const float ui=std::max(.8f,scale);
    const ImVec2 control_size(156*ui,56*ui);
    aim_button_size=ImVec2(control_size.x,control_size.y+(fly?64.f:0));
    aim_button_pos.x=std::clamp(aim_button_pos.x,0.f,std::max(0.f,display.x-aim_button_size.x));
    aim_button_pos.y=std::clamp(aim_button_pos.y,0.f,std::max(0.f,display.y-aim_button_size.y));
    ImGui::SetNextWindowPos(aim_button_pos,ImGuiCond_Always);ImGui::SetNextWindowSize(aim_button_size,ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::Begin("##aim_toggle",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground);
    ImGui::SetWindowFontScale(1.0f/scale);
    bool enabled=Cheat_GetAimbot().enabled;
    static bool dragged=false;
    if(floating_control("##aim_action",aim_button_pos,control_size,true,enabled,dragged))Cheat_ToggleAimbot();
    if(fly){
        float half=(aim_button_size.x-16)*.5f;
        ImGui::Button(tr("SUBIR"),ImVec2(half,52));if(ImGui::IsItemActive()&&ImGui::IsMouseDown(0))fly_vertical=1;
        ImGui::SameLine(0,8);ImGui::Button(tr("DESCER"),ImVec2(half,52));if(ImGui::IsItemActive()&&ImGui::IsMouseDown(0))fly_vertical=-1;
    }
    Cheat_SetFlyVertical(fly_vertical);
    ImGui::End();ImGui::PopStyleVar();
}
#ifdef JM_HYBRID
#include "HybridBridge.inl"
#endif
void Menu_Render() {
#ifdef JM_HYBRID
    hybrid::tick();
#endif
    auto& io=ImGui::GetIO();if(io.DisplaySize.x<=0||io.DisplaySize.y<=0)return;
    if(menu_toggle_requests.exchange(0,std::memory_order_relaxed)&1u)opened=!opened;
    scale=std::clamp(std::min(io.DisplaySize.x/1120,io.DisplaySize.y/680),.3f,1.35f);theme();io.FontGlobalScale=scale;
    if(first_layout) {
        menu_size=ImVec2(1080*scale,640*scale);
        menu_pos=ImVec2(std::max(0.f,(io.DisplaySize.x-menu_size.x)*.5f),std::max(0.f,(io.DisplaySize.y-menu_size.y)*.5f));
        center_bubble(io.DisplaySize);aim_button_pos=ImVec2(io.DisplaySize.x*.82f,io.DisplaySize.y*.42f);first_layout=false;
    }
    static ImVec2 previous_display;
    if(!opened&&!bubble_moved&&(previous_display.x!=io.DisplaySize.x||previous_display.y!=io.DisplaySize.y))center_bubble(io.DisplaySize);
    previous_display=io.DisplaySize;
    render_aim_button(io.DisplaySize);
    if(!opened) {
        bubble_pos.x=std::clamp(bubble_pos.x,0.f,std::max(0.f,io.DisplaySize.x-bubble_size.x));bubble_pos.y=std::clamp(bubble_pos.y,0.f,std::max(0.f,io.DisplaySize.y-bubble_size.y));
        ImGui::SetNextWindowPos(bubble_pos,ImGuiCond_Always);ImGui::SetNextWindowSize(bubble_size,ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        ImGui::Begin("##reopen",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBackground);
        static bool dragged=false;
        if(floating_control("##open_action",bubble_pos,bubble_size,false,true,dragged))opened=true;
        if(dragged)bubble_moved=true;
        ImGui::End();ImGui::PopStyleVar();publish();return;
    }
    menu_size=ImVec2(1080*scale,640*scale);menu_pos.x=std::clamp(menu_pos.x,0.f,std::max(0.f,io.DisplaySize.x-menu_size.x));menu_pos.y=std::clamp(menu_pos.y,0.f,std::max(0.f,io.DisplaySize.y-menu_size.y));
    ImGui::SetNextWindowPos(menu_pos,ImGuiCond_Always);ImGui::SetNextWindowSize(menu_size,ImGuiCond_Always);
    ImGui::Begin("##FlexCityV3",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
    auto p=ImGui::GetCursorScreenPos();float width=ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("##drag",ImVec2(width-44*scale,52*scale));
    if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(0,0)){menu_pos.x+=io.MouseDelta.x;menu_pos.y+=io.MouseDelta.y;}
    auto d=ImGui::GetWindowDrawList();
    d->AddCircleFilled(ImVec2(p.x+18*scale,p.y+18*scale),18*scale,ImGui::GetColorU32(Red),32);
    d->AddText(ImVec2(p.x+7*scale,p.y+9*scale),IM_COL32(20,20,25,255),"JM");
    const char* titles[]={tr("Aimbot"),tr("Visuais"),tr("Stream"),tr("Sessao"),tr("Jogadores"),tr("Movimento"),tr("Personalizar"),tr("Carregamento")};
    const char* title=titles[std::clamp(page,0,7)];
    d->AddText(ImVec2(p.x+47*scale,p.y+1*scale),ImGui::GetColorU32(White),MOD_NAME);
    d->AddText(ImGui::GetFont(),ImGui::GetFontSize()*.8f,ImVec2(p.x+47*scale,p.y+26*scale),ImGui::GetColorU32(Muted),Auth_Allowed()?title:tr("Acesso por licenca"));
    d->AddText(ImGui::GetFont(),ImGui::GetFontSize()*.78f,ImVec2(p.x+width-175*scale,p.y+8*scale),ImGui::GetColorU32(Muted),JM_DISPLAY_EDITION "  /  v" JM_DISPLAY_VERSION);
    ImGui::SameLine(0,8*scale);if(ImGui::Button("x",ImVec2(36*scale,36*scale))){opened=false;if(!bubble_moved)center_bubble(io.DisplaySize);}
    ImGui::Separator();float height=ImGui::GetContentRegionAvail().y-12*scale;
    if(!Auth_Allowed()) {
        auto auth=Auth_Get();static char key[257]={};static bool loaded_saved_key=false;
        if(auth.ready&&!loaded_saved_key){auto saved=Auth_SavedKey();std::snprintf(key,sizeof(key),"%s",saved.c_str());std::fill(saved.begin(),saved.end(),0);loaded_saved_key=true;}
        const float login_width=ImGui::GetContentRegionAvail().x;
        ImGui::BeginChild("##identity",{login_width*.36f,height},true);
        auto art=ImGui::GetWindowDrawList();const auto origin=ImGui::GetCursorScreenPos();const auto area=ImGui::GetContentRegionAvail();
        for(int strand=0;strand<7;++strand) {
            ImVec2 from(origin.x+area.x*(strand+1)/8,origin.y);
            for(int segment=1;segment<8;++segment) {
                ImVec2 to(origin.x+area.x*(strand+1)/8+std::sin(float(segment*7+strand*3))*28*scale,origin.y+area.y*segment/7);
                art->AddLine(from,to,IM_COL32(148,148,161,27),3*scale);art->AddLine(from,to,IM_COL32(192,192,205,47),scale);from=to;
            }
        }
        const char* monogram="JM";const float size=62*scale;
        const auto text_size=ImGui::GetFont()->CalcTextSizeA(size,10000,0,monogram);
        art->AddText(ImGui::GetFont(),size,{origin.x+(area.x-text_size.x)*.5f,origin.y+area.y*.35f},IM_COL32(240,240,245,255),monogram);
        art->AddText({origin.x+32*scale,origin.y+area.y*.61f},IM_COL32(211,211,221,255),"JM FLEXMOD");
        art->AddText({origin.x+32*scale,origin.y+area.y*.68f},IM_COL32(130,130,144,255),JM_DISPLAY_EDITION "  /  v" JM_DISPLAY_VERSION);
        ImGui::EndChild();ImGui::SameLine();
        ImGui::BeginChild("##access",{0,height},true);
        heading(tr("Bem-vindo"));
        language_picker();
        ImGui::TextWrapped("%s",tr("Cole sua chave de licenca para entrar."));
        ImGui::Spacing();ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##license",key,sizeof(key),ImGuiInputTextFlags_Password|ImGuiInputTextFlags_ReadOnly);
        ImGui::BeginDisabled(auth.busy||!auth.ready);
        if(ImGui::Button(tr("Colar key"),ImVec2(-1,42*scale))) {
            auto clipboard=Auth_Clipboard();std::snprintf(key,sizeof(key),"%s",clipboard.c_str());
        }
        if(ImGui::Button(auth.busy?tr("Validando..."):tr("Ativar licenca"),ImVec2(-1,42*scale))) {
            Auth_Login(key);std::fill(std::begin(key),std::end(key),0);
        }
        if(ImGui::Button(tr("Esquecer key salva"),ImVec2(-1,34*scale))){Auth_ForgetKey();std::fill(std::begin(key),std::end(key),0);}
        ImGui::EndDisabled();ImGui::Spacing();ImGui::TextWrapped("%s",auth.message.c_str());
        ImGui::TextDisabled("%s",auth.ready?tr("Conexao HTTPS / validacao no servidor"):tr("Aguardando contexto Android..."));
        ImGui::EndChild();ImGui::End();publish();return;
    }
    auto st=Cheat_GetStatus();auto tune=Cheat_GetAimbot();bool changed=false;
    const float content_top=ImGui::GetCursorPosY(),sidebar=204*scale;
    const char* tabs[]={tr("Mira"),tr("Visuais"),tr("Stream"),tr("Sessao"),tr("Jogadores"),tr("Movimento"),tr("Personalizar"),tr("Carregamento")};
    ImGui::BeginChild("##navigation",{sidebar,height},true);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(0,3*scale));
    ImGui::TextDisabled("%s",tr("Principal"));ImGui::Spacing();
    for(int i:{0,1,4})nav(i,tabs[i]);
    ImGui::Spacing();ImGui::Separator();ImGui::Spacing();ImGui::TextDisabled("%s",tr("Ferramentas"));ImGui::Spacing();
    for(int i:{5,7,6,2,3})nav(i,tabs[i]);
    ImGui::PopStyleVar();ImGui::EndChild();
    ImGui::SetCursorPos({ImGui::GetStyle().WindowPadding.x+sidebar+14*scale,content_top});
    float remaining=ImGui::GetContentRegionAvail().x,left=(remaining-12*scale)*.5f;
    if(page==4) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(16*scale,16*scale));
        ImGui::BeginChild("##players",ImVec2(0,height),true);
        heading(tr("Jogadores da sessao"),"Exclusoes valem para aimbot e silent aim");
        static SessionPlayer entries[1024];bool ready=false;
        int count=Cheat_GetSessionPlayers(entries,1024,&ready);
        ImGui::TextDisabled("%d jogadores / atualiza a cada segundo",count);
        if(ImGui::Button(tr("Limpar exclusoes"),ImVec2(-1,34*scale)))Cheat_ClearExclusions();
        if(!ready)ImGui::TextWrapped("%s",tr("Aguardando a lista de jogadores da sala."));
        static int player_page=0;
        int page_size=std::clamp(int((ImGui::GetContentRegionAvail().y-65*scale)/std::max(32.f,42*scale)),3,12);
        int pages=std::max(1,(count+page_size-1)/page_size);player_page=std::clamp(player_page,0,pages-1);
        float page_button_width=(ImGui::GetContentRegionAvail().x-12*scale)*.5f;
        ImGui::BeginDisabled(player_page==0);
        if(ImGui::Button(tr("Anterior"),ImVec2(page_button_width,std::max(32.f,34*scale))))--player_page;
        ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(player_page+1>=pages);
        if(ImGui::Button(tr("Proxima"),ImVec2(page_button_width,std::max(32.f,34*scale))))++player_page;
        ImGui::EndDisabled();ImGui::TextDisabled(tr("Pagina %d / %d"),player_page+1,pages);
        if(ImGui::BeginTable("##roster",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_ScrollY|ImGuiTableFlags_SizingStretchProp,ImVec2(0,std::max(60.f,ImGui::GetContentRegionAvail().y)))) {
            ImGui::TableSetupColumn(tr("Nickname"),ImGuiTableColumnFlags_WidthStretch,2);
            ImGui::TableSetupColumn(tr("Conta"),ImGuiTableColumnFlags_WidthStretch,1.5f);
            ImGui::TableSetupColumn(tr("Mira"),ImGuiTableColumnFlags_WidthStretch,1.5f);
            ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
            for(int i=player_page*page_size;i<std::min(count,(player_page+1)*page_size);++i) {
                auto& entry=entries[i];ImGui::PushID(int(entry.entity));
                ImGui::TableNextRow(0,std::max(32.f,42*scale));
                ImGui::TableSetColumnIndex(0);ImGui::TextWrapped("%s%s",entry.name,entry.is_local?tr(" (voce)"):"");
                ImGui::TableSetColumnIndex(1);ImGui::TextWrapped("%s",*entry.account?entry.account:tr("Indisponivel"));
                ImGui::TableSetColumnIndex(2);ImGui::BeginDisabled(entry.is_local||!*entry.account);
                bool excluded=entry.excluded;
                if(ImGui::Checkbox(tr("Excluir da mira"),&excluded))Cheat_SetExcluded(entry.account,excluded);
                ImGui::EndDisabled();ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();ImGui::PopStyleVar();
        ImGui::End();publish();return;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(16*scale,16*scale));
    ImGui::BeginChild("##controls",ImVec2(left,height),true);
    if(page==0) {
        heading(tr("Geral"));
        changed|=toggle(tr("Solicitar 120 FPS"),tune.fps120);
        
        changed|=toggle(tr("Ativar mira"),tune.enabled);changed|=toggle(tr("Somente enquanto mira"),tune.require_aiming);
#if !defined(JM_APK) && !defined(JM_HYBRID)
        changed|=toggle(tr("Assistencia de mira do jogo"),tune.native_assist);
#endif
        ImGui::Spacing();ImGui::TextUnformatted(tr("ATALHO DA MIRA"));
        int key_index=0;for(int i=0;i<int(sizeof(toggle_keys)/sizeof(*toggle_keys));++i)if(toggle_keys[i]==toggle_key.load())key_index=i;
        if(choice(tr("Tecla liga/desliga"),&key_index,toggle_names,int(sizeof(toggle_names)/sizeof(*toggle_names))))toggle_key=toggle_keys[key_index];
        toggle(tr("Botao flutuante MIRA"),aim_button_enabled);
        
        if(!st.keybind_ready)ImGui::TextWrapped("Teclado indisponivel nesta versao do jogo; use o botao na tela.");
        changed|=toggle(tr("Travar consumo de municao"),tune.freeze_ammo);
        changed|=toggle(tr("Qualquer tiro como HS"),tune.body_headshot);
        changed|=toggle(tr("Rapid fire"),tune.rapid_fire);
        if(tune.rapid_fire)changed|=range(tr("Multiplicador de disparo"),tune.rapid_multiplier,1,5,"%.1fx");
        changed|=toggle(tr("Sem dispersao"),tune.no_spread);
        changed|=toggle(tr("Sem recuo"),tune.no_recoil);
        if(toggle(tr("Disparo automatico"),tune.auto_fire)){if(tune.auto_fire)tune.enabled=true;changed=true;}

    } else if(page==7) {
        heading(tr("Auto-farme Carregamento"));
        if(toggle(tr("Ativar auto-farme"),tune.cargo_farm)){if(tune.cargo_farm)tune.speed=tune.fly=false;changed=true;}
        changed|=range(tr("Passo TP apos entrega"),tune.cargo_step,2.f,4.f,"%.1f m");
    } else if(page==6) {
        heading(tr("Personagem local"));
        changed|=toggle(tr("Spin visual"),tune.spin_visual);
        changed|=range(tr("Velocidade do giro"),tune.spin_speed,30,1080,"%.0f graus/s");
        changed|=toggle(tr("Modo formiga (visual)"),tune.tiny_visual);
        changed|=range(tr("Escala do modelo"),tune.model_scale,.05f,1,"%.2fx");
    } else if(page==5) {
        heading(tr("Vida e movimento"));
        changed|=toggle(tr("Auto-revive gratuito + retorno"),tune.auto_revive);
        if(tune.auto_revive){char state[160];Cheat_GetReviveMessage(state,sizeof(state));ImGui::TextWrapped("%s",state);}
        ImGui::Spacing();heading(tr("Veiculos"));
        changed|=toggle(tr("Potencia do carro"),tune.vehicle_car);
        if(tune.vehicle_car)changed|=range(tr("Multiplicador do carro"),tune.vehicle_car_power,1,3,"%.1fx");
        changed|=toggle(tr("Potencia da moto"),tune.vehicle_bike);
        if(tune.vehicle_bike)changed|=range(tr("Multiplicador da moto"),tune.vehicle_bike_power,1,3,"%.1fx");
        changed|=toggle(tr("Potencia do barco"),tune.vehicle_boat);
        if(tune.vehicle_boat)changed|=range(tr("Multiplicador do barco"),tune.vehicle_boat_power,1,3,"%.1fx");
        ImGui::Spacing();changed|=toggle(tr("TP automatico ao marcar GPS"),tune.tp_marker);
        if(toggle(tr("Speed"),tune.speed)){if(tune.speed)tune.fly=tune.cargo_farm=false;changed=true;}
        if(tune.speed)changed|=range(tr("Velocidade a pe"),tune.move_speed,2,40,"%.0f m/s");
        if(toggle(tr("Fly"),tune.fly)){if(tune.fly)tune.speed=tune.cargo_farm=false;changed=true;}
        if(tune.fly)changed|=range(tr("Velocidade de voo"),tune.fly_speed,2,30,"%.0f m/s");
        if(tune.speed||tune.fly)changed|=range(tr("Resposta do movimento"),tune.move_response,0,.4f,"%.2f s");
        bool follow=Cheat_FollowSearchEnabled();
        if(toggle(tr("Seguir pela pesquisa"),follow))Cheat_SetFollowSearchEnabled(follow);
    } else if(page==1) {
        heading(tr("Elementos do ESP"),"Somente jogadores vivos e renderizados");
        toggle(tr("Ativar ESP"),g_cfg.esp_enabled);
        const char* rates[]={"30 FPS","60 FPS"};int rate=g_cfg.esp_hz.load()==60?1:0;
        if(choice(tr("Atualizacao do ESP"),&rate,rates,2))g_cfg.esp_hz=rate?60:30;toggle(tr("Caixa"),g_cfg.esp_box);toggle(tr("Caixa de cantos"),g_cfg.esp_corner);
        float esp_distance=g_cfg.esp_distance.load();
        if(range(tr("Distancia do ESP"),esp_distance,10,1000,"%.0f m"))g_cfg.esp_distance=esp_distance;
        bool skeleton=g_cfg.esp_skeleton.load();if(toggle(tr("Esqueleto"),skeleton))g_cfg.esp_skeleton=skeleton;
        bool passive=g_cfg.hide_passive.load(),invulnerable=g_cfg.hide_invulnerable.load(),friends=g_cfg.hide_friends.load();
        if(toggle(tr("Ocultar passivos"),passive))g_cfg.hide_passive=passive;
        if(toggle(tr("Ocultar invulneraveis"),invulnerable))g_cfg.hide_invulnerable=invulnerable;
        if(toggle(tr("Ocultar amigos"),friends))g_cfg.hide_friends=friends;
        toggle(tr("Circulo na cabeca"),g_cfg.esp_head);toggle(tr("Caixa preenchida"),g_cfg.esp_fill);toggle(tr("Vida numerica"),g_cfg.esp_hp_text);
        toggle(tr("Barra de vida"),g_cfg.esp_health);toggle(tr("Nickname e distancia"),g_cfg.esp_name);toggle(tr("Linhas"),g_cfg.esp_line);
        if(ImGui::ColorButton("##cor",ImVec4(g_cfg.esp_color[0],g_cfg.esp_color[1],g_cfg.esp_color[2],g_cfg.esp_color[3]),ImGuiColorEditFlags_NoTooltip,ImVec2(35*scale,25*scale)))color_editor=!color_editor;ImGui::SameLine();if(ImGui::Button(tr("Cor do ESP")))color_editor=!color_editor;
        range(tr("Espessura da caixa"),g_cfg.esp_thickness,1,3,"%.1f");
        range(tr("Largura da barra"),g_cfg.bar_width,3,12,"%.0f px");
    } else if(page==2) {
        heading(tr("Stream mode"));
        ImGui::TextWrapped("Captura seletiva do overlay indisponivel nesta edicao iOS.");
    } else {
        heading(tr("Estado da sessao"),st.initialization);
        const char* stages[]={tr("Callback"),tr("Pose IK"),tr("Descoberta"),tr("ESP"),tr("Lista de jogadores")};
        ImGui::TextDisabled("%s",tr("Custo medio / pico (ms)"));
        for(int i=0;i<5;++i)ImGui::TextDisabled("%s: %.2f / %.2f",stages[i],st.cost_mean_us[i]/1000.f,st.cost_peak_us[i]/1000.f);
        ImGui::TextDisabled(tr("Veiculo: %s / %.1fx"),st.vehicle_active==1?tr("Carro"):st.vehicle_active==2?tr("Moto"):st.vehicle_active==3?tr("Barco"):tr("Inativo"),st.vehicle_power);
        ImGui::TextDisabled("%s: %s / %s / %s",tr("Controles nativos"),(st.vehicle_ready&1)?tr("Carro"):"--",(st.vehicle_ready&2)?tr("Moto"):"--",(st.vehicle_ready&4)?tr("Barco"):"--");
        ImGui::TextDisabled(tr("Pose IK: %s / amostras: %u"),st.pose_ready?tr("conectado"):tr("indisponivel"),st.pose_samples);
        metric(tr("Atualizacoes"),st.hook_ready?tr("Conectadas"):tr("Aguardando"),st.hook_ready);metric(tr("Camera"),st.camera_ready?tr("Pronta"):tr("Aguardando"));
        number(tr("Jogadores"),st.players);number(tr("Desenhados"),st.visible);number(tr("Mortos filtrados"),st.dead_filtered);number(tr("Ocultos filtrados"),st.hidden_filtered);number(tr("Erros de chamadas"),st.invoke_errors);
        ImGui::Separator();ImGui::Spacing();ImGui::TextWrapped("%s",st.aim_state);ImGui::TextDisabled(tr("%.0f FPS do overlay"),io.Framerate);
        ImGui::TextDisabled(tr("Trava de camera: %s"),st.camera_lock_ready?tr("pronta"):tr("indisponivel"));
        ImGui::TextDisabled("Silent aim: %s",st.silent_ready?tr("conectado"):tr("indisponivel"));
        ImGui::TextDisabled(tr("Automatico: %s"),st.auto_fire_ready?(st.auto_firing?tr("disparando"):tr("pronto")):tr("indisponivel"));
        ImGui::TextDisabled(tr("Sem recuo: %s"),st.no_recoil_ready?"experimental / nao validado":tr("indisponivel"));
        ImGui::TextDisabled(tr("Disparo local: %s | Camera: %u"),st.local_fire_ready?tr("conectado"):tr("indisponivel"),st.recoil_camera_impulses);
        ImGui::TextDisabled(tr("Dispersao base: %s | IK residual: %s"),st.accuracy_ready?tr("conectada"):tr("indisponivel"),st.recoil_apply_ready?tr("conectado"):tr("indisponivel"));
        ImGui::Text(tr("Dispersao por tiro: %u / impulsos visuais: %u"),st.spread_overrides,st.recoil_impulses);
        ImGui::Text(tr("Consumo bloqueado: %u / %s"),st.ammo_blocked,st.ammo_ready?tr("conectado"):tr("indisponivel"));
        ImGui::Text(tr("Dispersao: %s / impactos marcados: %u"),st.spread_ready?tr("conectado"):tr("indisponivel"),st.head_marks);
        ImGui::Text(tr("Tiros / raios / redirecionados: %u / %u / %u"),st.shots,st.rays,st.redirects);
        ImGui::Text(tr("Protegidos / filtro sem dados: %d / %d"),st.protected_targets,st.filter_unknown);
        if(ImGui::Button(tr("Centralizar menu"),ImVec2(-1,38*scale)))menu_pos=ImVec2((io.DisplaySize.x-menu_size.x)/2,(io.DisplaySize.y-menu_size.y)/2);
    }
    ImGui::EndChild();ImGui::SameLine();ImGui::BeginChild("##details",ImVec2(0,height),true);
    if(page==0) {
        heading(tr("Configuracao"));
        bool all=tune.fov_deg>=359;if(toggle(tr("Aimbot 360 graus"),all)){tune.fov_deg=all?360:30;changed=true;}
        int mode=tune.silent?1:0;const char* modes[]={tr("Aimbot (camera)"),tr("Silent aim (disparo)")};
        if(choice(tr("Tipo de mira"),&mode,modes,2)) {tune.silent=mode==1;changed=true;}
        const char* bone_labels[5];
        for(int i=0;i<5;++i)bone_labels[i]=tr(AimBoneNames[i]);
        changed|=choice(tr("Alvo"),&tune.bone,bone_labels,5);

        const char* priorities[]={tr("Centro da mira"),tr("Menor distancia"),tr("Menor vida")};
        changed|=choice(tr("Prioridade do alvo"),&tune.target_priority,priorities,3);
        changed|=toggle(tr("Multiponto da cabeca"),tune.multipoint);
        ImGui::Spacing();changed|=range(tr("Campo de visao"),tune.fov_deg,1,360,"%.0f deg");changed|=range(tr("Distancia maxima"),tune.distanceMax,10,500,"%.0f m");if(tune.silent){changed|=range(tr("Chance de acerto"),tune.hit_chance,0,100,"%.0f%%");changed|=range(tr("Limite de correcao"),tune.silent_max_angle,1,15,"%.0f deg");}
        else {
            changed|=range(tr("Suavizacao (0 = instantaneo)"),tune.smoothing,0,10,"%.1f");
        }
        changed|=toggle(tr("Somente alvos visiveis"),tune.require_visible);
#if !defined(JM_APK) && !defined(JM_HYBRID)
        if(tune.native_assist){
            ImGui::Spacing();ImGui::Separator();ImGui::TextUnformatted(tr("ASSISTENCIA NATIVA"));
            changed|=range(tr("Velocidade X"),tune.assist_x,1,6,"%.1fx");
            changed|=range(tr("Velocidade Y"),tune.assist_y,1,6,"%.1fx");
            changed|=range(tr("Distancia da assistencia"),tune.assist_distance,1,4,"%.1fx");
            changed|=range(tr("Raio minimo"),tune.assist_radius_min,1,3,"%.1fx");
            changed|=range(tr("Raio maximo"),tune.assist_radius_max,1,3,"%.1fx");
            ImGui::TextDisabled("%s",st.native_assist_active?tr("Assistencia aplicada"):st.native_assist_ready?tr("Aguardando servico do jogo"):tr("Servico indisponivel"));
            ImGui::TextDisabled(tr("Leituras / ajustes: %u / %u"),st.native_assist_calls,st.native_assist_scaled);
            ImGui::Separator();
        }
#endif
        changed|=toggle(tr("Ignorar passivos"),tune.ignore_passive);
        changed|=toggle(tr("Ignorar invulneraveis"),tune.ignore_invulnerable);
        changed|=toggle(tr("Ignorar amigos"),tune.ignore_friends);
        toggle(tr("Exibir FOV"),g_cfg.show_fov);
        changed|=toggle(tr("Modo aleatorio"),tune.random_bones);
        if(tune.random_bones) {
            changed|=range(tr("Intervalo de troca"),tune.bone_interval,.2f,3,"%.2f s");ImGui::TextDisabled("%s",tr("Bones incluidos"));
            for(int i=0;i<5;++i){if(i%2)ImGui::SameLine(ImGui::GetWindowContentRegionMax().x*.5f);bool included=tune.bone_pool&(1u<<i);if(ImGui::Checkbox(bone_labels[i],&included)){tune.bone_pool=included?tune.bone_pool|(1u<<i):tune.bone_pool&~(1u<<i);tune.bone_pool=ValidBonePool(tune.bone_pool);changed=true;}}
        }
        ImGui::Spacing();ImGui::Separator();ImGui::TextColored(st.locked?Red:Muted,"%s",st.locked?tr("ALVO ADQUIRIDO"):tr("SEM ALVO"));
        ImGui::Text(tr("Bone atual: %s"),bone_labels[std::clamp(st.aim_bone,0,4)]);
        ImGui::TextDisabled("%s",tr("Estado da mira"));ImGui::TextWrapped("%s",st.aim_state);
        ImGui::TextDisabled(tr("Raio HS: %u / corrigido: %u / sobreposicao: %u / nao confirmado: %u"),st.hs_checked,st.hs_steered,st.hs_promoted,st.hs_missing);
        ImGui::TextDisabled(tr("Ponto: %s"),st.aim_point==8?tr("centro da cabeca pos-IK"):st.aim_point==7?tr("osso da cabeca pos-IK"):st.aim_point==6?tr("centro da geometria da cabeca"):st.aim_point==5?tr("pivo da malha (sem centro geometrico)"):st.aim_point==1?tr("ponto HS validado"):st.aim_point==2?tr("cabeca do Animator"):st.aim_point==3?tr("osso do Animator"):tr("aguardando"));
        if(st.aim_error_pixels>=0)ImGui::TextDisabled(tr("Erro no centro da mira: %.1f px"),st.aim_error_pixels);
        if(st.aim_rig_gap_cm>=0)ImGui::TextDisabled(tr("Distancia HS/cabeca visivel: %.1f cm"),st.aim_rig_gap_cm);
    } else if(page==7) {
        heading(tr("Estado da entrega"));char message[460];Cheat_GetFarmMessage(message,sizeof(message));ImGui::TextWrapped("%s",message);
    } else if(page==6) {
        heading(tr("Interface"));
        language_picker();
        int key_index=0;for(int i=0;i<int(sizeof(toggle_keys)/sizeof(*toggle_keys));++i)if(toggle_keys[i]==menu_toggle_key.load())key_index=i;
        if(choice(tr("Abrir/fechar menu"),&key_index,toggle_names,int(sizeof(toggle_names)/sizeof(*toggle_names))))menu_toggle_key=toggle_keys[key_index];
        if(ImGui::Button(tr("Centralizar menu"),ImVec2(-1,38*scale)))
            menu_pos=ImVec2((io.DisplaySize.x-menu_size.x)*.5f,(io.DisplaySize.y-menu_size.y)*.5f);
        if(ImGui::Button(tr("Reposicionar botoes"),ImVec2(-1,38*scale))){bubble_moved=false;center_bubble(io.DisplaySize);aim_button_pos={io.DisplaySize.x*.82f,io.DisplaySize.y*.42f};}
    } else if(page==5) {
        heading(tr("Estado e controles"));
        metric(tr("Speed"),tr(tune.speed?"Ligado":"Desligado"),tune.speed);metric(tr("Fly"),tr(tune.fly?"Ligado":"Desligado"),tune.fly);metric("TP GPS",tr(tune.tp_marker?"Ligado":"Desligado"),tune.tp_marker);
        ImGui::Spacing();
        ImGui::BeginDisabled(!tune.speed&&!tune.fly&&!tune.vehicle_car&&!tune.vehicle_bike&&!tune.vehicle_boat);
        if(ImGui::Button(tr("Parar movimento"),ImVec2(-1,42*scale))){tune.speed=tune.fly=tune.vehicle_car=tune.vehicle_bike=tune.vehicle_boat=false;Cheat_SetFlyVertical(0);changed=true;}
        ImGui::EndDisabled();
        char message[160];Cheat_GetMovementMessage(message,sizeof(message));ImGui::Spacing();ImGui::TextWrapped("%s",message);
        ImGui::Spacing();if(ImGui::CollapsingHeader(tr("Diagnostico"))){char diagnostics[300];Cheat_GetMovementDiagnostics(diagnostics,sizeof(diagnostics));ImGui::TextWrapped("%s",diagnostics);}
    } else if(page==1) {
        if(color_editor) { heading(tr("Cor do ESP"),"Ajuste a cor e a transparencia");ImGui::SetNextItemWidth(-1);ImGui::ColorPicker4("##picker",g_cfg.esp_color,ImGuiColorEditFlags_NoSidePreview|ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_AlphaBar);if(ImGui::Button(tr("Voltar ao preview"),ImVec2(-1,34*scale)))color_editor=false; }
        else preview();
    }
    else if(page==2) {
        heading(tr("Teste rapido"),"Verifique o resultado no arquivo gravado");
        const char* steps[]={"1. Ative o stream mode.","2. Grave com o menu aberto.","3. Feche a gravacao e reproduza.","4. Confira se o jogo aparece limpo."};
        for(auto text:steps){ImGui::Spacing();ImGui::TextWrapped("%s",text);ImGui::Spacing();}
    } else {
        heading(tune.silent?tr("Perfis de hit chance"):tr("Perfis de suavizacao"),"Aplicados sem trocar o bone selecionado");
        if(ImGui::Button(tune.silent?"50%":tr("Suave"),ImVec2(-1,42*scale))){if(tune.silent)tune.hit_chance=50;else {tune.fov_deg=15;tune.smoothing=6;}changed=true;}
        if(ImGui::Button(tune.silent?"75%":tr("Equilibrado"),ImVec2(-1,42*scale))){if(tune.silent)tune.hit_chance=75;else {tune.fov_deg=30;tune.smoothing=3;}changed=true;}
        if(ImGui::Button(tune.silent?"100%":tr("Direto"),ImVec2(-1,42*scale))){if(tune.silent)tune.hit_chance=100;else {tune.fov_deg=45;tune.smoothing=0;}changed=true;}
        ImGui::Spacing();ImGui::Separator();ImGui::TextDisabled("%s",tr("ENTRADA DE MIRA"));ImGui::TextUnformatted(st.aim_mode==0?tr("Solta"):st.aim_mode==1?tr("Mirando"):st.aim_mode==2?tr("Luneta"):tr("Aguardando"));
        ImGui::TextDisabled("%s",tr("Estado da mira"));ImGui::TextWrapped("%s",st.aim_state);
        ImGui::TextDisabled("%s",tr("STREAM MODE"));ImGui::TextWrapped("%s",stream_label());
    }
    ImGui::EndChild();ImGui::PopStyleVar();if(changed)Cheat_SetAimbot(tune);
    ImGui::End();publish();
}

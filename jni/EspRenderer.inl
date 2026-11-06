// Screen-space only. No Unity objects, memory reads or managed calls here.
#include <cmath>
#include <algorithm>
#include <cstdio>
static bool EspFriendFiltered(bool ignore,bool hide,bool known,bool is_friend){return (ignore||hide)&&(!known||is_friend);}
static bool esp_point_valid(Vector3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&p.z>.01f;}
struct EspBounds {float left,right,top,bottom,height,center;};
static bool esp_bounds(const TargetData& t,float width,float height,EspBounds& box){
    Vector3 head=t.screen_head,feet=t.screen_pos;
    if(!esp_point_valid(head)&&(t.bone_mask&(1u<<10)))head=t.bones[10];
    const int fallbacks[]={5,6,3,4,0};
    if(!esp_point_valid(feet))for(int i:fallbacks)if((t.bone_mask&(1u<<i))&&esp_point_valid(t.bones[i])){feet=t.bones[i];break;}
    if(!esp_point_valid(head)||!esp_point_valid(feet))return false;
    float head_y=height-head.y,feet_y=height-feet.y;
    float body=std::fabs(feet_y-head_y);
    if(!std::isfinite(body)||body<2||body>height*6.f)return false;
    float center=(head.x+feet.x)*.5f;
    float top=std::fmin(head_y,feet_y),bottom=std::fmax(head_y,feet_y);
    float left=std::fmin(head.x,feet.x)-body*.18f;
    float right=std::fmax(head.x,feet.x)+body*.18f;
    // Include valid animated joints, bounded to this actor. Detached limbs or
    // near-plane projection spikes cannot expand the box across the screen.
    for(int i=0;i<19;++i){if(!(t.bone_mask&(1u<<i)))continue;auto p=t.bones[i];
        if(!esp_point_valid(p))continue;float y=height-p.y;
        if(std::fabs(p.x-center)>body*.85f||y<top-body*.35f||y>bottom+body*.35f)continue;
        left=std::fmin(left,p.x);right=std::fmax(right,p.x);
        top=std::fmin(top,y);bottom=std::fmax(bottom,y);
    }
    box={std::clamp(left-body*.025f,-width,width*2),std::clamp(right+body*.025f,-width,width*2),
         std::clamp(top-body*.08f,-height,height*2),std::clamp(bottom+body*.025f,-height,height*2),body,center};
    return box.right>=0&&box.left<=width&&box.bottom>=0&&box.top<=height;
}

// Draw exclusively from a validated snapshot. All coordinates are clipped to
// the viewport; this layer never invokes Unity or changes the camera.
static void draw_esp_target(ImDrawList* dl,const TargetData& t,float width,float height,float scale,bool ignore_friends){
    if(!dl||!g_cfg.esp_enabled||!t.valid||!std::isfinite(t.distance))return;
    if(t.is_local&&!g_cfg.esp_self)return;
    if((g_cfg.hide_passive.load()&&t.passive)||(g_cfg.hide_invulnerable.load()&&t.invulnerable)||EspFriendFiltered(ignore_friends,g_cfg.hide_friends.load(),t.friend_known,t.friend_player))return;
    EspBounds b;if(!esp_bounds(t,width,height,b))return;
    ImU32 color=ImGui::GetColorU32(ImVec4(g_cfg.esp_color[0],g_cfg.esp_color[1],g_cfg.esp_color[2],g_cfg.esp_color[3]));
    float stroke=std::clamp(g_cfg.esp_thickness,1.f,3.f)*scale;
    const ImU32 shadow=IM_COL32(5,8,12,230),text=IM_COL32(239,242,247,255);
    auto line=[&](ImVec2 a,ImVec2 z){if(g_cfg.esp_outline)dl->AddLine(a,z,shadow,stroke+2*scale);dl->AddLine(a,z,color,stroke);};
    auto caption=[&](const char* value,float y){
        ImVec2 size=ImGui::CalcTextSize(value);float x=std::clamp(b.center-size.x*.5f,3.f,std::max(3.f,width-size.x-3));
        y=std::clamp(y,2.f,std::max(2.f,height-size.y-2));
        dl->AddRectFilled({x-5*scale,y-2*scale},{x+size.x+5*scale,y+size.y+2*scale},shadow,4*scale);
        dl->AddText({x,y},text,value);
    };
    dl->PushClipRect({0,0},{width,height},true);
    if(g_cfg.esp_fill){ImVec4 fill(g_cfg.esp_color[0],g_cfg.esp_color[1],g_cfg.esp_color[2],g_cfg.esp_color[3]*.12f);dl->AddRectFilled({b.left,b.top},{b.right,b.bottom},ImGui::GetColorU32(fill));}
    if(g_cfg.esp_skeleton.load())for(const auto& edge:SkeletonEdges){
        uint32_t mask=(1u<<edge[0])|(1u<<edge[1]);if((t.bone_mask&mask)!=mask)continue;
        auto a=t.bones[edge[0]],z=t.bones[edge[1]];if(!esp_point_valid(a)||!esp_point_valid(z))continue;
        float dx=a.x-z.x,dy=a.y-z.y,limit=std::min(height*.5f,b.height*.8f);
        if(dx*dx+dy*dy>limit*limit)continue;
        line({a.x,height-a.y},{z.x,height-z.y});
    }
    if(g_cfg.esp_box){
        if(g_cfg.esp_corner){float w=(b.right-b.left)*.25f,h=(b.bottom-b.top)*.17f;
            for(int x=0;x<2;++x)for(int y=0;y<2;++y){ImVec2 a{x?b.right:b.left,y?b.bottom:b.top};line(a,{a.x+(x?-w:w),a.y});line(a,{a.x,a.y+(y?-h:h)});}
        }else{line({b.left,b.top},{b.right,b.top});line({b.right,b.top},{b.right,b.bottom});line({b.right,b.bottom},{b.left,b.bottom});line({b.left,b.bottom},{b.left,b.top});}
    }
    if(g_cfg.esp_health&&t.maxHp>0){
        float fraction=std::clamp(float(t.hp)/t.maxHp,0.f,1.f),bar=std::clamp(g_cfg.bar_width,3.f,12.f)*scale;
        float x=b.left-bar-4*scale;dl->AddRectFilled({x-scale,b.top-scale},{x+bar+scale,b.bottom+scale},shadow);
        ImU32 health=IM_COL32(int(225-153*fraction),int(83+131*fraction),int(91+65*fraction),255);
        dl->AddRectFilled({x,b.bottom-(b.bottom-b.top)*fraction},{x+bar,b.bottom},health);
    }
    if(g_cfg.esp_name&&t.name[0])caption(t.name,b.top-ImGui::GetFontSize()-5*scale);
    if(g_cfg.esp_hp_text){char value[32];std::snprintf(value,sizeof(value),"%d / %d",int(t.hp),int(t.maxHp));caption(value,b.bottom+4*scale);}
    if(t.armor>0){char value[32];std::snprintf(value,sizeof(value),"%d / 100 AP",int(t.armor));
        const ImVec2 size=ImGui::CalcTextSize(value);const ImVec2 p(b.center-size.x*.5f,b.bottom+(g_cfg.esp_hp_text?23:4)*scale);
        dl->AddText({p.x+scale,p.y+scale},shadow,value);dl->AddText(p,IM_COL32(110,185,255,255),value);
    }
    if(g_cfg.esp_head){auto head=t.bone_mask&(1u<<10)?t.bones[10]:t.screen_head;
        if(esp_point_valid(head))dl->AddCircle({head.x,height-head.y},std::clamp(b.height*.04f,3.f,14.f),color,20,stroke);
    }
    if(g_cfg.esp_line)line({width*.5f,height},{b.center,b.bottom});
    dl->PopClipRect();
}

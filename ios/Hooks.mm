#include "Hooks.h"
#include "Cheat.h"
#include "Menu.h"
#include "Auth.h"
#include "EspRenderer.inl"
#include "imgui.h"
#include "imgui_impl_metal.h"
#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>

static std::atomic<bool> visible{true};
void SetOverlayVisible(bool next){visible=next;}
bool IsOverlayVisible(){return visible.load();}

@interface JMOverlayView : MTKView<MTKViewDelegate>
@end
@implementation JMOverlayView {
    bool _captured;
    std::chrono::steady_clock::time_point _last;
    id<MTLCommandQueue> _queue;
}
- (instancetype)initWithFrame:(CGRect)frame device:(id<MTLDevice>)device {
    self=[super initWithFrame:frame device:device];if(self)_queue=[device newCommandQueue];return self;
}
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
    if(!visible)return NO;
    CGFloat s=self.contentScaleFactor;
    CGPoint p={point.x*s,point.y*s};
    return Menu_HitTest(p.x,p.y);
}
- (void)sendTouch:(UITouch*)touch action:(int)action {
    CGPoint p=[touch locationInView:self];CGFloat s=self.contentScaleFactor;
    Menu_QueueTouch(action,p.x*s,p.y*s);
}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    [self sendTouch:touches.anyObject action:0];_captured=true;
}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if(_captured)[self sendTouch:touches.anyObject action:2];
}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if(_captured)[self sendTouch:touches.anyObject action:1];_captured=false;
}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if(_captured)[self sendTouch:touches.anyObject action:3];_captured=false;
}
- (void)drawInMTKView:(MTKView*)view {
    if(!visible)return;
    auto pass=view.currentRenderPassDescriptor;auto drawable=view.currentDrawable;
    if(!pass||!drawable)return;
    const int width=int(view.drawableSize.width),height=int(view.drawableSize.height);
    Cheat_SetScreenSize(width,height);
    auto& io=ImGui::GetIO();io.DisplaySize=ImVec2(width,height);io.DisplayFramebufferScale=ImVec2(1,1);
    const auto now=std::chrono::steady_clock::now();
    io.DeltaTime=_last.time_since_epoch().count()?std::max(1.f/240.f,std::chrono::duration<float>(now-_last).count()):1.f/60.f;
    _last=now;
    Menu_DrainInput();ImGui_ImplMetal_NewFrame(pass);ImGui::NewFrame();
    Menu_Render();
    TargetData targets[64];int count=g_cfg.esp_enabled?Cheat_GetTargets(targets,64):0;
    auto dl=ImGui::GetBackgroundDrawList();
    float scale=height>1080?1.5f:1.f;
    for(int i=0;i<count;++i){const auto& target=targets[i];if(!target.valid)continue;
        if((g_cfg.hide_passive.load()&&target.passive)||(g_cfg.hide_invulnerable.load()&&target.invulnerable)||
           (g_cfg.hide_friends.load()&&(!target.friend_known||target.friend_player)))continue;
        if(target.is_local&&!g_cfg.esp_self)continue;
        draw_esp_target(dl,target,float(width),float(height),scale,false);
    }
    auto tune=Cheat_GetAimbot();auto state=Cheat_GetStatus();
    if(Auth_Allowed()&&g_cfg.show_fov&&tune.enabled&&state.camera_ready&&tune.fov_deg<179&&state.camera_fov>1&&state.camera_fov<179){
        constexpr float radians=.01745329252f;
        float radius=height*.5f*std::tan(tune.fov_deg*.5f*radians)/std::tan(state.camera_fov*.5f*radians);
        dl->AddCircle(ImVec2(width*.5f,height*.5f),radius,IM_COL32(247,69,89,130),96,1.2f);
    }
    ImGui::Render();
    id<MTLCommandBuffer> command=[_queue commandBuffer];
    id<MTLRenderCommandEncoder> encoder=[command renderCommandEncoderWithDescriptor:pass];
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(),command,encoder);
    [encoder endEncoding];[command presentDrawable:drawable];[command commit];
}
- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {}
@end

static JMOverlayView* overlay=nil;
bool InstallGraphicsOverlay(){
    if(overlay)return true;
    UIWindow* window=nil;
    for(UIWindow* candidate in UIApplication.sharedApplication.windows)if(candidate.isKeyWindow){window=candidate;break;}
    if(!window)return false;
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();if(!device)return false;
    IMGUI_CHECKVERSION();ImGui::CreateContext();ImGui::StyleColorsDark();
    ImGui::GetIO().IniFilename=nullptr;
    if(!ImGui_ImplMetal_Init(device))return false;
    overlay=[[JMOverlayView alloc] initWithFrame:window.bounds device:device];
    overlay.delegate=overlay;overlay.paused=NO;overlay.enableSetNeedsDisplay=NO;
    overlay.preferredFramesPerSecond=60;overlay.colorPixelFormat=MTLPixelFormatBGRA8Unorm;
    overlay.clearColor=MTLClearColorMake(0,0,0,0);overlay.opaque=NO;
    overlay.backgroundColor=UIColor.clearColor;overlay.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    [window addSubview:overlay];return true;
}

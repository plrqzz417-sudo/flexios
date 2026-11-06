#pragma once
// Unity CPU projection (column-major, view-space forward = -Z). ESP only.
inline bool EspProjectMatrices(const Matrix4& view,const Matrix4& projection,Vector3 world,
                               int width,int height,Vector3& screen){
    if(width<=0||height<=0||!std::isfinite(world.x)||!std::isfinite(world.y)||!std::isfinite(world.z))return false;
    const auto* v=view.m;const auto* p=projection.m;
    const float x=v[0]*world.x+v[4]*world.y+v[8]*world.z+v[12];
    const float y=v[1]*world.x+v[5]*world.y+v[9]*world.z+v[13];
    const float z=v[2]*world.x+v[6]*world.y+v[10]*world.z+v[14];
    const float w=v[3]*world.x+v[7]*world.y+v[11]*world.z+v[15];
    const float cx=p[0]*x+p[4]*y+p[8]*z+p[12]*w;
    const float cy=p[1]*x+p[5]*y+p[9]*z+p[13]*w;
    const float cw=p[3]*x+p[7]*y+p[11]*z+p[15]*w;
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(w)||
       !std::isfinite(cx)||!std::isfinite(cy)||!std::isfinite(cw)||-z<=.01f||cw<=.00001f)return false;
    screen={(cx/cw*.5f+.5f)*width,(cy/cw*.5f+.5f)*height,-z};
    return std::isfinite(screen.x)&&std::isfinite(screen.y);
}

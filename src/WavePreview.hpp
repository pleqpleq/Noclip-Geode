#pragma once

// Inert, lightweight *approximate* wave predictor. The game state is never
// modified by this code. Real GD collision/physics is more complicated.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace noclip::wave {

struct Point {float x=0.f,y=0.f;};
struct Rect {float left=0.f,bottom=0.f,right=0.f,top=0.f;};
struct Collider {
    Rect bounds;
    std::array<Point,4> points{};
    int vertices=0; // 3=triangle, 4=quadrilateral, 0=circle
    Point center{};
    float radius=0.f;
};
inline bool valid(Rect r) {
    return std::isfinite(r.left) && std::isfinite(r.bottom)
        && std::isfinite(r.right) && std::isfinite(r.top)
        && r.left<r.right && r.bottom<r.top;
}
inline Collider box(Rect r) {
    Collider c; c.bounds=r; c.vertices=4;
    c.points={{{r.left,r.bottom},{r.right,r.bottom},
               {r.right,r.top},{r.left,r.top}}};
    return c;
}
inline Collider triangle(Rect r,int slope) {
    Collider c; c.bounds=r; c.vertices=3;
    Point bl{r.left,r.bottom}, br{r.right,r.bottom};
    Point tl{r.left,r.top}, tr{r.right,r.top};
    switch(slope) {
    case 0: case 7: c.points={br,tr,bl,{}}; break;
    case 3: case 6: c.points={tr,tl,bl,{}};break;
    case 1: case 5: c.points={br,tr,tl,{}};break;
    default: c.points={br,tl,bl,{}};break;
    }
    return c;
}
inline Collider oriented(std::array<Point,4> p) {
    Point center{};
    for(auto v:p){center.x+=v.x*.25f;center.y+=v.y*.25f;}
    std::sort(p.begin(),p.end(),[&](Point a, Point b) {
        return std::atan2(a.y-center.y,a.x-center.x)
             < std::atan2(b.y-center.y,b.x-center.x);
    });
    Collider c;c.points=p;c.vertices=4;
    c.bounds={p[0].x,p[0].y,p[0].x,p[0].y};
    for(auto v:p){
        c.bounds.left=std::min(c.bounds.left,v.x);
        c.bounds.right=std::max(c.bounds.right,v.x);
        c.bounds.bottom=std::min(c.bounds.bottom,v.y);
        c.bounds.top=std::max(c.bounds.top,v.y);
    }
    return c;
}
inline Collider circle(Point center,float radius) {
    Collider c; c.center=center;c.radius=radius;
    c.bounds={center.x-radius,center.y-radius,
              center.x+radius,center.y+radius};
    return c;
}
inline bool valid(Collider const& c) {
    if(!valid(c.bounds))return false;
    if(c.vertices==0)return c.radius>0.f && std::isfinite(c.radius);
    if(c.vertices!=3 && c.vertices!=4)return false;
    for(int i=0;i<c.vertices;++i)
        if(!std::isfinite(c.points[i].x)||!std::isfinite(c.points[i].y))
            return false;
    return true;
}
inline float dot(Point a,Point b){return a.x*b.x+a.y*b.y;}
inline float segmentDistance2(Point a, Point b, Point p) {
    Point v{b.x-a.x,b.y-a.y}, w{p.x-a.x,p.y-a.y};
    float len=dot(v,v);
    float t=len>0.f?std::clamp(dot(v,w)/len,0.f,1.f):0.f;
    float dx=a.x+t*v.x-p.x,dy=a.y+t*v.y-p.y;
    return dx*dx+dy*dy;
}

struct State {
    float x=0.f,y=0.f,dx=0.f;
    float halfW=1.f,halfH=1.f;
    float verticalSlope=1.f;  // mini wave may use 2.0
    bool held=false,upsideDown=false;
};

inline bool swept(Point a,Point b,State const& player,Collider const& c) {
    if(c.vertices==0) {
        auto radius=c.radius+std::hypot(player.halfW,player.halfH);
        return segmentDistance2(a,b,c.center)<=radius*radius;
    }
    auto axis=[&](Point n){
        if(n.x*n.x+n.y*n.y<.00000001f)return true;
        float lo=std::numeric_limits<float>::infinity(),hi=-lo;
        for(int k=0;k<c.vertices;++k){
            float d=dot(n,c.points[k]);lo=std::min(lo,d);hi=std::max(hi,d);
        }
        float pad=std::abs(n.x)*player.halfW+std::abs(n.y)*player.halfH;
        float da=dot(n,a),db=dot(n,b);
        return std::min(da,db)<=hi+pad && std::max(da,db)>=lo-pad;
    };
    if(!axis({1.f,0.f})||!axis({0.f,1.f}))return false;
    for(int k=0;k<c.vertices;++k){
        Point p=c.points[k],q=c.points[(k+1)%c.vertices];
        if(!axis({q.y-p.y,p.x-q.x}))return false;
    }
    return axis({b.y-a.y,a.x-b.x});
}

struct Score {
    int survived=0;
    bool crossedSpecial=false;
};
inline Score simulate(
    State state,bool nextHeld,int delay,int horizon,
    std::vector<Collider> const& hazards,
    std::vector<Collider> const& specials={}
) {
    Score result;
    Point p{state.x,state.y};
    float vy=std::abs(state.dx)*state.verticalSlope
        * (state.upsideDown?-1.f:1.f);
    for(int tick=0;tick<horizon;++tick) {
        bool held=tick>=delay?nextHeld:state.held;
        Point next{p.x+state.dx,p.y+(held?vy:-vy)};
        for(auto const& c:specials)
            if(swept(p,next,state,c)){
                result.crossedSpecial=true;
                return result;
            }
        for(auto const& c:hazards)
            if(swept(p,next,state,c))return result;
        p=next;
        ++result.survived;
    }
    return result;
}
struct Decision {
    int delay=0;
    int immediateSurvival=0;
    int bestSurvival=0;
    bool supported=false;
    bool corrected=false;
};
inline Decision choose(
    State const& state,bool nextHeld,int maxDelay,int horizon,
    std::vector<Collider> const& hazards,
    std::vector<Collider> const& specials={}
) {
    Decision result;
    if(!std::isfinite(state.x)||!std::isfinite(state.y)||
       !std::isfinite(state.dx)||state.dx<=.05f||state.dx>30.f||
       state.verticalSlope<=0.f||state.halfW<=0.f||state.halfH<=0.f||
       nextHeld==state.held||maxDelay<1||maxDelay>8||
       horizon<maxDelay+2||horizon>60||hazards.size()>256||
       specials.size()>256) return result;
    for(auto const& c:hazards)if(!valid(c))return result;
    for(auto const& c:specials)if(!valid(c))return result;
    result.supported=true;
    auto now=simulate(state,nextHeld,0,horizon,hazards,specials);
    result.immediateSurvival=now.survived;
    result.bestSurvival=now.survived;
    if(now.crossedSpecial || now.survived==horizon)return result;
    int best=0;
    for(int delay=1;delay<=maxDelay;++delay) {
        auto candidate=simulate(state,nextHeld,delay,horizon,hazards,specials);
        if(candidate.crossedSpecial || candidate.survived<delay)continue;
        if(candidate.survived>result.bestSurvival) {
            result.bestSurvival=candidate.survived;
            best=delay;
        }
    }
    // Don't delay for a one-physics-step improvement that may be rounding
    // noise; avoid arbitrarily changing a valid input on approximate data.
    if(best>0 && result.bestSurvival>=result.immediateSurvival+2) {
        result.delay=best;
        result.corrected=true;
    }
    return result;
}
} // namespace noclip::wave
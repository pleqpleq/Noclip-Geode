#pragma once
// Experimental invisible wave input timing predictor.
// Important: approximate GD wave motion; collision geometry is taken from
// live GameObject hitboxes where possible. No guarantee of a perfect click.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace noclip::wave {

struct Rect { float left=0, bottom=0, right=0, top=0; };
struct Point { float x=0, y=0; };

inline bool valid(Rect r) {
    return std::isfinite(r.left) && std::isfinite(r.right)
        && std::isfinite(r.bottom) && std::isfinite(r.top)
        && r.left < r.right && r.bottom < r.top;
}
inline float dot(Point a, Point b) { return a.x*b.x + a.y*b.y; }

struct Collider {
    Rect bounds;
    std::array<Point,4> points{};
    int count = 0;     // 3 triangle, 4 quad, 0 round object
    float radius = 0; // Round object when radius > 0
    Point center{};
};
inline Collider makeBox(Rect b) {
    Collider c; c.bounds=b; c.count=4;
    c.points = {{{b.left,b.bottom},{b.right,b.bottom},
                 {b.right,b.top},{b.left,b.top}}};
    return c;
}
inline Collider makeTriangle(Rect b, int slopeDirection) {
    Collider c; c.bounds=b; c.count=3;
    // Same triangle orientation selected by GeodeMenu's slope hitbox drawing.
    Point bl{b.left,b.bottom},br{b.right,b.bottom},
          tl{b.left,b.top},tr{b.right,b.top};
    switch (slopeDirection) {
        case 0: case 7: c.points={br,tr,bl, {}};break;
        case 3: case 6: c.points={tr,tl,bl, {}};break;
        case 1: case 5: c.points={br,tr,tl, {}};break;
        default: c.points={br,tl,bl, {}};break;
    }
    return c;
}
inline Collider makeQuad(std::array<Point,4> p) {
    Collider c; c.count=4;
    // Sort corners counterclockwise to ensure polygon edges don't cross.
    Point mid{};
    for (auto v:p) {mid.x+=v.x/4.f;mid.y+=v.y/4.f;}
    std::sort(p.begin(),p.end(),[&](Point a,Point b) {
        return std::atan2(a.y-mid.y,a.x-mid.x)
             < std::atan2(b.y-mid.y,b.x-mid.x);
    });
    c.points=p;
    c.bounds={p[0].x,p[0].y,p[0].x,p[0].y};
    for (auto v:p) {
        c.bounds.left=std::min(c.bounds.left,v.x);
        c.bounds.right=std::max(c.bounds.right,v.x);
        c.bounds.bottom=std::min(c.bounds.bottom,v.y);
        c.bounds.top=std::max(c.bounds.top,v.y);
    }
    return c;
}
inline Collider makeCircle(Point center,float radius) {
    Collider c;c.center=center;c.radius=radius;c.count=0;
    c.bounds={center.x-radius,center.y-radius,
              center.x+radius,center.y+radius};
    return c;
}
inline bool valid(Collider const& c) {
    if (!valid(c.bounds) || c.count < 0 || c.count > 4) return false;
    for (int i=0;i<c.count;++i)
        if (!std::isfinite(c.points[i].x) || !std::isfinite(c.points[i].y))
            return false;
    return c.count>=3 || (c.count==0 && c.radius>0 &&
        std::isfinite(c.radius));
}

struct State {
    float x=0,y=0,dxPerFrame=0,halfWidth=3.5f,halfHeight=3.5f;
    bool held=false,gravityReversed=false;
};
enum class Verdict { Unsupported, AlreadySafe, TooEarly, NoCorrection, Buffered };
inline char const* verdictName(Verdict v) {
    switch (v) {
    case Verdict::Unsupported:return "unsupported";
    case Verdict::AlreadySafe:return "already-safe";
    case Verdict::TooEarly:return "collision-too-far";
    case Verdict::NoCorrection:return "no-safe-frame";
    case Verdict::Buffered:return "buffered";
    }
    return "unknown";
}
struct Decision {
    bool evaluated=false,immediatePathHits=false;
    int delayFrames=0,firstCollisionFrame=-1,comparedFrames=0,safeCandidates=0;
    Verdict verdict=Verdict::Unsupported;
};

inline float segmentDistanceSquared(Point a, Point b, Point p) {
    Point d{b.x-a.x,b.y-a.y}, w{p.x-a.x,p.y-a.y};
    auto len=dot(d,d);
    float t=len>0.f?std::clamp(dot(w,d)/len,0.f,1.f):0.f;
    float dx=a.x+t*d.x-p.x,dy=a.y+t*d.y-p.y;
    return dx*dx+dy*dy;
}
inline bool sweptHit(
    Point a, Point b, State const& player, Collider const& obs
) {
    // Conservative circle test; rotated polygons and triangles use actual
    // convex shapes instead of treating the entire bounding box as solid.
    if (obs.radius>0.f) {
        float rad=obs.radius + std::hypot(player.halfWidth,player.halfHeight);
        return segmentDistanceSquared(a,b,obs.center)<=rad*rad;
    }
    auto overlapOnAxis=[&](Point axis) {
        if (axis.x*axis.x + axis.y*axis.y < 0.0000001f) return true;
        float minPoly=std::numeric_limits<float>::infinity();
        float maxPoly=-minPoly;
        for (int i=0;i<obs.count;++i) {
            float p=dot(axis,obs.points[i]);
            minPoly=std::min(minPoly,p);
            maxPoly=std::max(maxPoly,p);
        }
        float expansion=std::abs(axis.x)*player.halfWidth
                      + std::abs(axis.y)*player.halfHeight;
        float pa=dot(axis,a),pb=dot(axis,b);
        return std::max(pa,pb)>=minPoly-expansion
            && std::min(pa,pb)<=maxPoly+expansion;
    };
    // SAT for a moving centerline segment against polygon expanded by
    // player's axis-aligned collision box (Minkowski sum).
    if(!overlapOnAxis({1.f,0.f}) || !overlapOnAxis({0.f,1.f}))return false;
    for(int i=0;i<obs.count;++i) {
        Point p=obs.points[i], q=obs.points[(i+1)%obs.count];
        if(!overlapOnAxis({q.y-p.y, p.x-q.x})) return false;
    }
    return overlapOnAxis({b.y-a.y,a.x-b.x});
}
inline float clearance(Point p, State const& player, Collider const& obs) {
    if (obs.radius>0.f) {
        return std::max(0.f,std::hypot(p.x-obs.center.x,
                p.y-obs.center.y)-obs.radius
                -std::hypot(player.halfWidth,player.halfHeight));
    }
    // Use nearest polygon EDGE, not its AABB, for score ranking.
    float minD2=std::numeric_limits<float>::infinity();
    for(int i=0;i<obs.count;++i)
        minD2=std::min(minD2,segmentDistanceSquared(
            obs.points[i],obs.points[(i+1)%obs.count],p));
    return std::max(0.f,std::sqrt(minD2)-
            std::hypot(player.halfWidth,player.halfHeight));
}
struct Trace {
    int firstCollision=-1,hitIndex=-1;
    float clearance=std::numeric_limits<float>::infinity();
    bool crossesSpecial=false;
};
inline Trace trace(
    State const& initial,bool nextHeld,int inputAfterFrames,int frames,
    std::vector<Collider> const& obstacles,
    std::vector<Collider> const& specials={}
) {
    Trace result;
    Point p{initial.x,initial.y};
    float dy=std::abs(initial.dxPerFrame)*
        (initial.gravityReversed?-1.f:1.f);
    for(int frame=0;frame<frames;++frame) {
        bool held=frame>=inputAfterFrames?nextHeld:initial.held;
        Point q{p.x+initial.dxPerFrame,p.y+(held?dy:-dy)};
        for(auto const& special:specials)
            if(sweptHit(p,q,initial,special))result.crossesSpecial=true;
        for(std::size_t i=0;i<obstacles.size();++i) {
            auto const& obs=obstacles[i];
            if(sweptHit(p,q,initial,obs)) {
                result.firstCollision=frame;
                result.hitIndex=static_cast<int>(i);
                return result;
            }
            result.clearance=std::min(result.clearance,
                clearance(p,initial,obs));
            result.clearance=std::min(result.clearance,
                clearance({(p.x+q.x)*.5f,(p.y+q.y)*.5f},initial,obs));
            result.clearance=std::min(result.clearance,
                clearance(q,initial,obs));
        }
        p=q;
    }
    return result;
}
inline Decision chooseDelay(
    State const& initial,bool nextHeld,int maxDelay,int horizonFrames,
    std::vector<Collider> const& obstacles,
    std::vector<Collider> const& specials={}
) {
    Decision out;
    if(!std::isfinite(initial.x)||!std::isfinite(initial.y)||
       !std::isfinite(initial.dxPerFrame)||std::abs(initial.dxPerFrame)<.1f||
       initial.halfWidth<=0.f||initial.halfHeight<=0.f||
       maxDelay<0||maxDelay>72||horizonFrames<maxDelay+2||
       horizonFrames>150||obstacles.size()>1024||specials.size()>1024||
       nextHeld==initial.held) return out;
    for(auto const& c:obstacles)if(!valid(c))return out;
    for(auto const& c:specials)if(!valid(c))return out;

    out.evaluated=true;
    Trace immediate=trace(initial,nextHeld,0,horizonFrames,obstacles,specials);
    if(immediate.crossesSpecial) {out.evaluated=false;return out;}
    if(immediate.firstCollision<0){out.verdict=Verdict::AlreadySafe;return out;}
    out.immediatePathHits=true;
    out.firstCollisionFrame=immediate.firstCollision;
    if(immediate.firstCollision>maxDelay+3){
        out.verdict=Verdict::TooEarly;return out;
    }
    auto const& first=obstacles[static_cast<std::size_t>(immediate.hitIndex)];
    int exitFrame=static_cast<int>(std::ceil(
        (first.bounds.right+initial.halfWidth-initial.x)/
        std::abs(initial.dxPerFrame)))+1;
    int localEnd=std::max(immediate.firstCollision+2,exitFrame);
    localEnd=std::clamp(localEnd,2,horizonFrames);
    localEnd=std::min(localEnd,immediate.firstCollision+maxDelay+3);
    out.comparedFrames=localEnd;
    int bestDelay=0;
    float bestClearance=-1.f;
    for(int d=1;d<=maxDelay;++d) {
        auto candidate=trace(initial,nextHeld,d,localEnd,obstacles,specials);
        if(candidate.firstCollision>=0||candidate.crossesSpecial)continue;
        ++out.safeCandidates;
        if(!bestDelay||candidate.clearance>bestClearance+.001f){
            bestDelay=d;bestClearance=candidate.clearance;
        }
    }
    if(bestDelay){
        out.verdict=Verdict::Buffered;out.delayFrames=bestDelay;
    } else out.verdict=Verdict::NoCorrection;
    return out;
}
} // namespace noclip::wave

#include <cassert>
#include <iostream>
#include <vector>
#include "../src/WavePredictor.hpp"
using namespace noclip::wave;

int main() {
    State s{.x=0.f,.y=0.f,.dxPerFrame=1.f,.halfWidth=.1f,
            .halfHeight=.1f,.held=false};
    std::vector<Collider> channel{
        makeBox({3.85f,-1.2f,4.15f,20.f}),
        makeBox({3.85f,-20.f,4.15f,-2.8f})
    };
    auto exact=chooseDelay(s,true,5,20,channel);
    assert(exact.verdict==Verdict::Buffered && exact.delayFrames==3);
    assert(chooseDelay(s,true,2,20,channel).delayFrames==0);
    s.x=-10.f;
    auto far=chooseDelay(s,true,5,20,channel);
    assert(far.delayFrames==0);
    assert(far.verdict==Verdict::AlreadySafe ||
           far.verdict==Verdict::TooEarly);
    s.x=0.f;
    auto multi=channel;
    multi.push_back(makeBox({11.85f,-100.f,12.15f,100.f}));
    assert(chooseDelay(s,true,5,20,multi).delayFrames==3);
    s.held=true;
    std::vector<Collider> mirror{
        makeBox({3.85f,2.8f,4.15f,20.f}),
        makeBox({3.85f,-20.f,4.15f,1.2f})
    };
    assert(chooseDelay(s,false,5,20,mirror).delayFrames==3);
    s.held=false;
    std::vector<Collider> farOrb{makeCircle({5.f,25.f},1.f)};
    assert(chooseDelay(s,true,5,20,channel,farOrb).delayFrames==3);
    std::vector<Collider> nearPortal{makeBox({3.f,2.f,4.f,5.f})};
    assert(chooseDelay(s,true,5,20,channel,nearPortal).verdict
           ==Verdict::Unsupported);
    std::vector<Collider> solid{makeBox({3.85f,-20.f,4.15f,20.f})};
    assert(chooseDelay(s,true,5,20,solid).verdict==Verdict::NoCorrection);
    std::vector<Collider> empty;
    assert(chooseDelay(s,true,5,20,empty).verdict==Verdict::AlreadySafe);
    assert(!chooseDelay(s,false,5,20,channel).evaluated);

    // Sloped triangular geometry: path inside the AABB but outside the
    // triangle is NOT a collision. Important for sloped V-shaped corridors.
    auto triangle=makeTriangle({0.f,0.f,4.f,4.f},0);
    assert(valid(triangle));
    assert(!sweptHit({1.f,3.5f},{2.f,3.5f},s,triangle));
    assert(sweptHit({1.f,.5f},{2.f,.5f},s,triangle));

    // Convex rotated hitbox corners must be used, not AABB corners.
    auto diamond=makeQuad({{{0.f,2.f},{2.f,4.f},{4.f,2.f},{2.f,0.f}}});
    assert(valid(diamond));
    assert(!sweptHit({.1f,.1f},{.3f,.3f},s,diamond));
    assert(sweptHit({1.f,2.f},{3.f,2.f},s,diamond));

    // Circle hazards (saws) are not full square rectangles.
    auto circle=makeCircle({2.f,2.f},.5f);
    assert(!sweptHit({0.f,0.f},{1.f,1.f},s,circle));
    assert(sweptHit({1.5f,2.f},{2.5f,2.f},s,circle));
    // The concrete 0.2-second early-input case: at 240 TPS, 0.2s
    // means 48 steps, NOT 5. Only switching direction at step 48
    // navigates this single narrow gate at x~=50.
    State early{.x=0.f,.y=0.f,.dxPerFrame=1.f,.halfWidth=.1f,
                .halfHeight=.1f,.held=false};
    std::vector<Collider> lateGate{
        makeBox({49.85f,-45.5f,50.15f,100.f}),
        makeBox({49.85f,-100.f,50.15f,-47.5f})
    };
    auto fortyEight=chooseDelay(early,true,48,76,lateGate);
    assert(fortyEight.evaluated && fortyEight.verdict==Verdict::Buffered);
    assert(fortyEight.delayFrames==48);
    auto five=chooseDelay(early,true,5,76,lateGate);
    assert(five.delayFrames==0);
    // The same 48-step behavior should work for wave RELEASE.
    early.held=true;
    std::vector<Collider> releaseGate{
        makeBox({49.85f,45.5f,50.15f,100.f}),
        makeBox({49.85f,-100.f,50.15f,47.5f})
    };
    auto heldRelease=chooseDelay(early,false,48,76,releaseGate);
    assert(heldRelease.delayFrames==48);

    std::cout << "Noclip v0.5 geometry and 200ms frame timing tests passed\n";
}

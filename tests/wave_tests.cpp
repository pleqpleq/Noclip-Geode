#include <cassert>
#include <iostream>
#include <vector>
#include "../src/WavePreview.hpp"
using namespace noclip::wave;
int main() {
 State state{0.f,0.f,1.f,.1f,.1f,1.f,false,false};
 // Forcing an immediate press rises into the top hazard, while waiting
 // precisely three steps crosses an x=4 vertical gate through a tiny gap.
 std::vector<Collider> gate={box({3.85f,-1.2f,4.15f,20.f}),
                            box({3.85f,-20.f,4.15f,-2.8f})};
 auto d=choose(state,true,4,9,gate);
 assert(d.supported && d.corrected && d.delay==3);
 assert(d.bestSurvival>d.immediateSurvival);
 auto tooSmall=choose(state,true,2,9,gate);
 assert(!tooSmall.corrected);
 std::vector<Collider> none;
 auto safe=choose(state,true,4,9,none);
 assert(safe.supported && !safe.corrected);
 assert(!choose(state,false,4,9,gate).supported);
 auto portal=choose(state,true,4,9,gate,{box({1.f,.4f,2.f,2.f})});
 assert(!portal.corrected);
 // Press and release mirror each other in the same gap.
 state.held=true;
 std::vector<Collider> mirrored={box({3.85f,2.8f,4.15f,20.f}),
                                 box({3.85f,-20.f,4.15f,1.2f})};
 auto release=choose(state,false,4,9,mirrored);
 assert(release.corrected && release.delay==3);
 // Accurate triangular collision is important; AABB is not a triangle.
 state.held=false;
 auto tri=triangle({0.f,0.f,4.f,4.f},0);
 assert(valid(tri));
 assert(!swept({1.f,3.5f},{2.f,3.5f},state,tri));
 assert(swept({1.f,.5f},{2.f,.5f},state,tri));
 // Rotated diamond corners do not collide near the rectangle's empty corner.
 auto diamond=oriented({{{0.f,2.f},{2.f,4.f},{4.f,2.f},{2.f,0.f}}});
 assert(!swept({.1f,.1f},{.3f,.3f},state,diamond));
 assert(swept({1.f,2.f},{3.f,2.f},state,diamond));
 auto saw=circle({2.f,2.f},.5f);
 assert(!swept({0.f,0.f},{1.f,1.f},state,saw));
 assert(swept({1.5f,2.f},{2.5f,2.f},state,saw));
 // Mini wave uses a different vertical scale; its future path must reflect it.
 state.verticalSlope=2.f;
 auto mini=simulate(state,true,0,2,{},{});
 assert(mini.survived==2);
 std::cout<<"WavePreview: press/release, hitbox, fallback tests passed\n";
}
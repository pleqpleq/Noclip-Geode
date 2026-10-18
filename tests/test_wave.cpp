#include <cassert>
#include <iostream>
#include "../src/WavePredictor.hpp"
using namespace noclip::wave;
int main(){State s{.x=0.f,.y=0.f,.dxPerFrame=1.f,.halfWidth=.1f,.halfHeight=.1f,.held=false};std::vector<Rect> blockers={{3.85f,-1.2f,4.15f,20.f},{3.85f,-20.f,4.15f,-2.8f}};auto r=chooseDelay(s,true,5,8,blockers);assert(r.evaluated&&r.immediatePathHits&&r.delayFrames==3);assert(chooseDelay(s,true,2,8,blockers).delayFrames==0);s.x=-30.f;assert(chooseDelay(s,true,5,8,blockers).delayFrames==0);s.x=0.f;s.held=true;std::vector<Rect> mirror={{3.85f,2.8f,4.15f,20.f},{3.85f,-20.f,4.15f,1.2f}};assert(chooseDelay(s,false,5,8,mirror).delayFrames==3);std::cout<<"Wave predictor tests passed\n";}

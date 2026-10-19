#include <cassert>
#include <iostream>
#include "../src/InputQueue.hpp"
using namespace noclip;
int main(){
  InputQueue q;
  // An ordinary input never gets buffered when no correction is chosen.
  assert(!q.intercept(true,true,1,10,0));
  assert(!q.hasPending(0));
  assert(q.players[0].deliveredHeld);
  // A predicted press must be held until the selected physics tick.
  assert(q.intercept(false,true,1,12,3));
  assert(!q.popDue(0,14));
  auto release=q.popDue(0,15);assert(release && !release->pressed && release->due==15);
  // Release before a buffered press preserves the original tap duration.
  q.clear();
  assert(q.intercept(true,true,1,20,4));
  assert(q.intercept(false,true,1,21,0));
  assert(!q.popDue(0,23));
  auto press=q.popDue(0,24); assert(press && press->pressed);
  assert(!q.popDue(0,24));
  release=q.popDue(0,25); assert(release && !release->pressed);
  assert(!q.hasPending(0));
  // Two input channels can never interfere with one another.
  q.clear();
  assert(q.intercept(true,true,1,40,2));
  assert(q.intercept(false,false,1,40,3));
  auto a=q.popDue(0,42); auto b=q.popDue(1,43);
  assert(a && a->player1); assert(b && !b->player1);
  // Force flush on dual transition returns *all* pending edges in order.
  q.clear();
  assert(q.intercept(true,true,1,55,5));
  assert(q.intercept(false,true,1,56,0));
  a=q.popFirst(0); b=q.popFirst(0);
  assert(a && a->pressed && b && !b->pressed);
  assert(!q.popFirst(0));
  // Reset eliminates stale events from older level attempts.
  q.intercept(true,true,1,60,4); q.clear();
  assert(!q.popDue(0,100));
  // Rapid alternating edges preserve order and leave no permanent hold.
  q.clear();
  q.intercept(true,true,1,100,2);
  q.intercept(false,true,1,100,0);
  q.intercept(true,true,1,100,0);
  q.intercept(false,true,1,100,0);
  for(int i=0;i<4;++i){auto e=q.popDue(0,102+i);assert(e && e->pressed==(i%2==0));}
  assert(!q.players[0].deliveredHeld);
  std::cout<<"InputQueue: all 6 cases passed\n";
}
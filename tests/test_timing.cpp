#include "../src/timing.hpp"
#include <cassert>
#include <vector>
#include <iostream>
int main() {
    using namespace buffer;
    assert(fixedTarget(100,2)==102);
    assert(gridTarget(11,4)==12);
    assert(gridTarget(12,4)==12);
    assert(gridTarget(13,4)==16);
    Queue q;
    q.push({true,true,1,100,102});
    q.push({false,true,1,100,102});
    std::vector<Edge> dispatched;
    q.fireDue(102,[&](Edge e){dispatched.push_back(e);});
    assert(dispatched.size()==1 && dispatched[0].down);
    q.fireDue(103,[&](Edge e){dispatched.push_back(e);});
    assert(dispatched.size()==2 && !dispatched[1].down);
    Queue q2;
    q2.push({true,true,1,101,104});
    q2.push({false,true,1,102,105});
    q2.flush([&](Edge e){dispatched.push_back(e);});
    assert(q2.empty() && dispatched[2].received==101 && dispatched[3].received==102);
    std::cout << "Native buffer tick scheduler tests passed\n";
}

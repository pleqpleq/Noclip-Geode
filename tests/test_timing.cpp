#include <cassert>
#include <iostream>
#include <string>
#include "../src/TimingCore.hpp"

int main() {
    using namespace noclip;
    constexpr float speed = 6.f;
    constexpr float targetX = 100.f;

    // Three frames before target: eligible and fired only when it reaches x=100.
    assert(isWithinEarlyWindow(82.f, targetX, speed, 5));
    Deferred press{true, 1, true, true, targetX, 1, 40, 45};
    assert(!shouldDispatch(press, 82.f, speed, 40));
    assert(!shouldDispatch(press, 94.f, speed, 42));
    assert(shouldDispatch(press, 100.f, speed, 43));

    // Ten frames early: do not buffer.
    assert(!isWithinEarlyWindow(40.f, targetX, speed, 5));
    // Two-frame window: 3 early is no longer eligible.
    assert(!isWithinEarlyWindow(82.f, targetX, speed, 2));
    assert(isWithinEarlyWindow(88.f, targetX, speed, 2));

    // Both release and press follow the same scheduling logic.
    Deferred release{false, 1, true, true, targetX, 1, 40, 45};
    assert(!shouldDispatch(release, 94.f, speed, 42));
    assert(shouldDispatch(release, 100.f, speed, 43));

    // Reverse direction sections.
    assert(isWithinEarlyWindow(118.f, targetX, -speed, 5));
    Deferred reverse{true, 1, true, true, targetX, -1, 10, 15};
    assert(!shouldDispatch(reverse, 106.f, -speed, 12));
    assert(shouldDispatch(reverse, 100.f, -speed, 13));

    // Cap always fires so buffered input does not remain stuck indefinitely.
    assert(shouldDispatch(press, 82.f, speed, 45));
    assert(!isWithinEarlyWindow(82.f, targetX, speed, 0));
    assert(!isWithinEarlyWindow(82.f, targetX, 0.f, 5));

    std::cout << "All Noclip timing core tests passed.\n";
}

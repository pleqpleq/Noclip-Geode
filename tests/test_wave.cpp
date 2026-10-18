#include <cassert>
#include <iostream>
#include <vector>
#include "../src/WavePredictor.hpp"

using namespace noclip::wave;

int main() {
    State s{.x = 0.f, .y = 0.f, .dxPerFrame = 1.f,
            .halfWidth = .1f, .halfHeight = .1f, .held = false};

    // The ONLY good click is 3 simulation frames after the keydown.
    std::vector<Rect> channel = {
        {3.85f, -1.2f, 4.15f, 20.f},
        {3.85f, -20.f, 4.15f, -2.8f}
    };
    auto exact = chooseDelay(s, true, 5, 20, channel);
    assert(exact.evaluated && exact.immediatePathHits);
    assert(exact.delayFrames == 3 && exact.verdict == Verdict::Buffered);
    assert(exact.safeCandidates == 1);

    // A max window of 2 must NOT shift a 3-frame-early click.
    auto two = chooseDelay(s, true, 2, 20, channel);
    assert(two.delayFrames == 0 && two.verdict == Verdict::NoCorrection);

    // Never delay a click that is 10 frames away from the same obstacle.
    s.x = -10.f;
    auto far = chooseDelay(s, true, 5, 20, channel);
    assert(far.delayFrames == 0);
    assert(far.verdict == Verdict::TooEarly || far.verdict == Verdict::AlreadySafe);

    // Future obstacles will require another wave click; don't demand
    // surviving every spike in a 20-frame horizon with a single direction.
    s.x = 0.f;
    auto multiClick = channel;
    multiClick.push_back({11.85f, -100.f, 12.15f, 100.f});
    auto local = chooseDelay(s, true, 5, 20, multiClick);
    assert(local.delayFrames == 3 && local.comparedFrames < 12);

    // Mirrored release should also get exactly 3 frames of input buffering.
    s.held = true;
    std::vector<Rect> mirrored = {
        {3.85f, 2.8f, 4.15f, 20.f},
        {3.85f, -20.f, 4.15f, 1.2f}
    };
    auto release = chooseDelay(s, false, 5, 20, mirrored);
    assert(release.delayFrames == 3 && release.verdict == Verdict::Buffered);

    // An unrelated ring high above the player must not disable prediction.
    s.held = false;
    std::vector<Rect> distantOrb = {{4.f, 20.f, 6.f, 22.f}};
    assert(chooseDelay(s, true, 5, 20, channel, distantOrb).delayFrames == 3);

    // An actual orb/portal ON the attempted path is an unsafe prediction.
    std::vector<Rect> nearbyPortal = {{3.f, 2.f, 4.f, 5.f}};
    assert(chooseDelay(s, true, 5, 20, channel, nearbyPortal).verdict
           == Verdict::Unsupported);

    // Impossible corridor -> no false claim that any delayed timing is safe.
    std::vector<Rect> solid = {{3.85f, -20.f, 4.15f, 20.f}};
    auto impossible = chooseDelay(s, true, 5, 20, solid);
    assert(impossible.verdict == Verdict::NoCorrection);
    assert(impossible.delayFrames == 0);

    // Immediate safe input must pass through unchanged.
    std::vector<Rect> empty;
    auto immediate = chooseDelay(s, true, 5, 20, empty);
    assert(immediate.verdict == Verdict::AlreadySafe);
    assert(immediate.delayFrames == 0);

    // No real change of held state should be treated as an input correction.
    assert(!chooseDelay(s, false, 5, 20, channel).evaluated);
    std::cout << "Noclip v0.3 wave timing tests passed\n";
}

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "TimingCore.hpp"
#include "WavePredictor.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {

constexpr int kSurvivalSteps = 12;
constexpr std::size_t kMaxAnchors = 8000;
constexpr std::size_t kMaxQueuedPerPlayer = 48;
constexpr std::size_t kMaxLearningPerPlayer = 96;

struct Config {
    bool enabled;
    int window;
    bool autoLearn;
    bool recordReference;
    bool blindDelay;
    bool platformer;
    bool clearLearned;
    bool predictWave;
    int waveHorizon;
    bool logWave;
};

Config config() {
    auto* mod = Mod::get();
    return {
        mod->getSettingValue<bool>("enabled"),
        std::clamp(static_cast<int>(mod->getSettingValue<int64_t>("window-frames")), 0, 20),
        mod->getSettingValue<bool>("auto-learn"),
        mod->getSettingValue<bool>("record-reference"),
        mod->getSettingValue<bool>("blind-delay"),
        mod->getSettingValue<bool>("platformer"),
        mod->getSettingValue<bool>("clear-learned"),
        mod->getSettingValue<bool>("predict-wave"),
        std::clamp(static_cast<int>(mod->getSettingValue<int64_t>("wave-horizon")), 12, 60),
        mod->getSettingValue<bool>("log-wave-decisions")
    };
}

// Capturing the mode ensures a wave release is never snapped to a cube press,
// or a different type of gameplay in the same approximate level position.
int playerMode(PlayerObject const* player) {
    if (player->m_isDart) return 1;    // Wave
    if (player->m_isShip) return 2;
    if (player->m_isBird) return 3;    // UFO
    if (player->m_isBall) return 4;
    if (player->m_isRobot) return 5;
    if (player->m_isSpider) return 6;
    if (player->m_isSwing) return 7;
    return 0;                        // Cube / other
}

std::string levelKey(PlayLayer* layer) {
    if (!layer || !layer->m_level) return "";
    auto* level = layer->m_level;
    // Including the level name and serialized length separates most local
    // levels, which often all have an online ID of 0.
    return std::to_string(static_cast<int>(level->m_levelID))
        + ":" + std::string(level->m_levelName)
        + ":" + std::to_string(level->m_levelString.size());
}

struct UnverifiedInput {
    std::string level;
    int step = 0;
    float x = 0.f;
    bool player1 = true;
    bool down = true;
    int mode = 0;
};

struct Session {
    PlayLayer* layer = nullptr;
    std::string level;
    int lastStep = std::numeric_limits<int>::min();
    std::array<float, 2> lastX{0.f, 0.f};
    std::array<float, 2> speed{0.f, 0.f};
    std::array<bool, 2> hasX{false, false};
    std::array<int, 2> lastSendStep{-100000, -100000};
    // State of the inputs actually delivered to GD (not the physical keyboard).
    std::array<bool, 2> deliveredHeld{false, false};
    std::array<std::deque<noclip::Deferred>, 2> delayed;
    std::array<std::deque<UnverifiedInput>, 2> unverified;

    void resetTransient() {
        lastStep = std::numeric_limits<int>::min();
        lastX = {0.f, 0.f};
        speed = {0.f, 0.f};
        hasX = {false, false};
        lastSendStep = {-100000, -100000};
        deliveredHeld = {false, false};
        for (auto& q : delayed) q.clear();
        for (auto& q : unverified) q.clear();
    }

    void bind(PlayLayer* pl) {
        if (layer == pl && level == levelKey(pl)) return;
        resetTransient();
        layer = pl;
        level = levelKey(pl);
    }
};

Session g_session;
std::vector<noclip::Anchor> g_anchors;
bool g_loaded = false;
bool g_dispatchingBuffered = false;
bool g_dirty = false;

void saveAnchors() {
    std::ostringstream out;
    out << std::setprecision(9);
    for (auto const& a : g_anchors) {
        out << std::quoted(a.levelKey) << ' ' << a.player1 << ' '
            << a.down << ' ' << a.mode << ' ' << a.x << '\n';
    }
    Mod::get()->setSavedValue("reference-positions-v1", out.str());
    g_dirty = false;
}

void loadAnchors() {
    if (g_loaded) return;
    g_loaded = true;
    std::istringstream in(
        Mod::get()->getSavedValue<std::string>("reference-positions-v1", "")
    );
    noclip::Anchor a;
    while (in >> std::quoted(a.levelKey) >> a.player1 >> a.down
              >> a.mode >> a.x) {
        if (!std::isfinite(a.x) || a.levelKey.size() > 300) continue;
        if (g_anchors.size() >= kMaxAnchors) break;
        g_anchors.push_back(a);
    }
    log::info("Noclip loaded {} timing references", g_anchors.size());
}

void clearReferencesIfRequested(Config const& cfg) {
    if (!cfg.clearLearned) return;
    loadAnchors();
    if (!g_anchors.empty()) {
        g_anchors.clear();
        g_dirty = true;
        saveAnchors();
        log::info("Noclip timing references cleared");
    }
}

PlayerObject* playerFor(PlayLayer* layer, bool player1) {
    return layer ? (player1 ? layer->m_player1 : layer->m_player2) : nullptr;
}

void addReference(noclip::Anchor const& incoming, bool manual) {
    if (incoming.levelKey.empty() || !std::isfinite(incoming.x)) return;
    loadAnchors();
    // Preserve different edges and modes, even at almost identical positions.
    for (auto& old : g_anchors) {
        if (old.levelKey != incoming.levelKey || old.player1 != incoming.player1
            || old.down != incoming.down || old.mode != incoming.mode) continue;
        if (std::abs(old.x - incoming.x) <= 2.f) {
            if (manual && old.x != incoming.x) {
                old.x = incoming.x;
                g_dirty = true;
            }
            return;
        }
    }
    if (g_anchors.size() >= kMaxAnchors) g_anchors.erase(g_anchors.begin());
    g_anchors.push_back(incoming);
    g_dirty = true;
}

// Choose an already-learned target ahead of the current player position and
// no more than the configured number of *physics steps* away.
std::optional<float> nearestReference(
    std::string const& level, bool player1, bool down, int mode,
    float x, float signedSpeed, int window
) {
    float nearest = std::numeric_limits<float>::infinity();
    std::optional<float> result;
    for (auto const& a : g_anchors) {
        if (a.levelKey != level || a.player1 != player1 || a.down != down
            || a.mode != mode) continue;
        if (!noclip::isWithinEarlyWindow(x, a.x, signedSpeed, window)) continue;
        float dist = std::abs(a.x - x);
        if (dist < nearest) {
            nearest = dist;
            result = a.x;
        }
    }
    return result;
}

void rememberDelivered(PlayLayer* layer, bool player1, bool down, int mode,
                       int step, Config const& cfg) {
    if (!cfg.autoLearn || cfg.recordReference || cfg.clearLearned) return;
    if (!layer || layer->m_playerDied) return;
    auto* player = playerFor(layer, player1);
    if (!player || player->m_isDead) return;
    auto i = player1 ? 0 : 1;
    auto& q = g_session.unverified[i];
    if (q.size() >= kMaxLearningPerPlayer) q.pop_front();
    q.push_back({g_session.level, step, player->m_position.x,
                 player1, down, mode});
}

void learnFromSurvival(PlayLayer* layer, Config const& cfg, int step) {
    if (!cfg.autoLearn || cfg.recordReference || cfg.clearLearned) return;
    for (int i = 0; i != 2; ++i) {
        bool p1 = i == 0;
        auto* player = playerFor(layer, p1);
        auto& queue = g_session.unverified[i];
        if (!player || player->m_isDead || layer->m_playerDied) {
            queue.clear();
            continue;
        }
        while (!queue.empty()) {
            auto const& sample = queue.front();
            if (step - sample.step < kSurvivalSteps) break;
            if (sample.level == g_session.level) {
                addReference({sample.level, sample.player1, sample.down,
                              sample.mode, sample.x}, false);
            }
            queue.pop_front();
        }
    }
}

void updateVelocity(PlayLayer* layer, int step) {
    for (int i = 0; i != 2; ++i) {
        auto* player = playerFor(layer, i == 0);
        if (!player) continue;
        float x = player->m_position.x;
        if (!std::isfinite(x)) continue;
        if (g_session.hasX[i]) {
            int steps = std::max(1, step - g_session.lastStep);
            float dx = (x - g_session.lastX[i]) / static_cast<float>(steps);
            // Ignore teleports, checkpoints, or a level reset.
            if (std::abs(dx) > .03f && std::abs(dx) < 70.f) {
                g_session.speed[i] = dx;
            }
        }
        g_session.lastX[i] = x;
        g_session.hasX[i] = true;
    }
}

// Collect currently active, static collision boxes. The predictor uses GD's
// object type metadata instead of brittle lists of specific object IDs.
// It intentionally does not simulate portals, orbs, triggers, move actions,
// rotated polygon edges, or platformer movement. Unknown timing passes through.
struct WaveScene {
    std::vector<noclip::wave::Rect> hazards;
    std::vector<noclip::wave::Rect> special;
};

std::optional<WaveScene> waveObstacles(
    PlayLayer* layer, noclip::wave::State const& state, int horizon
) {
    if (!layer || !layer->m_objects || state.dxPerFrame <= .1f)
        return std::nullopt; // Reversed X movement not modelled yet.
    auto* children = layer->m_objects;

    constexpr std::size_t kMaxCandidates = 1024;
    WaveScene scene;
    const float reach = state.dxPerFrame * horizon + 20.f;
    const float start = state.x - 25.f;
    const float end = state.x + reach;
    const float vertical = std::abs(state.dxPerFrame) * horizon + 30.f;
    for (auto* object : geode::cocos::CCArrayExt<GameObject>(children)) {
        if (!object || object->m_isNoTouch || object->m_isDisabled) continue;
        auto type = object->m_objectType;
        bool collision = type == GameObjectType::Hazard
            || type == GameObjectType::Solid
            || type == GameObjectType::Breakable
            || type == GameObjectType::Slope
            || type == GameObjectType::AnimatedHazard;
        // If a portal/pad/ring can change physics during our lookahead,
        // our constant-slope prediction is NOT valid. Don't delay anything.
        bool special = type == GameObjectType::InverseGravityPortal
            || type == GameObjectType::NormalGravityPortal
            || type == GameObjectType::WavePortal
            || type == GameObjectType::CubePortal
            || type == GameObjectType::ShipPortal
            || type == GameObjectType::BallPortal
            || type == GameObjectType::UfoPortal
            || type == GameObjectType::RobotPortal
            || type == GameObjectType::SpiderPortal
            || type == GameObjectType::SwingPortal
            || type == GameObjectType::TeleportPortal
            || type == GameObjectType::MiniSizePortal
            || type == GameObjectType::RegularSizePortal
            || type == GameObjectType::GravityTogglePortal
            || type == GameObjectType::YellowJumpPad
            || type == GameObjectType::PinkJumpPad
            || type == GameObjectType::GravityPad
            || type == GameObjectType::RedJumpPad
            || type == GameObjectType::YellowJumpRing
            || type == GameObjectType::PinkJumpRing
            || type == GameObjectType::RedJumpRing
            || type == GameObjectType::GravityRing
            || type == GameObjectType::GreenRing
            || type == GameObjectType::DashRing
            || type == GameObjectType::GravityDashRing
            || type == GameObjectType::SpiderOrb
            || type == GameObjectType::SpiderPad
            || type == GameObjectType::TeleportOrb;
        if (!collision && !special) continue;
        auto rect = object->getObjectRect();
        noclip::wave::Rect box{
            rect.getMinX(), rect.getMinY(), rect.getMaxX(), rect.getMaxY()
        };
        if (!noclip::wave::valid(box)) continue;
        if (box.right < start || box.left > end
            || box.top < state.y - vertical || box.bottom > state.y + vertical)
            continue;
        if (special) scene.special.push_back(box);
        else scene.hazards.push_back(box);
        if (scene.hazards.size() + scene.special.size() > kMaxCandidates)
            return std::nullopt;
    }
    return scene;
}

std::optional<noclip::wave::Decision> wavePredictedDelay(
    PlayLayer* layer, PlayerObject* player, bool currentHeld,
    bool down, float dxPerStep, Config const& cfg
) {
    if (!cfg.predictWave || !player->m_isDart || layer->m_isPlatformer
        || !std::isfinite(dxPerStep) || dxPerStep <= .1f)
        return std::nullopt;

    noclip::wave::State state;
    state.x = player->m_position.x;
    state.y = player->m_position.y;
    state.dxPerFrame = dxPerStep;
    state.held = currentHeld;
    state.gravityReversed = player->m_isUpsideDown;
    state.halfWidth = player->m_vehicleSize < 1.f ? 2.f : 3.5f;
    state.halfHeight = player->m_vehicleSize < 1.f ? 2.f : 3.5f;
    auto scene = waveObstacles(layer, state, cfg.waveHorizon);
    if (!scene) return std::nullopt;

    return noclip::wave::chooseDelay(
        state, down, cfg.window, cfg.waveHorizon,
        scene->hazards, scene->special
    );
}

bool inPlay(PlayLayer* layer, Config const& cfg) {
    return layer && layer->m_level && !layer->m_isPaused
        && !layer->m_playerDied && !layer->m_useReplay
        && (cfg.platformer || !layer->m_isPlatformer);
}

} // namespace

class $modify(NoclipBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        // In Geode/GD, the jump button is used for both presses and releases
        // across cube, wave, ship, UFO, ball, robot, spider, and swing.
        auto* layer = PlayLayer::get();
        auto cfg = config();
        if (g_dispatchingBuffered || !cfg.enabled || cfg.window == 0
            || !inPlay(layer, cfg) || layer != static_cast<GJBaseGameLayer*>(this)
            || button != static_cast<int>(PlayerButton::Jump)) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            if (layer && layer == static_cast<GJBaseGameLayer*>(this)
                && button == static_cast<int>(PlayerButton::Jump))
                g_session.deliveredHeld[isPlayer1 ? 0 : 1] = down;
            return;
        }

        loadAnchors();
        clearReferencesIfRequested(cfg);
        g_session.bind(layer);
        auto* player = playerFor(layer, isPlayer1);
        if (!player || player->m_isDead) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        int step = layer->m_currentStep;
        int mode = playerMode(player);
        float x = player->m_position.x;
        int i = isPlayer1 ? 0 : 1;

        // Training mode captures explicit reference timing without modifying
        // the current attempt's inputs.
        if (cfg.recordReference) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            g_session.deliveredHeld[i] = down;
            if (!cfg.clearLearned) {
                addReference({g_session.level, isPlayer1, down, mode, x}, true);
            }
            return;
        }

        // Only wave uses the collision-aware micro predictor. Other modes
        // continue to use optional learned reference positions.
        auto& pending = g_session.delayed[i];
        bool preserveQueueOrder = !pending.empty();
        bool waveMode = cfg.predictWave && mode == 1;
        auto prediction = (waveMode && !preserveQueueOrder)
            ? wavePredictedDelay(layer, player, g_session.deliveredHeld[i],
                                 down, g_session.speed[i], cfg)
            : std::nullopt;
        int predictedFrames = prediction ? prediction->delayFrames : 0;
        bool predictiveDelay = predictedFrames > 0;

        if (waveMode && cfg.logWave) {
            if (preserveQueueOrder) {
                log::info("Noclip wave {}: queued behind prior edge at step {}",
                          down ? "PRESS" : "RELEASE", step);
            } else if (prediction) {
                log::info(
                    "Noclip wave {}: {}, delay={} steps, hit={}, checked={}, "
                    "candidates={}, x={:.1f}, vx={:.2f}",
                    down ? "PRESS" : "RELEASE",
                    noclip::wave::verdictName(prediction->verdict),
                    prediction->delayFrames, prediction->firstCollisionFrame,
                    prediction->comparedFrames, prediction->safeCandidates,
                    x, g_session.speed[i]
                );
            } else {
                log::info(
                    "Noclip wave {}: unavailable (unsupported state/speed), "
                    "x={:.1f}, vx={:.2f}",
                    down ? "PRESS" : "RELEASE", x, g_session.speed[i]
                );
            }
        }

        // When wave correction is on, never blindly delay an input.
        auto found = waveMode ? std::optional<float>{} : nearestReference(
            g_session.level, isPlayer1, down, mode,
            x, g_session.speed[i], cfg.window
        );
        bool blind = !waveMode && cfg.blindDelay && !found.has_value();
        if (!found && !blind && !preserveQueueOrder && !predictiveDelay) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            g_session.deliveredHeld[i] = down;
            rememberDelivered(layer, isPlayer1, down, mode, step, cfg);
            g_session.lastSendStep[i] = step;
            return;
        }

        if (pending.size() >= kMaxQueuedPerPlayer) {
            pending.clear();
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            g_session.deliveredHeld[i] = down;
            return;
        }

        noclip::Deferred event;
        event.down = down;
        event.button = button;
        event.player1 = isPlayer1;
        event.receivedStep = step;
        event.expiresStep = step + (predictiveDelay ? predictedFrames
            : (found || blind ? cfg.window : 0));
        event.hasTarget = found.has_value();
        event.targetX = found.value_or(0.f);
        event.direction = noclip::normalizeDirection(g_session.speed[i]);

        if (preserveQueueOrder) {
            // Keep the physical press/release gap intact after buffering:
            // a 1-step click followed by release remains a 1-step tap.
            auto const& prev = pending.back();
            int physicalGap = std::max(1, step - prev.receivedStep);
            event.expiresStep = std::max(step, prev.expiresStep + physicalGap);
            event.hasTarget = false;
        }
        pending.push_back(event);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto* layer = PlayLayer::get();
        auto cfg = config();
        if (layer && layer == static_cast<GJBaseGameLayer*>(this)) {
            g_session.bind(layer);
            loadAnchors();
            clearReferencesIfRequested(cfg);

            int step = layer->m_currentStep;
            if (step != g_session.lastStep) {
                updateVelocity(layer, step);
                if (!cfg.enabled || cfg.window == 0 || cfg.recordReference) {
                    for (auto& q : g_session.delayed) q.clear();
                }
                if (inPlay(layer, cfg) && cfg.enabled && cfg.window > 0
                    && !cfg.recordReference) {
                    for (int i = 0; i != 2; ++i) {
                        auto& queue = g_session.delayed[i];
                        auto* player = playerFor(layer, i == 0);
                        if (!player || player->m_isDead) {
                            queue.clear();
                            continue;
                        }
                        if (queue.empty() || g_session.lastSendStep[i] == step)
                            continue;

                        auto const& event = queue.front();
                        if (!noclip::shouldDispatch(
                                event, player->m_position.x,
                                g_session.speed[i], step)) continue;

                        // Remove first, then call original via hook with guard.
                        // This preserves press/release ordering even if the
                        // user lets go before their buffered press is fired.
                        auto edge = event;
                        queue.pop_front();
                        g_dispatchingBuffered = true;
                        this->handleButton(edge.down, edge.button, edge.player1);
                        g_dispatchingBuffered = false;
                        g_session.lastSendStep[i] = step;
                        rememberDelivered(layer, edge.player1, edge.down,
                                          playerMode(player), step, cfg);
                    }
                }
                learnFromSurvival(layer, cfg, step);
                if (g_dirty && step > 0 && step % 240 == 0) saveAnchors();
                g_session.lastStep = step;
            }
            if (layer->m_playerDied) g_session.resetTransient();
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

class $modify(NoclipPlayLayer, PlayLayer) {
    void resetLevel() {
        if (g_dirty) saveAnchors();
        g_session.resetTransient();
        PlayLayer::resetLevel();
    }
    void onExit() {
        if (g_dirty) saveAnchors();
        g_session.resetTransient();
        g_session.layer = nullptr;
        g_session.level.clear();
        PlayLayer::onExit();
    }
};

$on_mod(Loaded) {
    log::info("Noclip v0.3.0 loaded: local wave snap prediction");
}

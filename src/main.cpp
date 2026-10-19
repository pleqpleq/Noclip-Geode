#include <Geode/Geode.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "InputQueue.hpp"
#include "WavePreview.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

using namespace geode::prelude;

namespace {
// An input is only intercepted in a normal single-player WAVE.  These
// safeguards are deliberate: dual mode and CBF have their own input systems.
struct Settings {
    bool enabled = false;
    bool fixedDelayTest = false;
    bool debug = false;
    bool f6Enabled = true;
    int fixedTicks = 2;
    int maxTicks = 4;
    int lookahead = 18;
};

int g_stageOverride = -1;
constexpr std::array<int, 5> kStages{2, 4, 8, 8, 0};
std::uint64_t g_resetEpoch = 1;
bool g_warnedCbf = false;

Settings readSettings() {
    auto* mod = Mod::get();
    Settings cfg;
    cfg.enabled = mod->getSettingValue<bool>("enabled");
    cfg.fixedDelayTest = mod->getSettingValue<bool>("fixed-delay-test");
    cfg.debug = mod->getSettingValue<bool>("debug-logging");
    cfg.f6Enabled = mod->getSettingValue<bool>("f6-cycle");
    cfg.fixedTicks = std::clamp(
        static_cast<int>(mod->getSettingValue<int64_t>("fixed-delay-ticks")), 1, 8);
    cfg.maxTicks = std::clamp(
        static_cast<int>(mod->getSettingValue<int64_t>("max-wave-ticks")), 1, 8);
    cfg.lookahead = std::clamp(
        static_cast<int>(mod->getSettingValue<int64_t>("lookahead-ticks")), 12, 48);
    if (g_stageOverride >= 0)
        cfg.maxTicks = kStages[g_stageOverride];
    return cfg;
}

bool cbfInstalled() {
    return Loader::get()->isModLoaded("syzzi.click_between_frames");
}

bool dualMode(GJBaseGameLayer* layer) {
    if (!layer) return true;
    // m_gameState is authoritative when a dual portal changes modes midlevel.
    // Two-player-mode level setting also implies that we should not buffer.
    if (layer->m_gameState.m_isDualMode) return true;
    if (layer->m_levelSettings && layer->m_levelSettings->m_twoPlayerMode)
        return true;
    return false;
}

bool mayBuffer(GJBaseGameLayer* layer, Settings const& cfg) {
    if (!cfg.enabled || cfg.maxTicks == 0 || cbfInstalled()) return false;
    auto* play = PlayLayer::get();
    if (!play || static_cast<GJBaseGameLayer*>(play) != layer) return false;
    if (play->m_playerDied || play->m_isPaused || play->m_useReplay
        || play->m_isPlatformer || dualMode(layer)) return false;
    if (!layer->m_player1 || layer->m_player1->m_isDead) return false;
    return layer->m_player1->m_isDart;
}

// The only game objects we predict are currently active static solid/hazard
// shapes. Portals/orbs are tracked as unknown physics and disable prediction
// along the sampled trajectories. This is NOT the actual GD physics engine.
struct Scene {
    std::vector<noclip::wave::Collider> hazards;
    std::vector<noclip::wave::Collider> specials;
};

bool isSpecial(GameObjectType type) {
    switch (type) {
        case GameObjectType::InverseGravityPortal:
        case GameObjectType::NormalGravityPortal:
        case GameObjectType::GravityTogglePortal:
        case GameObjectType::CubePortal:
        case GameObjectType::ShipPortal:
        case GameObjectType::WavePortal:
        case GameObjectType::BallPortal:
        case GameObjectType::UfoPortal:
        case GameObjectType::RobotPortal:
        case GameObjectType::SpiderPortal:
        case GameObjectType::SwingPortal:
        case GameObjectType::TeleportPortal:
        case GameObjectType::MiniSizePortal:
        case GameObjectType::RegularSizePortal:
        case GameObjectType::YellowJumpRing:
        case GameObjectType::PinkJumpRing:
        case GameObjectType::RedJumpRing:
        case GameObjectType::GravityRing:
        case GameObjectType::GreenRing:
        case GameObjectType::YellowJumpPad:
        case GameObjectType::PinkJumpPad:
        case GameObjectType::RedJumpPad:
        case GameObjectType::GravityPad:
        case GameObjectType::DashRing:
        case GameObjectType::GravityDashRing:
        case GameObjectType::SpiderOrb:
        case GameObjectType::SpiderPad:
        case GameObjectType::TeleportOrb:
            return true;
        default:
            return false;
    }
}

std::optional<Scene> sceneFor(
    GJBaseGameLayer* layer, noclip::wave::State const& state, int horizon
) {
    if (!layer || !layer->m_objects) return std::nullopt;
    using namespace noclip::wave;
    Scene result;
    result.hazards.reserve(64);
    result.specials.reserve(16);
    float xMin=state.x-12.f;
    float xMax=state.x+state.dx*horizon+16.f;
    float yReach=std::abs(state.dx)*state.verticalSlope*horizon+20.f;
    for (GameObject* object : geode::cocos::CCArrayExt<GameObject*>(layer->m_objects)) {
        if (!object || object->m_isNoTouch || object->m_isDisabled
            || object->m_isPassable || object->m_isDecoration) continue;
        auto type=object->m_objectType;
        bool dangerous=(type==GameObjectType::Solid
            || type==GameObjectType::Hazard
            || type==GameObjectType::AnimatedHazard
            || type==GameObjectType::Slope);
        bool special=isSpecial(type);
        if(!dangerous && !special)continue;
        auto r=object->getObjectRect();
        Rect hitbox{r.getMinX(),r.getMinY(),r.getMaxX(),r.getMaxY()};
        if(!valid(hitbox))continue;
        if(hitbox.right<xMin || hitbox.left>xMax ||
           hitbox.top<state.y-yReach || hitbox.bottom>state.y+yReach)continue;
        Collider shape=box(hitbox);
        if(type==GameObjectType::Slope)
            shape=triangle(hitbox,object->m_slopeDirection);
        else if(object->m_objectRadius>0.f) {
            float radius=object->m_objectRadius*std::max(
                std::abs(object->m_scaleX),std::abs(object->m_scaleY));
            if(radius>.01f)
                shape=circle({static_cast<float>(object->m_positionX),
                              static_cast<float>(object->m_positionY)},radius);
        } else if(object->m_orientedBox && object->m_shouldUseOuterOb
                   && !object->m_isOrientedBoxDirty) {
            auto const& c=object->m_orientedBox->m_corners;
            shape=oriented({{{c[0].x,c[0].y},{c[1].x,c[1].y},
                            {c[2].x,c[2].y},{c[3].x,c[3].y}}});
        }
        if(!valid(shape))return std::nullopt;
        (special?result.specials:result.hazards).push_back(shape);
        if(result.hazards.size()>256 || result.specials.size()>256)
            return std::nullopt;
    }
    return result;
}

} // namespace

class $modify(NoclipInputLayer, GJBaseGameLayer) {
    struct Fields {
        noclip::InputQueue buffer;
        std::uint64_t tick = 0;
        std::uint64_t resetEpoch = 0;
        float observedDx = 0.f;
        bool haveVelocity = false;
        bool injecting = false;
    };

    void resetIfNeeded() {
        if(m_fields->resetEpoch==g_resetEpoch)return;
        m_fields->buffer.clear();
        m_fields->tick=0;
        m_fields->observedDx=0.f;
        m_fields->haveVelocity=false;
        m_fields->resetEpoch=g_resetEpoch;
    }

    void sendBuffered(noclip::Edge const& edge, Settings const& cfg) {
        // Unlike v0.4, use the *normal game-layer input path* for playback;
        // PlayerObject direct calls broke input routing in dual and with mods.
        m_fields->injecting=true;
        GJBaseGameLayer::handleButton(edge.pressed,edge.button,edge.player1);
        m_fields->injecting=false;
        if (cfg.debug) {
            bool received = m_player1 && m_player1->m_holdingButtons[
                static_cast<int>(PlayerButton::Jump)] == edge.pressed;
            log::info("Noclip DISPATCH {} raw={} applied={} delay={} actualHeld={}",
                edge.pressed ? "PRESS" : "RELEASE", edge.received,
                m_fields->tick, m_fields->tick-edge.received, received);
        }
    }

    void flush(Settings const& cfg) {
        for(int i=0;i<2;++i) {
            while(auto edge=m_fields->buffer.popFirst(i))
                sendBuffered(*edge,cfg);
        }
    }

    void handleButton(bool down,int button,bool player1) {
        if(m_fields->injecting) {
            GJBaseGameLayer::handleButton(down,button,player1);
            return;
        }
        resetIfNeeded();
        Settings cfg=readSettings();
        bool eligible=mayBuffer(this,cfg) && player1
            && button==static_cast<int>(PlayerButton::Jump);
        if(!eligible) {
            // This includes all dual-mode events, CBF, portals, and disabled
            // states. Never hold an event when we cannot safely re-emit it.
            if(m_fields->buffer.hasPending(0) || m_fields->buffer.hasPending(1))
                flush(cfg);
            if(cfg.enabled && cbfInstalled() && !g_warnedCbf) {
                log::warn("Noclip: CBF detected; buffering safely disabled. Turn CBF off to test.");
                g_warnedCbf=true;
            }
            GJBaseGameLayer::handleButton(down,button,player1);
            if(button==static_cast<int>(PlayerButton::Jump))
                m_fields->buffer.players[player1?0:1].deliveredHeld=down;
            return;
        }

        int delay=0;
        if(!m_fields->buffer.hasPending(0)) {
            if(cfg.fixedDelayTest) {
                delay=cfg.fixedTicks;
            } else if(m_fields->haveVelocity) {
                auto player=m_player1;
                auto rect=player->getObjectRect(.3f,.3f);
                if(rect.size.width>0.f && rect.size.height>0.f) {
                    noclip::wave::State state;
                    state.x=rect.getMidX();
                    state.y=rect.getMidY();
                    state.dx=m_fields->observedDx;
                    state.halfW=rect.size.width*.5f;
                    state.halfH=rect.size.height*.5f;
                    state.verticalSlope=player->m_vehicleSize<1.f?2.f:1.f;
                    state.held=player->m_holdingButtons[
                        static_cast<int>(PlayerButton::Jump)];
                    state.upsideDown=player->m_isUpsideDown;
                    int horizon=std::max(cfg.lookahead,cfg.maxTicks+8);
                    if(auto scene=sceneFor(this,state,horizon)) {
                        auto result=noclip::wave::choose(
                            state,down,cfg.maxTicks,horizon,
                            scene->hazards,scene->specials);
                        delay=result.delay;
                        if(cfg.debug) {
                            log::info("Noclip PREVIEW {} tick={} candidates=0..{} "
                                      "survivalNow={} survivalBest={} chosenDelay={} "
                                      "supported={} hazards={} specials={}",
                                down?"PRESS":"RELEASE",m_fields->tick,cfg.maxTicks,
                                result.immediateSurvival,result.bestSurvival,delay,
                                result.supported,scene->hazards.size(),scene->specials.size());
                        }
                    } else if(cfg.debug) {
                        log::info("Noclip PREVIEW unsupported: scene scan failed");
                    }
                }
            }
        }
        if(cfg.debug)
            log::info("Noclip INPUT {} tick={} delay={} pending={}",
                down?"PRESS":"RELEASE",m_fields->tick,delay,
                m_fields->buffer.hasPending(0));
        bool buffered=m_fields->buffer.intercept(
            down,true,button,m_fields->tick,delay);
        if(!buffered) {
            if(m_fields->buffer.hasPending(0))flush(cfg);
            GJBaseGameLayer::handleButton(down,button,player1);
        }
    }

    void processCommands(float dt,bool isHalfTick,bool isLastTick) {
        resetIfNeeded();
        Settings cfg=readSettings();
        if (!isHalfTick) ++m_fields->tick;

        // Flush/deliver BEFORE physics advances, not after the player
        // collides. The old builds delivered too late and could lose inputs.
        if(m_fields->buffer.hasPending(0) || m_fields->buffer.hasPending(1)) {
            if(!mayBuffer(this,cfg))flush(cfg);
            else if(!isHalfTick) {
                for(int i=0;i<2;++i)
                    while(auto edge=m_fields->buffer.popDue(i,m_fields->tick))
                        sendBuffered(*edge,cfg);
            }
        }

        float before=(m_player1 ? m_player1->getPositionX() : 0.f);
        GJBaseGameLayer::processCommands(dt,isHalfTick,isLastTick);
        if(!isHalfTick && m_player1) {
            float dx=m_player1->getPositionX()-before;
            if(std::isfinite(dx) && dx>.05f && dx<30.f) {
                m_fields->observedDx=dx;
                m_fields->haveVelocity=true;
            } else if(dx < -1.f || dx>30.f) {
                // A teleport/reset invalidates previous velocity and queues.
                m_fields->haveVelocity=false;
                m_fields->buffer.clear();
            }
        }
        if(m_playerDied) {
            m_fields->buffer.clear();
            m_fields->haveVelocity=false;
        }
    }
};

class $modify(NoclipResetHook, PlayLayer) {
    void resetLevel() {
        ++g_resetEpoch;
        PlayLayer::resetLevel();
    }
    void onExit() {
        ++g_resetEpoch;
        PlayLayer::onExit();
    }
};

class $modify(NoclipKeyboardHook, CCKeyboardDispatcher) {
    void dispatchKeyboardMSG(
        enumKeyCodes key,bool isDown,bool isRepeat,double timestamp
    ) {
        if(key==enumKeyCodes::KEY_F6 && isDown && !isRepeat
           && Mod::get()->getSettingValue<bool>("f6-cycle")) {
            g_stageOverride=(g_stageOverride+1)%5;
            if(kStages[g_stageOverride]==0) {
                log::info("Noclip F6: OFF (raw input)");
            } else {
                log::info("Noclip F6: stage {}, wave correction max {} ticks",
                          g_stageOverride+1,kStages[g_stageOverride]);
            }
        }
        CCKeyboardDispatcher::dispatchKeyboardMSG(
            key,isDown,isRepeat,timestamp);
    }
};

$on_mod(Loaded) {
    log::info("Noclip v0.6.0 probe: independent press/release queue, "
              "safe dual/CBF bypass, wave preview. Mod OFF until enabled.");
}
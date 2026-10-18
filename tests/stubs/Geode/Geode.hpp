#pragma once
#include <cstdint>
#include <string>
#include <utility>

enum class PlayerButton : int { Jump = 1, Left = 2, Right = 3 };
struct Point { float x = 0.f; float y = 0.f; };
struct GJGameLevel {
    int m_levelID = 1;
    std::string m_levelName = "test";
    std::string m_levelString = "level";
};
struct PlayerObject {
    bool m_isDart = false;
    bool m_isShip = false;
    bool m_isBird = false;
    bool m_isBall = false;
    bool m_isRobot = false;
    bool m_isSpider = false;
    bool m_isSwing = false;
    bool m_isDead = false;
    Point m_position;
};
struct GJBaseGameLayer {
    virtual ~GJBaseGameLayer() = default;
    virtual void handleButton(bool, int, bool) {}
    virtual void processCommands(float, bool, bool) {}
    PlayerObject* m_player1 = nullptr;
    PlayerObject* m_player2 = nullptr;
    int m_currentStep = 0;
    bool m_isPlatformer = false;
    bool m_playerDied = false;
    bool m_useReplay = false;
};
struct PlayLayer : GJBaseGameLayer {
    static PlayLayer* get() { return nullptr; }
    GJGameLevel* m_level = nullptr;
    bool m_isPaused = false;
    virtual void resetLevel() {}
    virtual void onExit() {}
};
struct Mod {
    static Mod* get() { static Mod m; return &m; }
    template <class T> T getSettingValue(char const*) { return T{}; }
    template <class T> T getSavedValue(char const*, T def) { return def; }
    template <class T> void setSavedValue(char const*, T const&) {}
};
namespace geode::prelude {
    using ::GJBaseGameLayer;
    using ::PlayLayer;
    using ::GJGameLevel;
    using ::PlayerObject;
    using ::PlayerButton;
    using ::Mod;
    namespace log {
        template <typename... T>
        void info(char const*, T&&...) {}
    }
}
#define $modify(NAME, BASE) NAME : public BASE
#define $on_mod(NAME) static void on_mod_##NAME()

// ============================================================================
//  Predictive Input Buffer v2.0.0  (Windows, Geode 5.11.2, GD 2.2081)
//
//  v1 trajectory buffer (lilWC) + ideas from Lyophi's "Input Buffer" (MacOS)
//  + new modes: Hybrid, Debounce, lethal input filter, presets, auto TPS,
//  rebindable Geode keybinds, on-screen overlay.
// ============================================================================

#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/modify/PauseLayer.hpp>

#include "Config.hpp"
#include "Engine.hpp"
#include "Overlay.hpp"
#include "Predictor.hpp"

using namespace geode::prelude;
using namespace pbuf;

// ---------------------------------------------------------------------------
//  Hotkeys
// ---------------------------------------------------------------------------

static void hotkeyToggle() {
    bool v = !cfg().enabled;
    Mod::get()->setSettingValue<bool>("enabled", v);
    overlay::toast(v ? "Input Buffer: ON" : "Input Buffer: OFF");
    out(fmt::format("Buffer -> {}", v ? "ON" : "OFF"));
}

static void hotkeyCycleWindow() {
    static constexpr int CYCLE[] = {2, 4, 8, 16, 0};
    int cur = cfg().mainWindow;
    int next = 2;
    if (cur == 0) next = 2;
    else if (cur >= 16) next = 0;
    else {
        for (int v : CYCLE) {
            if (v > cur) { next = v; break; }
        }
    }
    Mod::get()->setSettingValue<int64_t>("main-window", next);
    std::string msg = next == 0 ? "Main window: OFF" : fmt::format("Main window: {} ticks ({:.2f} frames @ 60fps)", next, next / 4.0f);
    overlay::toast(msg);
    out(msg);
}

static void hotkeyCycleStrategy() {
    auto pl = PlayLayer::get();
    if (!pl) return;
    if (cfg().preset > 0) {
        overlay::toast(fmt::format("Preset \"{}\" is active (set it to Custom first)", presetName(cfg().preset)));
        return;
    }
    GM gm = engine().currentMode(pl);
    int cur = static_cast<int>(stratFor(gm));
    int count = static_cast<int>(Strat::Count);
    Strat next = Strat::Off;
    for (int i = 1; i <= count; i++) {
        auto cand = static_cast<Strat>((cur + i) % count);
        if (stratValidFor(gm, cand)) { next = cand; break; }
    }
    Mod::get()->setSettingValue<std::string>(stratSettingKey(gm), stratName(next));
    auto msg = fmt::format("{} strategy: {}", gmName(gm), stratName(next));
    overlay::toast(msg);
    out(msg);
}

static void hotkeyCyclePreset() {
    int next = (cfg().preset + 1) % presetCount();
    Mod::get()->setSettingValue<std::string>("preset", presetName(next));
    auto msg = fmt::format("Preset: {}", presetName(next));
    overlay::toast(msg);
    out(msg);
}

static void hotkeyOverlay() {
    bool v = !cfg().overlay;
    Mod::get()->setSettingValue<bool>("overlay", v);
}

static void hotkeyDebug() {
    bool v = !cfg().debug;
    Mod::get()->setSettingValue<bool>("debug", v);
    overlay::toast(v ? "Debug logging: ON" : "Debug logging: OFF");
}

template <class F>
static void bindKey(char const* key, F action) {
    listenForKeybindSettingPresses(key, [action](Keybind const&, bool down, bool repeat, double) {
        if (!down || repeat) return;
        if (!PlayLayer::get()) return;
        action();
    });
}

$on_mod(Loaded) {
    loadConfig();

    listenForAllSettingChanges([](std::string_view, std::shared_ptr<SettingV3>) {
        loadConfig();
        engine().markDirty();
    });

    bindKey("key-toggle", hotkeyToggle);
    bindKey("key-cycle-window", hotkeyCycleWindow);
    bindKey("key-cycle-strategy", hotkeyCycleStrategy);
    bindKey("key-cycle-preset", hotkeyCyclePreset);
    bindKey("key-overlay", hotkeyOverlay);
    bindKey("key-debug", hotkeyDebug);

    log::info("[InputBuffer] v2.0.0 loaded. Preset: {}, main window: {} ticks",
              presetName(cfg().preset), cfg().mainWindow);
}

// ---------------------------------------------------------------------------
//  PlayLayer
// ---------------------------------------------------------------------------

class $modify(BufPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("PlayLayer::destroyPlayer", -1000000);
        (void)self.setHookPriority("PlayLayer::playEndAnimationToPos", -1000000);
        (void)self.setHookPriority("PlayLayer::flipGravity", -1000000);
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        predictor().init(this);
        engine().onLevelStart();
        overlay::attach(this);
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        engine().onReset();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        // Deaths of the hidden simulation players never reach the game
        if (predictor().onDestroyPlayer(player)) return;
        PlayLayer::destroyPlayer(player, object);
    }

    void playEndAnimationToPos(cocos2d::CCPoint pos) {
        if (predictor().isSimulating()) return;
        PlayLayer::playEndAnimationToPos(pos);
    }

    void flipGravity(PlayerObject* player, bool flip, bool noEffects) {
        if (predictor().isSimulating()) return;
        PlayLayer::flipGravity(player, flip, noEffects);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        overlay::refresh(this);
    }

    void onQuit() {
        PlayLayer::onQuit();
        predictor().quit();
        overlay::detach();
    }
};

class $modify(BufEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool noUI) {
        bool res = LevelEditorLayer::init(level, noUI);
        predictor().quit();
        return res;
    }
};

// ---------------------------------------------------------------------------
//  Side effects muted while the trajectory is simulated (v1 behaviour)
// ---------------------------------------------------------------------------

class $modify(BufHardStreak, HardStreak) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("HardStreak::addPoint", -1000000);
    }

    void addPoint(cocos2d::CCPoint point) {
        if (predictor().isSimulating()) return;
        HardStreak::addPoint(point);
    }
};

class $modify(BufEffectGameObject, EffectGameObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("EffectGameObject::triggerObject", -1000000);
    }

    void triggerObject(GJBaseGameLayer* layer, int uniqueID, gd::vector<int> const* remapKeys) {
        if (predictor().isSimulating()) return;
        EffectGameObject::triggerObject(layer, uniqueID, remapKeys);
    }
};

class $modify(BufGameObject, GameObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("GameObject::playShineEffect", -1000000);
    }

    void playShineEffect() {
        if (predictor().isSimulating()) return;
        GameObject::playShineEffect();
    }
};

class $modify(BufPlayerObject, PlayerObject) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("PlayerObject::playSpiderDashEffect", -1000000);
        (void)self.setHookPriority("PlayerObject::incrementJumps", -1000000);
        (void)self.setHookPriority("PlayerObject::ringJump", -1000000);
    }

    void playSpiderDashEffect(cocos2d::CCPoint from, cocos2d::CCPoint to) {
        if (predictor().isSimulating()) return;
        PlayerObject::playSpiderDashEffect(from, to);
    }

    void incrementJumps() {
        if (predictor().isSimulating()) return;
        PlayerObject::incrementJumps();
    }

    void ringJump(RingObject* ring, bool unk) {
        if (predictor().isSimulating()) return;
        PlayerObject::ringJump(ring, unk);
    }

    void update(float dt) {
        PlayerObject::update(dt);
        if (predictor().isSimulating()) return;
        if (auto pl = PlayLayer::get(); pl && pl->m_player1 == this) {
            predictor().onRealPlayerUpdate(dt);
        }
    }
};

// ---------------------------------------------------------------------------
//  Input interception + physics step
// ---------------------------------------------------------------------------

class $modify(BufBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("GJBaseGameLayer::canBeActivatedByPlayer", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::handleButton", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::collisionCheckObjects", -1000000);
        (void)self.setHookPriority("GJBaseGameLayer::playerTouchedRing", -1000000);
    }

    bool canBeActivatedByPlayer(PlayerObject* player, EffectGameObject* object) {
        if (predictor().isSimulating()) return false;
        return GJBaseGameLayer::canBeActivatedByPlayer(player, object);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (button == 1 && !predictor().isSimulating() && !engine().isInjecting()) {
            if (engine().onInput(this, down, isPlayer1)) return;
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        engine().beginStep(this);
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
        engine().endStep(this);
    }

    void collisionCheckObjects(PlayerObject* player, gd::vector<GameObject*>* objects, int count, float dt) {
        if (predictor().isSimulating()) {
            // Only solids, hazards and slopes matter for the lookahead
            gd::vector<GameObject*> filtered;
            filtered.reserve(count);
            for (int i = 0; i < count; i++) {
                GameObject* obj = objects->at(i);
                if (!obj) continue;
                switch (obj->m_objectType) {
                    case GameObjectType::Solid:
                    case GameObjectType::Hazard:
                    case GameObjectType::AnimatedHazard:
                    case GameObjectType::CollisionObject:
                    case GameObjectType::Slope:
                        filtered.push_back(obj);
                        break;
                    default:
                        break;
                }
            }
            GJBaseGameLayer::collisionCheckObjects(player, &filtered, static_cast<int>(filtered.size()), dt);
            return;
        }
        GJBaseGameLayer::collisionCheckObjects(player, objects, count, dt);
    }

    void playerTouchedRing(PlayerObject* player, RingObject* ring) {
        if (predictor().isSimulating()) return;
        GJBaseGameLayer::playerTouchedRing(player, ring);
    }
};

// ---------------------------------------------------------------------------
//  Pause menu shortcut to the settings
// ---------------------------------------------------------------------------

class $modify(BufPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto label = CCLabelBMFont::create("BUF", "bigFont.fnt");
        auto spr = CircleButtonSprite::create(label, CircleBaseColor::DarkAqua, CircleBaseSize::MediumAlt);
        spr->setScale(0.75f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(BufPauseLayer::onBufferSettings));
        btn->setID("buffer-settings-button"_spr);

        if (auto menu = this->getChildByID("right-button-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        } else {
            auto own = CCMenu::create();
            own->setID("buffer-menu"_spr);
            own->addChild(btn);
            auto win = CCDirector::get()->getWinSize();
            own->setPosition({win.width - 45.f, 22.f});
            this->addChild(own);
        }
    }

    void onBufferSettings(CCObject*) {
        openSettingsPopup(Mod::get(), false);
    }
};

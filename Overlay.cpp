#include "Overlay.hpp"
#include "Config.hpp"
#include "Engine.hpp"

using namespace geode::prelude;

namespace pbuf::overlay {

static Ref<CCLabelBMFont> s_label = nullptr;
static int s_lastTps = -1;

void attach(PlayLayer* pl) {
    s_label = CCLabelBMFont::create(" ", "chatFont.fnt");
    if (!s_label) return;
    s_label->setID("predictive-buffer-overlay"_spr);
    s_label->setZOrder(9999);
    pl->addChild(s_label);
    s_lastTps = -1;
    refresh(pl, true);
}

void detach() {
    s_label = nullptr;
}

static void place(CCLabelBMFont* l) {
    auto const& c = cfg();
    auto win = CCDirector::get()->getWinSize();
    float m = 4.f;
    switch (c.overlayPos) {
        case 0: l->setAnchorPoint({0.f, 1.f}); l->setPosition({m, win.height - m}); break;
        case 1: l->setAnchorPoint({1.f, 1.f}); l->setPosition({win.width - m, win.height - m}); break;
        case 3: l->setAnchorPoint({1.f, 0.f}); l->setPosition({win.width - m, m}); break;
        default: l->setAnchorPoint({0.f, 0.f}); l->setPosition({m, m}); break;
    }
    l->setScale(c.overlayScale);
    l->setOpacity(static_cast<GLubyte>(std::clamp(c.overlayOpacity, 0, 255)));
}

void refresh(PlayLayer* pl, bool force) {
    if (!s_label || !pl || s_label->getParent() != pl) return;
    auto const& c = cfg();
    auto& eng = engine();

    if (!c.overlay) {
        s_label->setVisible(false);
        eng.consumeDirty();
        eng.consumeFlash();
        return;
    }
    s_label->setVisible(true);

    int tps = eng.tps();
    if (tps != s_lastTps) { s_lastTps = tps; force = true; }

    if (eng.consumeDirty() || force) {
        std::string text;
        if (!c.enabled) {
            text = "BUFFER: OFF";
        } else if (!eng.isActive(pl)) {
            text = c.practiceOnly && !pl->m_isPracticeMode ? "BUFFER: practice only" : "BUFFER: paused (platformer)";
        } else {
            GM gm = eng.currentMode(pl);
            Strat st = stratFor(gm);
            int w = windowTicksFor(gm);
            if (w <= 0) st = Strat::Off;
            text = fmt::format("BUFFER  {}: {}", gmName(gm), stratShort(st));
            if (st != Strat::Off) text += fmt::format(" {}t", w);
            text += fmt::format("  |  {} TPS{}", tps, c.tpsOverride > 0 ? "" : " (auto)");
            if (c.preset > 0) text += fmt::format("  |  {}", presetName(c.preset));
            auto const& st2 = eng.stats();
            text += fmt::format("  |  saved {} ({})", st2.savedAttempt, st2.savedSession);
            if (!st2.lastKind.empty()) {
                text += fmt::format("  last +{:.1f}t {}", st2.lastSavedTicksX10 / 10.f, st2.lastKind);
            }
        }
        s_label->setString(text.c_str());
        place(s_label.data());
    }

    if (eng.consumeFlash() && c.overlayFlash) {
        s_label->stopAllActions();
        s_label->setColor({0, 255, 255});
        s_label->runAction(CCTintTo::create(0.35f, 255, 255, 255));
    }
}

void toast(std::string const& text) {
    if (!cfg().notifyHotkeys) return;
    Notification::create(text, NotificationIcon::Info, 1.2f)->show();
}

} // namespace pbuf::overlay

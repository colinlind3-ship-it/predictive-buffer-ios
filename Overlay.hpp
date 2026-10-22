#pragma once

#include <Geode/Geode.hpp>

// Small on-screen status label shown while playing
namespace pbuf::overlay {

void attach(PlayLayer* pl);
void detach();
void refresh(PlayLayer* pl, bool force = false);
void toast(std::string const& text);   // hotkey feedback (Geode notification)

} // namespace pbuf::overlay

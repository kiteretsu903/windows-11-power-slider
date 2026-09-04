#pragma once
#include <cstdint>

// A tray click is decided at mouse-down, not after focus-loss has hidden the
// flyout. The short focus-loss latch covers Explorer activating before it
// forwards the tray button-down notification.
class TrayClick {
public:
    void focus_lost_on_icon(std::uint64_t now) noexcept { suppress_until_=now+1000; }
    void down(bool open,std::uint64_t now) noexcept {
        close_=open || now<suppress_until_;
        armed_=true;
        suppress_until_=0;
    }
    bool up(bool open,std::uint64_t now) noexcept {
        const bool close=armed_?close_:(open || now<suppress_until_);
        armed_=false;suppress_until_=0;
        return close;
    }
private:
    bool armed_{},close_{};
    std::uint64_t suppress_until_{};
};

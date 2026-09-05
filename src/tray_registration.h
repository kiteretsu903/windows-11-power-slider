#pragma once
#include <windows.h>
#include <shellapi.h>

// Explorer can forget registration independently of our process. Reconcile by
// modifying first, then adding, so duplicate TaskbarCreated messages are safe.
// Work on a copy: balloon-only flags must not replace the icon's full payload.
template<class Notify>
bool reconcile_tray_icon(const NOTIFYICONDATAW& icon, Notify notify) noexcept {
    if (!icon.hWnd || !icon.hIcon) return false;
    NOTIFYICONDATAW data=icon;
    data.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;
    if (notify(NIM_MODIFY,&data)) return true;
    if (notify(NIM_ADD,&data)) return true;
    // A concurrent registration may make ADD fail even though the icon exists.
    return notify(NIM_MODIFY,&data)!=FALSE;
}

// Generated icons are owned; LoadIcon fallback resources are shared. Never
// discard a working image until a replacement exists, and keep retrying a
// failed mode-image generation even when Explorer accepts a fallback icon.
class TrayImage {
public:
    TrayImage()=default;
    TrayImage(const TrayImage&)=delete;
    TrayImage& operator=(const TrayImage&)=delete;
    ~TrayImage() { reset(); }

    template<class Fallback>
    void update(HICON generated, Fallback fallback) noexcept {
        pending_=generated==nullptr;
        if (generated) {
            if (owned_ && icon_) DestroyIcon(icon_);
            icon_=generated;
            owned_=true;
        } else if (!icon_) {
            icon_=fallback();
            owned_=false;
        }
    }
    HICON get() const noexcept { return icon_; }
    bool pending() const noexcept { return pending_; }
    void reset() noexcept {
        if (owned_ && icon_) DestroyIcon(icon_);
        icon_=nullptr;
        owned_=false;
        pending_=true;
    }
private:
    HICON icon_{};
    bool owned_{};
    bool pending_{true};
};

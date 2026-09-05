#include "tray_registration.h"
#include <cassert>
#include <cstdio>
#include <vector>

int main() {
    NOTIFYICONDATAW data{};
    data.cbSize=sizeof(data);data.uID=1;data.uCallbackMessage=WM_APP+1;
    std::vector<DWORD> calls;
    auto unavailable=[&](DWORD op,NOTIFYICONDATAW* payload) {
        assert(payload->hWnd && payload->hIcon);
        assert(payload->uFlags==(NIF_ICON|NIF_TIP|NIF_MESSAGE));
        assert(payload->uCallbackMessage==WM_APP+1);
        calls.push_back(op);return FALSE;
    };
    assert(!reconcile_tray_icon(data,unavailable));
    assert(calls.empty());
    data.hWnd=reinterpret_cast<HWND>(1);
    assert(!reconcile_tray_icon(data,unavailable));
    assert(calls.empty()); // Do not let the Shell accept a null image.

    HICON shared=LoadIconW(nullptr,IDI_APPLICATION);assert(shared);
    data.hIcon=shared;
    assert(!reconcile_tray_icon(data,unavailable));
    assert((calls==std::vector<DWORD>{NIM_MODIFY,NIM_ADD,NIM_MODIFY}));

    // Shell has lost the registration, then becomes available again.
    bool exists=false;
    auto shell=[&](DWORD op,NOTIFYICONDATAW* payload) {
        assert(payload->hIcon && payload->uFlags==(NIF_ICON|NIF_TIP|NIF_MESSAGE));
        calls.push_back(op);
        if(op==NIM_MODIFY) return exists?TRUE:FALSE;
        assert(op==NIM_ADD);
        if(exists) return FALSE;
        exists=true;return TRUE;
    };
    calls.clear();
    assert(reconcile_tray_icon(data,shell));
    assert((calls==std::vector<DWORD>{NIM_MODIFY,NIM_ADD}));
    calls.clear();
    assert(reconcile_tray_icon(data,shell));
    assert((calls==std::vector<DWORD>{NIM_MODIFY})); // Duplicate notification.
    exists=false;calls.clear();
    data.uFlags=NIF_INFO;
    assert(reconcile_tray_icon(data,shell));
    assert(data.uFlags==NIF_INFO); // Reconciliation doesn't mutate the input.

    calls.clear();
    auto racing=[&](DWORD op,NOTIFYICONDATAW*) {
        calls.push_back(op);return calls.size()==3?TRUE:FALSE;
    };
    assert(reconcile_tray_icon(data,racing));
    assert((calls==std::vector<DWORD>{NIM_MODIFY,NIM_ADD,NIM_MODIFY}));

    TrayImage image;
    int fallback_calls=0;
    auto fallback=[&]{++fallback_calls;return shared;};
    image.update(nullptr,[]{return static_cast<HICON>(nullptr);});
    assert(image.pending() && !image.get());
    image.update(nullptr,fallback);
    assert(image.pending() && image.get()==shared && fallback_calls==1);
    image.update(nullptr,fallback);
    assert(image.pending() && image.get()==shared && fallback_calls==1);
    HICON generated=CopyIcon(shared);assert(generated);
    image.update(generated,fallback);
    assert(!image.pending() && image.get()==generated);
    image.update(nullptr,fallback);
    assert(image.pending() && image.get()==generated && fallback_calls==1);
    image.reset();
    // Shared fallback survives cleanup and repeated failed generations.
    ICONINFO info{};assert(GetIconInfo(shared,&info));
    DeleteObject(info.hbmColor);DeleteObject(info.hbmMask);
    const DWORD before=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
    for(int i=0;i<200;++i) {
        image.update(nullptr,fallback);
        image.update(CopyIcon(shared),fallback);
        image.update(nullptr,fallback);
        image.reset();
    }
    assert(GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS)==before);
    std::puts("Tray registration: missing/null icon, unavailable Shell, lost registration, duplicate event, race, fallback ownership and handle cleanup passed.");
}

#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include "tray_artwork.h"

inline bool light_taskbar() noexcept {
    DWORD value=1,bytes=sizeof(value);
    RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"SystemUsesLightTheme",RRF_RT_REG_DWORD,nullptr,&value,&bytes);
    return value!=0;
}

inline int taskbar_icon_size() noexcept {
    HWND taskbar=FindWindowW(L"Shell_TrayWnd",nullptr);
    UINT dpi=taskbar?GetDpiForWindow(taskbar):96;
    return GetSystemMetricsForDpi(SM_CXSMICON,dpi?dpi:96);
}

inline HICON make_mode_tray_icon(int mode,bool awake,int requested_size=32) noexcept {
    // Monochrome artwork: leaf, leaf/bolt, centered dial, lightning.
    // Embedded coverage masks avoid emoji/font fallback at runtime.
    const TrayArtwork* artwork=&kTrayArtwork[0];
    for(const auto& candidate:kTrayArtwork) {
        artwork=&candidate;
        if(candidate.size>=requested_size) break;
    }
    const int size=artwork->size;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=size;info.bmiHeader.biHeight=-size;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;
    HDC dc=CreateCompatibleDC(nullptr);
    if(!dc) return nullptr;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bitmap || !bits) {if(bitmap)DeleteObject(bitmap);DeleteDC(dc);return nullptr;}
    auto* pixels=static_cast<std::uint32_t*>(bits);
    std::fill(pixels,pixels+size*size,0);
    const auto* coverage=artwork->modes[std::clamp(mode,0,3)];
    const DWORD channel=light_taskbar()?28:245;
    for(int i=0;i<size*size;++i) {
        DWORD alpha=coverage[i];
        DWORD c=channel*alpha/255;
        pixels[i]=(alpha<<24)|(c<<16)|(c<<8)|c;
    }
    if(awake) { // Small status dot, independent of mode silhouette.
        const int radius=std::max(1,size*3/32),center=size-radius-2;
        for(int y=center-radius;y<=center+radius;++y) for(int x=center-radius;x<=center+radius;++x) {
            if((x-center)*(x-center)+(y-center)*(y-center)<=radius*radius)
                pixels[y*size+x]=0xFF000000|(channel<<16)|(channel<<8)|channel;
        }
    }
    DeleteDC(dc);
    // Monochrome bitmap rows are WORD-aligned, including 20/28px icons.
    BYTE mask_bits[((64+15)/16)*2*64]{};
    HBITMAP mask=CreateBitmap(size,size,1,1,mask_bits);
    ICONINFO icon_info{};icon_info.fIcon=TRUE;icon_info.hbmColor=bitmap;icon_info.hbmMask=mask;
    HICON icon=mask?CreateIconIndirect(&icon_info):nullptr;
    DeleteObject(bitmap);if(mask)DeleteObject(mask);
    return icon;
}

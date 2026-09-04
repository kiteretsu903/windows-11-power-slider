#include "renderer.h"
#include "appearance.h"
#include "diagnostics.h"
#include <algorithm>
#include <cmath>
#include <wincodec.h>

namespace {
template<class T> void release(T*& p) { if(p) { p->Release(); p=nullptr; } }
D2D1_COLOR_F rgba(float r,float g,float b,float a=1) { return D2D1::ColorF(r,g,b,a); }
}
Renderer::~Renderer() {
    discard_device_resources();
    release(title_format_); release(body_format_); release(small_format_);
    release(label_format_); release(icon_format_);
    release(dwrite_factory_); release(d2d_factory_);
}
bool Renderer::initialize(HWND window,UINT dpi) noexcept {
    window_=window; dpi_=dpi;
    if(FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&d2d_factory_))) return false;
    if(FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown**>(&dwrite_factory_)))) return false;
    create_text_formats();
    return true; // Allocate composition resources only while the flyout is open.
}
void Renderer::create_text_formats() noexcept {
    // Explicit Chinese font avoids the heavy display-font fallback of the old UI.
    auto make=[&](float size,DWRITE_FONT_WEIGHT weight,IDWriteTextFormat** out) {
        dwrite_factory_->CreateTextFormat(L"Microsoft YaHei UI",nullptr,weight,
            DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"zh-CN",out);
        if(*out) { (*out)->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            (*out)->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
    };
    make(16,DWRITE_FONT_WEIGHT_SEMI_BOLD,&title_format_);
    make(13,DWRITE_FONT_WEIGHT_NORMAL,&body_format_);
    make(11,DWRITE_FONT_WEIGHT_NORMAL,&small_format_);
    make(10,DWRITE_FONT_WEIGHT_NORMAL,&label_format_);
}
bool Renderer::create_device_resources() noexcept {
    if(target_) return true;
    if(!d2d_factory_ || !window_) return false;
    IDXGIDevice* dxgi=nullptr; IDXGIAdapter* adapter=nullptr; IDXGIFactory2* factory=nullptr;
    IDXGISurface* surface=nullptr; ID2D1Bitmap1* bitmap=nullptr;
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&gpu_,nullptr,nullptr);
    if(FAILED(hr)) hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&gpu_,nullptr,nullptr);
    if(SUCCEEDED(hr)) hr=gpu_->QueryInterface(__uuidof(IDXGIDevice),reinterpret_cast<void**>(&dxgi));
    if(SUCCEEDED(hr)) hr=d2d_factory_->CreateDevice(dxgi,&device_);
    if(SUCCEEDED(hr)) hr=device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,&target_);
    if(SUCCEEDED(hr)) hr=dxgi->GetAdapter(&adapter);
    if(SUCCEEDED(hr)) hr=adapter->GetParent(__uuidof(IDXGIFactory2),reinterpret_cast<void**>(&factory));
    if(SUCCEEDED(hr)) {
        const HMONITOR monitor=MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST);
        // The default render adapter may not own the selected monitor (hybrid
        // graphics). Enumerate outputs independently to pace the right screen.
        for(UINT a=0;!animation_output_;++a) {
            IDXGIAdapter* candidate=nullptr;
            if(factory->EnumAdapters(a,&candidate)!=S_OK) break;
            for(UINT o=0;!animation_output_;++o) {
                IDXGIOutput* output=nullptr;
                if(candidate->EnumOutputs(o,&output)!=S_OK) break;
                DXGI_OUTPUT_DESC output_desc{};
                if(SUCCEEDED(output->GetDesc(&output_desc)) && output_desc.Monitor==monitor)
                    animation_output_=output;
                else output->Release();
            }
            candidate->Release();
        }
    }
    RECT r{}; GetClientRect(window_,&r);
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width=std::max(1L,r.right); desc.Height=std::max(1L,r.bottom);
    desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM; desc.SampleDesc.Count=1;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount=2;
    desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;
    if(SUCCEEDED(hr)) hr=factory->CreateSwapChainForComposition(gpu_,&desc,nullptr,&swap_);
    if(SUCCEEDED(hr)) hr=swap_->GetBuffer(0,__uuidof(IDXGISurface),reinterpret_cast<void**>(&surface));
    auto props=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(desc.Format,D2D1_ALPHA_MODE_PREMULTIPLIED),float(dpi_),float(dpi_));
    if(SUCCEEDED(hr)) hr=target_->CreateBitmapFromDxgiSurface(surface,&props,&bitmap);
    if(SUCCEEDED(hr)) { target_->SetTarget(bitmap); target_->SetDpi(float(dpi_),float(dpi_));
        target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE); }
    if(SUCCEEDED(hr)) hr=DCompositionCreateDevice(dxgi,__uuidof(IDCompositionDevice),reinterpret_cast<void**>(&composition_));
    if(SUCCEEDED(hr)) hr=composition_->CreateTargetForHwnd(window_,TRUE,&composition_target_);
    if(SUCCEEDED(hr)) hr=composition_->CreateVisual(&visual_);
    if(SUCCEEDED(hr)) hr=visual_->SetContent(swap_);
    if(SUCCEEDED(hr)) hr=composition_target_->SetRoot(visual_);
    // Do not submit an empty visual before the first full frame is painted.
    if(SUCCEEDED(hr)) hr=target_->CreateSolidColorBrush(rgba(1,1,1),&brush_);
    release(bitmap); release(surface); release(factory); release(adapter); release(dxgi);
    if(FAILED(hr)) discard_device_resources();
    trace_event("create-hr",hr,dpi_);
    return SUCCEEDED(hr);
}
void Renderer::resize(UINT width,UINT height,UINT dpi) noexcept {
    if(swap_ && dpi_==dpi) {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        if(SUCCEEDED(swap_->GetDesc1(&desc)) && desc.Width==width && desc.Height==height) return;
    }
    dpi_=dpi; discard_device_resources();
}
bool Renderer::wait_for_first_frame() noexcept {
    if(!composition_) return false;
    const HRESULT hr=composition_->WaitForCommitCompletion();
    trace_event("first-frame-ready",hr);
    return SUCCEEDED(hr);
}
bool Renderer::wait_for_animation_frame() noexcept {
    // DwmFlush can return immediately when no DirectX work is queued. Waiting
    // on this monitor's vblank provides real pacing even for HWND-only moves.
    return animation_output_ && SUCCEEDED(animation_output_->WaitForVBlank());
}
void Renderer::discard_device_resources() noexcept {
    trace_event("discard",target_!=nullptr);
    for(auto& icon:icons_) release(icon);
    if(composition_target_) composition_target_->SetRoot(nullptr);
    if(composition_) composition_->Commit();
    release(brush_); release(target_); release(visual_); release(composition_target_);
    release(composition_); release(swap_); release(device_); release(gpu_);
    release(animation_output_);
}
void Renderer::set_color(const D2D1_COLOR_F& c) noexcept { brush_->SetColor(c); }
void Renderer::text(const wchar_t* s,D2D1_RECT_F r,IDWriteTextFormat* f,const D2D1_COLOR_F& c) noexcept {
    if(!f) return; set_color(c);
    target_->DrawTextW(s,UINT32(wcslen(s)),f,r,brush_,D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
void Renderer::rounded_fill(D2D1_RECT_F r,float radius,const D2D1_COLOR_F& c) noexcept {
    set_color(c); target_->FillRoundedRectangle(D2D1::RoundedRect(r,radius,radius),brush_);
}
void Renderer::rounded_stroke(D2D1_RECT_F r,float radius,const D2D1_COLOR_F& c,float w) noexcept {
    set_color(c); target_->DrawRoundedRectangle(D2D1::RoundedRect(r,radius,radius),brush_,w);
}
void Renderer::draw_power_icon(float x,float y,const D2D1_COLOR_F& c) noexcept {
    (void)c; draw_asset(0,x,y);
}
void Renderer::draw_battery_icon(float x,float y,const D2D1_COLOR_F& c) noexcept {
    (void)c; draw_asset(1,x,y);
}
void Renderer::draw_coffee_icon(float x,float y,const D2D1_COLOR_F& c) noexcept {
    (void)c; draw_asset(2,x,y);
}
void Renderer::draw_asset(int index,float x,float y) noexcept {
    if(!icons_[index]) {
        auto module=GetModuleHandleW(nullptr);
        auto resource=FindResourceW(module,MAKEINTRESOURCEW(201+index),RT_RCDATA);
        if(!resource) return;
        auto data=LoadResource(module,resource);
        IWICImagingFactory* factory=nullptr; IWICStream* stream=nullptr;
        IWICBitmapDecoder* decoder=nullptr; IWICBitmapFrameDecode* frame=nullptr;
        IWICFormatConverter* converter=nullptr;
        HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,
            __uuidof(IWICImagingFactory),reinterpret_cast<void**>(&factory));
        if(SUCCEEDED(hr)) hr=factory->CreateStream(&stream);
        if(SUCCEEDED(hr)) hr=stream->InitializeFromMemory(static_cast<BYTE*>(LockResource(data)),SizeofResource(module,resource));
        if(SUCCEEDED(hr)) hr=factory->CreateDecoderFromStream(stream,nullptr,WICDecodeMetadataCacheOnLoad,&decoder);
        if(SUCCEEDED(hr)) hr=decoder->GetFrame(0,&frame);
        if(SUCCEEDED(hr)) hr=factory->CreateFormatConverter(&converter);
        if(SUCCEEDED(hr)) hr=converter->Initialize(frame,GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
        if(SUCCEEDED(hr)) target_->CreateBitmapFromWicBitmap(converter,nullptr,&icons_[index]);
        release(converter);release(frame);release(decoder);release(stream);release(factory);
    }
    if(icons_[index]) target_->DrawBitmap(icons_[index],D2D1::RectF(x-5,y-5,x+29,y+29),1,D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
}
void Renderer::draw_slider(float y,int n,int selected,const wchar_t* const* labels,
    const D2D1_COLOR_F& primary,const D2D1_COLOR_F& secondary,const D2D1_COLOR_F& accent) noexcept {
    float left=40,right=352,step=(right-left)/(n-1),sx=left+step*std::clamp(selected,0,n-1);
    rounded_fill(D2D1::RectF(left,y-2,right,y+2),2,secondary);
    rounded_fill(D2D1::RectF(left,y-2,sx,y+2),2,accent);
    set_color(rgba(0,0,0,.12f)); target_->FillEllipse(D2D1::Ellipse({sx,y+1},10.5f,10.5f),brush_);
    set_color(rgba(1,1,1)); target_->FillEllipse(D2D1::Ellipse({sx,y},10,10),brush_);
    set_color(accent); target_->FillEllipse(D2D1::Ellipse({sx,y},5,5),brush_);
    for(int i=0;i<n;++i) {
        float x=left+step*i;
        label_format_->SetTextAlignment(i==0?DWRITE_TEXT_ALIGNMENT_LEADING:i==n-1?DWRITE_TEXT_ALIGNMENT_TRAILING:DWRITE_TEXT_ALIGNMENT_CENTER);
        text(labels[i],D2D1::RectF(i==0?28:x-48,y+14,i==n-1?364:x+48,y+33),label_format_,primary);
    }
}
bool Renderer::draw(const RenderState& s) noexcept {
    if(!create_device_resources()) return false;
    auto primary=s.light_theme?rgba(.12f,.12f,.13f):rgba(.96f,.96f,.97f);
    auto secondary=s.light_theme?rgba(.42f,.42f,.44f):rgba(.63f,.63f,.66f);
    auto accent=s.light_theme?rgba(0,.40f,.75f):rgba(.40f,.76f,1);
    auto card=s.light_theme?rgba(1,1,1,.22f):rgba(1,1,1,.035f);
    auto border=s.light_theme?rgba(0,0,0,.055f):rgba(1,1,1,.075f);
    const float opacity=s.acrylic?kBackgroundOpacity:1.0f;
    target_->BeginDraw();
    target_->Clear(s.light_theme?rgba(.95f,.95f,.95f,opacity):rgba(.12f,.12f,.12f,opacity));
    body_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    small_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    text(s.chinese?L"电源模式":L"Power mode",{24,14,290,45},title_format_,primary);
    const wchar_t* zh[]={L"省电模式",L"最佳能效",L"平衡",L"最佳性能"};
    const wchar_t* en[]={L"Saver",L"Efficiency",L"Balanced",L"Performance"};
    auto names=s.chinese?zh:en;
    for(int i=0;i<2;++i) {
        float top=i?182.f:58.f; bool active=i?s.battery_active:s.ac_active;
        rounded_fill({16,top,376,top+112},8,card);
        rounded_stroke({16,top,376,top+112},8,border);
        if(i) draw_battery_icon(29,top+16,active?accent:secondary);
        else draw_power_icon(29,top+16,active?accent:secondary);
        text(i?(s.chinese?L"电池":L"On battery"):(s.chinese?L"插电":L"Plugged in"),
             {62,top+12,182,top+42},body_format_,primary);
        int pos=(i?s.battery_position:s.ac_position)+1;
        if(i && s.battery_active && s.energy_saver_active) pos=0;
        text(names[std::clamp(pos,0,3)],{182,top+12,354,top+42},small_format_,active?accent:secondary);
        auto rail=s.light_theme?rgba(0,0,0,.30f):rgba(1,1,1,.38f);
        draw_slider(top+62,3,i?s.battery_position:s.ac_position,names+1,secondary,rail,accent);
    }
    rounded_fill({16,306,376,354},8,s.hover==2?border:card);
    draw_coffee_icon(29,318,primary);
    text(s.chinese?L"保持唤醒":L"Keep awake",{62,308,270,352},body_format_,primary);
    rounded_fill({312,320,352,340},10,s.keep_awake?accent:border);
    rounded_stroke({312,320,352,340},10,s.keep_awake?accent:secondary,1);
    set_color(s.keep_awake?rgba(1,1,1):secondary);
    target_->FillEllipse(D2D1::Ellipse({s.keep_awake?342.f:322.f,330},6,6),brush_);
    HRESULT hr=target_->EndDraw();
    if(SUCCEEDED(hr)) hr=swap_->Present(1,0);
    if(SUCCEEDED(hr)) hr=composition_->Commit();
    trace_event("present-hr",hr,s.acrylic);
    if(FAILED(hr)) discard_device_resources();
    return SUCCEEDED(hr);
}

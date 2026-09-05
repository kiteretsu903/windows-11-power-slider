#pragma once

#include <windows.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <dwrite.h>

struct RenderState {
    int ac_position{1};
    int battery_position{1};
    bool ac_active{};
    bool battery_active{};
    bool energy_saver_active{};
    bool keep_awake{};
    bool light_theme{};
    bool acrylic{};
    bool chinese{true};
    int hover{-1};
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool initialize(HWND window, UINT dpi) noexcept;
    bool factories_ready() const noexcept { return d2d_factory_ && dwrite_factory_ && title_format_ && body_format_ && small_format_ && label_format_; }
    void resize(UINT width, UINT height, UINT dpi) noexcept;
    void discard_device_resources() noexcept;
    bool draw(const RenderState& state) noexcept;
    bool wait_for_first_frame() noexcept;
    bool wait_for_animation_frame() noexcept;

private:
    bool create_device_resources() noexcept;
    bool ensure_factories() noexcept;
    bool create_text_formats() noexcept;
    void draw_asset(int index,float x,float y) noexcept;
    ID2D1Bitmap* icons_[3]{};
    void set_color(const D2D1_COLOR_F& color) noexcept;
    void text(const wchar_t* value, D2D1_RECT_F rect, IDWriteTextFormat* format,
              const D2D1_COLOR_F& color) noexcept;
    void rounded_fill(D2D1_RECT_F rect, float radius, const D2D1_COLOR_F& color) noexcept;
    void rounded_stroke(D2D1_RECT_F rect, float radius, const D2D1_COLOR_F& color,
                        float width = 1.0f) noexcept;
    void draw_slider(float y, int positions, int selected, const wchar_t* const* labels,
                     const D2D1_COLOR_F& primary, const D2D1_COLOR_F& secondary,
                     const D2D1_COLOR_F& accent) noexcept;
    void draw_power_icon(float x, float y, const D2D1_COLOR_F& color) noexcept;
    void draw_battery_icon(float x, float y, const D2D1_COLOR_F& color) noexcept;
    void draw_coffee_icon(float x, float y, const D2D1_COLOR_F& color) noexcept;

    HWND window_{};
    UINT dpi_{96};
    ID2D1Factory1* d2d_factory_{};
    IDWriteFactory* dwrite_factory_{};
    ID2D1DeviceContext* target_{};
    ID3D11Device* gpu_{};
    ID2D1Device* device_{};
    IDXGISwapChain1* swap_{};
    IDXGIOutput* animation_output_{};
    IDCompositionDevice* composition_{};
    IDCompositionTarget* composition_target_{};
    IDCompositionVisual* visual_{};
    ID2D1SolidColorBrush* brush_{};
    IDWriteTextFormat* title_format_{};
    IDWriteTextFormat* body_format_{};
    IDWriteTextFormat* small_format_{};
    IDWriteTextFormat* label_format_{};
    IDWriteTextFormat* icon_format_{};
};

#pragma once
#include "core/com_ptr.hpp"
#include "telemetry/system_monitor.hpp"
#include "media/media_service.hpp"
#include <d2d1_1.h>
#include <d2d1_3.h>
#include <d2d1svg.h>
#include "ui/view_model.hpp"
#include "ui/icons.hpp"
#include <map>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>
#include <string>
#include <vector>

namespace ni {
class DCompRenderer {
public:
  ~DCompRenderer(){discard_device();}
  HANDLE frame_ready()const{return frame_ready_;}
  unsigned allocations()const{return allocations_;}
  HRESULT initialize(HWND window, UINT width, UINT height, float dpi);
  HRESULT resize(UINT width, UINT height, float dpi);
  HRESULT render(const SystemSnapshot& system, const MediaSnapshot& media,
                 const ViewModel& view,bool frame_ready=false);
  const std::vector<HitTarget>& hit_targets() const { return hits_; }
  void capture_next(const std::wstring& path){capture_path_=path;}
  void discard_device();
private:
  HRESULT create_device_resources();
  HRESULT create_size_resources(UINT width, UINT height);
  HRESULT capture();
  std::wstring capture_path_;
  void draw_text(const std::wstring& text, const D2D1_RECT_F& rect, float size,
                 D2D1_COLOR_F color, DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING);
  void box(D2D1_RECT_F rect, float radius, unsigned color, float alpha = 1, bool outline = false);
  void icon(Icon icon, float x, float y, float size, unsigned color);
  void button(D2D1_RECT_F rect, Icon icon, Action action, size_t index = 0, bool enabled = true);
  void navigation(float width);
  void widgets(const SystemSnapshot& system, float width);
  void media_page(const MediaSnapshot& media, float width);
  void audio_controls(float width);
  void files_page(float width);
  void clipboard_page(float width);
  void applications_page(float width);
  void reminders_page(float width);
  void tasks_page(float width);
  ComPtr<ID2D1Geometry> island_geometry(float w,float h,float radius,bool droplet);
  const ViewModel* view_{};
  std::vector<HitTarget> hits_;
  ComPtr<ID2D1SolidColorBrush> brush_;
  ComPtr<ID2D1DeviceContext5> svg_context_;
  std::map<std::pair<int,unsigned>, ComPtr<ID2D1SvgDocument>> icons_;
  unsigned cached_accent_{},cached_background_{};
  std::map<int, ComPtr<IDWriteTextFormat>> formats_;
  ComPtr<ID2D1Bitmap1> artwork_;
  std::map<std::wstring,ComPtr<ID2D1Bitmap1>> app_icons_;
  std::map<std::wstring,ComPtr<ID2D1Bitmap1>> file_icons_;
  std::map<uint64_t,ComPtr<ID2D1Bitmap1>> clip_images_;
  std::uint64_t artwork_version_{};
  HWND window_{}; UINT width_{}; UINT height_{}; float dpi_{96.0f};
  UINT capacity_width_{},capacity_height_{};
  HANDLE frame_ready_{};unsigned allocations_{};UINT swap_flags_{};
  float motion_x_{};
  ComPtr<ID3D11Device> d3d_; ComPtr<IDXGIDevice> dxgi_device_; ComPtr<IDXGISwapChain1> swap_chain_;
  ComPtr<ID2D1Factory1> d2d_factory_; ComPtr<ID2D1Device> d2d_device_; ComPtr<ID2D1DeviceContext> context_;
  ComPtr<ID2D1Bitmap1> target_bitmap_; ComPtr<IDWriteFactory> dwrite_;
  ComPtr<IDCompositionDevice> dcomp_; ComPtr<IDCompositionTarget> dcomp_target_; ComPtr<IDCompositionVisual> visual_;
  ComPtr<IDCompositionEffectGroup> opacity_effect_;
};
}


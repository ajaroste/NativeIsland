#include "rendering/dcomp_renderer.hpp"
#include <shlwapi.h>
#include <format>
#include <algorithm>
#include <wincodec.h>
#include <cmath>
#include "ui/notification_motion.hpp"
#include "ui/notification_layout.hpp"

namespace ni {
HRESULT DCompRenderer::initialize(HWND window,UINT width,UINT height,float dpi) {
  window_=window; width_=width; height_=height; dpi_=dpi;
  HRESULT hr=create_device_resources(); return SUCCEEDED(hr)?resize(width,height,dpi):hr;
}
HRESULT DCompRenderer::create_device_resources() {
  D3D_FEATURE_LEVEL level{};
  HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,d3d_.put(),&level,nullptr);
  if(FAILED(hr)) hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,d3d_.put(),&level,nullptr);
  if(FAILED(hr)||FAILED(hr=d3d_.as(dxgi_device_))) return hr;
  D2D1_FACTORY_OPTIONS options{};
  if(FAILED(hr=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,options,d2d_factory_.put())))return hr;
  if(FAILED(hr=d2d_factory_->CreateDevice(dxgi_device_.get(),d2d_device_.put())))return hr;
  if(FAILED(hr=d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,context_.put())))return hr;
  context_.as(svg_context_);
  if(FAILED(hr=context_->CreateSolidColorBrush(D2D1::ColorF(0),brush_.put())))return hr;
  if(FAILED(hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(dwrite_.put()))))return hr;
  if(FAILED(hr=DCompositionCreateDevice(dxgi_device_.get(),__uuidof(IDCompositionDevice),reinterpret_cast<void**>(dcomp_.put()))))return hr;
  if(FAILED(hr=dcomp_->CreateTargetForHwnd(window_,TRUE,dcomp_target_.put())))return hr;
  if(FAILED(hr=dcomp_->CreateVisual(visual_.put())))return hr;
  if(FAILED(hr=dcomp_->CreateEffectGroup(opacity_effect_.put())))return hr;
  if(FAILED(hr=visual_->SetEffect(opacity_effect_.get())))return hr;
  return dcomp_target_->SetRoot(visual_.get());
}
HRESULT DCompRenderer::create_size_resources(UINT width,UINT height) {
  if(!context_||!width||!height)return S_FALSE;
  context_->SetTarget(nullptr); target_bitmap_.reset(); HRESULT hr{};
  if(swap_chain_)hr=swap_chain_->ResizeBuffers(2,width,height,DXGI_FORMAT_B8G8R8A8_UNORM,swap_flags_);
  else {
    ComPtr<IDXGIAdapter> adapter; ComPtr<IDXGIFactory2> factory;
    if(FAILED(hr=dxgi_device_->GetAdapter(adapter.put())))return hr;
    if(FAILED(hr=adapter->GetParent(__uuidof(IDXGIFactory2),reinterpret_cast<void**>(factory.put()))))return hr;
    DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=width;desc.Height=height;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;
    desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;desc.AlphaMode=DXGI_ALPHA_MODE_PREMULTIPLIED;desc.Scaling=DXGI_SCALING_STRETCH;
    desc.Flags=DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    hr=factory->CreateSwapChainForComposition(d3d_.get(),&desc,nullptr,swap_chain_.put());
    if(FAILED(hr)){desc.Flags=0;hr=factory->CreateSwapChainForComposition(d3d_.get(),&desc,nullptr,swap_chain_.put());}
    swap_flags_=desc.Flags;
    if(SUCCEEDED(hr)&&swap_flags_){ComPtr<IDXGISwapChain2> chain;if(SUCCEEDED(swap_chain_.as(chain))){chain->SetMaximumFrameLatency(1);frame_ready_=chain->GetFrameLatencyWaitableObject();}}
    if(SUCCEEDED(hr))hr=visual_->SetContent(swap_chain_.get());
  }
  if(FAILED(hr))return hr;
  ComPtr<IDXGISurface> surface;
  if(FAILED(hr=swap_chain_->GetBuffer(0,__uuidof(IDXGISurface),reinterpret_cast<void**>(surface.put()))))return hr;
  auto props=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),dpi_,dpi_);
  if(FAILED(hr=context_->CreateBitmapFromDxgiSurface(surface.get(),&props,target_bitmap_.put())))return hr;
  capacity_width_=width;capacity_height_=height;++allocations_;
  context_->SetTarget(target_bitmap_.get());context_->SetDpi(dpi_,dpi_);return dcomp_->Commit();
}
HRESULT DCompRenderer::resize(UINT width,UINT height,float dpi) {
  if(width==width_&&height==height_&&dpi==dpi_&&target_bitmap_)return S_OK;
  const bool dpi_changed=dpi_!=dpi;width_=width;height_=height;dpi_=dpi;
  if(!target_bitmap_||dpi_changed||width>capacity_width_||height>capacity_height_){
    UINT reserve_w=std::max(width,static_cast<UINT>(std::ceil(720*dpi/96))),reserve_h=std::max(height,static_cast<UINT>(std::ceil(560*dpi/96)));
    HRESULT hr=create_size_resources(reserve_w,reserve_h);if(FAILED(hr))return hr;
  }
  // The backing texture stays allocated while the visible HWND grows/shrinks.
  return visual_->SetClip(D2D1::RectF(0,0,static_cast<float>(width),static_cast<float>(height)));
}
void DCompRenderer::draw_text(const std::wstring& text,const D2D1_RECT_F& rect,float size,D2D1_COLOR_F color,DWRITE_TEXT_ALIGNMENT align) {
  auto& format=formats_[static_cast<int>(size*10)];
  if(!format) {
    if(FAILED(dwrite_->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_SEMI_BOLD,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"tr-TR",format.put())))return;
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    ComPtr<IDWriteInlineObject> ellipsis;dwrite_->CreateEllipsisTrimmingSign(format.get(),ellipsis.put());
    DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};format->SetTrimming(&trim,ellipsis.get());
  }
  format->SetTextAlignment(align);brush_->SetColor(color);
  context_->DrawTextW(text.c_str(),static_cast<UINT32>(text.size()),format.get(),rect,brush_.get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
void DCompRenderer::draw_notice_preview(const std::wstring& text,const D2D1_RECT_F& rect,float size,D2D1_COLOR_F color){
  auto& format=notice_formats_[static_cast<int>(size*10)];
  if(!format){
    if(FAILED(dwrite_->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_MEDIUM,
      DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"tr-TR",format.put())))return;
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,size+6,size+1);
    ComPtr<IDWriteInlineObject> ellipsis;
    if(SUCCEEDED(dwrite_->CreateEllipsisTrimmingSign(format.get(),ellipsis.put()))){
      DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_WORD,0,0};
      format->SetTrimming(&trim,ellipsis.get());
    }
  }
  format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
  brush_->SetColor(color);
  context_->DrawTextW(text.c_str(),static_cast<UINT32>(text.size()),format.get(),rect,brush_.get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
void DCompRenderer::update_notice_image(const std::shared_ptr<const std::vector<std::uint8_t>>& bytes,std::shared_ptr<const std::vector<std::uint8_t>>& source,ComPtr<ID2D1Bitmap1>& bitmap){
  if(bytes==source)return;
  source=bytes;bitmap.reset();
  if(!context_||!bytes||bytes->empty()||bytes->size()>512*1024)return;
  if(bytes->size()>=12&&(*bytes)[0]=='N'&&(*bytes)[1]=='I'&&
     (*bytes)[2]=='R'&&(*bytes)[3]=='G'){
    auto read_u32=[&](std::size_t i)->UINT {
      return static_cast<UINT>((*bytes)[i])|(static_cast<UINT>((*bytes)[i+1])<<8)|
             (static_cast<UINT>((*bytes)[i+2])<<16)|(static_cast<UINT>((*bytes)[i+3])<<24);
    };
    const UINT w=read_u32(4),h=read_u32(8);
    if(w&&h&&w<=128&&h<=128&&bytes->size()==12+static_cast<std::size_t>(w)*h*4){
      auto props=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
      context_->CreateBitmap(D2D1::SizeU(w,h),bytes->data()+12,w*4,&props,bitmap.put());
    }
    return;
  }
  ComPtr<IStream> stream;
  stream.reset(SHCreateMemStream(reinterpret_cast<const BYTE*>(bytes->data()),static_cast<UINT>(bytes->size())));
  if(!stream)return;
  ComPtr<IWICImagingFactory> factory;
  ComPtr<IWICBitmapDecoder> decoder;
  ComPtr<IWICBitmapFrameDecode> frame;
  ComPtr<IWICFormatConverter> converter;
  if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,
     __uuidof(IWICImagingFactory),reinterpret_cast<void**>(factory.put())))||
     FAILED(factory->CreateDecoderFromStream(stream.get(),nullptr,WICDecodeMetadataCacheOnLoad,decoder.put()))||
     FAILED(decoder->GetFrame(0,frame.put())))return;
  UINT width{},height{};
  if(FAILED(frame->GetSize(&width,&height))||!width||!height||width>512||height>512)return;
  if(FAILED(factory->CreateFormatConverter(converter.put()))||
     FAILED(converter->Initialize(frame.get(),GUID_WICPixelFormat32bppPBGRA,
       WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return;
  context_->CreateBitmapFromWicBitmap(converter.get(),nullptr,bitmap.put());
}
void DCompRenderer::box(D2D1_RECT_F rect,float radius,unsigned color,float alpha,bool outline) {
  brush_->SetColor(D2D1::ColorF(color,alpha));auto rounded=D2D1::RoundedRect(rect,radius,radius);
  if(outline)context_->DrawRoundedRectangle(rounded,brush_.get(),1);else context_->FillRoundedRectangle(rounded,brush_.get());
}
void DCompRenderer::icon(Icon symbol,float x,float y,float size,unsigned color) {
  if(!svg_context_)return;
  auto key=std::make_pair(static_cast<int>(symbol),color);auto& svg=icons_[key];
  if(!svg) {
    auto source=std::format("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='#{:06x}' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'>{}</svg>",color,icon_shapes[key.first]);
    ComPtr<IStream> stream;stream.reset(SHCreateMemStream(reinterpret_cast<const BYTE*>(source.data()),static_cast<UINT>(source.size())));
    if(!stream||FAILED(svg_context_->CreateSvgDocument(stream.get(),D2D1::SizeF(size,size),svg.put())))return;
  }
  svg->SetViewportSize(D2D1::SizeF(size,size));context_->SetTransform(D2D1::Matrix3x2F::Translation(x+shadow_padding+motion_x_,y+shadow_padding));svg_context_->DrawSvgDocument(svg.get());context_->SetTransform(D2D1::Matrix3x2F::Translation(shadow_padding+motion_x_,shadow_padding));
}
ComPtr<ID2D1Geometry> DCompRenderer::island_geometry(float w,float h,float r,bool droplet){
  ComPtr<ID2D1Geometry> result;
  if(!droplet){ComPtr<ID2D1RoundedRectangleGeometry> rounded;d2d_factory_->CreateRoundedRectangleGeometry(D2D1::RoundedRect(D2D1::RectF(.5f,.5f,w-.5f,h-.5f),r,r),rounded.put());if(rounded)rounded->QueryInterface(__uuidof(ID2D1Geometry),reinterpret_cast<void**>(result.put()));return result;}
  ComPtr<ID2D1PathGeometry> path;ComPtr<ID2D1GeometrySink> sink;if(FAILED(d2d_factory_->CreatePathGeometry(path.put()))||FAILED(path->Open(sink.put())))return result;
  sink->BeginFigure(D2D1::Point2F(-16,0),D2D1_FIGURE_BEGIN_FILLED);
  sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(-7.164f,0),D2D1::Point2F(0,7.164f),D2D1::Point2F(0,16)));
  sink->AddLine(D2D1::Point2F(0,h-r));sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(r,h),D2D1::SizeF(r,r),0,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
  sink->AddLine(D2D1::Point2F(w-r,h));sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(w,h-r),D2D1::SizeF(r,r),0,D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
  sink->AddLine(D2D1::Point2F(w,16));sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(w,7.164f),D2D1::Point2F(w+7.164f,0),D2D1::Point2F(w+16,0)));sink->AddLine(D2D1::Point2F(-16,0));sink->EndFigure(D2D1_FIGURE_END_CLOSED);if(FAILED(sink->Close()))return result;path->QueryInterface(__uuidof(ID2D1Geometry),reinterpret_cast<void**>(result.put()));return result;
}
HRESULT DCompRenderer::render(const SystemSnapshot& system,const MediaSnapshot& media,const ViewModel& view,bool frame_ready) {
  if(!context_||!target_bitmap_)return S_FALSE;
  if(frame_ready_&&!frame_ready)WaitForSingleObjectEx(frame_ready_,50,FALSE);
  if(cached_accent_!=view.theme.accent||cached_background_!=view.theme.background){icons_.clear();cached_accent_=view.theme.accent;cached_background_=view.theme.background;}
  view_=&view;hits_.clear();context_->BeginDraw();context_->Clear(D2D1::ColorF(0,0.f));
  const bool windows_notice=view.notice_kind==8&&!view.notice.empty();
  update_notice_image(windows_notice?view.notice_logo:nullptr,notice_logo_source_,notice_logo_bitmap_);
  update_notice_image(windows_notice?view.notice_portrait:nullptr,notice_portrait_source_,notice_portrait_bitmap_);
  for(auto it=clip_images_.begin();it!=clip_images_.end();)if(std::none_of(view.clipboard.begin(),view.clipboard.end(),[&](auto& entry){return entry.id==it->first;}))it=clip_images_.erase(it);else ++it;
  motion_x_=view.notice.empty()?0:notification_shake(view.notice_kind,view.notice_age,view.reduced_motion);
  context_->SetTransform(D2D1::Matrix3x2F::Translation(shadow_padding+motion_x_,shadow_padding));
  const float w=width_*96.f/dpi_-2*shadow_padding,h=height_*96.f/dpi_-2*shadow_padding,progress=std::clamp(view.expansion,0.f,1.f);
  const float radius=h/2+(30-h/2)*progress;
  for(int spread=12;spread>0;--spread) {
    float s=static_cast<float>(spread);box(D2D1::RectF(-s,-s*.45f,w+s,h+s),radius+s,0,view.shadow*.014f);
  }
  opacity_effect_->SetOpacity(view.notice_kind==8&&!view.notice.empty()?1.f:view.opacity);
  auto shape=island_geometry(w,h,radius,view.island_shape==1);
  brush_->SetColor(D2D1::ColorF(view.theme.background));context_->FillGeometry(shape.get(),brush_.get());brush_->SetColor(D2D1::ColorF(view.theme.border));context_->DrawGeometry(shape.get(),brush_.get(),1);
  const float alpha=std::clamp(1-progress*3,0.f,1.f);
  if(alpha>0&&view.notice.empty()) {
    box(D2D1::RectF(13,7,36,30),12,view.theme.accent_soft,alpha);
    icon(!view.pending_today_reminders.empty()?Icon::bell:media.available?Icon::music:Icon::cpu,17,11,15,view.theme.accent);
    const auto label=!view.pending_today_reminders.empty()?std::format(L"Bugün {} · {}",view.pending_today_reminders.size(),view.pending_today_reminders.front().title):media.available?media.title:system.ready?std::format(L"CPU {:.0f}%  |  RAM {:.0f}%",system.cpu_percent,system.ram_total?100.0*system.ram_used/system.ram_total:0):L"Sistem ölçülüyor…";
    draw_text(label,D2D1::RectF(45,0,w-(system.bluetooth_audio.empty()?31.f:51.f),37),11,D2D1::ColorF(view.theme.text,alpha));
    if(!system.bluetooth_audio.empty())icon(Icon::bluetooth,w-49,12,13,view.theme.accent);
    if(media.available)for(int i=0;i<3;++i) {
      const float wave=3+static_cast<float>((std::sin(view.wave_phase*5+i*1.7f)+1)*4);
      const float half=1.5f+(wave-1.5f)*view.wave_mix;
      box(D2D1::RectF(w-27+i*6.f,18.5f-half,w-24+i*6.f,18.5f+half),1.5f,view.theme.accent,alpha);
    }
  }
  if(progress>.15f) {
    auto layer=D2D1::LayerParameters1();float reveal=std::clamp((progress-.15f)/.6f,0.f,1.f);layer.opacity=reveal*reveal*(3-2*reveal);context_->PushLayer(layer,nullptr);
    auto capsule_layer=D2D1::LayerParameters1();capsule_layer.geometricMask=shape.get();context_->PushLayer(capsule_layer,nullptr);
    // Keep page layout at its final width; reveal it through the animated mask.
    float content_width=std::max(w,view.content_width);
    navigation(content_width);auto page_layer=D2D1::LayerParameters1();page_layer.opacity=view.page_opacity;context_->PushLayer(page_layer,nullptr);
    switch(view.page) {case Page::widgets:widgets(system,content_width);break;case Page::media:media_page(media,content_width);break;case Page::sound:audio_controls(content_width);break;case Page::tasks:tasks_page(content_width);break;case Page::files:files_page(content_width);break;case Page::clipboard:clipboard_page(content_width);break;case Page::applications:applications_page(content_width);break;case Page::reminders:reminders_page(content_width);break;}
    if(!system.bluetooth_audio.empty()&&(view.page==Page::media||view.page==Page::widgets)) {
      float base=h-(view.notice.empty()?0.f:34.f);icon(Icon::bluetooth,20,base-24,15,view.theme.accent);
      draw_text(system.bluetooth_audio.front(),D2D1::RectF(44,base-28,w-18,base-5),11,D2D1::ColorF(view.theme.secondary));
    }
    context_->PopLayer();
    context_->PopLayer();context_->PopLayer();
  }
  if(!view.notice.empty()){
    // Full text opacity for Windows notifications throughout their visibility.
    float fade=view.notice_kind==8?1.f:(view.reduced_motion?1.f:std::clamp(std::min(view.notice_age/.14f,(2.6f-view.notice_age)/.25f),0.f,1.f));
    auto layer=D2D1::LayerParameters1();layer.opacity=fade;layer.geometricMask=shape.get();context_->PushLayer(layer,nullptr);
    float y=progress>.15f?h-32:1;
    const bool compact_windows=view.notice_kind==8&&progress<=.15f;
    if(!compact_windows){
      Icon symbol=view.notice_kind==7||view.notice_kind==8?Icon::bell:view.notice_kind==1||view.notice_kind==6?Icon::microphone:view.notice_kind==2?Icon::headphones:view.notice_kind==3?Icon::battery:view.notice_kind==5?Icon::settings:Icon::volume;
      if(progress>.15f)box(D2D1::RectF(16,y,w-16,y+27),9,view.theme.surface);
      float pulse=!view.reduced_motion&&(view.notice_kind==2||view.notice_kind==3||view.notice_kind==6||view.notice_kind==7)?2*std::sin(std::min(view.notice_age/.4f,1.f)*3.14159f):0;
      unsigned color=view.notice_kind==1?0xef4444:view.notice_kind==3?0xf5a524:view.theme.accent;
      if(view.notice_kind==8&&notice_logo_bitmap_){
        context_->DrawBitmap(notice_logo_bitmap_.get(),D2D1::RectF(20,y+4,41,y+25));
      }else icon(symbol,20-pulse/2,y+7-pulse/2,18+pulse,color);
      if(view.notice_kind==1){brush_->SetColor(D2D1::ColorF(color));context_->DrawLine(D2D1::Point2F(19,y+6),D2D1::Point2F(38,y+25),brush_.get(),2);}
    }
    if(compact_windows){
      // The app logo belongs beside its name. Only a distinct portrait gets a large circle.
      const auto& source=view.notice_app.empty()?std::wstring(L"Windows"):view.notice_app;
      const auto image_layout=notice_image_layout(static_cast<bool>(notice_portrait_bitmap_),static_cast<bool>(notice_logo_bitmap_));
      if(notice_logo_bitmap_)context_->DrawBitmap(notice_logo_bitmap_.get(),D2D1::RectF(image_layout.source_left-22,14,image_layout.source_left-6,30),1.f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
      if(image_layout.portrait_visible){
        const float avatar_y=std::max(38.f,h/2-9.f);
        const auto avatar_rect=D2D1::RectF(18,avatar_y,58,avatar_y+40);
        ComPtr<ID2D1EllipseGeometry> mask;
        d2d_factory_->CreateEllipseGeometry(D2D1::Ellipse(D2D1::Point2F(38,avatar_y+20),20,20),mask.put());
        if(mask){auto clip=D2D1::LayerParameters1();clip.geometricMask=mask.get();context_->PushLayer(clip,nullptr);}
        const auto size=notice_portrait_bitmap_->GetSize();const float side=std::min(size.width,size.height);
        const auto crop=D2D1::RectF((size.width-side)/2,(size.height-side)/2,(size.width+side)/2,(size.height+side)/2);
        if(mask)context_->DrawBitmap(notice_portrait_bitmap_.get(),avatar_rect,1.f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,crop);
        if(mask)context_->PopLayer();
      }
      const float x=image_layout.text_left;
      draw_text(source,D2D1::RectF(image_layout.source_left,12,w-34,32),11,D2D1::ColorF(view.theme.secondary));
      constexpr float title_y=36.f;
      draw_text(view.notice_title.empty()?L"Yeni bildirim":view.notice_title,
                D2D1::RectF(x,title_y,w-34,title_y+24),14,D2D1::ColorF(view.theme.text));
      if(!view.notice_body.empty()){
        constexpr float body_y=64.f;
        const float body_bottom=std::min(h-12.f,body_y+(h>=140.f?60.f:(h>=112.f?41.f:22.f)));
        draw_notice_preview(view.notice_body,D2D1::RectF(x,body_y,w-34,body_bottom),
                            13,D2D1::ColorF(view.theme.text,.9f));
      }
    }else if(view.notice_kind==8){
      const auto& source=view.notice_app.empty()?std::wstring(L"Windows"):view.notice_app;
      draw_text(source+L" · "+(view.notice_title.empty()?L"Yeni bildirim":view.notice_title),D2D1::RectF(46,y,w-18,y+30),11,D2D1::ColorF(view.theme.text));
    }else draw_text(view.notice,D2D1::RectF(46,y,w-18,y+30),11,D2D1::ColorF(view.theme.text));
    if(view.notice_kind==0)box(D2D1::RectF(47,y+31,47+(w-65)*view.notice_value,y+33),1,view.theme.accent);
    context_->PopLayer();
  }
  HRESULT hr=context_->EndDraw();if(SUCCEEDED(hr)&&!capture_path_.empty())hr=capture();if(SUCCEEDED(hr))hr=swap_chain_->Present(1,0);if(SUCCEEDED(hr))hr=dcomp_->Commit();view_=nullptr;return hr;
}
HRESULT DCompRenderer::capture() {
  ComPtr<ID2D1Bitmap1> copy;auto props=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
  HRESULT hr=context_->CreateBitmap(D2D1::SizeU(width_,height_),nullptr,0,&props,copy.put());if(FAILED(hr))return hr;
  auto region=D2D1::RectU(0,0,width_,height_);if(FAILED(hr=copy->CopyFromBitmap(nullptr,target_bitmap_.get(),&region)))return hr;
  D2D1_MAPPED_RECT mapped{};if(FAILED(hr=copy->Map(D2D1_MAP_OPTIONS_READ,&mapped)))return hr;
  ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
  hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IWICImagingFactory),reinterpret_cast<void**>(factory.put()));
  if(SUCCEEDED(hr))hr=factory->CreateStream(stream.put());
  if(SUCCEEDED(hr))hr=stream->InitializeFromFilename(capture_path_.c_str(),GENERIC_WRITE);
  if(SUCCEEDED(hr))hr=factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,encoder.put());
  if(SUCCEEDED(hr))hr=encoder->Initialize(stream.get(),WICBitmapEncoderNoCache);
  if(SUCCEEDED(hr))hr=encoder->CreateNewFrame(frame.put(),nullptr);
  if(SUCCEEDED(hr))hr=frame->Initialize(nullptr);
  if(SUCCEEDED(hr))hr=frame->SetSize(width_,height_);
  WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
  if(SUCCEEDED(hr))hr=frame->SetPixelFormat(&format);
  if(SUCCEEDED(hr))hr=frame->WritePixels(height_,mapped.pitch,mapped.pitch*height_,mapped.bits);
  if(SUCCEEDED(hr))hr=frame->Commit();if(SUCCEEDED(hr))hr=encoder->Commit();
  copy->Unmap();capture_path_.clear();return hr;
}
void DCompRenderer::discard_device() {
  if(frame_ready_){CloseHandle(frame_ready_);frame_ready_=nullptr;}capacity_width_=capacity_height_=0;swap_flags_=0;
  app_icons_.clear();file_icons_.clear();clip_images_.clear();opacity_effect_.reset();artwork_.reset();artwork_version_=0;notice_logo_bitmap_.reset();notice_logo_source_.reset();notice_portrait_bitmap_.reset();notice_portrait_source_.reset();formats_.clear();notice_formats_.clear();icons_.clear();svg_context_.reset();brush_.reset();
  if(context_)context_->SetTarget(nullptr);target_bitmap_.reset();context_.reset();swap_chain_.reset();visual_.reset();dcomp_target_.reset();dcomp_.reset();
  d2d_device_.reset();d2d_factory_.reset();dwrite_.reset();dxgi_device_.reset();d3d_.reset();
}
}

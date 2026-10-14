#include "rendering/dcomp_renderer.hpp"
#include <filesystem>
#include <format>
#include <algorithm>
#include <cwctype>
#include <cmath>
#include "services/clipboard_history.hpp"
namespace ni {
void DCompRenderer::button(D2D1_RECT_F r,Icon symbol,Action action,size_t index,bool enabled) {
  if(static_cast<int>(hits_.size())==view_->hovered && enabled)box(r,9,view_->theme.elevated);
  icon(symbol,(r.left+r.right)/2-9,(r.top+r.bottom)/2-9,18,enabled?view_->theme.text:view_->theme.secondary);
  hits_.push_back({r,action,index,enabled});
}
void DCompRenderer::navigation(float w) {
  const bool compact=w<540;
  constexpr const wchar_t* names[]{L"Widget",L"Dosya",L"Medya",L"Pano",L"Uygulamalar"};
  constexpr Icon symbols[]{Icon::grid,Icon::files,Icon::music,Icon::clipboard,Icon::grid};
  const float extra=std::clamp((w-465)/5,0.f,12.f);
  const float sizes[]{compact?60.f+extra:66.f,compact?52.f+extra:62.f,compact?54.f+extra:65.f,compact?48.f+extra:55.f,compact?78.f+extra:95.f};float x=10;
  for(size_t i=0;i<5;++i) {
    auto r=D2D1::RectF(x,13,x+sizes[i],46);bool selected=static_cast<int>(view_->page)==static_cast<int>(i);
    if(selected||static_cast<int>(hits_.size())==view_->hovered)box(r,10,view_->theme.elevated);
    icon(symbols[i],x+(compact?4.f:9.f),compact?22.f:21.f,compact?14.f:16.f,selected?view_->theme.accent:view_->theme.secondary);
    draw_text(names[i],D2D1::RectF(x+(compact?22:28),13,x+sizes[i]-(compact?2:4),46),9.f,D2D1::ColorF(selected?view_->theme.text:view_->theme.secondary));
    hits_.push_back({r,Action::tab,i});x+=sizes[i]+3;
  }
  auto bell=D2D1::RectF(w-148,13,w-116,46);
  if(view_->page==Page::reminders)box(bell,10,view_->theme.elevated);
  button(bell,Icon::bell,Action::tab,5);
  if(view_->has_today_reminders)box(D2D1::RectF(w-126,16,w-120,22),3,view_->theme.accent);
  auto tasks=D2D1::RectF(w-113,13,w-83,46);if(view_->page==Page::tasks)box(tasks,10,view_->theme.elevated);button(tasks,Icon::tasks,Action::tab,7);
  if(tasks_connected(view_->google)&&std::any_of(view_->google.tasks.begin(),view_->google.tasks.end(),[](const auto& task){return !task.completed;}))box(D2D1::RectF(w-92,16,w-87,21),2.5f,view_->theme.accent);
  auto sound=D2D1::RectF(w-80,13,w-49,46);if(view_->page==Page::sound)box(sound,10,view_->theme.elevated);button(sound,Icon::volume,Action::tab,6);
  button(D2D1::RectF(w-48,13,w-16,46),Icon::settings,Action::settings);
  box(D2D1::RectF(16,58,w-16,59),0,view_->theme.border);
}
void DCompRenderer::reminders_page(float w){const auto& t=view_->theme;
  draw_text(L"Hatırlatıcılar",D2D1::RectF(18,71,w-65,101),15,D2D1::ColorF(t.text));
  button(D2D1::RectF(w-53,70,w-18,102),Icon::plus,Action::manage_reminders);
  constexpr const wchar_t* days[]{L"Dün",L"Bugün",L"Yarın"};float tab=(w-36)/3;
  box(D2D1::RectF(18,108,w-18,140),10,t.surface);
  for(size_t i=0;i<3;++i){auto r=D2D1::RectF(20+tab*i,110,16+tab*(i+1),138);bool active=view_->reminder_offset==static_cast<int>(i)-1;if(active)box(r,8,t.elevated);draw_text(days[i],r,12,D2D1::ColorF(active?t.accent:t.secondary),DWRITE_TEXT_ALIGNMENT_CENTER);hits_.push_back({r,Action::reminder_day,i});}
  auto date=view_->reminder_date?view_->reminder_date:reminder_day_at_offset(reminder_now(),view_->reminder_offset);
  draw_text(reminder_day(date)+L" · "+std::to_wstring(view_->reminders.size())+L" kayıt",D2D1::RectF(18,142,w-18,168),11,D2D1::ColorF(t.secondary));
  if(view_->reminders.empty()){icon(Icon::bell,w/2-18,176,36,t.accent);const wchar_t* empty=view_->reminder_offset<0?L"Dün için hatırlatıcı yok":view_->reminder_offset>0?L"Yarın için hatırlatıcı yok":L"Bugün için hatırlatıcı yok";draw_text(empty,D2D1::RectF(18,216,w-18,243),13,D2D1::ColorF(t.secondary),DWRITE_TEXT_ALIGNMENT_CENTER);}
  for(size_t row=0;row<4&&view_->scroll+row<view_->reminders.size();++row){size_t index=view_->scroll+row;auto& r=view_->reminders[index];float y=174+static_cast<float>(row)*54;box(D2D1::RectF(18,y,w-18,y+48),10,t.surface);icon(r.done?Icon::check:Icon::bell,28,y+14,18,r.done?t.secondary:t.accent);draw_text(r.title,D2D1::RectF(56,y+2,w-59,y+26),12,D2D1::ColorF(r.done?t.secondary:t.text));draw_text(reminder_time(r)+(r.google?L" · Google Takvim":L" · Yerel")+(r.done?L" · Tamamlandı":L""),D2D1::RectF(56,y+25,w-59,y+46),10,D2D1::ColorF(t.secondary));button(D2D1::RectF(w-56,y+7,w-23,y+41),Icon::check,Action::complete_reminder,index);}
  float base=view_->height-(view_->notice.empty()?0.f:34.f);
  auto manage=D2D1::RectF(18,base-68,w-18,base-38);box(manage,9,t.elevated);draw_text(L"Hatırlatıcı ekle",manage,11,D2D1::ColorF(t.text),DWRITE_TEXT_ALIGNMENT_CENTER);hits_.push_back({manage,Action::manage_reminders});
  draw_text(view_->calendar_status,D2D1::RectF(18,base-34,w-18,base-8),10,D2D1::ColorF(t.secondary));
}
void DCompRenderer::widgets(const SystemSnapshot& s,float w) {
  const auto& t=view_->theme;
  draw_text(L"Sistem izleme",D2D1::RectF(18,71,210,94),13,D2D1::ColorF(t.text));
  draw_text(L"Canlı görünüm",D2D1::RectF(w-145,71,w-18,94),11,D2D1::ColorF(t.secondary),DWRITE_TEXT_ALIGNMENT_TRAILING);
  double ram=s.ram_total?100.0*s.ram_used/s.ram_total:0;float cw=(w-46)/2;
  struct Card {Icon symbol;const wchar_t* name;std::wstring value,detail;double percent;};
  Card cards[]{
    {Icon::cpu,L"CPU",s.ready?std::format(L"{:.0f}%",s.cpu_percent):L"Ölçülüyor",L"İşlemci kullanımı",s.ready?s.cpu_percent:-1},
    {Icon::gpu,L"GPU",s.gpu_available?std::format(L"{:.0f}%",s.gpu_percent):L"Veri yok",L"En yoğun GPU motoru",s.gpu_available?s.gpu_percent:-1},
    {Icon::ram,L"RAM",s.ready?std::format(L"{:.0f}%",ram):L"Ölçülüyor",s.ready?std::format(L"{:.1f} / {:.1f} GB",s.ram_used/1073741824.0,s.ram_total/1073741824.0):L"Bellek ölçümü bekleniyor",s.ready?ram:-1},
    {Icon::disk,L"Disk",s.disk_total?std::format(L"{:.0f}%",100.0*s.disk_used/s.disk_total):L"Veri yok",L"Sistem diski",s.disk_total?100.0*s.disk_used/s.disk_total:-1}
  };
  for(int i=0;i<4;++i) {
    float x=18+static_cast<float>(i%2)*(cw+10),y=104+static_cast<float>(i/2)*83;
    box(D2D1::RectF(x,y,x+cw,y+73),13,t.surface);box(D2D1::RectF(x,y,x+cw,y+73),13,t.border,1,true);
    icon(cards[i].symbol,x+12,y+12,16,t.secondary);
    draw_text(cards[i].name,D2D1::RectF(x+35,y+8,x+90,y+30),11,D2D1::ColorF(t.secondary));
    draw_text(cards[i].value,D2D1::RectF(x+90,y+8,x+cw-12,y+30),cards[i].percent<0?12.f:17.f,D2D1::ColorF(t.text),DWRITE_TEXT_ALIGNMENT_TRAILING);
    draw_text(cards[i].detail,D2D1::RectF(x+12,y+30,x+cw-12,y+51),10,D2D1::ColorF(t.secondary));
    box(D2D1::RectF(x+12,y+59,x+cw-12,y+63),2,t.border);
    if(cards[i].percent>=0)box(D2D1::RectF(x+12,y+59,x+12+(cw-24)*static_cast<float>(std::clamp(cards[i].percent,0.,100.))/100,y+63),2,t.accent);
  }
  icon(Icon::network,20,286,18,t.accent);
  auto rate=[](double b){return b>=1048576?std::format(L"{:.1f} MB/s",b/1048576):std::format(L"{:.0f} KB/s",b/1024);};
  draw_text(L"İndir "+rate(s.download_bytes_per_sec)+L"   Yükle "+rate(s.upload_bytes_per_sec),D2D1::RectF(47,278,w-20,302),12,D2D1::ColorF(t.text));
  draw_text(L"Toplam adaptör trafiği · "+s.adapter,D2D1::RectF(47,301,w-20,321),10,D2D1::ColorF(t.secondary));
}
void DCompRenderer::media_page(const MediaSnapshot& m,float w) {
  const auto& t=view_->theme;
  if(!m.available) {
    artwork_.reset();artwork_version_=0;
    icon(Icon::music,w/2-24,108,48,t.accent);
    draw_text(L"Aktif medya oturumu yok",D2D1::RectF(18,170,w-18,202),17,D2D1::ColorF(t.text),DWRITE_TEXT_ALIGNMENT_CENTER);
    draw_text(L"Spotify veya tarayıcınızda medya oynatın",D2D1::RectF(18,204,w-18,235),12,D2D1::ColorF(t.secondary),DWRITE_TEXT_ALIGNMENT_CENTER);return;
  }
  box(D2D1::RectF(18,78,92,152),17,t.accent_soft);
  if(m.art_version!=artwork_version_) {
    artwork_.reset();artwork_version_=m.art_version;
    if(!m.art_pixels.empty()) {
      auto p=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
      context_->CreateBitmap(D2D1::SizeU(m.art_width,m.art_height),m.art_pixels.data(),m.art_width*4,&p,artwork_.put());
    }
  }
  if(artwork_) {
    ComPtr<ID2D1RoundedRectangleGeometry> mask;d2d_factory_->CreateRoundedRectangleGeometry(D2D1::RoundedRect(D2D1::RectF(18,78,92,152),17,17),mask.put());
    auto layer=D2D1::LayerParameters1();layer.geometricMask=mask.get();context_->PushLayer(layer,nullptr);
    context_->DrawBitmap(artwork_.get(),D2D1::RectF(18,78,92,152));context_->PopLayer();
  }else icon(Icon::music,37,97,36,t.accent);
  draw_text(m.title,D2D1::RectF(106,76,w-18,104),17,D2D1::ColorF(t.text));
  draw_text(m.artist,D2D1::RectF(106,104,w-18,125),12,D2D1::ColorF(t.secondary));
  std::wstring source=m.source;
  std::wstring lowered=source;std::transform(lowered.begin(),lowered.end(),lowered.begin(),::towlower);
  if(lowered.find(L"spotify")!=std::wstring::npos)source=L"Spotify";
  else if(lowered.find(L"chrome")!=std::wstring::npos)source=L"Chrome";
  else if(lowered.find(L"msedge")!=std::wstring::npos)source=L"Edge";
  draw_text(source+(m.playing?L" · Oynatılıyor":L" · Duraklatıldı"),D2D1::RectF(106,126,w-18,148),11,D2D1::ColorF(t.accent));
  for(int i=0;i<3;++i){float wave=3+static_cast<float>((std::sin(view_->wave_phase*5+i*1.7f)+1)*4);float half=1.5f+(wave-1.5f)*view_->wave_mix;box(D2D1::RectF(w-37+i*6.f,136-half,w-34+i*6.f,136+half),1.5f,t.accent);}
  button(D2D1::RectF(106,159,143,196),Icon::previous,Action::previous,0,m.can_previous);
  box(D2D1::RectF(160,159,197,196),19,t.elevated);
  button(D2D1::RectF(160,159,197,196),m.playing?Icon::pause:Icon::play,Action::play,0,m.can_play);
  button(D2D1::RectF(214,159,251,196),Icon::next,Action::next,0,m.can_next);
  box(D2D1::RectF(18,206,w-18,210),2,t.border);
  float progress=m.duration>0?static_cast<float>(std::clamp(m.position/m.duration,0.,1.)):0;
  box(D2D1::RectF(18,206,18+(w-36)*progress,210),2,t.accent);
  hits_.push_back({D2D1::RectF(18,199,w-18,218),Action::seek,0,m.can_seek});
  auto time=[](double s){int v=static_cast<int>(std::max(0.,s));return std::format(L"{}:{:02}",v/60,v%60);};
  draw_text(time(m.position),D2D1::RectF(18,218,120,241),11,D2D1::ColorF(t.secondary));
  draw_text(time(m.duration),D2D1::RectF(w-120,218,w-18,241),11,D2D1::ColorF(t.secondary),DWRITE_TEXT_ALIGNMENT_TRAILING);
}
void DCompRenderer::files_page(float w) {
  for(auto it=file_icons_.begin();it!=file_icons_.end();)if(std::find(view_->files.begin(),view_->files.end(),it->first)==view_->files.end())it=file_icons_.erase(it);else ++it;
  const auto& t=view_->theme;bool empty=view_->files.empty();
  draw_text(L"Dosya rafı",D2D1::RectF(18,71,210,94),13,D2D1::ColorF(t.text));
  draw_text(std::format(L"{} dosya",view_->files.size()),D2D1::RectF(w-110,71,w-18,94),11,D2D1::ColorF(t.secondary),DWRITE_TEXT_ALIGNMENT_TRAILING);
  box(D2D1::RectF(18,104,w-18,empty?210.f:148.f),15,view_->drop_active?t.accent_soft:t.surface);box(D2D1::RectF(18,104,w-18,empty?210.f:148.f),15,view_->drop_active?t.accent:t.border,1,true);
  if(empty)icon(Icon::files,w/2-14,119,28,t.accent);
  draw_text(L"Dosyaları buraya sürükle",D2D1::RectF(25,empty?152.f:108.f,w-25,empty?180.f:142.f),13,D2D1::ColorF(t.text),DWRITE_TEXT_ALIGNMENT_CENTER);
  if(empty)draw_text(L"Kaynak dosyalar yerlerinde kalır",D2D1::RectF(25,180,w-25,202),11,D2D1::ColorF(t.secondary),DWRITE_TEXT_ALIGNMENT_CENTER);
  for(size_t row=0;row<3&&view_->scroll+row<view_->files.size();++row) {
    size_t i=view_->scroll+row;float y=158+static_cast<float>(row)*44;
    box(D2D1::RectF(18,y,w-18,y+38),9,t.surface);
    auto [cached,inserted]=file_icons_.try_emplace(view_->files[i]);
    if(inserted){auto file=shell_icon(view_->files[i]);if(!file.pixels.empty()){auto p=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));context_->CreateBitmap(D2D1::SizeU(file.width,file.height),file.pixels.data(),file.width*4,&p,cached->second.put());}}
    if(cached->second)context_->DrawBitmap(cached->second.get(),D2D1::RectF(26,y+7,50,y+31));else icon(Icon::files,27,y+10,18,t.secondary);
    draw_text(std::filesystem::path(view_->files[i]).filename().wstring(),D2D1::RectF(55,y,w-95,y+38),12,D2D1::ColorF(t.text));
    hits_.push_back({D2D1::RectF(18,y,w-95,y+38),Action::drag_file,i});
    button(D2D1::RectF(w-89,y,w-57,y+38),Icon::reveal,Action::reveal_file,i);
    button(D2D1::RectF(w-54,y,w-22,y+38),Icon::close,Action::remove_file,i);
  }
  float footer=view_->height-(view_->notice.empty()?0.f:34.f);if(!empty)draw_text(L"Dışarı sürükle: kopyala · Tekerlek: listeyi kaydır",D2D1::RectF(18,footer-26,w-18,footer-6),10,D2D1::ColorF(t.secondary));
}
void DCompRenderer::clipboard_page(float w) {
  for(auto it=clip_images_.begin();it!=clip_images_.end();)if(std::none_of(view_->clipboard.begin(),view_->clipboard.end(),[&](auto& entry){return entry.id==it->first;}))it=clip_images_.erase(it);else ++it;
  const auto& t=view_->theme;
  draw_text(L"Pano geçmişi",D2D1::RectF(18,71,210,94),13,D2D1::ColorF(t.text));
  auto toggle=D2D1::RectF(w-165,72,w-18,100);box(toggle,9,view_->clipboard_enabled?t.accent_soft:t.elevated);
  draw_text(view_->clipboard_enabled?L"İzlemeyi durdur":L"İzlemeyi etkinleştir",toggle,11,D2D1::ColorF(t.text),DWRITE_TEXT_ALIGNMENT_CENTER);hits_.push_back({toggle,Action::toggle_clip});
  auto results=clipboard_matches(view_->clipboard,view_->clipboard_query);
  auto search=D2D1::RectF(18,108,w-18,140);box(search,9,t.surface);
  draw_text(view_->clipboard_query.empty()?L"Pano içinde ara…":L"Ara: "+view_->clipboard_query, D2D1::RectF(29,108,w-75,140),11,D2D1::ColorF(t.secondary));
  draw_text(std::to_wstring(results.size()),D2D1::RectF(w-70,108,w-28,140),11,D2D1::ColorF(t.accent),DWRITE_TEXT_ALIGNMENT_TRAILING);hits_.push_back({search,Action::search_clips});
  if(!view_->clipboard_enabled||view_->clipboard.empty()) {
    icon(Icon::clipboard,w/2-22,154,44,t.accent);
    draw_text(view_->clipboard_enabled?L"Henüz kopyalanan metin veya görsel yok":L"Pano izleme kapalı",D2D1::RectF(18,196,w-18,226),15,D2D1::ColorF(t.text),DWRITE_TEXT_ALIGNMENT_CENTER);
  }
  if(view_->clipboard_enabled&&results.empty()&&!view_->clipboard.empty())draw_text(L"Arama sonucu yok",D2D1::RectF(18,172,w-18,220),14,D2D1::ColorF(t.secondary),DWRITE_TEXT_ALIGNMENT_CENTER);
  for(size_t row=0;view_->clipboard_enabled&&row<4&&view_->scroll+row<results.size();++row) {
    size_t i=results[view_->scroll+row];float y=151+static_cast<float>(row)*43;
    box(D2D1::RectF(18,y,w-18,y+37),9,t.surface);const auto& entry=view_->clipboard[i];
    auto preview=entry.text;for(auto& c:preview)if(c=='\n'||c=='\r')c=' ';
    if(entry.image()){
      auto& image=clip_images_[entry.id];if(!image){auto p=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
      context_->CreateBitmap(D2D1::SizeU(entry.width,entry.height),entry.pixels.data(),entry.width*4,&p,image.put());}
      float scale=std::min(48.f/entry.width,31.f/entry.height);if(image)context_->DrawBitmap(image.get(),D2D1::RectF(28,y+3,28+entry.width*scale,y+3+entry.height*scale));
      preview=std::format(L"Görsel · {} × {} · Aç",entry.width,entry.height);
    }
    draw_text(preview,D2D1::RectF(entry.image()?83.f:30.f,y,w-133,y+37),12,D2D1::ColorF(t.text));
    hits_.push_back({D2D1::RectF(18,y,w-130,y+37),Action::preview_clip,i});
    icon(Icon::pin,w-122,y+10,17,entry.pinned?t.accent:t.secondary);hits_.push_back({D2D1::RectF(w-127,y,w-94,y+37),Action::pin_clip,i});
    button(D2D1::RectF(w-89,y,w-57,y+37),Icon::clipboard,Action::copy_clip,i);button(D2D1::RectF(w-54,y,w-22,y+37),Icon::close,Action::remove_clip,i);
  }
  float footer=view_->height-(view_->notice.empty()?0.f:34.f);draw_text(L"Tıkla: önizle · Yıldız: sabitle · RAM",D2D1::RectF(18,footer-30,w-130,footer-6),10,D2D1::ColorF(t.secondary));
  if(view_->clipboard_enabled&&!view_->clipboard.empty()) {
    auto r=D2D1::RectF(w-122,footer-34,w-18,footer-5);box(r,9,t.elevated);draw_text(L"Sabit olmayanı sil",r,10,D2D1::ColorF(t.text),DWRITE_TEXT_ALIGNMENT_CENTER);hits_.push_back({r,Action::clear_clip});
  }
}
void DCompRenderer::audio_controls(float w) {
  const auto& a=view_->audio;const auto& t=view_->theme;
  draw_text(L"Ses mikseri",D2D1::RectF(18,70,w-150,97),15,D2D1::ColorF(t.text));
  auto select=D2D1::RectF(w-116,70,w-18,100);box(select,8,t.elevated);draw_text(L"Çıkış seç",select,11,D2D1::ColorF(t.text),DWRITE_TEXT_ALIGNMENT_CENTER);hits_.push_back({select,Action::select_output,0,!a.outputs.empty()});
  box(D2D1::RectF(18,108,w-18,177),12,t.accent_soft);
  box(D2D1::RectF(18,108,w-18,177),12,t.accent,.8f,true);
  box(D2D1::RectF(21,120,24,165),1.5f,t.accent);
  draw_text(L"ANA SES",D2D1::RectF(34,109,115,138),12,D2D1::ColorF(t.accent));
  draw_text(a.output_name,D2D1::RectF(120,109,w-30,138),11,D2D1::ColorF(t.text));
  button(D2D1::RectF(24,138,56,172),Icon::volume,Action::mute_output,0,a.available);if(a.muted)box(D2D1::RectF(30,154,49,156),1,t.accent);
  float end=w-72;box(D2D1::RectF(67,152,end,157),2,t.border);box(D2D1::RectF(67,152,67+(end-67)*a.volume,157),2,a.muted?t.secondary:t.accent);hits_.push_back({D2D1::RectF(60,138,end,174),Action::volume,0,a.available});draw_text(std::format(L"{:.0f}%",a.volume*100),D2D1::RectF(end+6,138,w-23,174),11,D2D1::ColorF(t.secondary));
  draw_text(L"Uygulamalar",D2D1::RectF(20,181,w-18,206),11,D2D1::ColorF(t.secondary));
  for(size_t row=0;row<4&&view_->scroll+row<a.sessions.size();++row){size_t index=view_->scroll+row;const auto& s=a.sessions[index];float y=210+66*static_cast<float>(row);box(D2D1::RectF(18,y,w-18,y+60),10,t.surface);draw_text(s.name,D2D1::RectF(30,y,w-65,y+26),12,D2D1::ColorF(t.text));
    auto star=D2D1::RectF(w-57,y+1,w-22,y+29);if(static_cast<int>(hits_.size())==view_->hovered)box(star,8,t.elevated);icon(Icon::pin,w-48,y+6,17,s.favorite?t.accent:t.secondary);hits_.push_back({star,Action::favorite_audio,index,!s.app_key.empty(),s.app_key});
    button(D2D1::RectF(24,y+25,56,y+57),Icon::volume,Action::session_mute,index);hits_.back().key=s.id;if(s.muted)box(D2D1::RectF(30,y+40,49,y+42),1,t.accent);
    box(D2D1::RectF(67,y+39,end,y+44),2,t.border);box(D2D1::RectF(67,y+39,67+(end-67)*s.volume,y+44),2,s.muted?t.secondary:t.accent);hits_.push_back({D2D1::RectF(60,y+25,end,y+58),Action::session_volume,index,true,s.id});draw_text(std::format(L"{:.0f}%",s.volume*100),D2D1::RectF(end+6,y+25,w-23,y+58),11,D2D1::ColorF(t.secondary));}
  auto settings=D2D1::RectF(18,view_->height-34-(view_->notice.empty()?0.f:34.f),w-160,view_->height-8-(view_->notice.empty()?0.f:34.f));draw_text(a.error.empty()?(a.sessions.empty()?L"Ses üreten uygulama yok · Windows ses ayarları":L"Tekerlek: uygulamaları kaydır · Windows ses ayarları"):a.error,settings,10,D2D1::ColorF(t.secondary));hits_.push_back({settings,Action::sound_settings});
  auto mic=D2D1::RectF(w-150,settings.top,w-18,settings.bottom);box(mic,8,t.elevated);icon(Icon::microphone,w-143,mic.top+4,17,t.accent);draw_text(a.mic_muted?L"Mikrofon kapalı":L"Mikrofon açık",D2D1::RectF(w-120,mic.top,w-20,mic.bottom),10,D2D1::ColorF(t.text));hits_.push_back({mic,Action::mute_mic,0,a.mic_available});
}
void DCompRenderer::tasks_page(float w){const auto& t=view_->theme;const auto& g=view_->google;
  draw_text(L"Yapılacaklar",D2D1::RectF(18,70,w-105,100),15,D2D1::ColorF(t.text));button(D2D1::RectF(w-96,70,w-62,103),Icon::network,Action::refresh_tasks,0,g.tasks_enabled&&!g.busy);button(D2D1::RectF(w-54,70,w-18,103),Icon::plus,Action::add_task,0,g.tasks_enabled&&!g.task_list_id.empty()&&!g.busy);
  std::wstring name=L"Liste seç";for(auto& l:g.task_lists)if(l.id==g.task_list_id)name=l.name;auto list=D2D1::RectF(18,108,w-18,139);box(list,9,t.elevated);draw_text(name,list,12,D2D1::ColorF(t.accent),DWRITE_TEXT_ALIGNMENT_CENTER);hits_.push_back({list,Action::select_task_list,0,!g.task_lists.empty()&&!g.busy});
  const auto pending=pending_task_indices(g.tasks);
  if(pending.empty()){icon(Icon::check,w/2-17,156,34,t.accent);draw_text(g.tasks_enabled?L"Bekleyen görev yok":L"Google Tasks bağlantısını ayarlardan aç",D2D1::RectF(20,194,w-20,224),12,D2D1::ColorF(t.secondary),DWRITE_TEXT_ALIGNMENT_CENTER);if(!g.tasks_enabled)hits_.push_back({D2D1::RectF(18,146,w-18,224),Action::settings});}
  const size_t start=pending.empty()?0:std::min(view_->scroll,pending.size()-1);
  for(size_t row=0;row<4&&start+row<pending.size();++row){auto index=pending[start+row];const auto& task=g.tasks[index];float y=150+60*static_cast<float>(row);box(D2D1::RectF(18,y,w-18,y+53),10,t.surface);box(D2D1::RectF(30,y+15,51,y+37),6,t.border,1,true);hits_.push_back({D2D1::RectF(24,y+9,57,y+43),Action::complete_task,index,!g.busy,task.id});draw_text(task.title,D2D1::RectF(66,y+2,w-102,y+30),12,D2D1::ColorF(t.text));draw_text(!task.due.empty()?L"Tarih · "+task.due.substr(0,10):task.notes.empty()?L"Google Tasks":task.notes,D2D1::RectF(66,y+29,w-102,y+49),10,D2D1::ColorF(t.secondary));button(D2D1::RectF(w-94,y+9,w-60,y+43),Icon::pencil,Action::edit_task,index,!g.busy);hits_.back().key=task.id;button(D2D1::RectF(w-57,y+9,w-23,y+43),Icon::close,Action::delete_task,index,!g.busy);hits_.back().key=task.id;}
  float footer=view_->height-(view_->notice.empty()?0.f:34.f);draw_text(g.busy?L"Google ile eşitleniyor…":g.tasks_message,D2D1::RectF(18,footer-30,w-18,footer-6),10,D2D1::ColorF(t.secondary));
}
void DCompRenderer::applications_page(float w) {
  const auto& t=view_->theme;
  draw_text(L"Uygulamalar",D2D1::RectF(18,71,w-90,99),13,D2D1::ColorF(t.text));
  button(D2D1::RectF(w-53,70,w-18,102),Icon::plus,Action::add_app);
  if(view_->applications.empty()) {
    icon(Icon::grid,w/2-16,112,32,t.accent);
    draw_text(L"+ ile uygulama veya kısayol ekle",D2D1::RectF(18,152,w-18,180),12,D2D1::ColorF(t.secondary),DWRITE_TEXT_ALIGNMENT_CENTER);
  }
  for(auto it=app_icons_.begin();it!=app_icons_.end();) {
    if(std::none_of(view_->applications.begin(),view_->applications.end(),[&](auto& app){return app.path==it->first;}))it=app_icons_.erase(it);else ++it;
  }
  for(size_t slot=0;slot<8&&view_->scroll+slot<view_->applications.size();++slot) {
    size_t i=view_->scroll+slot;const auto& app=view_->applications[i];float cw=(w-66)/4,x=18+static_cast<float>(slot%4)*(cw+10),y=108+static_cast<float>(slot/4)*112;
    box(D2D1::RectF(x,y,x+cw,y+102),12,t.surface);
    if(static_cast<int>(hits_.size())==view_->hovered)box(D2D1::RectF(x,y,x+cw,y+102),12,t.elevated);
    auto& bitmap=app_icons_[app.path];
    if(!bitmap&&!app.pixels.empty()) {
      auto props=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
      context_->CreateBitmap(D2D1::SizeU(app.width,app.height),app.pixels.data(),app.width*4,&props,bitmap.put());
    }
    if(bitmap)context_->DrawBitmap(bitmap.get(),D2D1::RectF(x+cw/2-20,y+16,x+cw/2+20,y+56));else icon(Icon::grid,x+cw/2-16,y+20,32,t.accent);
    draw_text(app.name,D2D1::RectF(x+5,y+63,x+cw-5,y+96),11,D2D1::ColorF(t.text),DWRITE_TEXT_ALIGNMENT_CENTER);
    button(D2D1::RectF(x+cw-24,y,x+cw,y+24),Icon::close,Action::remove_app,i);
    hits_.push_back({D2D1::RectF(x,y,x+cw,y+102),Action::launch_app,i});
  }
}
}

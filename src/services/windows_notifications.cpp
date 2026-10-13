#include "services/windows_notifications.hpp"
#include "services/notification_icon_resolver.hpp"
#include "core/com_ptr.hpp"
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Storage.Streams.h>
#include <thread>
#include <wincodec.h>
#include <shlwapi.h>
#include <algorithm>
#include <cwctype>
#include <string_view>
#include <windows.h>
#include <appmodel.h>

namespace ni {
using namespace winrt::Windows::UI::Notifications;
using namespace winrt::Windows::UI::Notifications::Management;
using winrt::Windows::Foundation::AsyncStatus;
std::vector<WindowsNotice> NotificationDelta::update(const std::vector<WindowsNotice>& current){
  std::map<uint32_t,int64_t> next;std::vector<WindowsNotice> added;
  for(const auto& n:current){next[n.id]=n.created;if(baseline_&&(!seen_.contains(n.id)||seen_.at(n.id)!=n.created))added.push_back(n);}
  seen_=std::move(next);baseline_=true;
  std::stable_sort(added.begin(),added.end(),[](const auto& a,const auto& b){return a.created<b.created;});
  if(added.size()>20)added.erase(added.begin(),added.end()-20);return added;
}
std::wstring windows_notice_text(const WindowsNotice& notice,bool content){
  std::wstring text=notice.app.empty()?L"Windows":notice.app;
  if(content){if(!notice.title.empty())text+=L" · "+notice.title;if(!notice.body.empty())text+=L" · "+notice.body;}
  else text+=L" · Yeni bildirim";
  for(auto& c:text)if(iswcntrl(c))c=L' ';
  if(text.size()>240){text.resize(239);if(!text.empty()&&text.back()>=0xd800&&text.back()<=0xdbff)text.pop_back();text+=L"…";}
  return text;
}
namespace {
// Keep externally supplied text bounded and single-line. Never classify the
// originating web site from a browser notification's title/body.
std::wstring cleaned_text(std::wstring_view input,size_t limit){
  std::wstring out;out.reserve(std::min(input.size(),limit));
  bool space=false;
  for(wchar_t ch:input){
    if(iswcntrl(ch)||iswspace(ch)){space=!out.empty();continue;}
    if(space&&out.size()<limit)out.push_back(L' ');
    space=false;
    if(out.size()>=limit)break;
    out.push_back(ch);
  }
  if(!out.empty()&&out.back()>=0xd800&&out.back()<=0xdbff)out.pop_back();
  return out;
}
}
WindowsNoticePresentation windows_notice_presentation(const WindowsNotice& notice,bool show_content){
  WindowsNoticePresentation result;
  result.source=cleaned_text(notice.app,72);
  if(result.source.empty())result.source=L"Windows";
  result.title=L"Yeni bildirim";
  if(show_content){
    auto title=cleaned_text(notice.title,160);
    auto body=cleaned_text(notice.body,240);
    if(title.empty()&&!body.empty()){title=std::move(body);body.clear();}
    if(!title.empty())result.title=std::move(title);
    result.preview=std::move(body);
  }
  return result;
}

namespace {
// Reject malformed or unrenderable WinRT logos: a non-empty stream is not
// necessarily a usable bitmap. This allows the local Win32 shell fallback.
bool renderable_logo(const std::vector<std::uint8_t>& bytes){
  if(bytes.empty()||bytes.size()>512*1024)return false;
  ComPtr<IWICImagingFactory> factory;
  ComPtr<IWICBitmapDecoder> decoder;
  ComPtr<IWICBitmapFrameDecode> frame;
  ComPtr<IStream> stream;
  stream.reset(SHCreateMemStream(reinterpret_cast<const BYTE*>(bytes.data()),static_cast<UINT>(bytes.size())));
  if(!stream)return false;
  if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,
    __uuidof(IWICImagingFactory),reinterpret_cast<void**>(factory.put())))||
     FAILED(factory->CreateDecoderFromStream(stream.get(),nullptr,WICDecodeMetadataCacheOnLoad,decoder.put()))||
     FAILED(decoder->GetFrame(0,frame.put())))return false;
  UINT w{},h{};
  return SUCCEEDED(frame->GetSize(&w,&h))&&w>0&&h>0&&w<=512&&h<=512;
}
// Best effort and bounded: application logos are loaded in the background.
void queue_app_logo(const std::shared_ptr<NoticeLogoCache>& cache,
                    const std::wstring& app_id,
                    const std::wstring& display_name,
                    winrt::Windows::ApplicationModel::AppInfo app){
  if(app_id.empty()||!app)return;
  {
    std::scoped_lock lock(cache->mutex);
    if(cache->ready.contains(app_id)||cache->pending.contains(app_id))return;
    if(auto failed=cache->failed_at.find(app_id);failed!=cache->failed_at.end()&&
       std::chrono::steady_clock::now()-failed->second<std::chrono::minutes(5))return;
    if(cache->ready.size()+cache->pending.size()>=32||cache->pending.size()>=3)return;
    cache->pending.insert(app_id);
  }
  std::thread([cache,app_id,display_name,app=std::move(app)]() mutable {
    std::vector<std::uint8_t> bytes;
    try {
      winrt::init_apartment(winrt::apartment_type::multi_threaded);
      auto logo=app.DisplayInfo().GetLogo(winrt::Windows::Foundation::Size{64.f,64.f});
      if(logo){
        auto stream=logo.OpenReadAsync().get();
        const auto size=stream.Size();
        if(size>0&&size<=512*1024){
          winrt::Windows::Storage::Streams::DataReader reader(stream.GetInputStreamAt(0));
          const auto loaded=reader.LoadAsync(static_cast<std::uint32_t>(size)).get();
          if(loaded==size){
            bytes.resize(loaded);
            reader.ReadBytes(winrt::array_view<std::uint8_t>(bytes));
          }
        }
      }
    }catch(...){bytes.clear();}
    // The WinRT logo may be absent for unpackaged desktop apps. Resolve the
    // shell's own identity icon without blocking the island's UI thread.
    if(!renderable_logo(bytes)){
      const auto actual_id=app_id.starts_with(L"@desktop:")?std::wstring{}:app_id;
      bytes=notification_shell_icon(actual_id,display_name);
    }
    const bool usable=bytes.size()>=12&&bytes[0]=='N'&&bytes[1]=='I'&&bytes[2]=='R'&&bytes[3]=='G';
    if(!usable&&!renderable_logo(bytes))bytes.clear();
    auto result=std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));
    std::scoped_lock lock(cache->mutex);
    cache->pending.erase(app_id);
    if(!result->empty()){cache->failed_at.erase(app_id);cache->ready.insert_or_assign(app_id,std::move(result));}
    else cache->failed_at.insert_or_assign(app_id,std::chrono::steady_clock::now());
  }).detach();
}
}
void WindowsNotifications::clear(){
  try{if(query_)query_.Cancel();if(access_)access_.Cancel();}catch(...){}
  query_=nullptr;access_=nullptr;incoming_.clear();delta_.reset();
}
void WindowsNotifications::enable(bool enabled){
  if(enabled_==enabled)return;enabled_=enabled;clear();
  status_=enabled?L"Windows erişim izni kontrol ediliyor…":L"Kapalı · Bildirim içeriği kaydedilmez.";
}
void WindowsNotifications::request_access(){
  if(!enabled_)return;
  UINT32 length{};if(GetCurrentPackageFullName(&length,nullptr)==APPMODEL_ERROR_NO_PACKAGE){status_=L"Bu EXE paket kimliğine sahip değil. Bildirim erişimi için MSIX sürümünü kurun.";return;}
  try{if(!listener_)listener_=UserNotificationListener::Current();if(!access_){access_=listener_.RequestAccessAsync();status_=L"Windows izin penceresini onaylayın.";}}
  catch(const winrt::hresult_error&){status_=L"Erişim başlatılamadı. Bildirim yeteneği olan MSIX paketi gerekli olabilir.";}
  catch(...){status_=L"Windows bildirim erişimi kullanılamıyor.";}
}
void WindowsNotifications::poll(){
  if(!enabled_)return;
  UINT32 length{};if(GetCurrentPackageFullName(&length,nullptr)==APPMODEL_ERROR_NO_PACKAGE){status_=L"Bu EXE paket kimliğine sahip değil. Bildirim erişimi için MSIX sürümünü kurun.";return;}
  try{
    if(!listener_)listener_=UserNotificationListener::Current();
    if(access_){if(access_.Status()==AsyncStatus::Started)return;auto op=access_;access_=nullptr;if(op.Status()==AsyncStatus::Completed)op.GetResults();}
    if(listener_.GetAccessStatus()!=UserNotificationListenerAccessStatus::Allowed){
      if(query_)query_.Cancel();query_=nullptr;incoming_.clear();delta_.reset();status_=L"İzin gerekli · Erişim izni ver düğmesini kullanın.";return;
    }
    if(query_){
      if(query_.Status()==AsyncStatus::Started)return;
      auto op=query_;query_=nullptr;
      if(op.Status()!=AsyncStatus::Completed){status_=L"Windows bildirimleri okunamadı; erişim iznini kontrol edin.";return;}
      std::vector<WindowsNotice> current;
      std::map<std::wstring,winrt::Windows::ApplicationModel::AppInfo> apps;
      for(const auto& item:op.GetResults())try{
        auto binding=item.Notification().Visual().GetBinding(KnownNotificationBindings::ToastGeneric());if(!binding)continue;
        WindowsNotice n;n.id=item.Id();n.created=item.CreationTime().time_since_epoch().count();
        auto info=item.AppInfo();
        n.app=info.DisplayInfo().DisplayName();
        const std::wstring actual_id(info.AppUserModelId());
        if(actual_id.find(L"NativeIsland")!=std::wstring::npos)continue;
        n.app_id=notification_icon_key(actual_id,n.app);
        if(!n.app_id.empty())apps.insert_or_assign(n.app_id,info);
        for(const auto& text:binding.GetTextElements()){if(n.title.empty())n.title=text.Text();else{if(!n.body.empty())n.body+=L" · ";n.body+=text.Text();}if(n.body.size()>1024)break;}
        if(!n.title.empty()||!n.body.empty())current.push_back(std::move(n));
      }catch(...){} // A malformed toast must not disable all other apps.
      incoming_=delta_.update(current);
      for(const auto& n:incoming_){auto it=apps.find(n.app_id);if(it!=apps.end())queue_app_logo(logo_cache_,n.app_id,n.app,it->second);}
      status_=L"İzin verildi · Yeni Windows bildirimleri adada gösterilir.";
    }
    query_=listener_.GetNotificationsAsync(NotificationKinds::Toast);
  }catch(const winrt::hresult_error&){clear();status_=L"Windows erişimi kullanılamıyor. Bildirim yeteneği olan MSIX paketi gerekli olabilir.";}
  catch(...){clear();status_=L"Windows bildirimleri geçici olarak okunamıyor.";}
}
std::shared_ptr<const std::vector<std::uint8_t>> WindowsNotifications::logo_for(const std::wstring& app_id)const{
  if(app_id.empty())return {};
  std::scoped_lock lock(logo_cache_->mutex);
  auto it=logo_cache_->ready.find(app_id);
  return it==logo_cache_->ready.end()?nullptr:it->second;
}
std::vector<WindowsNotice> WindowsNotifications::take(){auto out=std::move(incoming_);incoming_.clear();return out;}
}

#include "media/media_service.hpp"
#include "core/diagnostics.hpp"
#include "core/com_ptr.hpp"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <chrono>
#include <algorithm>

namespace ni {
using namespace winrt::Windows::Media::Control;
using namespace winrt::Windows::Storage::Streams;
using Manager=GlobalSystemMediaTransportControlsSessionManager;
namespace {
template<class Operation> auto completed(Operation operation,std::stop_token stop) {
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
  while(operation.Status()==winrt::Windows::Foundation::AsyncStatus::Started) {
    if(stop.stop_requested()||std::chrono::steady_clock::now()>=deadline){operation.Cancel();throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_TIMEOUT));}
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return operation.GetResults();
}
void load_art(const IRandomAccessStreamReference& reference,MediaSnapshot& value,std::stop_token stop) {
  value.art_pixels.clear(); value.art_width=value.art_height=0; ++value.art_version;
  if(!reference)return;
  try {
    auto stream=completed(reference.OpenReadAsync(),stop);
    if(stream.Size()>8*1024*1024)return;
    auto reader=DataReader(stream); auto size=static_cast<unsigned>(stream.Size());
    completed(reader.LoadAsync(size),stop); std::vector<std::uint8_t> bytes(size);reader.ReadBytes(bytes);
    ComPtr<IStream> memory;memory.reset(SHCreateMemStream(bytes.data(),size));
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICBitmapScaler> scaler;ComPtr<IWICFormatConverter> converter;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IWICImagingFactory),reinterpret_cast<void**>(factory.put()))))return;
    if(FAILED(factory->CreateDecoderFromStream(memory.get(),nullptr,WICDecodeMetadataCacheOnLoad,decoder.put())))return;
    if(FAILED(decoder->GetFrame(0,frame.put())))return;
    if(FAILED(factory->CreateBitmapScaler(scaler.put())))return;
    if(FAILED(scaler->Initialize(frame.get(),148,148,WICBitmapInterpolationModeFant)))return;
    if(FAILED(factory->CreateFormatConverter(converter.put())))return;
    if(FAILED(converter->Initialize(scaler.get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return;
    value.art_pixels.resize(148*148*4);
    if(FAILED(converter->CopyPixels(nullptr,148*4,static_cast<UINT>(value.art_pixels.size()),value.art_pixels.data()))) {value.art_pixels.clear();return;}
    value.art_width=value.art_height=148;
  }catch(...) {value.art_pixels.clear();}
}
}
MediaService::~MediaService(){stop();}
void MediaService::start(Callback callback) {
  if(worker_.joinable())return;callback_=std::move(callback);
  worker_=std::jthread([this](std::stop_token stop){guarded_service("media-worker-exception",[&]{run(stop);});});
}
void MediaService::stop(){if(worker_.joinable()){worker_.request_stop();worker_.join();}}
MediaSnapshot MediaService::snapshot()const{std::scoped_lock lock(mutex_);return snapshot_;}
void MediaService::play_pause(){command_=Command::play_pause;}
void MediaService::next(){command_=Command::next;}
void MediaService::previous(){command_=Command::previous;}
void MediaService::seek(double seconds){seek_seconds_=seconds;command_=Command::seek;}
void MediaService::run(std::stop_token stop) {
  winrt::init_apartment(winrt::apartment_type::multi_threaded);
  try {
    auto manager=completed(Manager::RequestAsync(),stop);
    auto dirty=std::make_shared<std::atomic<bool>>(true);
    auto changed=[dirty](auto const&,auto const&){*dirty=true;};
    auto sessions=manager.SessionsChanged(winrt::auto_revoke,changed);
    auto current=manager.CurrentSessionChanged(winrt::auto_revoke,changed);
    GlobalSystemMediaTransportControlsSession selected{nullptr};
    GlobalSystemMediaTransportControlsSession::MediaPropertiesChanged_revoker properties_event;
    GlobalSystemMediaTransportControlsSession::PlaybackInfoChanged_revoker playback_event;
    GlobalSystemMediaTransportControlsSession::TimelinePropertiesChanged_revoker timeline_event;
    MediaSnapshot value;int ticks=0;std::uint64_t art_generation=0;
    while(!stop.stop_requested()) {
      const auto command=command_.exchange(Command::none);
      if(dirty->exchange(false)||command!=Command::none||(ticks++>=10 && value.playing && visible_)) {
        ticks=0;
        try {
          auto session=manager.GetCurrentSession();
          if(!session){value={};selected=nullptr;properties_event.revoke();playback_event.revoke();timeline_event.revoke();}
          else {
            bool new_session=session!=selected;
            if(new_session) {
              properties_event.revoke();playback_event.revoke();timeline_event.revoke();selected=session;
              properties_event=session.MediaPropertiesChanged(winrt::auto_revoke,changed);
              playback_event=session.PlaybackInfoChanged(winrt::auto_revoke,changed);
              timeline_event=session.TimelinePropertiesChanged(winrt::auto_revoke,changed);
            }
            auto info=session.GetPlaybackInfo();auto controls=info.Controls();
            if(command==Command::play_pause&&controls.IsPlayPauseToggleEnabled())completed(session.TryTogglePlayPauseAsync(),stop);
            if(command==Command::next&&controls.IsNextEnabled())completed(session.TrySkipNextAsync(),stop);
            if(command==Command::previous&&controls.IsPreviousEnabled())completed(session.TrySkipPreviousAsync(),stop);
            if(command==Command::seek&&controls.IsPlaybackPositionEnabled())completed(session.TryChangePlaybackPositionAsync(static_cast<std::int64_t>(seek_seconds_.load()*10000000)),stop);
            auto props=completed(session.TryGetMediaPropertiesAsync(),stop);
            auto source=std::wstring(session.SourceAppUserModelId());
            if(new_session||value.title!=props.Title()||value.artist!=props.Artist()||value.source!=source) {
              load_art(props.Thumbnail(),value,stop);value.art_version=++art_generation;
            }
            value.title=props.Title();value.artist=props.Artist();value.source=source;value.available=true;
            value.playing=info.PlaybackStatus()==GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
            value.can_play=controls.IsPlayPauseToggleEnabled();value.can_next=controls.IsNextEnabled();value.can_previous=controls.IsPreviousEnabled();value.can_seek=controls.IsPlaybackPositionEnabled();
            auto timeline=session.GetTimelineProperties();
            value.position=timeline.Position().count()/10000000.;value.duration=timeline.EndTime().count()/10000000.;
          }
        }catch(...){value={};selected=nullptr;}
        {std::scoped_lock lock(mutex_);snapshot_=value;}if(callback_)callback_();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }catch(...){std::scoped_lock lock(mutex_);snapshot_={};}
  winrt::uninit_apartment();
}
}

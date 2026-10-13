#include "services/audio_service.hpp"
#include "core/diagnostics.hpp"
#include "core/com_ptr.hpp"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <filesystem>
#include <functiondiscoverykeys_devpkey.h>
#include <algorithm>
#include <cmath>
namespace ni {
namespace {
// Private Windows ABI, isolated here. Only SetDefaultEndpoint is called.
struct __declspec(uuid("f8679f50-850a-41cf-9c72-430f290290c8")) PolicyConfig: IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR,WAVEFORMATEX**)=0;
  virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR,INT,WAVEFORMATEX**)=0;
  virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR)=0;
  virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR,WAVEFORMATEX*,WAVEFORMATEX*)=0;
  virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR,INT,PINT64,PINT64)=0;
  virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR,PINT64)=0;
  virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR,void*)=0;
  virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR,void*)=0;
  virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR,INT,const PROPERTYKEY&,PROPVARIANT*)=0;
  virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR,INT,const PROPERTYKEY&,const PROPVARIANT*)=0;
  virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR,ERole)=0;
  virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR,INT)=0;
};
const CLSID policy_class{0x870af99c,0x171d,0x4f9e,{0xaf,0x0d,0xe6,0x3d,0xf4,0x0c,0x2b,0xc9}};
std::wstring name(IMMDevice* device){ComPtr<IPropertyStore> store;PROPVARIANT value{};std::wstring result=L"Ses aygıtı";if(SUCCEEDED(device->OpenPropertyStore(STGM_READ,store.put()))&&SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName,&value))&&value.vt==VT_LPWSTR&&value.pwszVal)result=value.pwszVal;PropVariantClear(&value);return result;}
std::wstring id(IMMDevice* device){PWSTR raw{};std::wstring value;if(SUCCEEDED(device->GetId(&raw))){value=raw;CoTaskMemFree(raw);}return value;}
HRESULT endpoint(IMMDeviceEnumerator* devices,bool microphone,ComPtr<IAudioEndpointVolume>& volume,ComPtr<IMMDevice>& device){HRESULT hr=devices->GetDefaultAudioEndpoint(microphone?eCapture:eRender,eConsole,device.put());return FAILED(hr)?hr:device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(volume.put()));}
}
namespace {
template<class F> void sessions(IMMDeviceEnumerator* devices,F visit){
  ComPtr<IMMDeviceCollection> endpoints;if(FAILED(devices->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,endpoints.put())))return;
  UINT count{};endpoints->GetCount(&count);
  for(UINT e=0;e<count;++e){ComPtr<IMMDevice> device;ComPtr<IAudioSessionManager2> manager;ComPtr<IAudioSessionEnumerator> list;
    if(FAILED(endpoints->Item(e,device.put()))||FAILED(device->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(manager.put())))||FAILED(manager->GetSessionEnumerator(list.put())))continue;
    int n{};list->GetCount(&n);for(int i=0;i<n&&i<128;++i){ComPtr<IAudioSessionControl> control;ComPtr<IAudioSessionControl2> extended;ComPtr<ISimpleAudioVolume> volume;
      if(FAILED(list->GetSession(i,control.put()))||FAILED(control->QueryInterface(__uuidof(IAudioSessionControl2),reinterpret_cast<void**>(extended.put())))||FAILED(control->QueryInterface(__uuidof(ISimpleAudioVolume),reinterpret_cast<void**>(volume.put()))))continue;
      AudioSessionState state{};if(FAILED(control->GetState(&state))||state==AudioSessionStateExpired)continue;
      LPWSTR raw{};if(FAILED(extended->GetSessionInstanceIdentifier(&raw))||!raw)continue;std::wstring key=id(device.get())+L"|"+raw;CoTaskMemFree(raw);visit(key,extended.get(),volume.get());
    }
  }
}
std::wstring session_name(IAudioSessionControl2* control,std::wstring& app_key){
  if(control->IsSystemSoundsSession()==S_OK){app_key=L"windows:system-sounds";return L"Sistem sesleri";}
  DWORD pid{};control->GetProcessId(&pid);HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
  wchar_t path[32768]{};DWORD size=32768;bool ok=process&&QueryFullProcessImageNameW(process,0,path,&size);if(process)CloseHandle(process);
  if(ok){app_key=path;return std::filesystem::path(path).stem().wstring();}LPWSTR raw{};std::wstring result=L"Uygulama "+std::to_wstring(pid);if(SUCCEEDED(control->GetDisplayName(&raw))&&raw){if(*raw&&*raw!=L'@')result=raw;CoTaskMemFree(raw);}return result;
}
}
void AudioService::start(std::function<void()> callback){changed_=std::move(callback);worker_=std::jthread([this](std::stop_token stop){guarded_service("audio-worker-exception",[&]{run(stop);});});}
void AudioService::stop(){if(worker_.joinable()){worker_.request_stop();cv_.notify_all();worker_.join();}}
AudioSnapshot AudioService::snapshot()const{std::scoped_lock lock(mutex_);return snapshot_;}
void AudioService::queue(Command command){std::scoped_lock lock(mutex_);if(command.type==0&&!commands_.empty()&&commands_.back().type==0)commands_.back()=std::move(command);else commands_.push_back(std::move(command));cv_.notify_one();}
void AudioService::volume(float value){queue({0,std::clamp(value,0.f,1.f),{}});}
void AudioService::mute(bool microphone,bool value){queue({microphone?2:1,value?1.f:0.f,{}});}
void AudioService::output(std::wstring output){queue({3,0,std::move(output)});}
void AudioService::session_volume(std::wstring id,float value){queue({4,std::clamp(value,0.f,1.f),std::move(id)});}
void AudioService::session_mute(std::wstring id,bool value){queue({5,value?1.f:0.f,std::move(id)});}
void AudioService::run(std::stop_token stop){
  HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);ComPtr<IMMDeviceEnumerator> devices;
  if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(devices.put())))){if(SUCCEEDED(apartment))CoUninitialize();return;}
  AudioSnapshot current;unsigned tick=0;{ComPtr<PolicyConfig> policy;current.output_switch_supported=SUCCEEDED(CoCreateInstance(policy_class,nullptr,CLSCTX_ALL,__uuidof(PolicyConfig),reinterpret_cast<void**>(policy.put())));}
  while(!stop.stop_requested()){
    std::vector<Command> commands;{std::scoped_lock lock(mutex_);commands.swap(commands_);}
    for(auto& command:commands){HRESULT hr{};
      if(command.type>=4){hr=HRESULT_FROM_WIN32(ERROR_NOT_FOUND);sessions(devices.get(),[&](const std::wstring& key,IAudioSessionControl2*,ISimpleAudioVolume* volume){if(key==command.id)hr=command.type==4?volume->SetMasterVolume(command.value,nullptr):volume->SetMute(command.value!=0,nullptr);});
      }else if(command.type==3){ComPtr<IMMDevice> target;DWORD state{};hr=devices->GetDevice(command.id.c_str(),target.put());if(SUCCEEDED(hr))hr=target->GetState(&state);if(SUCCEEDED(hr)&&!(state&DEVICE_STATE_ACTIVE))hr=E_FAIL;
        ComPtr<PolicyConfig> policy;if(SUCCEEDED(hr))hr=CoCreateInstance(policy_class,nullptr,CLSCTX_ALL,__uuidof(PolicyConfig),reinterpret_cast<void**>(policy.put()));
        // Console is the role used by our controls. Communications is untouched.
        ComPtr<IMMDevice> old_console,old_multimedia;devices->GetDefaultAudioEndpoint(eRender,eConsole,old_console.put());devices->GetDefaultAudioEndpoint(eRender,eMultimedia,old_multimedia.put());
        if(SUCCEEDED(hr))hr=policy->SetDefaultEndpoint(command.id.c_str(),eConsole);
        if(SUCCEEDED(hr)){hr=policy->SetDefaultEndpoint(command.id.c_str(),eMultimedia);if(FAILED(hr)){if(old_console)policy->SetDefaultEndpoint(id(old_console.get()).c_str(),eConsole);if(old_multimedia)policy->SetDefaultEndpoint(id(old_multimedia.get()).c_str(),eMultimedia);}}
      }else{ComPtr<IAudioEndpointVolume> volume;ComPtr<IMMDevice> device;hr=endpoint(devices.get(),command.type==2,volume,device);if(SUCCEEDED(hr)){if(command.type==0)hr=volume->SetMasterVolumeLevelScalar(command.value,nullptr);else hr=volume->SetMute(command.value!=0,nullptr);}}
      current.error=FAILED(hr)?L"Ses işlemi başarısız · Windows ses ayarlarını kullanın":L"";tick=0;
    }
    ComPtr<IAudioEndpointVolume> output,mic;ComPtr<IMMDevice> output_device,mic_device;BOOL mute{};
    current.available=SUCCEEDED(endpoint(devices.get(),false,output,output_device));current.mic_available=SUCCEEDED(endpoint(devices.get(),true,mic,mic_device));
    if(current.available){output->GetMasterVolumeLevelScalar(&current.volume);output->GetMute(&mute);current.muted=mute!=FALSE;auto new_id=id(output_device.get());if(new_id!=current.output_id||tick%20==0)current.output_name=name(output_device.get());current.output_id=std::move(new_id);}else{current.volume=0;current.muted=false;current.output_id.clear();current.output_name=L"Ses çıkışı yok";}
    if(current.mic_available){mic->GetMute(&mute);current.mic_muted=mute!=FALSE;if(tick%20==0)current.mic_name=name(mic_device.get());}else{current.mic_muted=false;current.mic_name=L"Mikrofon yok";}
    if(tick++%20==0){current.outputs.clear();ComPtr<IMMDeviceCollection> list;if(SUCCEEDED(devices->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,list.put()))){UINT count{};list->GetCount(&count);for(UINT i=0;i<count;++i){ComPtr<IMMDevice> item;if(SUCCEEDED(list->Item(i,item.put())))current.outputs.push_back({id(item.get()),name(item.get())});}}}
    current.sessions.clear();sessions(devices.get(),[&](const std::wstring& key,IAudioSessionControl2* control,ISimpleAudioVolume* volume){AudioSession s;s.id=key;s.name=session_name(control,s.app_key);BOOL muted{};if(SUCCEEDED(volume->GetMasterVolume(&s.volume))&&SUCCEEDED(volume->GetMute(&muted))){s.muted=muted!=FALSE;current.sessions.push_back(std::move(s));}});
    std::sort(current.sessions.begin(),current.sessions.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    bool changed=false;{std::scoped_lock lock(mutex_);changed=snapshot_.sessions!=current.sessions||snapshot_.available!=current.available||snapshot_.mic_available!=current.mic_available||snapshot_.muted!=current.muted||snapshot_.mic_muted!=current.mic_muted||std::abs(snapshot_.volume-current.volume)>.002f||snapshot_.output_id!=current.output_id||snapshot_.error!=current.error||snapshot_.outputs!=current.outputs||snapshot_.output_name!=current.output_name||snapshot_.mic_name!=current.mic_name;snapshot_=current;}
    if(changed&&changed_)changed_();
    std::unique_lock lock(mutex_);cv_.wait_for(lock,stop,std::chrono::milliseconds(100),[this]{return !commands_.empty();});
  }
  devices.reset();if(SUCCEEDED(apartment))CoUninitialize();
}
}

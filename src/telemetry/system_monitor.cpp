#include "telemetry/system_monitor.hpp"
#include "core/diagnostics.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <algorithm>
#include <vector>
#include <chrono>
#include "core/com_ptr.hpp"
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>

namespace ni {
namespace {
std::uint64_t u64(const FILETIME& time) {
  ULARGE_INTEGER value{}; value.LowPart = time.dwLowDateTime; value.HighPart = time.dwHighDateTime; return value.QuadPart;
}
struct NetworkSample { std::uint64_t in{}; std::uint64_t out{}; std::wstring name{L"Veri yok"}; };
NetworkSample network_sample() {
  NetworkSample result;
  PMIB_IF_TABLE2 table{};
  if (GetIfTable2(&table) != NO_ERROR) return result;
  std::uint64_t best{};
  for (ULONG i = 0; i < table->NumEntries; ++i) {
    const auto& row = table->Table[i];
    if (row.OperStatus != IfOperStatusUp || row.Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
    result.in += row.InOctets; result.out += row.OutOctets;
    const auto traffic = row.InOctets + row.OutOctets;
    if (traffic >= best) { best = traffic; result.name = row.Alias; }
  }
  FreeMibTable(table);
  return result;
}
std::vector<std::wstring> bluetooth_audio_devices() {
  std::vector<GUID> containers;std::vector<std::wstring> names;
  HDEVINFO devices=SetupDiGetClassDevsW(nullptr,nullptr,nullptr,DIGCF_PRESENT|DIGCF_ALLCLASSES);
  if(devices==INVALID_HANDLE_VALUE)return names;
  SP_DEVINFO_DATA info{sizeof(info)};
  for(DWORD i=0;SetupDiEnumDeviceInfo(devices,i,&info);++i) {
    wchar_t instance[512]{};if(!SetupDiGetDeviceInstanceIdW(devices,&info,instance,512,nullptr))continue;
    // Radio/host devnodes can share the computer's container with wired audio.
    // Only remote Bluetooth devnodes may identify a Bluetooth sound endpoint.
    if(wcsncmp(instance,L"BTHENUM\\DEV_",12)!=0 && wcsncmp(instance,L"BTHLEDEVICE\\",12)!=0)continue;
    GUID container{};DEVPROPTYPE type{};
    if(SetupDiGetDevicePropertyW(devices,&info,&DEVPKEY_Device_ContainerId,&type,reinterpret_cast<BYTE*>(&container),sizeof(container),nullptr,0)&&type==DEVPROP_TYPE_GUID&&!IsEqualGUID(container,GUID_NULL))containers.push_back(container);
  }
  SetupDiDestroyDeviceInfoList(devices);
  ComPtr<IMMDeviceEnumerator> enumerator;ComPtr<IMMDeviceCollection> endpoints;
  if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(enumerator.put())))||
     FAILED(enumerator->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,endpoints.put())))return names;
  UINT count{};endpoints->GetCount(&count);
  for(UINT i=0;i<count;++i) {
    ComPtr<IMMDevice> endpoint;ComPtr<IPropertyStore> properties;
    if(FAILED(endpoints->Item(i,endpoint.put()))||FAILED(endpoint->OpenPropertyStore(STGM_READ,properties.put())))continue;
    PROPVARIANT container{};PropVariantInit(&container);
    bool bluetooth=false;
    if(SUCCEEDED(properties->GetValue(PKEY_Device_ContainerId,&container))&&container.vt==VT_CLSID&&container.puuid)
      bluetooth=std::any_of(containers.begin(),containers.end(),[&](auto& id){return IsEqualGUID(id,*container.puuid);});
    PropVariantClear(&container);
    if(!bluetooth)continue;
    PROPVARIANT friendly{};PropVariantInit(&friendly);
    if(SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName,&friendly))&&friendly.vt==VT_LPWSTR&&friendly.pwszVal)names.emplace_back(friendly.pwszVal);
    PropVariantClear(&friendly);
  }
  return names;
}
}

SystemMonitor::~SystemMonitor() { stop(); }
void SystemMonitor::start(Callback callback) {
  if (worker_.joinable()) return;
  callback_ = std::move(callback);
  worker_ = std::jthread([this](std::stop_token stop) { guarded_service("telemetry-worker-exception",[&]{run(stop);}); });
}
void SystemMonitor::stop() { if (worker_.joinable()) { worker_.request_stop(); worker_.join(); } }
SystemSnapshot SystemMonitor::snapshot() const { std::scoped_lock lock(mutex_); return snapshot_; }

void SystemMonitor::run(std::stop_token stop) {
  HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  std::vector<std::wstring> bluetooth;unsigned bluetooth_ticks=0;
  PDH_HQUERY gpu_query{};PDH_HCOUNTER gpu_counter{};
  if(PdhOpenQueryW(nullptr,0,&gpu_query)==ERROR_SUCCESS) {
    if(PdhAddEnglishCounterW(gpu_query,L"\\GPU Engine(*)\\Utilization Percentage",0,&gpu_counter)!=ERROR_SUCCESS)gpu_counter=nullptr;
    PdhCollectQueryData(gpu_query);
  }
  FILETIME idle{}, kernel{}, user{};
  GetSystemTimes(&idle, &kernel, &user);
  auto old_idle = u64(idle), old_kernel = u64(kernel), old_user = u64(user);
  auto old_net = network_sample();
  auto previous = std::chrono::steady_clock::now();
  while (!stop.stop_requested()) {
    for (int i = 0; i < (visible_?10:100) && !stop.stop_requested(); ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (stop.stop_requested()) break;
    GetSystemTimes(&idle, &kernel, &user);
    const auto now_idle = u64(idle), now_kernel = u64(kernel), now_user = u64(user);
    const auto total = (now_kernel - old_kernel) + (now_user - old_user);
    const auto idle_delta = now_idle - old_idle;
    MEMORYSTATUSEX memory{sizeof(memory)}; GlobalMemoryStatusEx(&memory);
    const auto net = network_sample();
    const auto now = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(now - previous).count();
    SystemSnapshot value;
    if(bluetooth_ticks++%10==0)bluetooth=bluetooth_audio_devices();
    value.bluetooth_audio=bluetooth;
    value.ready=true;
    if(gpu_counter && PdhCollectQueryData(gpu_query)==ERROR_SUCCESS) {
      DWORD bytes=0,count=0;
      if(PdhGetFormattedCounterArrayW(gpu_counter,PDH_FMT_DOUBLE,&bytes,&count,nullptr)==PDH_MORE_DATA && bytes<16*1024*1024) {
        std::vector<BYTE> storage(bytes);
        auto* items=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(storage.data());
        if(PdhGetFormattedCounterArrayW(gpu_counter,PDH_FMT_DOUBLE,&bytes,&count,items)==ERROR_SUCCESS) {
          for(DWORD index=0;index<count;++index) {
            if(items[index].FmtValue.CStatus==PDH_CSTATUS_VALID_DATA||items[index].FmtValue.CStatus==PDH_CSTATUS_NEW_DATA) {
              value.gpu_available=true;value.gpu_percent=std::max(value.gpu_percent,items[index].FmtValue.doubleValue);
            }
          }
          value.gpu_percent=std::clamp(value.gpu_percent,0.,100.);
        }
      }
    }
    wchar_t root[MAX_PATH]{}; GetWindowsDirectoryW(root,MAX_PATH); root[3]=0;
    ULARGE_INTEGER free{},total_disk{};
    if(GetDiskFreeSpaceExW(root,nullptr,&total_disk,&free)) {
      value.disk_total=total_disk.QuadPart; value.disk_used=total_disk.QuadPart-free.QuadPart;
    }
    value.cpu_percent = total ? 100.0 * static_cast<double>(total - idle_delta) / static_cast<double>(total) : 0.0;
    value.ram_total = memory.ullTotalPhys; value.ram_used = memory.ullTotalPhys - memory.ullAvailPhys;
    value.download_bytes_per_sec = net.in >= old_net.in ? static_cast<double>(net.in - old_net.in) / seconds : 0.0;
    value.upload_bytes_per_sec = net.out >= old_net.out ? static_cast<double>(net.out - old_net.out) / seconds : 0.0;
    value.adapter = net.name; value.uptime_seconds = GetTickCount64() / 1000;
    { std::scoped_lock lock(mutex_); snapshot_ = std::move(value); }
    old_idle = now_idle; old_kernel = now_kernel; old_user = now_user; old_net = net; previous = now;
    if (callback_) callback_();
  }
  if(gpu_query)PdhCloseQuery(gpu_query);
  if(SUCCEEDED(apartment))CoUninitialize();
}
}


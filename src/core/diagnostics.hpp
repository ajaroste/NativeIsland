#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <exception>
#include <typeinfo>
#include <winrt/base.h>

namespace ni {
// Only stage names and numeric error codes: never clipboard, calendar or tokens.
inline void diagnostic(const char* stage, unsigned long code=0) noexcept {
  try {
    static std::mutex mutex;
    std::scoped_lock lock(mutex);
    wchar_t base[32768]{};
    DWORD size=GetEnvironmentVariableW(L"LOCALAPPDATA",base,32768);
    if(!size||size>=32768)return;
    auto directory=std::filesystem::path(base)/L"NativeIsland";
    std::error_code error;std::filesystem::create_directories(directory,error);if(error)return;
    auto file=directory/L"startup.log";
    auto length=std::filesystem::file_size(file,error);
    std::ofstream out(file,(!error&&length>256*1024)?std::ios::trunc:std::ios::app);
    SYSTEMTIME now{};GetLocalTime(&now);
    out<<now.wYear<<'-'<<now.wMonth<<'-'<<now.wDay<<' '<<now.wHour<<':'<<now.wMinute<<':'<<now.wSecond
       <<" pid="<<GetCurrentProcessId()<<" tid="<<GetCurrentThreadId()<<' '<<stage<<" code="<<code<<'\n';
  }catch(...){}
}
template<class Work> bool guarded_service(const char* stage,Work&& work) noexcept {
  try {work();return true;}
  catch(const winrt::hresult_error& error){diagnostic(stage,static_cast<unsigned long>(error.code().value));}
  catch(const std::exception& error){diagnostic(stage,ERROR_UNHANDLED_EXCEPTION);diagnostic(typeid(error).name(),ERROR_UNHANDLED_EXCEPTION);}
  catch(...){diagnostic(stage,ERROR_UNHANDLED_EXCEPTION);}
  return false;
}
}

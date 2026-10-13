#pragma once
#include "core/com_ptr.hpp"
#include <shlobj.h>
#include <shobjidl.h>
#include <shldisp.h>
#include <string>
#include <filesystem>
namespace ni {
inline HRESULT file_data_object(const std::wstring& path,ComPtr<IDataObject>& data) {
  ComPtr<IShellItem> item;
  auto normalized=std::filesystem::path(path).make_preferred().wstring();
  HRESULT hr=SHCreateItemFromParsingName(normalized.c_str(),nullptr,__uuidof(IShellItem),reinterpret_cast<void**>(item.put()));
  if(FAILED(hr))return hr;
  return item->BindToHandler(nullptr,BHID_DataObject,__uuidof(IDataObject),reinterpret_cast<void**>(data.put()));
}
inline HRESULT drag_file_copy(HWND window,const std::wstring& path) {
  ComPtr<IDataObject> data;HRESULT hr=file_data_object(path,data);if(FAILED(hr))return hr;
  ComPtr<IDataObjectAsyncCapability> asynchronous;
  if(SUCCEEDED(data.as(asynchronous)))asynchronous->SetAsyncMode(TRUE);
  DWORD effect{};return SHDoDragDrop(window,data.get(),nullptr,DROPEFFECT_COPY,&effect);
}
}

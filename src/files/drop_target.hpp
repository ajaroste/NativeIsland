#pragma once
#include <windows.h>
#include <oleidl.h>
#include <shellapi.h>
#include <atomic>
#include <functional>
#include <vector>
#include <string>

namespace ni {
class DropTarget final:public IDropTarget {
public:
  std::function<void(bool)> hovering;
  std::function<void(std::vector<std::wstring>)> dropped;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** value)override {
    if(!value)return E_POINTER;*value=nullptr;
    if(id==IID_IUnknown||id==IID_IDropTarget){*value=static_cast<IDropTarget*>(this);AddRef();return S_OK;}return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef()override{return ++references_;}
  ULONG STDMETHODCALLTYPE Release()override{ULONG result=--references_;if(!result)delete this;return result;}
  HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* object,DWORD,POINTL,DWORD* effect)override {
    FORMATETC format{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};accepted_=object&&SUCCEEDED(object->QueryGetData(&format));
    *effect=accepted_?(*effect&DROPEFFECT_COPY):DROPEFFECT_NONE;if(hovering)hovering(accepted_);return S_OK;
  }
  HRESULT STDMETHODCALLTYPE DragOver(DWORD,POINTL,DWORD* effect)override{*effect=accepted_?(*effect&DROPEFFECT_COPY):DROPEFFECT_NONE;return S_OK;}
  HRESULT STDMETHODCALLTYPE DragLeave()override{accepted_=false;if(hovering)hovering(false);return S_OK;}
  HRESULT STDMETHODCALLTYPE Drop(IDataObject* object,DWORD,POINTL,DWORD* effect)override {
    FORMATETC format{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};STGMEDIUM medium{};std::vector<std::wstring> files;
    if(object&&SUCCEEDED(object->GetData(&format,&medium))) {
      HDROP drop=static_cast<HDROP>(medium.hGlobal);UINT count=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);
      for(UINT i=0;i<count&&files.size()<32;++i){UINT length=DragQueryFileW(drop,i,nullptr,0);if(length>32767)continue;std::wstring name(length+1,L'\0');DragQueryFileW(drop,i,name.data(),length+1);name.resize(length);files.push_back(std::move(name));}
      ReleaseStgMedium(&medium);
    }
    *effect=files.empty()?DROPEFFECT_NONE:DROPEFFECT_COPY;if(dropped&&!files.empty())dropped(std::move(files));
    return DragLeave();
  }
private:
  std::atomic<ULONG> references_{1};bool accepted_{};
};
}

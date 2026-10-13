#pragma once
#include "core/com_ptr.hpp"
#include <windows.h>
#include <wincodec.h>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <climits>
namespace ni {
struct ClipboardEntry {
  uint64_t id{};
  ULONGLONG created_at{GetTickCount64()};
  bool pinned{};
  std::wstring text;
  std::vector<unsigned char> dib,pixels;
  UINT width{},height{};
  bool image()const{return !pixels.empty();}
  bool operator==(const ClipboardEntry& other)const{return text==other.text&&dib==other.dib;}
};
inline bool decode_clip_image(ClipboardEntry& entry) {
  if(entry.dib.size()<sizeof(BITMAPINFOHEADER)||entry.dib.size()>8*1024*1024)return false;
  BITMAPINFOHEADER header{};memcpy(&header,entry.dib.data(),sizeof(header));
  if(header.biSize<40||header.biSize>entry.dib.size()||header.biWidth<=0||header.biHeight==0||header.biHeight==LONG_MIN)return false;
  if(header.biWidth>4096||std::abs(header.biHeight)>4096)return false;
  DWORD offset=header.biSize;
  if(header.biSize==40&&header.biCompression==BI_BITFIELDS)offset+=12;
  if(header.biSize==40&&header.biCompression==6)offset+=16;
  if(header.biBitCount<=8)offset+=4*(header.biClrUsed?header.biClrUsed:(1u<<header.biBitCount));
  if(offset>entry.dib.size())return false;
  BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfSize=static_cast<DWORD>(sizeof(file)+entry.dib.size());file.bfOffBits=sizeof(file)+offset;
  std::vector<unsigned char> bytes(file.bfSize);memcpy(bytes.data(),&file,sizeof(file));memcpy(bytes.data()+sizeof(file),entry.dib.data(),entry.dib.size());
  ComPtr<IWICImagingFactory> factory;ComPtr<IWICStream> stream;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
  if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IWICImagingFactory),reinterpret_cast<void**>(factory.put())))||
     FAILED(factory->CreateStream(stream.put()))||FAILED(stream->InitializeFromMemory(bytes.data(),static_cast<DWORD>(bytes.size())))||
     FAILED(factory->CreateDecoderFromStream(stream.get(),nullptr,WICDecodeMetadataCacheOnLoad,decoder.put()))||FAILED(decoder->GetFrame(0,frame.put()))||
     FAILED(factory->CreateFormatConverter(converter.put()))||FAILED(converter->Initialize(frame.get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return false;
  converter->GetSize(&entry.width,&entry.height);
  if(!entry.width||!entry.height||static_cast<uint64_t>(entry.width)*entry.height>4*1024*1024)return false;
  entry.pixels.resize(static_cast<size_t>(entry.width)*entry.height*4);
  if(FAILED(converter->CopyPixels(nullptr,entry.width*4,static_cast<UINT>(entry.pixels.size()),entry.pixels.data()))){entry.pixels.clear();return false;}return true;
}
inline ClipboardEntry read_clipboard(HWND owner) {
  ClipboardEntry entry;entry.id=GetClipboardSequenceNumber();if(!OpenClipboard(owner))return entry;
  UINT format=IsClipboardFormatAvailable(CF_DIBV5)?CF_DIBV5:CF_DIB;
  if(HANDLE data=GetClipboardData(format)) {
    size_t size=GlobalSize(data);if(size&&size<=8*1024*1024){if(void* buffer=GlobalLock(data)){entry.dib.assign(static_cast<unsigned char*>(buffer),static_cast<unsigned char*>(buffer)+size);GlobalUnlock(data);}}
  }else if(HBITMAP image=static_cast<HBITMAP>(GetClipboardData(CF_BITMAP))) {
    BITMAP bitmap{};GetObjectW(image,sizeof(bitmap),&bitmap);
    if(bitmap.bmWidth>0&&bitmap.bmHeight>0&&static_cast<uint64_t>(bitmap.bmWidth)*bitmap.bmHeight<=2*1024*1024){
      BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=bitmap.bmWidth;info.bmiHeader.biHeight=bitmap.bmHeight;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
      entry.dib.resize(sizeof(BITMAPINFOHEADER)+static_cast<size_t>(bitmap.bmWidth)*bitmap.bmHeight*4);memcpy(entry.dib.data(),&info.bmiHeader,sizeof(BITMAPINFOHEADER));HDC dc=GetDC(nullptr);if(!GetDIBits(dc,image,0,bitmap.bmHeight,entry.dib.data()+sizeof(BITMAPINFOHEADER),&info,DIB_RGB_COLORS))entry.dib.clear();ReleaseDC(nullptr,dc);
    }
  }
  if(entry.dib.empty())if(HANDLE data=GetClipboardData(CF_UNICODETEXT))if(auto* buffer=static_cast<const wchar_t*>(GlobalLock(data))){size_t length=std::min<size_t>(GlobalSize(data)/sizeof(wchar_t),1024*1024);size_t used=0;while(used<length&&buffer[used])++used;entry.text.assign(buffer,used);GlobalUnlock(data);}
  CloseClipboard();if(!entry.dib.empty()&&!decode_clip_image(entry))entry={};return entry;
}
inline bool copy_clipboard(HWND owner,const ClipboardEntry& entry) {
  size_t bytes=entry.image()?entry.dib.size():(entry.text.size()+1)*sizeof(wchar_t);
  HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!data)return false;
  void* buffer=GlobalLock(data);if(!buffer){GlobalFree(data);return false;}
  memcpy(buffer,entry.image()?static_cast<const void*>(entry.dib.data()):static_cast<const void*>(entry.text.c_str()),bytes);GlobalUnlock(data);
  if(!OpenClipboard(owner)){GlobalFree(data);return false;}bool ok=EmptyClipboard()&&SetClipboardData(entry.image()?CF_DIB:CF_UNICODETEXT,data);CloseClipboard();if(!ok)GlobalFree(data);return ok;
}
// A standard scrollable editor preserves line breaks and exposes the full text.
inline void show_clipboard_preview(HINSTANCE instance,HWND owner,const ClipboardEntry& entry) {
  if(!entry.image()) {
    WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=[](HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)->LRESULT{
      if(msg==WM_SIZE){MoveWindow(GetDlgItem(hwnd,1),12,12,LOWORD(lp)-24,HIWORD(lp)-24,TRUE);return 0;}
      if(msg==WM_CLOSE){DestroyWindow(hwnd);return 0;}return DefWindowProcW(hwnd,msg,wp,lp);};wc.lpszClassName=L"NativeIslandTextPreview";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
    HWND window=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Pano · Tam metin",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,720,520,owner,nullptr,instance,nullptr);
    std::wstring display;display.reserve(entry.text.size());for(size_t i=0;i<entry.text.size();++i){if(entry.text[i]==L'\n'&&(i==0||entry.text[i-1]!=L'\r'))display+=L'\r';display+=entry.text[i];}
    HWND edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",display.c_str(),WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,12,12,680,450,window,reinterpret_cast<HMENU>(1),instance,nullptr);
    SendMessageW(edit,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);RECT r{};GetClientRect(window,&r);MoveWindow(edit,12,12,r.right-24,r.bottom-24,TRUE);SetForegroundWindow(window);return;
  }
  WNDCLASSW wc{};wc.hInstance=instance;wc.lpszClassName=L"NativeIslandImagePreview";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
  wc.lpfnWndProc=[](HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)->LRESULT{
    auto* image=reinterpret_cast<ClipboardEntry*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){image=static_cast<ClipboardEntry*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(image));}
    if(msg==WM_PAINT&&image){PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);RECT r{};GetClientRect(hwnd,&r);FillRect(dc,&r,reinterpret_cast<HBRUSH>(COLOR_WINDOW+1));float scale=std::min((r.right-24.f)/image->width,(r.bottom-24.f)/image->height);int w=static_cast<int>(image->width*scale),h=static_cast<int>(image->height*scale);BITMAPINFO info{};info.bmiHeader.biSize=40;info.bmiHeader.biWidth=image->width;info.bmiHeader.biHeight=-static_cast<LONG>(image->height);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;SetStretchBltMode(dc,HALFTONE);StretchDIBits(dc,(r.right-w)/2,(r.bottom-h)/2,w,h,0,0,image->width,image->height,image->pixels.data(),&info,DIB_RGB_COLORS,SRCCOPY);EndPaint(hwnd,&ps);return 0;}
    if(msg==WM_SIZE){InvalidateRect(hwnd,nullptr,TRUE);return 0;}if(msg==WM_NCDESTROY){delete image;SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);}return DefWindowProcW(hwnd,msg,wp,lp);
  };RegisterClassW(&wc);
  HWND window=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Pano · Görsel",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,720,520,owner,nullptr,instance,nullptr);
  if(window){SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(new ClipboardEntry(entry)));ShowWindow(window,SW_SHOW);SetForegroundWindow(window);}
}
}

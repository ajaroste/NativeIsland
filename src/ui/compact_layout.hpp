#pragma once
#include "ui/view_model.hpp"
#include <algorithm>
namespace ni {
inline int page_height(Page page,size_t files,size_t clips,bool clipboard,bool bluetooth) {
  int height=250;
  switch(page) {
    case Page::widgets:height=330;break;
    case Page::media:height=250;break;
    case Page::sound:height=248+66*static_cast<int>(std::min<size_t>(4,files));break;
    case Page::tasks:height=files?174+60*static_cast<int>(std::min<size_t>(4,files)):254;break;
    case Page::files:height=files?182+44*static_cast<int>(std::min<size_t>(3,files)):224;break;
    case Page::clipboard:height=clipboard&&clips?195+43*static_cast<int>(std::min<size_t>(4,clips)):282;break;
    case Page::applications:height=files?126+112*static_cast<int>((std::min<size_t>(8,files)+3)/4):194;break;
    case Page::reminders:height=files?260+54*static_cast<int>(std::min<size_t>(4,files)):324;break;
  }
  if(bluetooth&&(page==Page::widgets||page==Page::media))height+=26;
  return height;
}
}

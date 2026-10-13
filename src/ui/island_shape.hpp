#pragma once
#include <algorithm>
#include <cmath>
namespace ni {
inline bool island_contains(float x,float y,float w,float h,float radius,bool droplet){
  if(y<0||y>=h)return false;
  if(droplet&&y<16){float edge=-16+std::sqrt(std::max(0.f,256-(y-16)*(y-16)));return x>=edge&&x<w-edge;}
  if(x<0||x>=w)return false;
  if(!droplet&&y<radius){if(x<radius)return std::hypot(x-radius,y-radius)<=radius;if(x>w-radius)return std::hypot(x-(w-radius),y-radius)<=radius;}
  if(y>h-radius){if(x<radius)return std::hypot(x-radius,y-(h-radius))<=radius;if(x>w-radius)return std::hypot(x-(w-radius),y-(h-radius))<=radius;}
  return true;
}
}

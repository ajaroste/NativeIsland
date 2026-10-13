#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
namespace ni {
class AnimationManager {
public:
  void begin(float width,float height,float target_width,float target_height,float seconds,int style=0) {
    style_=std::clamp(style,0,3);
    from_width_=width;from_height_=height;to_width_=target_width;to_height_=target_height;
    duration_=std::max(.01f,seconds);start_=std::chrono::steady_clock::now();active_=true;
  }
  void cancel(){active_=false;}
  bool active()const{return active_;}
  struct Frame{float width,height;};
  Frame frame() {
    if(style_==3){active_=false;return {to_width_,to_height_};}
    float t=std::chrono::duration<float>(std::chrono::steady_clock::now()-start_).count()/duration_;
    if(t>=1){active_=false;return {to_width_,to_height_};}
    float amount=ease(std::clamp(t,0.f,1.f),style_);
    return {from_width_+(to_width_-from_width_)*amount,from_height_+(to_height_-from_height_)*amount};
  }
  static float ease(float progress,int style=0) {
    float t=std::clamp(progress,0.f,1.f);
    if(t==0||t==1)return t;
    if(style==3)return 1;
    if(style==1){float u=t-1;return 1+1.6f*u*u*u+.6f*u*u;}
    if(style==2)return 1-std::pow(1-t,3.f);
    double p=t;
    return static_cast<float>(std::clamp(p*p*p*(p*(6*p-15)+10),0.,1.)); // stable at both endpoints
  }
private:
  int style_{};bool active_{};float from_width_{},from_height_{},to_width_{},to_height_{},duration_{};
  std::chrono::steady_clock::time_point start_{};
};
}

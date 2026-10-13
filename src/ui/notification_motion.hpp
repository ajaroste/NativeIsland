#pragma once
#include <cmath>
namespace ni {
inline float notification_shake(int kind,float age,bool reduced){return !reduced&&kind==1&&age>=0&&age<.3f?3.f*std::sin(age*90)*(1-age/.3f):0.f;}
}

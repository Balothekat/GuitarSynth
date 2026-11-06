#include "Tracker.h"
#include <iostream>
int main() {
 for(double rate:{44100.0,48000.0,96000.0})
  for(double f:{82.4069,110.0,146.832,220.0,440.0,659.255,1318.51}) {
   Tracker t;t.prepare(rate);double hz=0,last=0;int valid=0;
   for(int i=0;i<(int)(rate*0.15);++i) {
    if(t.push((float)(0.2*std::sin(6.283185307179586*f*i/rate)),0.008f,hz)&&hz>0) {
     last=hz;++valid;
    }
   }
   if(valid==0||std::abs(12*std::log2(last/f))>0.15) {
    std::cerr<<"Pitch failure "<<rate<<" "<<f<<" "<<last<<"\n";return 1;
   }
  }
 Tracker quiet;quiet.prepare(48000);double hz=0;
 for(int i=0;i<10000;++i)
  if(quiet.push(0,0.008f,hz)&&hz!=0)return 2;
 std::cout<<"Synthetic pitch and silence checks passed. Not a live performance benchmark.\n";
}

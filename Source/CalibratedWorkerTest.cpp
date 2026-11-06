#include "CalibratedWorker.h"
#include <iostream>
int main(){CalibratedWorker worker;worker.start(48000);CalibratedWorker::Strengths result{};bool correct=false,wrong=false;
 for(int block=0;block<400;++block){for(int i=0;i<128;++i){double x=0;
   for(int h=0;h<12;++h){auto partial=GuitarProfile::partials[2][h];double f=GuitarProfile::openHz[2]*partial.ratio;
    x+=0.025*partial.amplitude*std::sin(6.283185307179586*f*(block*128+i)/48000.0);}
   worker.push((float)x);}
  std::this_thread::sleep_for(std::chrono::milliseconds(3));
  if(worker.consume(result))for(int n=0;n<CalibratedTracker::notes;++n)if(result[n]>0){if(n+CalibratedTracker::first==52)correct=true;else wrong=true;}
 }
 worker.reset();
 for(int block=0;block<150;++block){for(int i=0;i<128;++i)worker.push(0);std::this_thread::sleep_for(std::chrono::milliseconds(3));worker.consume(result);}
 bool silent=true;for(float v:result)silent&=v==0;
 worker.stop();if(!correct||wrong||!silent){std::cerr<<"Worker note/reset regression\n";return 1;}
 std::cout<<"Worker publishes the intended note and clears it on reset.\n";
}

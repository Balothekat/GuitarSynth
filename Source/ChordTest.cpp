#include "ChordTracker.h"
#include "Tracker.h"
#include <iostream>
#include <chrono>
#include <vector>
bool chord(double sr,std::vector<int> expected) {
 ChordTracker tracker;tracker.prepare(sr);
 std::array<int,ChordTracker::notes> hits{};
 int frames=0;double firstHit=-1;
 auto begin=std::chrono::steady_clock::now();
 for(int i=0;i<(int)(sr*0.35);++i) {
  double x=0;
  for(int note:expected) {
   double f=440*std::pow(2.0,(note-69)/12.0);
   for(int h=1;h<=5;++h)x+=0.08/expected.size()*std::sin(6.283185307179586*f*h*i/sr)/std::pow((double)h,1.3);
  }
  if(tracker.push((float)x,0.003f)) {
   bool all=true;for(int note:expected)all&=tracker.getStrengths()[note-ChordTracker::first]>0;
   if(all&&firstHit<0)firstHit=1000.0*i/sr;
   if(i>sr*0.18){++frames;for(int n=0;n<ChordTracker::notes;++n)if(tracker.getStrengths()[n]>0)++hits[n];}
  }
 }
 auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
 bool ok=frames>0;
 for(int n=0;n<ChordTracker::notes;++n) {
  bool wanted=std::find(expected.begin(),expected.end(),n+ChordTracker::first)!=expected.end();
  if((wanted&&hits[n]<frames*0.8)||(!wanted&&hits[n]>frames*0.2)) {
   ok=false;std::cerr<<"Mismatch note "<<n+ChordTracker::first<<" hits "<<hits[n]<<"/"<<frames<<"\n";
  }
 }
 std::cout<<"CHORD "<<sr<<" Hz, ";for(int n:expected)std::cout<<n<<",";
 std::cout<<" first complete detection "<<firstHit<<" ms; CPU/audio ratio "<<elapsed/0.35<<"\n";
 for(int i=0;i<(int)(sr*0.2);++i)tracker.push(0,0.003f);
 for(float v:tracker.getStrengths())if(v>0)ok=false;
 return ok;
}
int main() {
 bool ok=true;
 for(double sr:{44100.0,48000.0,96000.0}) {
  for(auto notes:std::vector<std::vector<int>>{{52,55,59},{57,60,64},{48,52,55},{40,47},{64},{52,53}})
   ok=chord(sr,notes)&&ok;
  for(double f:{82.4069,110.0,220.0,440.0,659.255}) {
   Tracker tracker;tracker.prepare(sr);tracker.setFast(true);double hz=0,delay=-1;int hits=0;
   for(int i=0;i<(int)(sr*0.15);++i)
    if(tracker.push((float)(0.2*std::sin(6.283185307179586*f*i/sr)),0.008f,hz)&&hz>0&&std::abs(12*std::log2(hz/f))<0.15){
     if(delay<0)delay=1000.0*i/sr;++hits;
    }
   std::cout<<"FAST LEAD "<<sr<<" Hz note "<<f<<" first correct estimate "<<delay<<" ms\n";
   if(hits==0||delay>45)ok=false;
  }
 }
 return ok?0:1;
}

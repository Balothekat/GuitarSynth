#include "CalibratedTracker.h"
#include <iostream>
int main(){
 bool ok=true;
 for(double rate:{44100.0,48000.0})for(int s=0;s<6;++s){
  CalibratedTracker tracker;tracker.prepare(rate);int valid=0,falseNotes=0;
  for(int i=0;i<(int)(rate*0.7);++i){double x=0;for(int h=0;h<12;++h){auto p=GuitarProfile::partials[s][h];double f=GuitarProfile::openHz[s]*p.ratio;
    if(f<rate*0.45)x+=0.025*p.amplitude*std::sin(6.283185307179586*f*i/rate);}
   if(tracker.push((float)x,0.008f)&&i>rate*0.25){auto notes=tracker.getStrengths();for(int n=0;n<CalibratedTracker::notes;++n)if(notes[n]>0){if(n+CalibratedTracker::first==GuitarProfile::openNotes[s])++valid;else ++falseNotes;}}
  }
  std::cout<<"Calibrated string "<<s<<" sample rate "<<rate<<" valid frames "<<valid<<" false frames "<<falseNotes<<'\n';
  if(!valid||falseNotes)ok=false;
  for(int i=0;i<(int)(rate*0.3);++i)tracker.push(0,0.008f);
  for(float v:tracker.getStrengths())if(v>0)ok=false;
 }
 for(double rate:{44100.0,48000.0}){
  CalibratedTracker tracker;tracker.prepare(rate);std::array<bool,128> seen{};
  for(int i=0;i<(int)(rate*0.64);++i){double x=0;
   for(int string=0;string<4;++string){double shift=string<2?std::pow(2.0,4.0/12.0):1;
    for(int h=0;h<12;++h){auto p=GuitarProfile::partials[string][h];double f=GuitarProfile::openHz[string]*p.ratio*shift;
     x+=0.012*p.amplitude*std::sin(6.283185307179586*f*i/rate);}}
   if(tracker.push((float)x,0.008f))for(int n=0;n<CalibratedTracker::notes;++n)if(tracker.getStrengths()[n]>0)seen[n+CalibratedTracker::first]=true;
  }
  for(int note=0;note<128;++note){bool expected=note==42||note==49||note==52||note==57;
   if(seen[note]!=expected){ok=false;std::cerr<<"Mixed measured-profile regression note "<<note<<" seen "<<seen[note]<<" rate "<<rate<<'\n';}}
 }
 return ok?0:1;
}

#include "MeasuredLead.h"
#include <iostream>
#include <vector>
static double sample(int string,int fret,double time,double rate){
 double x=0,base=GuitarProfile::openHz[string]*std::pow(2.0,fret/12.0);
 for(auto part:GuitarProfile::partials[string])if(base*part.ratio<rate*0.45)
  x+=0.018*part.amplitude*std::sin(6.283185307179586*base*part.ratio*time);
 return x;
}
int main(){bool ok=true;
 for(double rate:{44100.0,48000.0,96000.0})for(int string=0;string<6;++string)for(int fret:{0,4,7,12,19,24}){
  MeasuredLead tracker;tracker.prepare(rate);int expected=GuitarProfile::openNotes[string]+fret,detected=0;double first=-1;
  for(int i=0;i<(int)(rate*0.18);++i)if(tracker.push((float)sample(string,fret,i/rate,rate),0.008f)){
   int key=tracker.getNote();if(key>=0){if(first<0)first=i/rate;if(key==expected)++detected;else{std::cerr<<"Wrong isolated note sr="<<rate<<" string="<<string<<" fret="<<fret<<" expected="<<expected<<" got="<<key<<" hz="<<tracker.getFrequency()<<'\n';ok=false;break;}}
  }
  if(!detected||first>0.080){std::cerr<<"Missing/late lead sr="<<rate<<" string="<<string<<" fret="<<fret<<" first="<<first<<'\n';ok=false;}
  for(int i=0;i<(int)(rate*0.10);++i)tracker.push(0,0.008f);
  if(tracker.getNote()!=-1){std::cerr<<"Lead stuck after silence\n";ok=false;}
 }
 // Picked adjacent notes and octave jumps, including quieter downward changes.
 for(double rate:{44100.0,48000.0}){
  MeasuredLead tracker;tracker.prepare(rate);std::vector<int> frets{0,2,4,5,7,12,7,5,4,2,0};int previous=-1;
  for(int fret:frets){int target=GuitarProfile::openNotes[1]+fret;bool reached=false;
   for(int i=0;i<(int)(rate*0.16);++i){double time=i/rate,attack=std::min(1.0,time/0.002),decay=std::exp(-time*1.5);
    if(tracker.push((float)(sample(1,fret,time,rate)*attack*decay),0.008f)){
     int key=tracker.getNote();if(key>=0&&key!=target&&key!=previous){std::cerr<<"Transition false note sr="<<rate<<" target="<<target<<" previous="<<previous<<" got="<<key<<'\n';ok=false;}
     if(time>0.09&&key==target)reached=true;
    }
   }
   if(!reached){std::cerr<<"Transition missed target "<<target<<" sr="<<rate<<'\n';ok=false;}previous=target;
  }
 }
 // A decaying fundamental with a longer-lived second partial must not turn
 // into a fresh octave note. This is a common guitar-release failure mode.
 for(double rate:{44100.0,48000.0}){
  MeasuredLead tracker;tracker.prepare(rate);double f=GuitarProfile::openHz[1];bool root=false;
  for(int i=0;i<(int)(rate*0.8);++i){double t=i/rate;
   double x=0.09*std::exp(-12*t)*std::sin(6.283185307179586*f*t)+0.1*std::exp(-3*t)*std::sin(6.283185307179586*f*2*t);
   if(tracker.push((float)x,0.008f)){int key=tracker.getNote();root|=key==45;if(key>=0&&key!=45){std::cerr<<"Tail harmonic became "<<key<<" sr="<<rate<<'\n';ok=false;break;}}
  }
  if(!root)ok=false;
 }
 // Noise alone must not enroll a stable pitch.
 MeasuredLead noise;noise.prepare(48000);unsigned random=12345;
 for(int i=0;i<24000;++i){random=random*1664525u+1013904223u;float x=((random>>8)/16777216.0f-0.5f)*0.1f;
  if(noise.push(x,0.008f)&&noise.getNote()>=0){std::cerr<<"Noise generated a MIDI note\n";ok=false;break;}}
 if(ok)std::cout<<"Measured lead: isolated strings/frets, note transitions, decay harmonics, silence and noise passed.\n";
 return ok?0:1;
}

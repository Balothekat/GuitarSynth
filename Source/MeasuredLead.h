#pragma once
#include "GuitarProfile.h"
#include <array>
#include <algorithm>
#include <cmath>

// One pitch only. Fixed-size periodicity analysis followed by measured tuning
// mapping, confirmation, vibrato hysteresis and decay-harmonic rejection.
// No spectral chord analysis or worker queue is used by this lead detector.
class MeasuredLead {
public:
 void prepare(double sr){
  divisor=std::max(1,(int)std::ceil(sr/12000.0));rate=sr/divisor;
  lowLag=std::max(3,(int)std::floor(rate/(GuitarProfile::openHz[5]*4*1.03)));
  highLag=std::min(250,(int)std::ceil(rate/(GuitarProfile::openHz[0]*0.975)));
  for(int i=0;i<2;++i){double q=i==0?0.5411961001:1.306562965;double w=6.283185307179586*1800/sr,c=std::cos(w),alpha=std::sin(w)/(2*q),a0=1+alpha;
   filter[i]={(1-c)/(2*a0),(1-c)/a0,(1-c)/(2*a0),-2*c/a0,(1-alpha)/a0,0,0};}
  analysisHop=64;reset();
 }
 void reset(){for(auto& section:filter){section.z1=section.z2=0;}ring.fill(0);scores.fill(1);index=filled=sub=hop=0;sum=0;
  envelope=peak=0;note=pending=-1;votes=missing=0;frequency=centre=0;}
 bool push(float x,float gate){
  envelope+=(std::abs(x)-envelope)*(std::abs(x)>envelope?0.02f:0.003f);
  double filtered=x;for(auto& section:filter){double y=section.b0*filtered+section.z1;section.z1=section.b1*filtered-section.a1*y+section.z2;section.z2=section.b2*filtered-section.a2*y;filtered=y;}
  sum+=filtered;if(++sub<divisor)return false;
  ring[index]=(float)(sum/divisor);sum=0;sub=0;index=(index+1)%1024;filled=std::min(1024,filled+1);
  if(++hop<analysisHop)return false;hop=0;
  if(envelope<(note>=0?gate*0.65f:gate)||filled<2*highLag+4){miss();return true;}
  auto past=[&](int n){return ring[(index-1-n+2048)%1024];};
  // A fixed comparison length makes minima at different lags comparable.
  for(int lag=lowLag-1;lag<=highLag+1;++lag){double difference=0,energy=1e-15;
   for(int j=0;j<highLag;++j){double a=past(j),b=past(j+lag);difference+=(a-b)*(a-b);energy+=a*a+b*b;}
   scores[lag]=difference/energy;
  }
  double best=1;
  auto quality=[&](int lag){double a=scores[lag-1],b=scores[lag],c=scores[lag+1],den=a-2*b+c;
   double delta=den>1e-12?std::clamp(0.5*(a-c)/den,-0.5,0.5):0.0;
   return std::max(0.0,b-0.25*(a-c)*delta);};
  for(int lag=lowLag;lag<=highLag;++lag)if(scores[lag]<=scores[lag-1]&&scores[lag]<scores[lag+1])best=std::min(best,quality(lag));
  if(best>0.10){miss();return true;}
  int selected=0;
  for(int lag=lowLag;lag<=highLag;++lag)
   if(scores[lag]<=scores[lag-1]&&scores[lag]<scores[lag+1]&&quality(lag)<=best*1.4+0.035){selected=lag;break;}
  if(!selected){miss();return true;}
  double a=scores[selected-1],b=scores[selected],c=scores[selected+1],den=a-2*b+c;
  double delta=den>1e-12?std::clamp(0.5*(a-c)/den,-0.5,0.5):0.0;
  double hz=rate/(selected+delta),mappedCentre=0;int target=map(hz,mappedCentre);
  if(target<0){miss();return true;}
  // Do not turn a fading note's upper partial into a new note. A real picked
  // octave can still replace it when it reaches a comparable input level.
  if(note>=0&&target!=note&&envelope<peak*0.6f){bool harmonic=false;
   for(int h=2;h<=6;++h)harmonic|=std::abs(12*std::log2(hz/(centre*h)))<0.55;
   if(harmonic){pending=-1;votes=0;missing=0;return true;}
  }
  missing=0;
  if(target==note){frequency=hz;peak=std::max(peak,envelope);pending=-1;votes=0;return true;}
  if(target==pending)++votes;else{pending=target;votes=1;}
  if(votes>=3){note=target;centre=mappedCentre;frequency=hz;peak=envelope;pending=-1;votes=0;}
  return true;
 }
 int getNote()const{return note;}
 double getFrequency()const{return frequency;}
 float level()const{return envelope;}
private:
 struct Biquad {double b0=0,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;};
 std::array<Biquad,2> filter{};
 std::array<float,1024> ring{};std::array<double,256> scores{};
 int divisor=4,lowLag=8,highLag=160,analysisHop=64,index=0,filled=0,sub=0,hop=0;
 int note=-1,pending=-1,votes=0,missing=0;double rate=11025,sum=0,frequency=0,centre=0;
 float envelope=0,peak=0;
 void clear(){note=pending=-1;votes=missing=0;frequency=centre=0;peak=0;}
 void miss(){pending=-1;votes=0;if(++missing>=3)clear();}
 int map(double hz,double& mappedCentre)const{
  if(note>=0&&centre>0&&std::abs(12*std::log2(hz/centre))<0.65){mappedCentre=centre;return note;}
  double best=0.48;int target=-1;
  for(int string=0;string<6;++string){double position=12*std::log2(hz/GuitarProfile::openHz[string]);int fret=(int)std::round(position);
   double error=std::abs(position-fret);if(fret>=0&&fret<=24&&error<best){best=error;target=GuitarProfile::openNotes[string]+fret;mappedCentre=GuitarProfile::openHz[string]*std::pow(2.0,fret/12.0);}}
  return target;
 }
};

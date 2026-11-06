#pragma once
#include <array>
#include <algorithm>
#include <cmath>

// Streaming, monophonic normalized-difference tracker. No heap allocation.
// Downsample by averaging before analysis. Prototype anti-aliasing only:
// strong distortion/high-frequency content can still cause incorrect tracking.
class Tracker {
public:
 void prepare(double sr) {
  divisor=std::max(1,(int)std::ceil(sr/16000.0)); rate=sr/divisor;
  ring.fill(0); scores.fill(1); index=filled=sub=hop=0; sum=0; envelope=0;
 }
 void reset(){ring.fill(0);scores.fill(1);index=filled=sub=hop=0;sum=0;envelope=0;}
 void setFast(bool fast){analysisHop=fast?16:32;}
 bool push(float x, float gate, double& hz) {
  envelope += (std::abs(x)-envelope)*(std::abs(x)>envelope?0.02f:0.001f);
  sum+=x;
  if(++sub<divisor) return false;
  float y=sum/divisor; sum=0;sub=0;
  ring[index]=y;index=(index+1)%2048;filled=std::min(filled+1,2048);
  if(++hop<analysisHop) return false;
  hop=0; hz=0;
  if(envelope<gate) return true;
  int lo=std::max(2,(int)(rate/1400.0));
  int hi=std::min({500,(int)(rate/65.0),filled/2-1});
  auto past=[&](int n){return ring[(index-1-n+4096)%2048];};
  for(int lag=lo;lag<=hi;++lag) {
   double d=0,e=1e-12;
   int count=std::max(32,lag);
   if(filled<count+lag) {scores[lag]=1;continue;}
   for(int j=0;j<count;++j) {
    double a=past(j),b=past(j+lag);d+=(a-b)*(a-b);e+=a*a+b*b;
   }
   scores[lag]=(float)(d/e);
   // First strong local minimum avoids choosing a multiple of the period.
   if(lag>lo+1 && scores[lag-1]<0.12f &&
      scores[lag-1]<=scores[lag-2] && scores[lag-1]<scores[lag]) {
    double a=scores[lag-2],b=scores[lag-1],c=scores[lag];
    double den=a-2*b+c;
    double delta=std::abs(den)>1e-12?0.5*(a-c)/den:0;
    hz=rate/(lag-1+std::clamp(delta,-0.5,0.5));
    return true;
   }
  }
  return true;
 }
 float level() const {return envelope;}
private:
 std::array<float,2048> ring{};
 std::array<float,512> scores{};
 int divisor=1,index=0,filled=0,sub=0,hop=0,analysisHop=32;
 double rate=16000,sum=0;
 float envelope=0;
};

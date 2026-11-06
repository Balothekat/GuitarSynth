#pragma once
#include <array>
#include <complex>
#include <algorithm>
#include <cmath>
// Experimental fixed-tuning multi-pitch estimator. 1024-sample Hann frame,
// 128-sample hop at <=16 kHz, six-harmonic templates and sparse nonnegative fit.
// Synthetic validation is not a guarantee for real guitar, bends or dense voicings.
class ChordTracker {
public:
 static constexpr int first=40,last=88,notes=last-first+1,N=1024,bins=N/2+1;
 using Strengths=std::array<float,notes>;
 void prepare(double sr) {
  divisor=std::max(1,(int)std::ceil(sr/16000.0));rate=sr/divisor;
  index=filled=sub=hop=0;sum=0;envelope=0;ring.fill(0);strength.fill(0);
  for(int j=0;j<N;++j)window[j]=0.5f-0.5f*std::cos(6.283185307179586*j/(N-1));
  for(int n=0;n<notes;++n) {
   auto& t=templates[n];t.fill(0);
   double hz=440*std::pow(2.0,(n+first-69)/12.0);
   for(int h=1;h<=6;++h) {
    double centre=hz*h*N/rate;if(centre>=bins-3)break;
    for(int k=std::max(1,(int)centre-3);k<=std::min(bins-1,(int)centre+4);++k) {
     double delta=k-centre,ad=std::abs(delta),lobe=0;
     if(ad<1e-8)lobe=1;
     else if(std::abs(ad-1)<1e-8)lobe=0.5;
     else lobe=std::abs(std::sin(3.141592653589793*delta)/
       (3.141592653589793*delta*(1-delta*delta)));
     t[k]+=(float)(lobe/std::pow((double)h,1.3));
    }
   }
   float norm=0;for(float v:t)norm+=v*v;
   norm=std::sqrt(norm);for(float& v:t)v/=std::max(norm,1e-9f);
  }
 }
 void reset(){ring.fill(0);strength.fill(0);index=filled=sub=hop=0;sum=0;envelope=0;}
 bool push(float x,float gate) {
  envelope+=(std::abs(x)-envelope)*(std::abs(x)>envelope?0.02f:0.001f);
  sum+=x;if(++sub<divisor)return false;
  ring[index]=(float)(sum/divisor);sum=0;sub=0;
  index=(index+1)%N;filled=std::min(N,filled+1);
  if(++hop<128)return false;hop=0;strength.fill(0);
  if(filled<N||envelope<gate)return true;
  for(int i=0;i<N;++i)fft[i]={ring[(index+i)%N]*window[i],0};
  for(int i=1,j=0;i<N;++i) {
   int bit=N>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;
   if(i<j)std::swap(fft[i],fft[j]);
  }
  for(int length=2;length<=N;length<<=1) {
   double angle=-6.283185307179586/length;
   std::complex<float> step((float)std::cos(angle),(float)std::sin(angle));
   for(int i=0;i<N;i+=length) {
    std::complex<float> w(1,0);
    for(int j=0;j<length/2;++j) {
     auto u=fft[i+j],v=fft[i+j+length/2]*w;
     fft[i+j]=u+v;fft[i+j+length/2]=u-v;w*=step;
    }
   }
  }
  float peak=0;
  for(int k=0;k<bins;++k){spectrum[k]=std::abs(fft[k]);peak=std::max(peak,spectrum[k]);}
  residual=spectrum;
  // L1 penalty discourages adding harmonics as independent notes.
  for(int pass=0;pass<14;++pass)for(int n=0;n<notes;++n) {
   float dot=0;for(int k=1;k<bins;++k)dot+=templates[n][k]*residual[k];
   float next=std::max(0.0f,strength[n]+dot-peak*0.075f);
   float delta=next-strength[n];strength[n]=next;
   if(std::abs(delta)>1e-7f)
    for(int k=1;k<bins;++k)residual[k]-=delta*templates[n][k];
  }
  float maximum=*std::max_element(strength.begin(),strength.end());
  for(int n=0;n<notes;++n) {
   double f=440*std::pow(2.0,(n+first-69)/12.0),centre=f*N/rate;
   int k=(int)std::round(centre);float fundamental=0;
   for(int b=std::max(1,k-1);b<=std::min(bins-1,k+1);++b)
    fundamental=std::max(fundamental,spectrum[b]);
   if(strength[n]<maximum*0.24f||fundamental<peak*0.065f)strength[n]=0;
  }
  // A weak octave explained by an active lower fundamental is usually an overtone.
  for(int n=12;n<notes;++n)
   if(strength[n-12]>0 && strength[n]<strength[n-12]*0.85f)strength[n]=0;
  // Six outputs maximum; no manufactured chord notes.
  for(int count=0;count<notes-6;++count) {
   int selected=-1;float smallest=1e30f;int positives=0;
   for(int n=0;n<notes;++n)if(strength[n]>0) {
    ++positives;if(strength[n]<smallest){smallest=strength[n];selected=n;}
   }
   if(positives<=6)break;strength[selected]=0;
  }
  return true;
 }
 const Strengths& getStrengths()const{return strength;}
 float level()const{return envelope;}
private:
 int divisor=1,index=0,filled=0,sub=0,hop=0;
 double rate=16000,sum=0;
 float envelope=0;
 std::array<float,N> ring{},window{};
 std::array<std::complex<float>,N> fft{};
 std::array<std::array<float,bins>,notes> templates{};
 std::array<float,bins> spectrum{},residual{};
 Strengths strength{};
};

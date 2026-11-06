#pragma once
#include "GuitarProfile.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

// Experimental calibrated fundamental tracker. Does not infer missing octave
// duplicates: an octave can be indistinguishable from a lower string's partial.
// All storage is allocated with the object; no audio or calibration is uploaded.
class CalibratedTracker {
public:
 static constexpr int first=38,last=88,notes=last-first+1,N=2048,hopSize=256;
 using Strengths=std::array<float,notes>;
 void prepare(double sr) {
  divisor=std::max(1,(int)std::ceil(sr/24000.0));rate=sr/divisor;
  for(int i=0;i<N;++i)window[i]=0.5-0.5*std::cos(2*pi*i/(N-1));
  reset();
 }
 void reset() {
  ring.fill(0);strength.fill(0);streak.fill(0);absence.fill(0);held.fill(false);
  prior.fill(0);lastAmplitude.fill(0);index=filled=sub=hop=0;sum=0;envelope=0;
 }
 bool push(float x,float gate) {
  envelope+=(std::abs(x)-envelope)*(std::abs(x)>envelope?0.02f:0.003f);
  sum+=x;if(++sub<divisor)return false;
  ring[index]=sum/divisor;sum=0;sub=0;index=(index+1)%N;filled=std::min(N,filled+1);
  if(++hop<hopSize)return false;hop=0;
  candidateAmp.fill(0);candidateHz.fill(0);
  if(filled==N&&envelope>=gate)analyse();
  update();return true;
 }
 const Strengths& getStrengths()const{return strength;}
 float level()const{return envelope;}
 double getNoiseRatio()const{return noiseRatio;}
private:
 static constexpr double pi=3.14159265358979323846;
 struct Tone {double hz=0,a=0,b=0;double amplitude()const{return std::hypot(a,b);}};
 struct Candidate {double hz=0,amp=0;int note=0,string=0;};
 std::array<double,N> ring{},window{},original{},residual{};
 std::array<std::complex<double>,N> fft{};
 std::array<double,N/2+1> magnitude{};
 std::array<Tone,24> tones{};
 std::array<Candidate,24> candidates{},accepted{};
 Strengths strength{};
 std::array<double,notes> candidateAmp{},candidateHz{},prior{},lastAmplitude{};
 std::array<int,notes> streak{},absence{};
 std::array<bool,notes> held{};
 int divisor=1,index=0,filled=0,sub=0,hop=0;
 double rate=22050,sum=0,noiseRatio=1;
 float envelope=0;

 void spectrum() {
  for(int i=0;i<N;++i)fft[i]={residual[i]*window[i],0};
  for(int i=1,j=0;i<N;++i){int bit=N>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(fft[i],fft[j]);}
  for(int length=2;length<=N;length<<=1){double angle=-2*pi/length;std::complex<double> step(std::cos(angle),std::sin(angle));
   for(int i=0;i<N;i+=length){std::complex<double> w(1,0);for(int j=0;j<length/2;++j){auto u=fft[i+j],v=fft[i+j+length/2]*w;fft[i+j]=u+v;fft[i+j+length/2]=u-v;w*=step;}}}
  for(int k=0;k<=N/2;++k)magnitude[k]=std::abs(fft[k]);
 }
 Tone fit(double hz)const {
  double angle=2*pi*hz/rate,cr=std::cos(angle),ci=std::sin(angle),c=1,s=0;
  double cc=0,ss=0,cs=0,xc=0,xs=0;
  for(int i=0;i<N;++i){double w=window[i],v=residual[i];cc+=w*c*c;ss+=w*s*s;cs+=w*c*s;xc+=w*v*c;xs+=w*v*s;
   double next=c*cr-s*ci;s=s*cr+c*ci;c=next;}
  double den=cc*ss-cs*cs;if(std::abs(den)<1e-12)return {hz,0,0};
  return {hz,(xc*ss-xs*cs)/den,(xs*cc-xc*cs)/den};
 }
 void apply(const Tone& tone,double scale) {
  double angle=2*pi*tone.hz/rate,cr=std::cos(angle),ci=std::sin(angle),c=1,s=0;
  for(int i=0;i<N;++i){residual[i]+=scale*(tone.a*c+tone.b*s);double next=c*cr-s*ci;s=s*cr+c*ci;c=next;}
 }
 bool map(double hz,Candidate& candidate)const {
  double best=0.32;bool found=false;
  for(int s=0;s<6;++s){double position=12*std::log2(hz/GuitarProfile::openHz[s]);int fret=(int)std::round(position);double error=std::abs(position-fret);
   if(fret>=0&&fret<=24&&error<best){best=error;candidate={hz,0,GuitarProfile::openNotes[s]+fret,s};found=true;}}
  return found&&candidate.note>=first&&candidate.note<=last;
 }
 void analyse() {
  for(int i=0;i<N;++i)residual[i]=original[i]=ring[(index+i)%N];
  int toneCount=0;double initial=0;
  for(int n=0;n<(int)tones.size();++n){spectrum();double best=0,hz=0;
   for(int k=2;k<N/2-1;++k){double v=magnitude[k];if(v<=magnitude[k-1]||v<magnitude[k+1])continue;
    double a=std::log(magnitude[k-1]+1e-9),b=std::log(v+1e-9),c=std::log(magnitude[k+1]+1e-9);
    double den=a-2*b+c,delta=std::abs(den)>1e-12?0.5*(a-c)/den:0;
    double f=(k+delta)*rate/N;if(f<=60||f>=5000)continue;
    bool nearby=false;for(int j=0;j<toneCount;++j)nearby|=std::abs(f-tones[j].hz)<rate/N*0.7;
    if(!nearby&&v>best){best=v;hz=f;}}
   if(n==0)initial=best;if(hz==0||best<initial*0.03)break;
   tones[toneCount]=fit(hz);apply(tones[toneCount],-1);++toneCount;
  }
  for(int pass=0;pass<3;++pass)for(int j=0;j<toneCount;++j){apply(tones[j],1);tones[j]=fit(tones[j].hz);apply(tones[j],-1);}
  double before=0,after=0,peak=0;
  for(int i=0;i<N;++i){before+=window[i]*original[i]*original[i];after+=window[i]*residual[i]*residual[i];}
  noiseRatio=after/(before+1e-12);if(noiseRatio>=0.15)return;
  for(int j=0;j<toneCount;++j)peak=std::max(peak,tones[j].amplitude());
  int count=0;
  for(int j=0;j<toneCount;++j){const auto& t=tones[j];if(t.hz<68||t.hz>1500)continue;Candidate c;
   if(!map(t.hz,c))continue;c.amp=t.amplitude();if(c.amp>peak*(held[c.note-first]?0.045:0.14))candidates[count++]=c;}
  std::sort(candidates.begin(),candidates.begin()+count,[](const auto& a,const auto& b){return a.hz<b.hz;});
  int kept=0;
  for(int j=0;j<count;++j){auto c=candidates[j];bool harmonic=false,duplicate=false;
   for(int k=0;k<kept;++k){const auto& lower=accepted[k];duplicate|=lower.note==c.note;
    for(int h=1;h<12;++h){auto partial=GuitarProfile::partials[lower.string][h];double expected=lower.hz*partial.ratio;
     if(std::abs(std::log2(c.hz/expected))<0.022&&c.amp<lower.amp*(partial.amplitude*2.5+0.15))harmonic=true;}}
   if(!harmonic&&!duplicate)accepted[kept++]=c;
  }
  std::sort(accepted.begin(),accepted.begin()+kept,[](const auto& a,const auto& b){return a.amp>b.amp;});
  for(int j=0;j<std::min(6,kept);++j){int n=accepted[j].note-first;candidateAmp[n]=accepted[j].amp;candidateHz[n]=accepted[j].hz;}
 }
 void update() {
  strength.fill(0);
  for(int n=0;n<notes;++n){if(candidateAmp[n]>0){absence[n]=0;double pitch=12*std::log2(candidateHz[n]);
    streak[n]=streak[n]>0&&std::abs(pitch-prior[n])<0.22?streak[n]+1:1;prior[n]=pitch;
    if(streak[n]>=3)held[n]=true;lastAmplitude[n]=candidateAmp[n];
   }else{streak[n]=0;if(++absence[n]>=4)held[n]=false;}
   if(held[n])strength[n]=(float)lastAmplitude[n];}
  // Release tails cannot expand the active voice set beyond six.
  for(int iteration=0;iteration<notes;++iteration){int count=0,weakest=-1;float lowest=1e30f;
   for(int n=0;n<notes;++n)if(strength[n]>0){++count;if(strength[n]<lowest){lowest=strength[n];weakest=n;}}
   if(count<=6)break;strength[weakest]=0;held[weakest]=false;
  }
 }
};

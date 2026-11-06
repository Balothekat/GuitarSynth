#pragma once
#include "GuitarProfile.h"
#include <algorithm>
#include <array>
#include <cmath>

// Experimental calibrated fundamental tracker. Does not infer missing octave
// duplicates: an octave can be indistinguishable from a lower string's partial.
// All storage is allocated with the object; no audio is uploaded by the detector.
class CalibratedTracker {
public:
 static constexpr int first=38,last=88,notes=last-first+1,N=2048,hopSize=512;
 using Strengths=std::array<float,notes>;
 void prepare(double sr) {
  divisor=std::max(1,(int)std::ceil(sr/24000.0));rate=sr/divisor;
  samplePeriod=1/sr;fastCoefficient=1-std::exp(-samplePeriod/0.005);slowCoefficient=1-std::exp(-samplePeriod/0.06);
  for(int i=0;i<N;++i){window[i]=0.5-0.5*std::cos(2*pi*i/(N-1));
   twiddleRe[i]=std::cos(-2*pi*i/N);twiddleIm[i]=std::sin(-2*pi*i/N);
   int value=i,reversed=0;for(int bit=N;bit>1;bit>>=1){reversed=(reversed<<1)|(value&1);value>>=1;}bitReverse[i]=reversed;}
  reset();
 }
 void reset() {
  ring.fill(0);strength.fill(0);streak.fill(0);absence.fill(0);held.fill(false);
  prior.fill(0);lastAmplitude.fill(0);heldHz.fill(0);index=filled=sub=hop=0;sum=0;envelope=0;recentPeak=0;fastEnergy=slowEnergy=0;attackAge=0;
 }
 bool push(float x,float gate) {
  double square=(double)x*x;fastEnergy+=(square-fastEnergy)*fastCoefficient;
  slowEnergy+=(square-slowEnergy)*slowCoefficient;
  attackAge=std::min(2.0,attackAge+samplePeriod);
  if(attackAge>0.15&&fastEnergy>slowEnergy*2.2&&fastEnergy>gate*gate)attackAge=0;
  envelope+=(std::abs(x)-envelope)*(std::abs(x)>envelope?0.02f:0.003f);
  sum+=x;if(++sub<divisor)return false;
  ring[index]=sum/divisor;sum=0;sub=0;index=(index+1)%N;filled=std::min(N,filled+1);
  if(++hop<hopSize)return false;hop=0;
  recentPeak*=std::exp(-hopSize/(rate*0.6));
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
 std::array<double,N> real{},imaginary{},twiddleRe{},twiddleIm{};
 std::array<int,N> bitReverse{};
 std::array<double,N/2+1> magnitude{};
 std::array<Tone,24> tones{};
 std::array<Candidate,24> candidates{},accepted{};
 Strengths strength{};
 std::array<double,notes> candidateAmp{},candidateHz{},prior{},lastAmplitude{},heldHz{};
 std::array<int,notes> streak{},absence{};
 std::array<bool,notes> held{};
 int divisor=1,index=0,filled=0,sub=0,hop=0;
 double rate=22050,sum=0,noiseRatio=1,recentPeak=0,fastEnergy=0,slowEnergy=0,attackAge=0;
 double samplePeriod=1.0/44100,fastCoefficient=0.0045,slowCoefficient=0.00038;
 float envelope=0;

 void spectrum() {
  for(int i=0;i<N;++i){int j=bitReverse[i];real[j]=residual[i]*window[i];imaginary[j]=0;}
  for(int length=2;length<=N;length<<=1){int stride=N/length;
   for(int i=0;i<N;i+=length)for(int j=0;j<length/2;++j){int left=i+j,right=left+length/2,tw=j*stride;
    double wr=twiddleRe[tw],wi=twiddleIm[tw],vr=real[right]*wr-imaginary[right]*wi,vi=real[right]*wi+imaginary[right]*wr;
    real[right]=real[left]-vr;imaginary[right]=imaginary[left]-vi;real[left]+=vr;imaginary[left]+=vi;}}
  for(int k=0;k<=N/2;++k)magnitude[k]=real[k]*real[k]+imaginary[k]*imaginary[k];
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
   if(n==0)initial=best;if(hz==0||best<initial*0.0009)break;
   tones[toneCount]=fit(hz);apply(tones[toneCount],-1);++toneCount;
  }
  for(int pass=0;pass<3;++pass)for(int j=0;j<toneCount;++j){apply(tones[j],1);tones[j]=fit(tones[j].hz);apply(tones[j],-1);}
  double before=0,after=0,peak=0;
  for(int i=0;i<N;++i){before+=window[i]*original[i]*original[i];after+=window[i]*residual[i]*residual[i];}
  noiseRatio=after/(before+1e-12);if(noiseRatio>=0.15)return;
  for(int j=0;j<toneCount;++j)peak=std::max(peak,tones[j].amplitude());
  recentPeak=std::max(peak,recentPeak);
  int count=0;
  for(int j=0;j<toneCount;++j){const auto& t=tones[j];if(t.hz<68||t.hz>1500)continue;Candidate c;
   if(!map(t.hz,c))continue;c.amp=t.amplitude();
   // A decaying/muted chord must not keep enrolling fresh notes from its tail.
   // This measured mode enrolls notes during a strum; it is not a legato mode.
   if(!held[c.note-first]&&attackAge>0.45)continue;
   double minimum=held[c.note-first]?peak*0.045:std::max(peak*0.14,recentPeak*0.13);
   if(c.amp<=minimum)continue;
   // Even a temporarily masked lower fundamental can explain an upper partial.
   bool explained=false;
   for(int k=0;k<toneCount&&!explained;++k){const auto& lower=tones[k];if(lower.hz>=t.hz*0.75||lower.amplitude()<peak*0.045)continue;Candidate lowerMap;
    if(!map(lower.hz,lowerMap))continue;
    for(int h=1;h<12;++h){auto part=GuitarProfile::partials[lowerMap.string][h];double target=lower.hz*part.ratio;
     if(std::abs(std::log2(t.hz/target))<0.04&&c.amp<lower.amplitude()*(std::max(1.0,part.amplitude*2.5)+0.15)){explained=true;break;}}}
   if(explained)continue;
   // A new guitar note needs corroborating partials, not just a pick/mute peak.
   bool supported=held[c.note-first]||(c.note<=64&&c.amp>peak*0.22);
   for(int h=1;h<=3&&!supported;++h){auto partial=GuitarProfile::partials[c.string][h];double expected=t.hz*partial.ratio;
    for(int k=0;k<toneCount;++k)if(k!=j&&tones[k].amplitude()>c.amp*0.075&&std::abs(std::log2(tones[k].hz/expected))<0.018){supported=true;break;}}
   if(supported)candidates[count++]=c;}
  std::sort(candidates.begin(),candidates.begin()+count,[](const auto& a,const auto& b){return a.hz<b.hz;});
  int kept=0;
  for(int j=0;j<count;++j){auto c=candidates[j];bool harmonic=false,duplicate=false;
   for(int k=0;k<kept;++k){const auto& lower=accepted[k];duplicate|=lower.note==c.note;
    for(int h=1;h<12;++h){auto partial=GuitarProfile::partials[lower.string][h];double expected=lower.hz*partial.ratio;
     if(std::abs(std::log2(c.hz/expected))<0.04&&c.amp<lower.amp*(std::max(1.0,partial.amplitude*2.5)+0.15))harmonic=true;}}
   if(!harmonic&&!duplicate)accepted[kept++]=c;
  }
  std::sort(accepted.begin(),accepted.begin()+kept,[](const auto& a,const auto& b){return a.amp>b.amp;});
  for(int j=0;j<std::min(6,kept);++j){int n=accepted[j].note-first;candidateAmp[n]=accepted[j].amp;candidateHz[n]=accepted[j].hz;}
 }
 void update() {
  strength.fill(0);
  for(int n=0;n<notes;++n){
   // Three agreements in six frames tolerate a weak string being briefly masked.
   streak[n]=((streak[n]<<1)|(candidateAmp[n]>0?1:0))&63;
   if(candidateAmp[n]>0){absence[n]=0;int votes=0;for(int bits=streak[n];bits;bits>>=1)votes+=bits&1;
    if(votes>=3)held[n]=true;lastAmplitude[n]=candidateAmp[n];heldHz[n]=candidateHz[n];
   }else{absence[n]=std::min(4,absence[n]+1);if(absence[n]>=4)held[n]=false;}
   if(held[n])strength[n]=(float)lastAmplitude[n];}
  // A six-string guitar cannot sustain two different frets on the same string.
  // Find a feasible available string for each pitch, strongest evidence first.
  std::array<int,notes> order{};for(int n=0;n<notes;++n)order[n]=n;
  std::sort(order.begin(),order.end(),[this](int a,int b){return strength[a]>strength[b];});
  std::array<bool,6> occupied{};
  for(int n:order)if(strength[n]>0){int selected=-1;double best=0.32;
   for(int s=0;s<6;++s)if(!occupied[s]){double position=12*std::log2(heldHz[n]/GuitarProfile::openHz[s]);int fret=(int)std::round(position);double error=std::abs(position-fret);
    if(fret>=0&&fret<=24&&GuitarProfile::openNotes[s]+fret==n+first&&error<best){selected=s;best=error;}}
   if(selected<0){strength[n]=0;held[n]=false;}else occupied[selected]=true;
  }
  // Release tails cannot expand the active voice set beyond six.
  for(int iteration=0;iteration<notes;++iteration){int count=0,weakest=-1;float lowest=1e30f;
   for(int n=0;n<notes;++n)if(strength[n]>0){++count;if(strength[n]<lowest){lowest=strength[n];weakest=n;}}
   if(count<=6)break;strength[weakest]=0;held[weakest]=false;
  }
 }
};

#include "CalibratedWorker.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <string>
#include <cstdint>
static uint32_t be32(const unsigned char* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
static uint16_t be16(const unsigned char* p){return uint16_t(p[0])<<8|p[1];}
int main(int argc,char** argv){
 if(argc<2||argc>3){std::cerr<<"Usage: calibrated_replay mono24bit.aif [--realtime]\n";return 2;}
 std::ifstream in(argv[1],std::ios::binary);std::vector<unsigned char> b((std::istreambuf_iterator<char>(in)),{});
 if(b.size()<54||std::string((char*)b.data(),4)!="FORM"||std::string((char*)b.data()+8,4)!="AIFF")return 3;
 size_t start=0,bytes=0;double sr=0;uint32_t frames=0;
 for(size_t p=12;p+8<=b.size();){auto n=be32(b.data()+p+4);size_t q=p+8;if(n>b.size()-q)return 4;std::string id((char*)b.data()+p,4);
  if(id=="COMM"){if(n<18||be16(b.data()+q)!=1||be16(b.data()+q+6)!=24)return 5;frames=be32(b.data()+q+2);int exponent=(be16(b.data()+q+8)&0x7fff)-16383;
   uint64_t mantissa=uint64_t(be32(b.data()+q+10))<<32|be32(b.data()+q+14);sr=std::ldexp((double)mantissa,exponent-63);}
  if(id=="SSND"){if(n<8)return 6;size_t offset=be32(b.data()+q);if(offset>n-8)return 6;start=q+8+offset;bytes=n-8-offset;}
  p=q+n+(n&1);
 }
 if(sr<8000||sr>192000||!start||bytes/3<frames)return 7;
 bool realtime=argc==3&&std::string(argv[2])=="--realtime";
 if(argc==3&&!realtime)return 8;
 if(realtime){
  CalibratedWorker worker;worker.setGate(0.008f);worker.start(sr);
  std::array<bool,CalibratedTracker::notes> active{};std::array<int,CalibratedTracker::notes> starts{};
  const auto begin=std::chrono::steady_clock::now();int lateBlocks=0;double maximumLateness=0;
  std::cout<<"sample_rate,"<<sr<<"\nmode,paced_worker_128_samples\ntime,event,midi,wall_seconds\n";
  for(uint32_t i=0;i<frames;i+=128){
   CalibratedTracker::Strengths values{};
   if(worker.consume(values))for(int n=0;n<CalibratedTracker::notes;++n){bool on=values[n]>0;if(on!=active[n]){
    double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    std::cout<<i/sr<<','<<(on?"on":"off")<<','<<n+CalibratedTracker::first<<','<<wall<<'\n';if(on)++starts[n];active[n]=on;}}
   for(uint32_t j=i;j<std::min(frames,i+128);++j){auto* p=b.data()+start+j*3;int32_t word=(int32_t(p[0])<<16)|(int32_t(p[1])<<8)|p[2];if(word&0x800000)word-=0x1000000;worker.push(word/8388608.0f);}
   auto target=begin+std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(std::min(frames,i+128)/sr));
   double late=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-target).count();if(late>3){++lateBlocks;maximumLateness=std::max(maximumLateness,late);}
   std::this_thread::sleep_until(target);
  }
  bool overloaded=worker.hasOverloaded();worker.stop();
  std::cout<<"QUEUE_OVERLOADED,"<<overloaded<<"\nLATE_FEED_BLOCKS,"<<lateBlocks<<"\nMAX_FEED_LATENESS_MS,"<<maximumLateness<<'\n';
  for(int n=0;n<CalibratedTracker::notes;++n)if(starts[n])std::cout<<"NOTE,"<<n+CalibratedTracker::first<<",onsets,"<<starts[n]<<'\n';
  return overloaded?9:0;
 }
 CalibratedTracker detector;detector.prepare(sr);std::array<bool,CalibratedTracker::notes> held{};std::array<int,CalibratedTracker::notes> onsets{},counts{};
 double maximumMs=0,totalMs=0;int analyses=0;
 std::cout<<"sample_rate,"<<sr<<"\nduration,"<<frames/sr<<"\ntime,event,midi,noise_ratio,input_level\n";
 for(uint32_t i=0;i<frames;++i){const auto* p=b.data()+start+i*3;int32_t word=(int32_t(p[0])<<16)|(int32_t(p[1])<<8)|p[2];if(word&0x800000)word-=0x1000000;float x=word/8388608.0f;
  auto begin=std::chrono::steady_clock::now();bool ready=detector.push(x,0.008f);
  if(ready){double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();maximumMs=std::max(maximumMs,ms);totalMs+=ms;++analyses;
   const auto& levels=detector.getStrengths();for(int n=0;n<CalibratedTracker::notes;++n){bool active=levels[n]>0;if(active)++counts[n];if(active!=held[n]){std::cout<<i/sr<<','<<(active?"on":"off")<<','<<n+CalibratedTracker::first<<','<<detector.getNoiseRatio()<<','<<detector.level()<<'\n';if(active)++onsets[n];held[n]=active;}}
  }
 }
 std::cout<<"ANALYSIS_MEAN_MS,"<<totalMs/std::max(1,analyses)<<"\nANALYSIS_MAX_MS,"<<maximumMs<<"\n";
 for(int n=0;n<CalibratedTracker::notes;++n)if(onsets[n])std::cout<<"NOTE,"<<n+CalibratedTracker::first<<",onsets,"<<onsets[n]<<",frames,"<<counts[n]<<'\n';
}

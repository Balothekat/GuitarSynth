#include "MeasuredLead.h"
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
 if(argc!=2){std::cerr<<"Usage: calibrated_replay mono24bit.aif\n";return 2;}
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
 MeasuredLead tracker;tracker.prepare(sr);int previous=-1;double total=0,maxMs=0;int analyses=0;
 std::cout<<"sample_rate,"<<sr<<"\ntime,event,midi,hz\n";
 for(uint32_t i=0;i<frames;++i){auto* p=b.data()+start+i*3;int32_t word=(int32_t(p[0])<<16)|(int32_t(p[1])<<8)|p[2];if(word&0x800000)word-=0x1000000;
  auto begin=std::chrono::steady_clock::now();if(tracker.push(word/8388608.0f,0.008f)){double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();total+=ms;maxMs=std::max(maxMs,ms);++analyses;
   int next=tracker.getNote();if(next!=previous){if(previous>=0)std::cout<<i/sr<<",off,"<<previous<<",0\n";if(next>=0)std::cout<<i/sr<<",on,"<<next<<','<<tracker.getFrequency()<<'\n';previous=next;}
  }
 }
 std::cout<<"ANALYSIS_MEAN_MS,"<<total/std::max(1,analyses)<<"\nANALYSIS_MAX_MS,"<<maxMs<<'\n';
}

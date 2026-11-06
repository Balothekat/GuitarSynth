#pragma once
#include "CalibratedTracker.h"
#include <atomic>
#include <thread>
#include <chrono>

// Single audio producer / single analysis consumer. The audio callback never
// locks, waits for analysis, or overwrites a frame owned by the analysis thread.
class CalibratedWorker {
public:
 using Strengths=CalibratedTracker::Strengths;
 ~CalibratedWorker(){stop();}
 void start(double rate){stop();detector.prepare(rate);write=0;read=0;epoch=1;overloaded=false;peakQueued=0;
  for(auto& result:results)result.state=0;running=true;worker=std::thread([this]{run();});}
 void stop(){running=false;if(worker.joinable())worker.join();}
 void reset(){epoch.fetch_add(1,std::memory_order_acq_rel);}
 void setGate(float value){gate.store(value,std::memory_order_relaxed);}
 void push(float x){if(!running.load(std::memory_order_relaxed))return;
  auto w=write.load(std::memory_order_relaxed),next=(w+1)%capacity;
  if(next==read.load(std::memory_order_acquire)){overloaded=true;reset();return;}
  unsigned depth=(w+capacity-read.load(std::memory_order_acquire))%capacity+1;peakQueued=std::max(peakQueued,depth);
  audio[w]=x;write.store(next,std::memory_order_release);}
 bool consume(Strengths& values){bool found=false;uint64_t newest=0;auto wanted=epoch.load(std::memory_order_acquire);
  for(auto& result:results){int expected=2;if(!result.state.compare_exchange_strong(expected,3,std::memory_order_acq_rel))continue;
   if(result.epoch==wanted&&result.serial>=newest){newest=result.serial;values=result.values;found=true;}
   result.state.store(0,std::memory_order_release);}
  return found;
 }
 // Called only by the single producer, after feeding (diagnostic replay).
 unsigned peakQueuedSamples()const{return peakQueued;}
 bool hasOverloaded()const{return overloaded.load(std::memory_order_relaxed);}
private:
 static constexpr unsigned capacity=8192;
 unsigned peakQueued=0;
 struct Result {std::atomic<int> state{0};Strengths values{};uint64_t epoch=0,serial=0;};
 std::array<Result,3> results;
 std::array<float,capacity> audio{};
 std::atomic<unsigned> write{0},read{0};
 std::atomic<uint64_t> epoch{1};
 std::atomic<float> gate{0.008f};
 std::atomic<bool> running{false},overloaded{false};
 std::thread worker;
 CalibratedTracker detector;
 void publish(uint64_t generation,uint64_t serial){
  for(int attempt=0;attempt<2;++attempt)for(auto& result:results){int expected=attempt==0?0:2;
   if(result.state.compare_exchange_strong(expected,1,std::memory_order_acq_rel)){
    result.values=detector.getStrengths();result.epoch=generation;result.serial=serial;
    result.state.store(2,std::memory_order_release);return;
   }}
 }
 void run(){uint64_t generation=0,serial=0;
  while(running.load(std::memory_order_acquire)){
   auto current=epoch.load(std::memory_order_acquire);
   if(current!=generation){generation=current;read.store(write.load(std::memory_order_acquire),std::memory_order_release);detector.reset();publish(generation,++serial);}
   auto r=read.load(std::memory_order_relaxed),w=write.load(std::memory_order_acquire);
   if(r==w){std::this_thread::sleep_for(std::chrono::microseconds(500));continue;}
   float x=audio[r];read.store((r+1)%capacity,std::memory_order_release);
   if(detector.push(x,gate.load(std::memory_order_relaxed)))publish(generation,++serial);
  }
 }
};

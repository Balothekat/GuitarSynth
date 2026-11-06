#include <JuceHeader.h>
#include "Tracker.h"
#include "ChordTracker.h"
#include "CalibratedWorker.h"
class GuitarLinkEditor;
class GuitarLink final:public juce::AudioProcessor {
public:
 GuitarLink():AudioProcessor(BusesProperties()
  .withInput("Guitar",juce::AudioChannelSet::stereo(),true)
  .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
  params(*this,nullptr,"GuitarLink",layout()) {
  for(auto& v:scope)v.store(0);
 }
 juce::AudioProcessorValueTreeState& state(){return params;}
 std::atomic<float> inputLevel{0},livePitch{-1},envelopeLevel{0};
 std::atomic<int> liveCount{0};
 std::atomic<bool> trackingBusy{false};
 std::atomic<uint64_t> activeMask{0};
 std::array<std::atomic<float>,256> scope{};
 std::atomic<unsigned> scopeIndex{0};
 static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
  juce::AudioProcessorValueTreeState::ParameterLayout p;
  auto add=[&](const char* id,const char* name,float lo,float hi,float def){
   p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,
    juce::NormalisableRange<float>(lo,hi),def));
  };
  add("gain","Input gain",0.1f,4,1);
  add("gate","Input gate",0.001f,0.1f,0.008f);
  add("dry","Dry guitar",0,1,0);add("synth","Synth level",0,1,0.5f);
  add("attack","Attack",0.001f,1,0.001f);add("decay","Decay",0.005f,1,0.15f);
  add("sustain","Sustain",0,1,0.75f);add("release","Release",0.005f,0.5f,0.035f);
  add("tone","Harmonics",0,1,0.25f);
  p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"wave",1},
   "Oscillator",juce::StringArray{"Sine","Warm","Hollow"},1));
  p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"tracking",1},
   "Tracking mode",juce::StringArray{"Fast Lead","Stable Lead","Chords (experimental)","DAEAC#E / measured"},0));
  p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bend",1},
   "Follow bends (lead only; receiver +/-2 semitones)",false));
  p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"panic",1},
   "Mute / stop all notes",false));return p;
 }
 const juce::String getName()const override{return "GuitarLink";}
 bool acceptsMidi()const override{return false;}
 bool producesMidi()const override{return true;}
 bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 0.5;}
 bool hasEditor()const override{return true;}
 juce::AudioProcessorEditor* createEditor()override;
 void prepareToPlay(double sr,int)override {
  rate=sr;tracker.prepare(sr);chords.prepare(sr);calibrated.prepare(sr);calWorker.start(sr);held.fill(false);
  presence.fill(0);absence.fill(0);candidate=note=-1;stable=miss=0;
  previousMode=-1;lastWheel=8192;scopeCounter=0;resetPending=true;
  for(auto& voice:voices){voice.envelope.setSampleRate(sr);voice.envelope.reset();voice.note=-1;voice.phase=0;}
  inputLevel=0;livePitch=-1;activeMask=0;liveCount=0;trackingBusy=false;rawEnvelope=0;previousPanic=false;quietSamples=0;quietReset=false;
 }
 void releaseResources()override{calWorker.stop();}
 bool isBusesLayoutSupported(const BusesLayout& b)const override {
  auto in=b.getMainInputChannelSet(),out=b.getMainOutputChannelSet();
  return (in==juce::AudioChannelSet::mono()||in==juce::AudioChannelSet::stereo())
   && (out==juce::AudioChannelSet::mono()||out==juce::AudioChannelSet::stereo());
 }
 void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi)override {
  juce::ScopedNoDenormals guard;midi.clear();
  if(audio.getNumSamples()==0)return;
  auto read=[&](const char* id){return params.getRawParameterValue(id)->load();};
  const float gain=read("gain"),gate=read("gate"),dry=read("dry"),synth=read("synth"),tone=read("tone");
  const int mode=juce::roundToInt(read("tracking")),wave=juce::roundToInt(read("wave"));
  const bool panic=read("panic")>0.5f,bend=mode<2&&read("bend")>0.5f;
  juce::ADSR::Parameters shape;shape.attack=read("attack");shape.decay=read("decay");
  shape.sustain=read("sustain");shape.release=read("release");
  for(auto& v:voices)v.envelope.setParameters(shape);
  if(resetPending||previousMode!=mode||panic) {
   clear(midi,0,true);resetPending=false;
   if(previousMode!=mode){tracker.reset();chords.reset();calibrated.reset();calWorker.reset();trackingBusy=false;}
   previousMode=mode;
  }
  if(panic&&!previousPanic){calibrated.reset();calWorker.reset();}
  previousPanic=panic;
  tracker.setFast(mode==0);
  if(mode==3&&!isNonRealtime()){
   calWorker.setGate(gate);CalibratedTracker::Strengths result{};
   if(!panic&&calWorker.consume(result))applyCalibrated(result,midi,0);
   trackingBusy.store(calWorker.hasOverloaded(),std::memory_order_relaxed);
  }
  for(int i=0;i<audio.getNumSamples();++i) {
   float x=audio.getSample(0,i);
   if(getTotalNumInputChannels()==2)x=0.5f*(x+audio.getSample(1,i));
   x*=gain;
   rawEnvelope+=(std::abs(x)-rawEnvelope)*(std::abs(x)>rawEnvelope?0.02f:0.003f);
   if(++scopeCounter>=64) {
    scopeCounter=0;auto pos=scopeIndex.load(std::memory_order_relaxed);
    scope[pos%256].store(x,std::memory_order_relaxed);scopeIndex.store(pos+1,std::memory_order_relaxed);
   }
   if(mode==3) {
    if(!panic){
     if(isNonRealtime()){if(calibrated.push(x,gate))applyCalibrated(calibrated.getStrengths(),midi,i);}
     else calWorker.push(x);
    }
    if(rawEnvelope<gate*0.5f)++quietSamples;else{quietSamples=0;quietReset=false;}
    if(!isNonRealtime()&&!quietReset&&quietSamples>rate*0.05){
     clear(midi,i,false);calWorker.reset();quietReset=true;
    }
   } else if(mode==2) {
    if(chords.push(x,gate)&&!panic) {
     const auto& strengths=chords.getStrengths();
     for(int n=0;n<ChordTracker::notes;++n) {
      int key=n+ChordTracker::first;
      if(strengths[n]>0) {
       absence[key]=0;presence[key]=std::min(2,presence[key]+1);
       if(presence[key]>=2&&!held[key])
        start(key,440*std::pow(2.0,(key-69)/12.0),chords.level(),midi,i);
      } else {
       presence[key]=0;absence[key]=std::min(2,absence[key]+1);
       if(absence[key]>=2&&held[key])finish(key,midi,i);
      }
     }
    }
   } else {
    double hz=0;
    if(tracker.push(x,gate,hz)&&!panic) {
     if(hz<=0) {
      candidate=-1;stable=0;
      if(++miss>=(mode==0?3:4)&&note>=0)stopLead(midi,i);
     } else {
      miss=0;double pitch=69+12*std::log2(hz/440.0);int target=juce::roundToInt(pitch);
      if(target>=36&&target<=100) {
       if(note>=0&&bend&&std::abs(pitch-note)<1.95){candidate=-1;stable=0;}
       else {
        if(candidate==target)++stable;else{candidate=target;stable=1;}
        int required=mode==0&&note<0?1:2;
        if(stable>=required&&target!=note) {
         stopLead(midi,i);note=target;candidate=target;stable=2;
         start(note,hz,tracker.level(),midi,i);
        }
       }
       if(note>=0) {
        for(auto& v:voices)if(v.note==note)v.frequency=hz;
        currentPitch=(float)pitch;
        int wheel=bend?juce::jlimit(0,16383,juce::roundToInt(8192+(pitch-note)*4096)):8192;
        if(std::abs(wheel-lastWheel)>8){midi.addEvent(juce::MidiMessage::pitchWheel(1,wheel),i);lastWheel=wheel;}
       }
      } else if(++miss>=3)stopLead(midi,i);
     }
    }
   }
   double sound=0;int voiceCount=0;float maxEnvelope=0;
   for(auto& v:voices)if(v.envelope.isActive()) {
    float amp=v.envelope.getNextSample();maxEnvelope=std::max(maxEnvelope,amp);++voiceCount;
    v.phase+=v.frequency/rate;v.phase-=std::floor(v.phase);
    double oscillator=std::sin(v.phase*juce::MathConstants<double>::twoPi);
    if(wave>0)for(int h=2;h<=8;++h) {
     if(v.frequency*h>rate*0.45)break;if(wave==2&&h%2==0)continue;
     oscillator+=tone*std::sin(v.phase*juce::MathConstants<double>::twoPi*h)/(h*h);
    }
    sound+=amp*oscillator;
   }
   float out=panic?0.0f:dry*x+synth*0.22f*(float)sound/std::sqrt((float)std::max(1,voiceCount));
   for(int ch=0;ch<audio.getNumChannels();++ch)audio.setSample(ch,i,out);
   envelopeLevel.store(maxEnvelope,std::memory_order_relaxed);
  }
  uint64_t mask=0;int count=0,lowest=-1;
  for(int key=0;key<128;++key)if(held[key]){++count;if(lowest<0)lowest=key;
   if(key>=40&&key<=88)mask|=uint64_t(1)<<(key-40);}
  activeMask.store(mask,std::memory_order_relaxed);liveCount.store(count,std::memory_order_relaxed);
  livePitch.store(count==0?-1:(mode>=2?(float)lowest:currentPitch),std::memory_order_relaxed);
  inputLevel.store(mode==3?rawEnvelope:mode==2?chords.level():tracker.level(),std::memory_order_relaxed);
 }
 void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer& midi)override {
  midi.clear();clear(midi,0,true);calibrated.reset();calWorker.reset();resetPending=true;livePitch=-1;liveCount=0;activeMask=0;
 }
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}
 void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return {};}
 void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock& block)override {
  auto tree=params.copyState();if(auto xml=tree.createXml())copyXmlToBinary(*xml,block);
 }
 void setStateInformation(const void* data,int size)override {
  if(auto xml=getXmlFromBinary(data,size))
   if(xml->hasTagName(params.state.getType()))params.replaceState(juce::ValueTree::fromXml(*xml));
 }
private:
 struct Voice {int note=-1;double phase=0,frequency=0;juce::ADSR envelope;};
 juce::AudioProcessorValueTreeState params;
 Tracker tracker;ChordTracker chords;CalibratedTracker calibrated;CalibratedWorker calWorker;std::array<Voice,6> voices;
 float rawEnvelope=0;bool previousPanic=false,quietReset=false;int quietSamples=0;
 std::array<bool,128> held{};std::array<int,128> presence{},absence{};
 double rate=48000;float currentPitch=-1;
 int note=-1,candidate=-1,stable=0,miss=0,lastWheel=8192,previousMode=-1,scopeCounter=0;
 bool resetPending=true;
 void applyCalibrated(const CalibratedTracker::Strengths& values,juce::MidiBuffer& midi,int offset){
  // Release disappearing notes before allocating the next six voices.
  for(int n=0;n<CalibratedTracker::notes;++n){int key=n+CalibratedTracker::first;if(values[n]<=0&&held[key])finish(key,midi,offset);}
  for(int n=0;n<CalibratedTracker::notes;++n){int key=n+CalibratedTracker::first;if(values[n]>0&&!held[key])start(key,440*std::pow(2.0,(key-69)/12.0),rawEnvelope,midi,offset);}
 }
 void start(int key,double hz,float level,juce::MidiBuffer& midi,int offset) {
  Voice* selected=nullptr;
  for(auto& v:voices)if(v.note==key){selected=&v;break;}
  if(!selected)for(auto& v:voices)if(!v.envelope.isActive()){selected=&v;break;}
  if(!selected)for(auto& v:voices)if(v.note>=0&&!held[v.note]){selected=&v;break;}
  if(!selected)return;
  selected->envelope.reset();selected->note=key;selected->frequency=hz;selected->phase=0;
  selected->envelope.noteOn();held[key]=true;
  int velocity=juce::jlimit(1,127,(int)(std::sqrt(level)*180));
  midi.addEvent(juce::MidiMessage::noteOn(1,key,(juce::uint8)velocity),offset);
 }
 void finish(int key,juce::MidiBuffer& midi,int offset) {
  if(!held[key])return;held[key]=false;
  midi.addEvent(juce::MidiMessage::noteOff(1,key),offset);
  for(auto& v:voices)if(v.note==key)v.envelope.noteOff();
 }
 void stopLead(juce::MidiBuffer& midi,int offset) {
  if(note>=0)finish(note,midi,offset);note=candidate=-1;stable=0;lastWheel=8192;
  midi.addEvent(juce::MidiMessage::pitchWheel(1,8192),offset);
 }
 void clear(juce::MidiBuffer& midi,int offset,bool reset) {
  for(int key=0;key<128;++key)finish(key,midi,offset);
  midi.addEvent(juce::MidiMessage::allNotesOff(1),offset);
  midi.addEvent(juce::MidiMessage::pitchWheel(1,8192),offset);
  note=candidate=-1;stable=miss=0;lastWheel=8192;presence.fill(0);absence.fill(0);
  if(reset)for(auto& v:voices){v.envelope.reset();v.note=-1;}
 }
};
#include "Editor.h"
juce::AudioProcessorEditor* GuitarLink::createEditor(){return new GuitarLinkEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new GuitarLink();}

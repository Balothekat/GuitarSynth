#include <JuceHeader.h>
#include "Tracker.h"

class GuitarLinkEditor;
class GuitarLink final : public juce::AudioProcessor {
public:
 GuitarLink():AudioProcessor(BusesProperties()
  .withInput("Guitar",juce::AudioChannelSet::stereo(),true)
  .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
  params(*this,nullptr,"GuitarLink",layout()) {for(auto& v:scope)v.store(0); }
 juce::AudioProcessorValueTreeState& state(){return params;}
 std::atomic<float> inputLevel{0}, livePitch{-1}, envelopeLevel{0};
 std::array<std::atomic<float>,256> scope{};
 std::atomic<unsigned> scopeIndex{0};
 static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
  juce::AudioProcessorValueTreeState::ParameterLayout p;
  auto add=[&](const char* id,const char* name,float lo,float hi,float def) {
   p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,
    juce::NormalisableRange<float>(lo,hi),def));
  };
  add("gain","Input gain",0.1f,4,1);
  add("attack","Attack",0.001f,1,0.005f);
  add("decay","Decay",0.005f,1,0.15f);
  add("sustain","Sustain",0,1,0.75f);
  add("tone","Harmonics",0,1,0.25f);
  p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"wave",1},
    "Oscillator",juce::StringArray{"Sine","Warm","Hollow"},1));
  add("gate","Input gate",0.001f,0.1f,0.008f);
  add("dry","Dry guitar",0,1,0);
  add("synth","Test synth volume",0,1,0.5f);
  add("release","Synth release (seconds)",0.005f,0.5f,0.035f);
  p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bend",1},
    "Pitch bend (receiver must use +/- 2 semitones)",true));
  p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"panic",1},
    "Mute / stop all notes",false));
  return p;
 }
 const juce::String getName() const override{return "GuitarLink";}
 bool acceptsMidi() const override{return false;}
 bool producesMidi() const override{return true;}
 bool isMidiEffect() const override{return false;}
 double getTailLengthSeconds() const override{return 0.5;}
 bool hasEditor() const override{return true;}
 juce::AudioProcessorEditor* createEditor() override;
 void prepareToPlay(double sr,int) override {
  rate=sr;tracker.prepare(sr);note=-1;candidate=-1;stable=miss=0;
  phase=env=frequency=0;lastWheel=8192;resetPending=true;
  adsr.setSampleRate(sr);adsr.reset();envelopeNote=-1;scopeCounter=0;
  inputLevel.store(0);livePitch.store(-1);envelopeLevel.store(0);
 }
 void releaseResources() override {}
 bool isBusesLayoutSupported(const BusesLayout& b) const override {
  auto in=b.getMainInputChannelSet(),out=b.getMainOutputChannelSet();
  return (in==juce::AudioChannelSet::mono()||in==juce::AudioChannelSet::stereo())
   && (out==juce::AudioChannelSet::mono()||out==juce::AudioChannelSet::stereo());
 }
 void stop(juce::MidiBuffer& midi,int offset) {
  if(note>=0)midi.addEvent(juce::MidiMessage::noteOff(1,note),offset);
  note=-1;candidate=-1;stable=0;lastWheel=8192;
  midi.addEvent(juce::MidiMessage::pitchWheel(1,8192),offset);
 }
 void processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi) override {
  juce::ScopedNoDenormals guard;
  midi.clear();
  const int count=audio.getNumSamples();
  if(count==0)return;
  const float gate=*params.getRawParameterValue("gate");
  const float dry=*params.getRawParameterValue("dry");
  const float synth=*params.getRawParameterValue("synth");
  const double release=*params.getRawParameterValue("release");
  const float gain=*params.getRawParameterValue("gain");
  const float tone=*params.getRawParameterValue("tone");
  const int wave=juce::roundToInt(params.getRawParameterValue("wave")->load());
  juce::ADSR::Parameters shape;
  shape.attack=*params.getRawParameterValue("attack");
  shape.decay=*params.getRawParameterValue("decay");
  shape.sustain=*params.getRawParameterValue("sustain");
  shape.release=(float)release;adsr.setParameters(shape);
  const bool bend=*params.getRawParameterValue("bend")>0.5f;
  const bool panic=*params.getRawParameterValue("panic")>0.5f;
  if(resetPending||panic) {
   stop(midi,0);midi.addEvent(juce::MidiMessage::allNotesOff(1),0);
   resetPending=false;
  }

  for(int i=0;i<count;++i) {
   // Select Scarlett's guitar input in Live's track input selector.
   float x=audio.getSample(0,i);
   if(getTotalNumInputChannels()==2)x=0.5f*(x+audio.getSample(1,i));
   x*=gain;
   if(++scopeCounter>=64) {
    scopeCounter=0;auto pos=scopeIndex.load(std::memory_order_relaxed);
    scope[pos%256].store(x,std::memory_order_relaxed);
    scopeIndex.store(pos+1,std::memory_order_relaxed);
   }
   double hz=0;
   if(tracker.push(x,gate,hz)&&!panic) {
    if(hz<=0) {
     candidate=-1;stable=0;
     if(++miss>=4&&note>=0)stop(midi,i);
    } else {
     miss=0;
     double pitch=69+12*std::log2(hz/440.0);
     int target=juce::roundToInt(pitch);
     if(target>=36&&target<=100) {
      // With bend enabled, retain the base note across a +/- 2 semitone bend.
      if(note>=0&&bend&&std::abs(pitch-note)<1.95) {
       candidate=-1;stable=0;
      } else {
       if(target==candidate)++stable;else{candidate=target;stable=1;}
       if(stable>=2&&target!=note) {
        stop(midi,i);note=target;candidate=target;stable=2;
        int velocity=juce::jlimit(1,127,(int)(std::sqrt(tracker.level())*180));
        midi.addEvent(juce::MidiMessage::noteOn(1,note,(juce::uint8)velocity),i);
       }
      }
      if(note>=0) {
       frequency=hz;
       int wheel=bend?juce::jlimit(0,16383,
         juce::roundToInt(8192+(pitch-note)*4096)):8192;
       if(std::abs(wheel-lastWheel)>8) {
        midi.addEvent(juce::MidiMessage::pitchWheel(1,wheel),i);lastWheel=wheel;
       }
      }
     }
    }
   }
   bool active=note>=0&&!panic;
   if(active && note!=envelopeNote){adsr.noteOn();envelopeNote=note;}
   if(!active && envelopeNote>=0){adsr.noteOff();envelopeNote=-1;}
   env=adsr.getNextSample();
   phase+=frequency/rate;phase-=std::floor(phase);
   double oscillator=std::sin(phase*juce::MathConstants<double>::twoPi);
   if(wave>0) {
    for(int harmonic=2;harmonic<=8;++harmonic) {
     if(frequency*harmonic>rate*0.45)break;
     if(wave==2 && harmonic%2==0)continue;
     oscillator+=tone*std::sin(phase*juce::MathConstants<double>::twoPi*harmonic)
       /(harmonic*harmonic);
    }
   }
   float output=panic?0.0f:dry*x+synth*0.22f*(float)(env*oscillator);
   for(int ch=0;ch<audio.getNumChannels();++ch)audio.setSample(ch,i,output);
  }
  inputLevel.store(tracker.level(),std::memory_order_relaxed);
  livePitch.store(note>=0&&!panic&&frequency>0?(float)(69+12*std::log2(frequency/440.0)):-1,
    std::memory_order_relaxed);
  envelopeLevel.store((float)env,std::memory_order_relaxed);
 }
 void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer& midi) override {
  midi.clear(); stop(midi,0); resetPending=true;adsr.reset();envelopeNote=-1;livePitch.store(-1);
 }
 int getNumPrograms() override{return 1;}
 int getCurrentProgram() override{return 0;}
 void setCurrentProgram(int) override{}
 const juce::String getProgramName(int) override{return {};}
 void changeProgramName(int,const juce::String&) override{}
 void getStateInformation(juce::MemoryBlock& block) override {
  auto state=params.copyState();if(auto xml=state.createXml())copyXmlToBinary(*xml,block);
 }
 void setStateInformation(const void* data,int size) override {
  if(auto xml=getXmlFromBinary(data,size))
   if(xml->hasTagName(params.state.getType()))params.replaceState(juce::ValueTree::fromXml(*xml));
 }
private:
 juce::AudioProcessorValueTreeState params;
 Tracker tracker;
 juce::ADSR adsr;
 int envelopeNote=-1,scopeCounter=0;
 double rate=48000,phase=0,env=0,frequency=0;
 int note=-1,candidate=-1,stable=0,miss=0,lastWheel=8192;
 bool resetPending=true;
};
#include "Editor.h"
juce::AudioProcessorEditor* GuitarLink::createEditor(){return new GuitarLinkEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new GuitarLink();}

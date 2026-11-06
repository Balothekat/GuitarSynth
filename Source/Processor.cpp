#include <JuceHeader.h>
#include "Tracker.h"

class GuitarLink final : public juce::AudioProcessor {
public:
 GuitarLink():AudioProcessor(BusesProperties()
  .withInput("Guitar",juce::AudioChannelSet::stereo(),true)
  .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
  params(*this,nullptr,"GuitarLink",layout()) {}
 static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
  juce::AudioProcessorValueTreeState::ParameterLayout p;
  auto add=[&](const char* id,const char* name,float lo,float hi,float def) {
   p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,
    juce::NormalisableRange<float>(lo,hi),def));
  };
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
 juce::AudioProcessorEditor* createEditor() override {
  return new juce::GenericAudioProcessorEditor(*this);
 }
 void prepareToPlay(double sr,int) override {
  rate=sr;tracker.prepare(sr);note=-1;candidate=-1;stable=miss=0;
  phase=env=frequency=0;lastWheel=8192;resetPending=true;
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
  const bool bend=*params.getRawParameterValue("bend")>0.5f;
  const bool panic=*params.getRawParameterValue("panic")>0.5f;
  if(resetPending||panic) {
   stop(midi,0);midi.addEvent(juce::MidiMessage::allNotesOff(1),0);
   resetPending=false;
  }
  const double releaseCoeff=1-std::exp(-1/(rate*release));
  const double attackCoeff=1-std::exp(-1/(rate*0.002));
  for(int i=0;i<count;++i) {
   // Select Scarlett's guitar input in Live's track input selector.
   float x=audio.getSample(0,i);
   if(getTotalNumInputChannels()==2)x=0.5f*(x+audio.getSample(1,i));
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
   env+=((active?1.0:0.0)-env)*(active?attackCoeff:releaseCoeff);
   phase+=frequency/rate;phase-=std::floor(phase);
   float output=panic?0.0f:dry*x+synth*0.22f*(float)(env*
    (std::sin(phase*juce::MathConstants<double>::twoPi)
     +0.2*std::sin(phase*juce::MathConstants<double>::twoPi*2)));
   for(int ch=0;ch<audio.getNumChannels();++ch)audio.setSample(ch,i,output);
  }
 }
 void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer& midi) override {
  midi.clear(); stop(midi,0); resetPending=true;
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
 double rate=48000,phase=0,env=0,frequency=0;
 int note=-1,candidate=-1,stable=0,miss=0,lastWheel=8192;
 bool resetPending=true;
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new GuitarLink();}

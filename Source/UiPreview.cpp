#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include "GuitarProfile.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
class Preview final:public juce::JUCEApplication {
 std::unique_ptr<juce::AudioProcessor> processor;
 std::unique_ptr<juce::AudioProcessorEditor> editor;
public:
 const juce::String getApplicationName() override{return "GuitarLink Preview";}
 const juce::String getApplicationVersion() override{return "0.4";}
 void initialise(const juce::String& command) override {
  processor.reset(createPluginFilter());
  processor->setNonRealtime(true);
  processor->prepareToPlay(48000,128);
  juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
  bool on=false,off=false;float peak=0;
  auto feed=[&](bool signal,int blocks) {
   for(int block=0;block<blocks;++block) {
    for(int i=0;i<128;++i) {
     float sample=signal?(float)(0.2*std::sin(6.283185307179586*220*(block*128+i)/48000.0)):0;
     audio.setSample(0,i,sample);audio.setSample(1,i,sample);
    }
    midi.clear();processor->processBlock(audio,midi);
    for(auto event:midi){auto message=event.getMessage();on|=message.isNoteOn();off|=message.isNoteOff();}
    if(!std::isfinite(audio.getMagnitude(0,128))){setApplicationReturnValue(2);quit();return false;}
    peak=juce::jmax(peak,audio.getMagnitude(0,128));
   }
   return true;
  };
  if(!feed(true,120)||!feed(false,300))return;
  if(!on||!off||peak<0.001f||audio.getMagnitude(0,128)>0.0001f){
   setApplicationReturnValue(3);quit();return;
  }
  if(!feed(true,120))return;
  for(auto* parameter:processor->getParameters())
   if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter))
    if(ranged->getParameterID()=="tracking")ranged->setValueNotifyingHost(ranged->convertTo0to1(2));
  std::array<bool,128> chordOn{},chordOff{};
  auto feedChord=[&](bool signal,int blocks) {
   for(int block=0;block<blocks;++block) {
    for(int i=0;i<128;++i) {
     double sample=0;
     if(signal)for(int key:{52,55,59}) {
      double f=440*std::pow(2.0,(key-69)/12.0);
      for(int h=1;h<=5;++h)sample+=0.04*std::sin(6.283185307179586*f*h*(block*128+i)/48000.0)/std::pow((double)h,1.3);
     }
     audio.setSample(0,i,(float)sample);audio.setSample(1,i,(float)sample);
    }
    midi.clear();processor->processBlock(audio,midi);
    for(auto event:midi) {
     auto message=event.getMessage();
     if(message.isNoteOn())chordOn[message.getNoteNumber()]=true;
     if(message.isNoteOff())chordOff[message.getNoteNumber()]=true;
     if(message.isPitchWheel()&&message.getPitchWheelValue()!=8192){setApplicationReturnValue(4);return false;}
    }
   }
   return true;
  };
  if(!feedChord(true,160)||!feedChord(false,320)) {quit();return;}
  for(int key=0;key<128;++key) {
   bool expected=key==52||key==55||key==59;
   if(chordOn[key]!=expected || (expected&&!chordOff[key])){
    std::cerr<<"Chord MIDI mismatch key "<<key<<" on "<<chordOn[key]<<" off "<<chordOff[key]<<" expected "<<expected<<"\n";
    setApplicationReturnValue(5);quit();return;
   }
  }
  if(audio.getMagnitude(0,128)>0.0001f){setApplicationReturnValue(6);quit();return;}
  if(!feedChord(true,160)){quit();return;}
  for(auto* parameter:processor->getParameters())
   if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter))
    if(ranged->getParameterID()=="tracking")ranged->setValueNotifyingHost(ranged->convertTo0to1(3));
  std::array<bool,128> measuredOn{},measuredOff{};
  auto feedMeasured=[&](bool signal,int blocks) {
   for(int block=0;block<blocks;++block) {
    for(int i=0;i<128;++i) {
     double sample=0;
     if(signal)for(int string=0;string<4;++string) {
      double shift=string<2?std::pow(2.0,4.0/12.0):1.0;
      for(int h=0;h<12;++h){auto part=GuitarProfile::partials[string][h];double f=GuitarProfile::openHz[string]*part.ratio*shift;
       sample+=0.012*part.amplitude*std::sin(6.283185307179586*f*(block*128+i)/48000.0);}
     }
     audio.setSample(0,i,(float)sample);audio.setSample(1,i,(float)sample);
    }
    midi.clear();processor->processBlock(audio,midi);
    for(auto event:midi){auto m=event.getMessage();if(m.isNoteOn())measuredOn[m.getNoteNumber()]=true;if(m.isNoteOff())measuredOff[m.getNoteNumber()]=true;}
   }
  };
  feedMeasured(true,240);feedMeasured(false,180);
  for(int key=0;key<128;++key) {
   bool expected=key==42||key==49||key==52||key==57;
   if(measuredOn[key]!=expected||(expected&&!measuredOff[key])){
    std::cerr<<"Measured-mode MIDI mismatch "<<key<<" on "<<measuredOn[key]<<" off "<<measuredOff[key]<<"\n";
    setApplicationReturnValue(7);quit();return;
   }
  }
  feedMeasured(true,240);
  editor.reset(processor->createEditor());
  editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
  editor->setVisible(true);
  juce::Timer::callAfterDelay(700,[this,command]{
   auto shot=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.0f);
   juce::File file(command.unquoted());file.deleteFile();
   auto stream=file.createOutputStream();
   if(!stream || !juce::PNGImageFormat().writeImageToStream(shot,*stream))
    setApplicationReturnValue(1);
   quit();
  });
 }
 void shutdown() override {editor.reset();processor.reset();}
};
START_JUCE_APPLICATION(Preview)

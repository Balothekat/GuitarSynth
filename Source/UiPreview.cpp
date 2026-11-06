#include <juce_audio_utils/juce_audio_utils.h>
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
class Preview final:public juce::JUCEApplication {
 std::unique_ptr<juce::AudioProcessor> processor;
 std::unique_ptr<juce::AudioProcessorEditor> editor;
public:
 const juce::String getApplicationName() override{return "GuitarLink Preview";}
 const juce::String getApplicationVersion() override{return "0.2";}
 void initialise(const juce::String& command) override {
  processor.reset(createPluginFilter());
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

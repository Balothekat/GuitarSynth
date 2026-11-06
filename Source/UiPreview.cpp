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

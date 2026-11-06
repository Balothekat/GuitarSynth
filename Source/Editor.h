#pragma once
// Original visual design inspired by the user's reference. No third-party assets.
class GuitarLinkEditor final : public juce::AudioProcessorEditor,private juce::Timer {
 using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
 using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
 using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
 struct PaperLook : juce::LookAndFeel_V4 {
  PaperLook(){
   setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xff303d37));
   setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
   setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xfff8f6ee));
   setColour(juce::ComboBox::textColourId,juce::Colour(0xff303d37));
   setColour(juce::ComboBox::outlineColourId,juce::Colour(0xffc9cbbf));
   setColour(juce::ToggleButton::textColourId,juce::Colour(0xff303d37));
   setColour(juce::ToggleButton::tickColourId,juce::Colour(0xff608f70));
  }
  void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float v,
                       float start,float end,juce::Slider&) override {
   auto r=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(5);
   auto size=juce::jmin(r.getWidth(),r.getHeight());r=r.withSizeKeepingCentre(size,size);
   g.setColour(juce::Colour(0xffedece1));g.fillEllipse(r);
   g.setColour(juce::Colour(0xffa7b3a4));g.drawEllipse(r,1);
   juce::Path arc;arc.addCentredArc(r.getCentreX(),r.getCentreY(),size*0.5f,size*0.5f,
    0,start,start+v*(end-start),true);
   g.setColour(juce::Colour(0xff608f70));g.strokePath(arc,juce::PathStrokeType(2.5f));
   float a=start+v*(end-start);
   g.drawLine(r.getCentreX(),r.getCentreY(),r.getCentreX()+std::sin(a)*size*0.36f,
              r.getCentreY()-std::cos(a)*size*0.36f,2);
  }
 };
public:
 explicit GuitarLinkEditor(GuitarLink& processor):AudioProcessorEditor(processor),p(processor){
  setLookAndFeel(&look);setSize(920,720);
  const char* ids[]={"gain","gate","dry","synth","tone","attack","decay","sustain","release"};
  const char* labels[]={"INPUT","GATE","DRY GUITAR","SYNTH LEVEL","HARMONICS",
    "ATTACK","DECAY","SUSTAIN","RELEASE"};
  for(int i=0;i<9;++i) {
   addAndMakeVisible(knobs[i]);knobs[i].setName(labels[i]);
   knobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
   knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow,false,80,20);
   knobs[i].setNumDecimalPlacesToDisplay(3);
   attachments[i]=std::make_unique<SA>(p.state(),ids[i],knobs[i]);
   knobs[i].textFromValueFunction=[i](double v){
    if(i<2)return juce::String(juce::Decibels::gainToDecibels((float)v),1)+" dB";
    if(i==5||i==6||i==8)return juce::String(v*1000,0)+" ms";
    return juce::String(v*100,0)+" %";
   };
   knobs[i].valueFromTextFunction=[i](const juce::String& t){
    double v=t.getDoubleValue();
    if(i<2)return (double)juce::Decibels::decibelsToGain((float)v);
    if(i==5||i==6||i==8)return v/1000;
    return v/100;
   };
   knobs[i].updateText();
   addAndMakeVisible(names[i]);names[i].setText(labels[i],juce::dontSendNotification);
   names[i].setJustificationType(juce::Justification::centred);
   names[i].setColour(juce::Label::textColourId,ink);
   names[i].setFont(juce::Font(11.0f));
  }
  addAndMakeVisible(waves);waves.setName("Oscillator");
  waves.addItemList({"Sine","Warm","Hollow"},1);
  waveAttachment=std::make_unique<CA>(p.state(),"wave",waves);
  bend.setButtonText("Follow bends  /  MIDI +/-2 st");addAndMakeVisible(bend);
  mute.setButtonText("MUTE / PANIC");addAndMakeVisible(mute);
  bendAttachment=std::make_unique<BA>(p.state(),"bend",bend);
  muteAttachment=std::make_unique<BA>(p.state(),"panic",mute);
  knobs[0].setTooltip("Input gain before tracking. Select your guitar's mono input in Live.");
  knobs[1].setTooltip("Raise to reject background noise; lower for quiet playing.");
  bend.setTooltip("Off: separate semitone notes. On: follow bends within two semitones.");
  for(auto& v:history)v=-1;
  startTimerHz(30);
 }
 ~GuitarLinkEditor() override {stopTimer();setLookAndFeel(nullptr);}
 void resized() override {
  for(int i=0;i<5;++i) {
   int x=28+i*112;names[i].setBounds(x,294,100,20);knobs[i].setBounds(x,316,100,94);
  }
  for(int i=5;i<9;++i) {
   int x=438+(i-5)*112;names[i].setBounds(x,540,100,20);knobs[i].setBounds(x,562,100,94);
  }
  waves.setBounds(658,330,218,32);
  bend.setBounds(38,680,370,24);mute.setBounds(734,680,150,24);
 }
 void paint(juce::Graphics& g) override {
  g.fillAll(paper);g.setColour(ink);g.setFont(juce::Font(34.0f,juce::Font::bold));
  g.drawText("GuitarLink",30,18,300,48,juce::Justification::centredLeft);
  text(g,"LIVE GUITAR / MIDI + SYNTH",340,30,330,24,12);
  text(g,"02",842,26,45,30,20);
  line(g,24,82,896,82);line(g,24,274,896,274);line(g,24,434,896,434);
  text(g,"TRACKER",36,94,120,22,12);
  float pitch=p.livePitch.load(std::memory_order_relaxed);
  juce::String note=pitch<0?"--":juce::MidiMessage::getMidiNoteName(
    juce::roundToInt(pitch),true,true,3);
  g.setColour(ink);g.setFont(juce::Font(42.0f));g.drawText(note,36,126,120,56,juce::Justification::centredLeft);
  float level=p.inputLevel.load(std::memory_order_relaxed);
  text(g,juce::String(juce::Decibels::gainToDecibels(juce::jmax(level,0.00001f)),1)+" dB",
    36,188,110,24,13);
  g.setColour(juce::Colour(0xffdfe4d4));g.fillRoundedRectangle(166,117,14,130,3);
  float meter=juce::jlimit(0.0f,1.0f,(juce::Decibels::gainToDecibels(
    juce::jmax(level,0.00001f))+60)/60);
  g.setColour(level>0.9f?juce::Colour(0xffb66a59):green);
  g.fillRoundedRectangle(166,247-130*meter,14,130*meter,3);
  drawTracker(g);
  text(g,"OSCILLATOR",654,296,226,20,12);
  text(g,"Single-note tracking",654,373,226,20,13);
  text(g,"Clean DI gives the clearest result",654,396,226,18,11);
  text(g,"INPUT WAVEFORM",36,450,340,24,12);
  drawScope(g);
  text(g,"AMPLITUDE ENVELOPE",438,440,440,20,12);
  drawEnvelope(g);
  line(g,24,670,896,670);
  text(g,"MIDI OUT  /  CH 1",426,682,260,20,11);
 }
private:
 GuitarLink& p;PaperLook look;
 juce::Colour paper{0xfff8f6ee},ink{0xff303d37},green{0xff719a76},grid{0xffdce0d3};
 std::array<juce::Slider,9> knobs;
 std::array<juce::Label,9> names;
 std::array<std::unique_ptr<SA>,9> attachments;
 juce::ComboBox waves;juce::ToggleButton bend,mute;
 std::unique_ptr<CA> waveAttachment;std::unique_ptr<BA> bendAttachment,muteAttachment;
 juce::TooltipWindow tips{this,700};
 std::array<float,180> history{};
 int head=0;
 void timerCallback() override {history[head]=p.livePitch.load(std::memory_order_relaxed);
  head=(head+1)%180;repaint();}
 void text(juce::Graphics& g,const juce::String& s,int x,int y,int w,int h,float font){
  g.setColour(ink);g.setFont(juce::Font(font));g.drawText(s,x,y,w,h,juce::Justification::centredLeft);
 }
 void line(juce::Graphics& g,float x,float y,float xx,float yy){
  g.setColour(grid);g.drawLine(x,y,xx,yy,1);
 }
 void drawTracker(juce::Graphics& g) {
  juce::Rectangle<float> r(210,116,432,134);
  g.setColour(juce::Colour(0xfff0f2e7));g.fillRoundedRectangle(r,4);
  for(int i=0;i<=4;++i){float y=r.getBottom()-i*r.getHeight()/4;
   line(g,r.getX(),y,r.getRight(),y);text(g,juce::String(36+i*12),214,(int)y-15,30,14,9);}
  juce::Path path;bool connected=false;
  for(int i=0;i<180;++i) {
   float v=history[(head+i)%180];
   if(v<0){connected=false;continue;}
   float x=r.getX()+i*r.getWidth()/179,y=r.getBottom()-juce::jlimit(0.0f,1.0f,(v-36)/48)*r.getHeight();
   if(!connected)path.startNewSubPath(x,y);else path.lineTo(x,y);connected=true;
  }
  g.setColour(green);g.strokePath(path,juce::PathStrokeType(2));
  float currentPitch=p.livePitch.load(std::memory_order_relaxed);
  if(currentPitch>=0) {
   float dotY=r.getBottom()-juce::jlimit(0.0f,1.0f,(currentPitch-36)/48)*r.getHeight();
   g.fillEllipse(r.getRight()-7,dotY-3,6,6);
  }
  // Passive note display: incoming guitar controls this keyboard.
  float live=p.livePitch.load();int current=live<0?-1:juce::roundToInt(live);
  const int whites[]={0,2,4,5,7,9,11};
  for(int i=0;i<14;++i) {
   int n=48+(i/7)*12+whites[i%7];float x=660+i*15.5f;
   g.setColour(n==current?green:juce::Colour(0xffe2e8d7));
   g.fillRect(x,122.0f,15.0f,104.0f);g.setColour(grid);g.drawRect(x,122.0f,15.0f,104.0f);
   if(whites[i%7]!=4&&whites[i%7]!=11) {
    g.setColour(n+1==current?ink:juce::Colour(0xffa8b7a4));g.fillRect(x+10,122.0f,9.0f,62.0f);
   }
  }
  text(g,"DETECTED NOTES  /  C2 - B3",660,236,222,18,10);
 }
 void drawScope(juce::Graphics& g) {
  juce::Rectangle<float> r(36,482,350,168);
  g.setColour(juce::Colour(0xffedf0e2));g.fillRoundedRectangle(r,5);
  line(g,r.getX(),r.getCentreY(),r.getRight(),r.getCentreY());
  juce::Path path;auto pos=p.scopeIndex.load(std::memory_order_relaxed);
  for(unsigned i=0;i<256;++i) {
   float sample=p.scope[(pos+i)%256].load(std::memory_order_relaxed);
   float x=r.getX()+i*r.getWidth()/255;
   float y=r.getCentreY()-juce::jlimit(-1.0f,1.0f,sample)*r.getHeight()*0.45f;
   if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);
  }
  g.setColour(green);g.strokePath(path,juce::PathStrokeType(1.2f));
 }
 void drawEnvelope(juce::Graphics& g) {
  // Schematic envelope shape; knobs below set the actual ADSR parameters.
  float a=(float)knobs[5].getValue(),d=(float)knobs[6].getValue();
  float sustain=(float)knobs[7].getValue(),release=(float)knobs[8].getValue();
  float total=a+d+0.3f+release;
  float x=438,w=438;
  // Compact curve overlaid in the heading area.
  float base=525,top=470,ax=x+w*a/total,dx=ax+w*d/total;
  float rx=dx+w*0.3f/total,sy=base-(base-top)*sustain;
  juce::Path curve;curve.startNewSubPath(x,base);curve.lineTo(ax,top);
  curve.lineTo(dx,sy);curve.lineTo(rx,sy);curve.lineTo(x+w,base);
  g.setColour(juce::Colour(0xffbb8a70));g.strokePath(curve,juce::PathStrokeType(1.4f));
 }
};

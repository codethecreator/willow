#include <JuceHeader.h>
#include "SoundEngine.h"
#include <array>
#include <memory>

namespace {
struct FactoryPreset {
 const char* name;
 std::array<float,10> values;
};
constexpr std::array<const char*,10> names={
 "memory","memoryFeel","shatter","shatterFeel","aura","auraFeel",
 "dreamy","dreamyFeel","mix","output"
};
// Factory settings are original to LunarField, not copies of Noctra presets.
constexpr std::array<FactoryPreset,16> presets{{
 {"Soft Moonlight",       {{.32f,.2f,.07f,.1f,.65f,.2f,.75f,.15f,.96f,-2.f}}},
 {"Worn Photograph",      {{.62f,1.12f,.09f,.23f,.22f,.0f,.19f,.17f,.86f,0.f}}},
 {"Wide Awake",           {{.10f,.0f,.03f,.0f,.89f,1.13f,.58f,.2f,.98f,-1.f}}},
 {"Velvet Room",          {{.08f,.6f,.03f,.0f,.68f,.12f,.87f,.15f,.98f,-2.f}}},
 {"Afterglow",            {{.29f,.0f,.12f,.3f,.73f,.35f,.79f,1.08f,.96f,-2.f}}},
 {"Ghost Transmission",   {{.54f,2.25f,.17f,2.2f,.18f,2.7f,.27f,3.f,.79f,-1.f}}},
 {"Tape Garden",          {{.54f,.15f,.24f,.0f,.39f,.12f,.20f,0.f,.90f,0.f}}},
 {"Tidal Glass",          {{.06f,0.f,.09f,.3f,.73f,.12f,.49f,1.0f,.89f,0.f}}},
 {"Fractured Signal",     {{.23f,2.2f,.49f,2.14f,.08f,0.f,.17f,0.f,.76f,-1.f}}},
 {"Horizon Echo",         {{.08f,0.f,.04f,0.f,.70f,1.f,.88f,2.f,.98f,-2.f}}},
 {"Backwards Bloom",      {{.12f,.0f,.07f,0.f,.58f,.0f,.86f,3.f,.96f,-2.f}}},
 {"Gentle Orbit",         {{.25f,.1f,.10f,0.f,.47f,2.f,.58f,0.f,.96f,-1.f}}},
 {"Deep Orbit",           {{.20f,.4f,.08f,.1f,.80f,1.1f,.92f,.2f,.94f,-1.f}}},
 {"Bright Regolith",      {{.07f,.1f,.12f,.0f,.88f,.0f,.82f,1.0f,.93f,-2.f}}},
 {"Signal Collapse",      {{.56f,2.3f,.72f,2.3f,.33f,2.8f,.48f,2.7f,.96f,-3.f}}},
 {"Cathedral Drift",      {{.14f,0.f,.03f,0.f,.74f,.2f,.90f,1.1f,.98f,-3.f}}}
}};
}

class LunarFieldProcessor final : public juce::AudioProcessor {
public:
 juce::AudioProcessorValueTreeState state;
 static juce::AudioProcessorValueTreeState::ParameterLayout createParameters(){
   juce::AudioProcessorValueTreeState::ParameterLayout layout;
   for (int i=0;i<10;++i){
     float max=(i<8 && i%2==1)?3.f:(i==9?12.f:1.f);
     float min=i==9?-12.f:0.f;
     float def=i==8?1.f:0.f;
     layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{names[(size_t)i],1},juce::String(names[(size_t)i]),
        juce::NormalisableRange<float>(min,max),def));
   }
   return layout;
 }
 LunarFieldProcessor():juce::AudioProcessor(
    BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                     .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    state(*this,nullptr,"LunarFieldState",createParameters()){}
 const juce::String getName()const override{return "LunarField";}
 bool acceptsMidi()const override{return false;}
 bool producesMidi()const override{return false;}
 bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 8.0;}
 int getNumPrograms()override{return 1;}
 int getCurrentProgram()override{return 0;}
 void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return {};}
 void changeProgramName(int,const juce::String&)override{}
 bool hasEditor()const override{return true;}
 bool isBusesLayoutSupported(const BusesLayout& b)const override{
   auto out=b.getMainOutputChannelSet();
   return (out==juce::AudioChannelSet::mono()||out==juce::AudioChannelSet::stereo())
      && out==b.getMainInputChannelSet();
 }
 lunar::Parameters parameters()const{
   lunar::Parameters p;
   p.memory=get("memory");p.memoryFeel=get("memoryFeel");
   p.shatter=get("shatter");p.shatterFeel=get("shatterFeel");
   p.aura=get("aura");p.auraFeel=get("auraFeel");
   p.dreamy=get("dreamy");p.dreamyFeel=get("dreamyFeel");
   p.mix=get("mix");p.output=get("output");
   return p;
 }
 float get(const char* name)const{return state.getRawParameterValue(name)->load();}
 void prepareToPlay(double sampleRate,int)override{
   engine.prepare((float)sampleRate);
   engine.resetParameters(parameters());
 }
 void releaseResources()override{}
 void processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)override{
   juce::ScopedNoDenormals noDenormals;
   if(buffer.getNumChannels()<1)return;
   float bpm=120.f;
   if(auto* ph=getPlayHead()){
     if(auto position=ph->getPosition()){
       if(auto hostBpm=position->getBpm())bpm=(float)*hostBpm;
     }
   }
   engine.setBpm(bpm);
   auto p=parameters();
   float* left=buffer.getWritePointer(0);
   float* right=buffer.getNumChannels()>1?buffer.getWritePointer(1):nullptr;
   for(int i=0;i<buffer.getNumSamples();++i){
     float x=left[i],y=right?right[i]:x;
     auto result=engine.process({x,y},p);
     left[i]=result.l;
     if(right)right[i]=result.r;
   }
 }
 void getStateInformation(juce::MemoryBlock& block)override{
   juce::MemoryOutputStream output(block,true);
   state.copyState().writeToStream(output);
 }
 void setStateInformation(const void* data,int bytes)override{
   auto tree=juce::ValueTree::readFromData(data,(size_t)bytes);
   if(tree.isValid())state.replaceState(tree);
 }
 void applyPreset(size_t id){
   if(id>=presets.size())return;
   for(size_t i=0;i<names.size();++i){
     auto* param=state.getParameter(names[i]);
     if(param) param->setValueNotifyingHost(param->convertTo0to1(presets[id].values[i]));
   }
 }
 class Editor final : public juce::AudioProcessorEditor {
   LunarFieldProcessor& owner;
   std::array<juce::Slider,10> controls;
   std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,10> attached;
   juce::ComboBox factory;
   juce::TextButton surprise{"INSPIRE ME"};
   juce::Label status;
   juce::Random random;
   juce::Colour accent=juce::Colour(0xffa8cafa);
 public:
   explicit Editor(LunarFieldProcessor& p):juce::AudioProcessorEditor(p),owner(p){
     setSize(960,570);setResizable(true,true);setResizeLimits(740,510,1600,900);
     for(size_t i=0;i<controls.size();++i){
       auto& s=controls[i];
       s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
       s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,64,22);
       s.setColour(juce::Slider::rotarySliderFillColourId,accent);
       s.setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour(0xff42516b));
       s.setColour(juce::Slider::thumbColourId,juce::Colours::white);
       s.setColour(juce::Slider::textBoxTextColourId,juce::Colours::white);
       s.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
       addAndMakeVisible(s);
       attached[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          owner.state,names[i],s);
     }
     factory.setTextWhenNothingSelected("Select sound...");
     for(size_t i=0;i<presets.size();++i)factory.addItem(presets[i].name,(int)i+1);
     addAndMakeVisible(factory);
     factory.onChange=[this]{
       if(factory.getSelectedId()>0)owner.applyPreset((size_t)factory.getSelectedId()-1);
     };
     addAndMakeVisible(surprise);
     surprise.onClick=[this]{
       int id=random.nextInt((int)presets.size());
       owner.applyPreset((size_t)id);
       factory.setSelectedId(id+1,juce::dontSendNotification);
     };
     status.setText("16 CURATED STARTING POINTS  /  SMOOTH FEEL BLENDING",juce::dontSendNotification);
     status.setColour(juce::Label::textColourId,juce::Colour(0xff9baabe));
     status.setJustificationType(juce::Justification::centredRight);
     addAndMakeVisible(status);
   }
   void paint(juce::Graphics& g)override{
     auto w=getWidth(),h=getHeight();
     juce::ColourGradient bg(juce::Colour(0xff18253e),0,0,juce::Colour(0xff090e18),(float)w,(float)h,false);
     g.setGradientFill(bg);g.fillAll();
     g.setColour(juce::Colours::white);
     g.setFont(juce::Font(juce::FontOptions(30.f,juce::Font::bold)));
     g.drawText("LUNARFIELD",26,19,335,42,juce::Justification::centredLeft);
     g.setColour(juce::Colour(0xff9caed0));g.setFont(13.f);
     g.drawText("TEXTURE  /  MOVEMENT  /  ATMOSPHERE",29,63,370,20,juce::Justification::centredLeft);
     const std::array<const char*,4> titles={"MEMORY","SHATTER","AURA","DREAMY"};
     const std::array<const char*,4> sub={"TAPE  +  AGE","WARMTH  +  DAMAGE","WIDTH  +  MOTION","REVERB  +  SPACE"};
     float boxW=(w-50.f)/4.f;
     for(int i=0;i<4;++i){
       auto x=25.f+(float)i*boxW;
       g.setColour(juce::Colour(0xff1e2c47));
       g.fillRoundedRectangle(x,133.f,boxW-9.f,260.f,13.f);
       g.setColour(juce::Colour(0xffdbe8ff));g.setFont(20.f);
       g.drawText(titles[(size_t)i],(int)x+8,145,(int)boxW-26,28,juce::Justification::centred);
       g.setColour(juce::Colour(0xff879bb8));g.setFont(11.f);
       g.drawText(sub[(size_t)i],(int)x+8,178,(int)boxW-26,23,juce::Justification::centred);
       g.setFont(12.f);g.setColour(juce::Colour(0xffc1cfe1));
       g.drawText("AMOUNT",(int)x+6,204,(int)boxW/2-10,20,juce::Justification::centred);
       g.drawText("FEEL",(int)(x+boxW*.5f),204,(int)boxW/2-13,20,juce::Justification::centred);
     }
     g.setColour(juce::Colour(0xffdbe8ff));g.setFont(13.f);
     g.drawText("MASTER MIX",50,409,116,25,juce::Justification::centred);
     g.drawText("OUTPUT dB",204,409,116,25,juce::Justification::centred);
     g.setColour(juce::Colour(0xff9caed0));g.setFont(12.f);
     g.drawText("LUNARFIELD 0.4  /  ORIGINAL DSP",w-270,h-33,247,18,juce::Justification::centredRight);
   }
   void resized()override{
     const auto w=getWidth(),h=getHeight();
     float boxW=(w-50.f)/4.f;
     for(int i=0;i<4;++i){
       int x=(int)(25.f+i*boxW);
       int half=(int)(boxW*.5f);
       controls[(size_t)(2*i)].setBounds(x+10,229,half-15,135);
       controls[(size_t)(2*i+1)].setBounds(x+half+2,229,half-20,135);
     }
     controls[8].setBounds(57,435,100,105);
     controls[9].setBounds(212,435,100,105);
     factory.setBounds(w-486,34,215,34);
     surprise.setBounds(w-249,34,208,34);
     status.setBounds(w-490,h-67,465,25);
   }
 };
 juce::AudioProcessorEditor* createEditor()override {return new Editor(*this);}
private:
 lunar::SoundEngine engine;
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new LunarFieldProcessor();}

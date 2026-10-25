#include <JuceHeader.h>
#include <cmath>
#include <array>
class LunarField final : public juce::AudioProcessor {
public:
  juce::AudioProcessorValueTreeState apvts;
  using AP=juce::AudioParameterFloat;
  static juce::AudioProcessorValueTreeState::ParameterLayout layout(){
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (auto id: {"memory","shatter","aura","dreamy","mix"})
      l.add(std::make_unique<AP>(juce::ParameterID{id,1},id,juce::NormalisableRange<float>(0.f,1.f),id==juce::String("mix")?1.f:0.f));
    for (auto id: {"memoryFeel","shatterFeel","auraFeel","dreamyFeel"})
      l.add(std::make_unique<AP>(juce::ParameterID{id,1},id,juce::NormalisableRange<float>(0.f,3.f),0.f));
    l.add(std::make_unique<AP>(juce::ParameterID{"output",1},"Output dB",juce::NormalisableRange<float>(-12.f,12.f),0.f));
    return l;
  }
  LunarField():juce::AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),apvts(*this,nullptr,"LunarFieldState",layout()){}
  const juce::String getName() const override {return "LunarField";}
  bool acceptsMidi()const override{return false;} bool producesMidi()const override{return false;}
  bool isMidiEffect()const override{return false;} double getTailLengthSeconds()const override{return 5.0;}
  int getNumPrograms()override{return 1;} int getCurrentProgram()override{return 0;}
  void setCurrentProgram(int)override{} const juce::String getProgramName(int)override{return {};}
  void changeProgramName(int,const juce::String&)override{}
  bool hasEditor()const override{return true;}
  bool isBusesLayoutSupported(const BusesLayout& b)const override{
    auto c=b.getMainOutputChannelSet();return (c==juce::AudioChannelSet::mono()||c==juce::AudioChannelSet::stereo())&&c==b.getMainInputChannelSet();
  }
  void prepareToPlay(double sr,int)override{
    sampleRate=sr;
    for(auto &d:delay){d.assign((size_t)(sr*1.6)+8,0.f);}
    index=0;phase=0;flutter=0;lp={};hp={};feedback={};rng=123456789;
  }
  void releaseResources()override{}
  float get(const char* key)const {return apvts.getRawParameterValue(key)->load();}
  float noise(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return (float(rng&0xffffff)/8388608.f)-1.f;}
  float tap(int c,float seconds)const {
    auto &d=delay[(size_t)c];if(d.empty())return 0.f;
    float f=seconds*float(sampleRate);int offset=juce::jlimit(1,(int)d.size()-2,(int)f);
    size_t at=(index+d.size()-size_t(offset))%d.size();
    return d[at];
  }
  void processBlock(juce::AudioBuffer<float>& buf,juce::MidiBuffer&)override{
    juce::ScopedNoDenormals nd;
    const int channels=buf.getNumChannels();if(channels==0||delay[0].empty())return;
    float mem=get("memory"), mFeel=get("memoryFeel"), sh=get("shatter"), sFeel=get("shatterFeel");
    float au=get("aura"), aFeel=get("auraFeel"), dream=get("dreamy"), dFeel=get("dreamyFeel"), mix=get("mix");
    float gain=juce::Decibels::decibelsToGain(get("output"));
    for(int i=0;i<buf.getNumSamples();++i){
      phase+=float(1.0/sampleRate);flutter=std::fmod(flutter+float(5.3/sampleRate),1.f);
      for(int c=0;c<channels;++c){
        float x=buf.getSample(c,i),v=x;
        float wob=tap(c,.014f+.002f*std::sin(6.2831853f*.47f*phase));
        lp[(size_t)c]+=.025f*(v-lp[(size_t)c]);
        float worn=lp[(size_t)c]*1.1f;
        float vinyl=.96f*v+.017f*noise();
        float warp=tap(c,.014f+.0015f*std::sin(6.2831853f*flutter));
        std::array<float,4> m={wob,vinyl,worn,warp};
        auto blend=[](const std::array<float,4>& a,float pos){int k=juce::jlimit(0,2,(int)pos);return juce::jmap(pos-float(k),a[(size_t)k],a[(size_t)k+1]);};
        v=juce::jmap(mem,v,blend(m,mFeel));
        float soft=std::tanh(v*2.6f)/std::tanh(2.6f);
        float hard=juce::jlimit(-1.f,1.f,v*4.f);
        float crush=std::round(v*24.f)/24.f;
        float rate=tap(c,.0005f);
        std::array<float,4> s={soft,hard,crush,rate};
        v=juce::jmap(sh,v,blend(s,sFeel));
        float chorus=tap(c,.02f+.004f*std::sin(6.2831853f*.28f*phase));
        float width=v*.9f+tap(1-c,.012f)*.1f;
        float pan=v*(.75f+(c?-.25f:.25f)*std::sin(6.2831853f*.28f*phase));
        hp[(size_t)c]+=(.018f+.08f*(.5f+.5f*std::sin(6.2831853f*.28f*phase)))*(v-hp[(size_t)c]);
        std::array<float,4> a={.5f*(v+chorus),width,pan,hp[(size_t)c]};
        v=juce::jmap(au,v,blend(a,aFeel));
        float echo=tap(c,.37f),reverse=tap(c,.10f+.07f*(.5f+.5f*std::sin(6.2831853f*phase)));
        feedback[(size_t)c]=.65f*feedback[(size_t)c]+.35f*echo;
        float room=tap(c,.043f)+.55f*tap(c,.029f);
        float shimmer=tap(c,.10f+.035f*std::sin(6.2831853f*phase));
        std::array<float,4> d={room*.45f,echo*.8f,shimmer*.75f,reverse*.75f};
        v=juce::jmap(dream,v,blend(d,dFeel));
        delay[(size_t)c][index]=juce::jlimit(-4.f,4.f,x+feedback[(size_t)c]*.45f);
        buf.setSample(c,i,juce::jlimit(-4.f,4.f,juce::jmap(mix,x,v)*gain));
      }
      index=(index+1)%delay[0].size();
    }
  }
  void getStateInformation(juce::MemoryBlock& mb)override {if(auto x=apvts.copyState().createXml())juce::copyXmlToBinary(*x,mb);}
  void setStateInformation(const void* d,int n)override {if(auto x=juce::getXmlFromBinary(d,n))apvts.replaceState(juce::ValueTree::fromXml(*x));}
  class Editor final:public juce::AudioProcessorEditor {
    LunarField& plugin;
    std::array<juce::Slider,10> sliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,10> links;
    juce::TextButton randomize{"RANDOMISE"};
    const std::array<const char*,10> ids={"memory","memoryFeel","shatter","shatterFeel","aura","auraFeel","dreamy","dreamyFeel","mix","output"};
  public:
    explicit Editor(LunarField& p):juce::AudioProcessorEditor(p),plugin(p){
      setSize(920,565);setResizable(true,true);setResizeLimits(720,500,1600,900);
      for(int i=0;i<10;++i){auto& s=sliders[(size_t)i];s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,72,20);
        addAndMakeVisible(s);links[(size_t)i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(plugin.apvts,ids[(size_t)i],s);}
      addAndMakeVisible(randomize);
      randomize.onClick=[this]{juce::Random r;for(int i=0;i<8;++i){auto* param=plugin.apvts.getParameter(ids[(size_t)i]);if(param)param->setValueNotifyingHost(r.nextFloat()*(i%2?1.f:.75f));}};
    }
    void paint(juce::Graphics& g)override{
      g.fillAll(juce::Colour(0xff0d1220));
      juce::ColourGradient bg(juce::Colour(0xff1f2c4e),0,0,juce::Colour(0xff080b14),float(getWidth()),float(getHeight()),false);
      g.setGradientFill(bg);g.fillRect(getLocalBounds());
      auto w=getWidth();g.setColour(juce::Colours::white);g.setFont(28.f);
      g.drawText("LUNARFIELD",25,17,330,42,juce::Justification::centredLeft);
      g.setFont(13.f);g.setColour(juce::Colour(0xffb1bfd3));g.drawText("CREATIVE ATMOSPHERIC MULTI-FX",27,61,330,20,juce::Justification::centredLeft);
      const char* labels[]={"MEMORY","SHATTER","AURA","DREAMY"};const float width=(w-50.f)/4.f;
      for(int i=0;i<4;++i){auto x=25.f+i*width;g.setColour(juce::Colour(0xff1b2540));g.fillRoundedRectangle(x,115.f,width-10.f,280.f,12.f);
        g.setColour(juce::Colour(0xffbed0eb));g.setFont(19.f);g.drawText(labels[i],int(x+8),127,int(width-27),32,juce::Justification::centred);
        g.setFont(12.f);g.drawText("AMOUNT",int(x+8),173,int(width/2-8),18,juce::Justification::centred);
        g.drawText("FEEL",int(x+width/2),173,int(width/2-15),18,juce::Justification::centred);}
      g.setColour(juce::Colour(0xffe0e8fa));g.setFont(14.f);
      g.drawText("MASTER MIX",40,416,140,20,juce::Justification::centred);g.drawText("OUTPUT dB",198,416,140,20,juce::Justification::centred);
      g.setFont(12.f);g.drawText("ORIGINAL DSP  •  WINDOWS VST3  •  v0.2",w-330,getHeight()-35,312,20,juce::Justification::centredRight);
    }
    void resized()override{
      auto w=getWidth();float width=(w-50.f)/4.f;
      for(int i=0;i<4;++i){int x=int(25+i*width);
        sliders[(size_t)(i*2)].setBounds(x+10,196,int(width/2-16),170);
        sliders[(size_t)(i*2+1)].setBounds(x+int(width/2),196,int(width/2-16),170);}
      sliders[8].setBounds(54,440,100,90);sliders[9].setBounds(215,440,100,90);
      randomize.setBounds(w-165,26,136,35);
    }
  };
  juce::AudioProcessorEditor* createEditor()override{return new Editor(*this);}
private:
  std::array<std::vector<float>,2> delay;
  std::array<float,2> lp{},hp{},feedback{};
  size_t index=0;double sampleRate=44100;float phase=0,flutter=0;uint32_t rng=123456789;
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new LunarField();}

#include <JuceHeader.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static void require(bool ok,const char* what){
 if(!ok){std::fprintf(stderr,"PLUGIN HOST SMOKE FAILED: %s\n",what);std::exit(1);}
}
int main(){
 auto plugin=std::unique_ptr<juce::AudioProcessor>(createPluginFilter());
 require(plugin!=nullptr,"processor factory");
 require(plugin->getTotalNumOutputChannels()==2,"stereo outputs");
 require(plugin->getTotalNumInputChannels()==2,"stereo inputs");
 plugin->prepareToPlay(48000.,256);
 juce::AudioBuffer<float> audio(2,256);
 juce::MidiBuffer midi;
 int rendered=0;
 double recovered=0.;
 auto& params=plugin->getParameters();
 for(int block=0;block<4000;++block){
   // Rapid parameter changes via exactly the same JUCE notification methods
   // used by the editor's preset selector.
   if(block%3==0){
     for(size_t i=0;i<params.size();++i){
       auto* param=params[i];
       float normalized=((block/3+i*13)%17)/16.f;
       param->setValueNotifyingHost(normalized);
     }
   }
   if(block%37==0){
      juce::MemoryBlock state;
      plugin->getStateInformation(state);
      if(!state.isEmpty())plugin->setStateInformation(state.getData(),(int)state.getSize());
   }
   auto* left=audio.getWritePointer(0);
   auto* right=audio.getWritePointer(1);
   for(int n=0;n<256;++n){
     double time=(rendered+n)/48000.;
     float sample=.24f*std::sin(2*juce::MathConstants<double>::pi*220.*time)
                 +.09f*std::sin(2*juce::MathConstants<double>::pi*444.*time);
     left[n]=sample;right[n]=sample;
   }
   if(block%211==0)left[54]=std::numeric_limits<float>::quiet_NaN();
   if(block%313==0)right[91]=std::numeric_limits<float>::infinity();
   plugin->processBlock(audio,midi);
   for(int channel=0;channel<2;++channel){
     for(int n=0;n<256;++n){
       float x=audio.getSample(channel,n);
       require(std::isfinite(x),"NaN/Inf sample delivered to host");
       require(std::abs(x)<1.6f,"unbounded audio delivered to host");
       if(block>3980)recovered+=std::abs(x);
     }
   }
   rendered+=256;
 }
 require(recovered>1.0,"audio should still be active after thousands of parameter changes");
 plugin->releaseResources();
 std::puts("PASS: real JUCE AudioProcessor switches, state recalls, NaN ingress and finite output.");
 return 0;
}

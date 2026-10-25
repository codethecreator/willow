#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace lunar {
constexpr float pi=3.14159265358979323846f;
inline float clamp(float x,float lo,float hi){return std::max(lo,std::min(hi,x));}
inline float finiteSample(float x,float maximum=16.f){
 return std::isfinite(x)?clamp(x,-maximum,maximum):0.f;
}
inline float softGuard(float x){
 if(!std::isfinite(x))return 0.f;
 float a=std::abs(x);
 if(a<=1.f)return x;
 float y=1.f+.5f*(1.f-std::exp(-2.f*(a-1.f)));
 return std::copysign(y,x);
}
inline float lerp(float a,float b,float t){return a+(b-a)*t;}
inline float smoothstep(float x){x=clamp(x,0.f,1.f);return x*x*(3.f-2.f*x);}
struct Stereo {float l=0,r=0; Stereo operator+(Stereo b)const{return {l+b.l,r+b.r};}
 Stereo operator-(Stereo b)const{return {l-b.l,r-b.r};}
 Stereo operator*(float k)const{return {l*k,r*k};}};
inline Stereo mix(Stereo a,Stereo b,float t){return {lerp(a.l,b.l,t),lerp(a.r,b.r,t)};}
inline Stereo choose(const std::array<Stereo,4>& modes,float position){
 position=clamp(position,0.f,3.f);int a=std::min(2,(int)position);
 float t=smoothstep(position-(float)a);return mix(modes[(size_t)a],modes[(size_t)a+1],t);
}
// Double-precision transport time via an integer sample counter.
// Unlike the old float-seconds accumulator this does not stall after
// a few minutes; even days-long sessions retain sample-accurate time.
struct TransportClock {
 std::uint64_t samplePosition=0;
 double sampleRate=48000.;
 void prepare(double rate){sampleRate=rate>0&&std::isfinite(rate)?rate:48000.;samplePosition=0;}
 void advance(std::uint64_t frames=1){samplePosition+=frames;}
 double seconds()const{return double(samplePosition)/sampleRate;}
 float sine(float frequency,float phase=0.f)const{
   double theta=6.283185307179586476925286766559 *
      (seconds()*double(frequency)+double(phase));
   return float(std::sin(theta));
 }
};
struct OnePole {
 float state=0,cachedHz=-1.f,cachedSr=-1.f,cachedCoeff=.1f;
 float low(float signal,float hz,float sr){
   float x=finiteSample(signal);
   if(!std::isfinite(state))state=0;
   if(!std::isfinite(hz))hz=1000.f;
   if(hz!=cachedHz || sr!=cachedSr){
     cachedHz=hz;cachedSr=sr;
     cachedCoeff=1.f-std::exp(-2.f*pi*clamp(hz,1.f,sr*.48f)/sr);
   }
   state=finiteSample(state+(x-state)*cachedCoeff);
   return state;
 }
 void reset(){state=0;cachedHz=-1.f;}
};
struct Ring {
 std::vector<float> samples;size_t write=0;
 void resize(size_t n){samples.assign(std::max((size_t)8,n),0.f);write=0;}
 // Keep nonfinite or runaway feedback from poisoning the entire delay buffer.
 void push(float v){samples[write]=finiteSample(v,8.f);write=(write+1)%samples.size();}
 float read(float samplesBack)const{
   if(samples.empty()||!std::isfinite(samplesBack))return 0.f;
   float off=clamp(samplesBack,1.f,(float)samples.size()-2.f);
   float pos=float(write)-off;
   if(pos<0)pos+=(float)samples.size();
   size_t i=(size_t)pos;
   float fraction=pos-(float)i;
   float a=finiteSample(samples[i%samples.size()],8.f);
   float b=finiteSample(samples[(i+1)%samples.size()],8.f);
   return finiteSample(lerp(a,b,fraction),8.f);
 }
};
struct Comb {
 Ring buf;float filtered=0,feedback=.74f,damp=.22f;
 void prepare(int n,float fb,float dp){buf.resize((size_t)n);feedback=fb;damp=dp;filtered=0;}
 float run(float input){float v=buf.read((float)buf.samples.size()-1.f);
   filtered=finiteSample(lerp(v,filtered,damp),8.f);
   buf.push(finiteSample(input+filtered*feedback,8.f));return v;}
};
struct Allpass {
 Ring buf;float gain=.5f;
 void prepare(int n){buf.resize((size_t)n);}
 float run(float x){x=finiteSample(x,8.f);float delayed=buf.read((float)buf.samples.size()-1.f);
 float out=finiteSample(delayed-x*gain,8.f);buf.push(finiteSample(x+delayed*gain,8.f));return out;}
};
struct Reverb {
 std::array<std::array<Comb,4>,2> comb;
 std::array<std::array<Allpass,2>,2> aps;
 std::array<float,2> crossState{};
 void prepare(float sr){
   crossState={};
   constexpr int lengths[2][4]={{1557,1617,1491,1422},{1653,1723,1601,1511}};
   constexpr int all[2][2]={{225,556},{248,579}};
   for(int c=0;c<2;++c){for(int i=0;i<4;++i)comb[(size_t)c][(size_t)i].prepare(std::max(32,(int)(lengths[c][i]*sr/44100.f)),.86f,.36f);
    for(int i=0;i<2;++i)aps[(size_t)c][(size_t)i].prepare(std::max(10,(int)(all[c][i]*sr/44100.f)));}
 }
 Stereo run(Stereo input){
   float ins[2]={input.l,input.r};float out[2]{};
   for(int c=0;c<2;++c){float s=0;
    // Gain-normalised diffusion: the previous 4-comb sum and
    // cross-channel reinjection could drive a sustained signal up to
    // the 1.5f softGuard ceiling. Reduce both feedback injection and
    // the post-comb sum so the reverb remains stable and musical.
    const float spatialFeed=ins[c]*.12f+crossState[(size_t)(1-c)]*.008f;
    for(auto &combFilter:comb[(size_t)c])s+=combFilter.run(spatialFeed);
    s*=.22f;
    for(auto &ap:aps[(size_t)c])s=ap.run(s);
    out[c]=s;
   }
   crossState={finiteSample(out[0],2.f),finiteSample(out[1],2.f)};
   return {crossState[0],crossState[1]};
 }
};
struct Parameters {
 float memory=0,memoryFeel=0,shatter=0,shatterFeel=0,aura=0,auraFeel=0,dreamy=0,dreamyFeel=0,mix=1,output=0;
 float wobble=.60f,wobbleRate=.45f;
};
class SoundEngine {
 float sr=48000.f;
 TransportClock clock;
 float transportDrift=0.f;
 uint32_t randState=0x98765431u;
 Ring tape[2],chorus[2],echo[2],pitch[2],reverse[2],early[2];
 std::array<OnePole,2> vintageLP,radioHP,sourceHF,noiseLP,airLP,echoHP,verbHP,revHP;
 std::array<OnePole,2> movingLP,postShatterLP,crushTone;
 Reverb verb;
 std::array<float,2> previous{},held{},analogNoise{},echoFB{};
 float crushCounter=0.f,pitchPhase=0.f,reversePhase=0.f;
 Parameters eased{};
 float bpm=120.f;
 static float db(float d){return std::pow(10.f,d/20.f);}
 float random(){randState^=randState<<13;randState^=randState>>17;randState^=randState<<5;
   return ((float)(randState&0xffffff)/8388608.f)-1.f;}
 float oscillator(float hz,float phaseOffset=0.f)const{return clock.sine(hz,phaseOffset);}
 static float safeParam(float x,float lo,float hi,float fallback){return std::isfinite(x)?clamp(x,lo,hi):fallback;}
 static Stereo blendAmount(Stereo dry,Stereo wet,float amount,float maxWet=1.f){
   return mix(dry,wet,smoothstep(clamp(amount,0.f,1.f))*maxWet);
 }
 void interpolateParameters(const Parameters& p){
  float a=1.f-std::exp(-1.f/(.028f*sr));
  auto approach=[a](float& smoothed,float value,float lo,float hi,float fallback){
    if(!std::isfinite(smoothed))smoothed=fallback;
    float target=safeParam(value,lo,hi,fallback);
    smoothed=lerp(smoothed,target,a);
  };
  approach(eased.memory,p.memory,0,1,0);
  approach(eased.memoryFeel,p.memoryFeel,0,3,0);
  approach(eased.shatter,p.shatter,0,1,0);
  approach(eased.shatterFeel,p.shatterFeel,0,3,0);
  approach(eased.aura,p.aura,0,1,0);
  approach(eased.auraFeel,p.auraFeel,0,3,0);
  approach(eased.dreamy,p.dreamy,0,1,0);
  approach(eased.dreamyFeel,p.dreamyFeel,0,3,0);
  approach(eased.mix,p.mix,0,1,1);
  approach(eased.output,p.output,-12,12,0);
  approach(eased.wobble,p.wobble,0,1,.60f);
  approach(eased.wobbleRate,p.wobbleRate,0,1,.45f);
 }
 Stereo memory(Stereo input) {
  const float x[2]={input.l,input.r};Stereo result[4]{};
  // Reel transport modelling: variable read-head displacement produces
  // actual pitch instability instead of simply filtering the signal.
  // The nonlinear depth mapping preserves subtle settings and enables
  // dramatic warble when the depth dial is pushed.
  const float depth=smoothstep(eased.wobble);
  const float speed=.22f+1.40f*eased.wobbleRate;
  const float wow=(.0002f+.0057f*depth)*oscillator(speed)
                 +(.00013f+.0023f*depth)*oscillator(speed*.53f,.17f);
  const float flutter=(.00006f+.00070f*depth)*oscillator(5.6f+6.f*eased.wobbleRate,.29f)
                     +(.00003f+.00024f*depth)*oscillator(11.8f+3.f*eased.wobbleRate,.71f);
  const float wowOffset=wow+flutter+transportDrift*(.0012f*depth);
  for(int c=0;c<2;++c){
    float delayed=tape[c].read(sr*(.023f+wowOffset));
    tape[c].push(x[c]);
    // Tape saturation and hiss: noise is deliberately tied to the source's high-band content.
    float low=vintageLP[(size_t)c].low(delayed,5600.f,sr);
    float tapeTone=std::tanh(low*1.2f)/1.2f;
    float hf=std::abs(x[c]-sourceHF[(size_t)c].low(x[c],1800.f,sr));
    float dustGate=clamp(hf*3.2f+.004f,0.f,1.f);
    float hiss=noiseLP[(size_t)c].low(random(),6200.f,sr)*dustGate*.020f;
    float aged=airLP[(size_t)c].low(delayed,3700.f,sr);
    float highpass=x[c]-radioHP[(size_t)c].low(x[c],260.f,sr);
    float worn=std::tanh(aged*1.38f)*.81f;
    float drift=tape[c].read(sr*(.023f+wowOffset*1.75f+.0011f*depth*oscillator(.32f,.15f)));
    if(c==0){result[0].l=tapeTone;result[1].l=delayed+hiss;result[2].l=highpass*.8f;result[3].l=worn*.32f+drift*.68f;}
    else{result[0].r=tapeTone;result[1].r=delayed+hiss;result[2].r=highpass*.8f;result[3].r=worn*.32f+drift*.68f;}
  }
  return blendAmount(input,choose({result[0],result[1],result[2],result[3]},eased.memoryFeel),eased.memory,1.f);
 }
 Stereo shatter(Stereo input){
  float in[2]={input.l,input.r};Stereo modes[4]{};
  for(int c=0;c<2;++c){
    float x=in[c],warm=0,hard=0;
    for(int k=0;k<2;++k){
      float v=lerp(previous[(size_t)c],x,(k+1)*.5f);
      warm+=std::tanh(v*4.3f)*.35f;
      hard+=std::tanh(v*10.5f)*.31f;
    }
    float bitcrush=held[(size_t)c]*.92f;
    float fold=crushTone[(size_t)c].low(std::sin(x*7.4f)*.68f,6500.f,sr);
    float values[4]={warm,hard,bitcrush,fold};
    for(int i=0;i<4;++i){
      if(c==0)modes[i].l=values[i];else modes[i].r=values[i];
    }
    previous[(size_t)c]=x;
  }
  if(++crushCounter>=sr/4400.f){
    crushCounter=0.f;
    held[0]=std::round(input.l*13.f)/13.f;
    held[1]=std::round(input.r*13.f)/13.f;
  }
  Stereo processed=choose({modes[0],modes[1],modes[2],modes[3]},eased.shatterFeel);
  Stereo shaped{postShatterLP[0].low(processed.l,11800.f,sr),
                postShatterLP[1].low(processed.r,11800.f,sr)};
  return blendAmount(input,shaped,eased.shatter,.96f);
 }
 Stereo aura(Stereo input){
  Stereo chorusOut{},widthOut{},panOut{},filterOut{};
  float mid=(input.l+input.r)*.5f,side=(input.l-input.r)*.5f;
  float panLfo=oscillator(.32f);
  for(int c=0;c<2;++c){
    const float x=c==0?input.l:input.r;
    const float phase=c==0?0.f:.37f;
    // Independent interpolated delay voices create genuine left/right decorrelation.
    const float a=chorus[c].read(sr*(.014f+.0058f*oscillator(.37f,phase)));
    const float b=chorus[c].read(sr*(.029f+.0072f*oscillator(.23f,phase+.29f)));
    chorus[c].push(x);
    float lush=x*.34f+a*.49f+b*.34f;
    float thick=(c==0?mid+side*1.65f:mid-side*1.65f)*.57f+a*.48f-b*.27f;
    float position=panLfo*.88f;
    float gain=(c==0?std::sqrt((1.f-position)*.5f):std::sqrt((1.f+position)*.5f))*1.46f;
    float pan=mid*gain+side*(c==0?.45f:-.45f);
    float hz=700.f+1700.f*(.5f+.5f*oscillator(.47f,phase));
    float moving=movingLP[(size_t)c].low(x,hz,sr)*.75f+(a-b)*.55f;
    if(c==0){chorusOut.l=lush;widthOut.l=thick;panOut.l=pan;filterOut.l=moving;}
    else{chorusOut.r=lush;widthOut.r=thick;panOut.r=pan;filterOut.r=moving;}
  }
  return blendAmount(input,choose({chorusOut,widthOut,panOut,filterOut},eased.auraFeel),eased.aura,.98f);
 }
 float granularPitch(Ring &ring,float source,float phase){
   float n=sr*.092f;
   ring.push(source);
   float p1=phase,p2=std::fmod(phase+.5f,1.f);
   auto grain=[&](float p){
     float w=.5f-.5f*std::cos(2.f*pi*p);
     float delay=sr*.012f+n*(1.f-p);
     return std::array<float,2>{ring.read(delay),w};
   };
   auto a=grain(p1),b=grain(p2);
   return (a[0]*a[1]+b[0]*b[1])/(a[1]+b[1]+.00001f);
 }
 float reverseGrains(Ring& ring,float src,float phase){
  float n=sr*.22f;ring.push(src);
  float p1=phase,p2=std::fmod(phase+.5f,1.f);
  auto grain=[&](float p){
    float win=.5f-.5f*std::cos(2.f*pi*p);
    return std::array<float,2>{ring.read(n+2.f*n*p),win};
  };
  auto a=grain(p1),b=grain(p2);
  return (a[0]*a[1]+b[0]*b[1])/(a[1]+b[1]+.00001f);
 }
 Stereo dreamy(Stereo source,Stereo clean){
  Stereo feed=mix(source,clean,.42f);
  float x[2]={feed.l,feed.r};
  Stereo reverbInput{};
  for(int c=0;c<2;++c){
    float highpass=x[c]-verbHP[(size_t)c].low(x[c],c==0?160.f:180.f,sr);
    if(c==0)reverbInput.l=highpass;else reverbInput.r=highpass;
  }
  // First reflections and a cross-fed late tail add space to centred mono sources.
  float la=early[0].read(sr*.019f),lb=early[0].read(sr*.047f);
  float ra=early[1].read(sr*.027f),rb=early[1].read(sr*.061f);
  early[0].push(reverbInput.l);early[1].push(reverbInput.r);
  Stereo reflection{la*.57f-rb*.23f,ra*.57f-lb*.23f};
  Stereo late=verb.run(reverbInput+reflection*.23f);
  Stereo room=late*1.9f+reflection*.28f;
  Stereo octave{granularPitch(pitch[0],room.l,pitchPhase),
                granularPitch(pitch[1],room.r,pitchPhase)};
  Stereo ping{};
  float delaySeconds=(60.f/bpm)*.75f;
  for(int c=0;c<2;++c){
    float delayed=echo[c].read(sr*delaySeconds);
    float softened=echoHP[(size_t)c].low(delayed,6100.f,sr);
    echoFB[(size_t)c]=softened*.52f;
    echo[c].push((c==0?reverbInput.l:reverbInput.r)+echoFB[(size_t)(1-c)]);
    if(c==0)ping.l=softened;else ping.r=softened;
  }
  Stereo reversed{reverseGrains(reverse[0],reverbInput.l,reversePhase),
                  reverseGrains(reverse[1],reverbInput.r,reversePhase)};
  std::array<Stereo,4> modes={room*1.9f,
    room*.90f+octave*1.80f,
    ping*1.40f+room*.78f,
    reversed*1.35f+room*.85f};
  return source+choose(modes,eased.dreamyFeel)*(smoothstep(eased.dreamy)*1.18f);
 }
public:
 void prepare(float sampleRate){
   sr=std::isfinite(sampleRate)?clamp(sampleRate,8000.f,192000.f):48000.f;
   clock.prepare(sr);transportDrift=0;
   bpm=120;pitchPhase=0;reversePhase=0;crushCounter=0;randState=0x98765431u;
   for(int c=0;c<2;++c){
     tape[c].resize((size_t)(sr*.1f));chorus[c].resize((size_t)(sr*.1f));
     echo[c].resize((size_t)(sr*3.f));pitch[c].resize((size_t)(sr*.5f));
     reverse[c].resize((size_t)(sr*1.f));
     early[c].resize((size_t)(sr*.12f));
   }
   verb.prepare(sr);
   for(auto& a:{&vintageLP,&radioHP,&sourceHF,&noiseLP,&airLP,&echoHP,&verbHP,&revHP,&movingLP,&postShatterLP,&crushTone})
     for(auto& f:*a)f.reset();
   previous={};held={};analogNoise={};echoFB={};
   eased={};eased.mix=1.f;
 }
 void setBpm(float value){bpm=std::isfinite(value)?clamp(value,35.f,240.f):120.f;}
 void resetParameters(const Parameters& p){eased=p;
  eased.memory=safeParam(eased.memory,0,1,0);eased.memoryFeel=safeParam(eased.memoryFeel,0,3,0);
  eased.shatter=safeParam(eased.shatter,0,1,0);eased.shatterFeel=safeParam(eased.shatterFeel,0,3,0);
  eased.aura=safeParam(eased.aura,0,1,0);eased.auraFeel=safeParam(eased.auraFeel,0,3,0);
  eased.dreamy=safeParam(eased.dreamy,0,1,0);eased.dreamyFeel=safeParam(eased.dreamyFeel,0,3,0);
  eased.mix=safeParam(eased.mix,0,1,1);eased.output=safeParam(eased.output,-12,12,0);
  eased.wobble=safeParam(eased.wobble,0,1,.60f);eased.wobbleRate=safeParam(eased.wobbleRate,0,1,.45f);
 }
 Stereo process(Stereo in,const Parameters& p){
   interpolateParameters(p);
   Stereo clean{finiteSample(in.l,8.f),finiteSample(in.r,8.f)};
   // Maintain finite values at every processor boundary so preset changes
   // cannot contaminate the downstream delay/reverb feedback networks.
   Stereo tonal=memory(clean);
   tonal={finiteSample(tonal.l,8.f),finiteSample(tonal.r,8.f)};
   tonal=shatter(tonal);
   tonal={finiteSample(tonal.l,8.f),finiteSample(tonal.r,8.f)};
   tonal=aura(tonal);
   tonal={finiteSample(tonal.l,8.f),finiteSample(tonal.r,8.f)};
   Stereo wet=dreamy(tonal,clean);
   wet={finiteSample(wet.l,8.f),finiteSample(wet.r,8.f)};
   Stereo out=mix(clean,wet,clamp(eased.mix,0.f,1.f))*db(eased.output);
   // Harsh input spikes are restrained, while regular programme levels remain transparent.
   out.l=softGuard(out.l);
   out.r=softGuard(out.r);
   clock.advance();
   // Slowly varying stochastic mechanical drift: bounded, not white-noise jitter.
   transportDrift=clamp(transportDrift*.99998f+random()*.0009f,-1.f,1.f);
   pitchPhase+=1.f/(sr*.092f);if(pitchPhase>=1.f)pitchPhase-=1.f;
   reversePhase+=1.f/(sr*.22f);if(reversePhase>=1.f)reversePhase-=1.f;
   return out;
 }
};
} // namespace lunar

#include "../Source/SoundEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
using lunar::SoundEngine;using lunar::Parameters;using lunar::Stereo;
static void ensure(bool ok,const char* msg){
 if(!ok){std::fprintf(stderr,"FAILED: %s\n",msg);std::exit(1);}
}
static float tone(int i,int sr){
 float s=(float)i/sr;
 return .23f*std::sin(2*lunar::pi*220.f*s)+.13f*std::sin(2*lunar::pi*440.f*s)+.06f*std::sin(2*lunar::pi*880.f*s);
}
struct Measure{double rms=0,max=0,diff=0;int bad=0;};
static Measure evaluate(Parameters p,int sr,int frames){
 SoundEngine engine;engine.prepare((float)sr);engine.resetParameters(p);
 double sum=0,diff=0,maxPeak=0;int nonFinite=0;
 for(int i=0;i<frames;++i){
   float x=tone(i,sr),y=tone(i,sr)*.9f;
   auto out=engine.process({x,y},p);
   if(!std::isfinite(out.l)||!std::isfinite(out.r))++nonFinite;
   double a=(double)out.l,b=(double)out.r;
   sum+=a*a+b*b;diff+=((double)x-a)*((double)x-a)+((double)y-b)*((double)y-b);
   maxPeak=std::max(maxPeak,std::max(std::abs(a),std::abs(b)));
 }
 return {std::sqrt(sum/frames/2.0),maxPeak,std::sqrt(diff/frames/2.0),nonFinite};
}
static void integer(std::ofstream& f,unsigned value,int bytes){
 for(int i=0;i<bytes;++i){char b=(char)((value>>(i*8))&255);f.write(&b,1);}
}
static void renderWav(const std::string& filename,Parameters p,int sr){
 constexpr int secs=5;int frames=secs*sr;
 std::ofstream file(filename,std::ios::binary);ensure((bool)file,"demo WAV output file");
 file.write("RIFF",4);integer(file,36+frames*4,4);file.write("WAVEfmt ",8);
 integer(file,16,4);integer(file,1,2);integer(file,2,2);integer(file,sr,4);
 integer(file,sr*4,4);integer(file,4,2);integer(file,16,2);
 file.write("data",4);integer(file,frames*4,4);
 SoundEngine e;e.prepare((float)sr);e.resetParameters(p);
 for(int i=0;i<frames;++i){
   float time=float(i)/sr;
   auto env=[&](float start,float len){
     float t=(time-start)/len;
     return t>=0.f&&t<1.f?std::sin(lunar::pi*t)*std::exp(-t*.3f):0.f;
   };
   float chord=0;
   constexpr float notes[]={110.f,164.81f,220.f,329.63f,440.f};
   for(int j=0;j<5;++j){
     float freq=notes[j];
     chord+=.055f*std::sin(2*lunar::pi*freq*time+std::sin(time*.26f)*.1f);
   }
   float pluck=.22f*env(.2f,.42f)*std::sin(2*lunar::pi*440.f*time);
   pluck+=.18f*env(1.2f,.46f)*std::sin(2*lunar::pi*493.88f*time);
   pluck+=.20f*env(2.2f,.56f)*std::sin(2*lunar::pi*523.25f*time);
   float ramp=std::min(1.f,time*.75f)*std::min(1.f,(5.f-time)*.7f);
   float x=(chord*ramp+pluck);
   auto out=e.process({x,x*.94f},p);
   int16_t l=(int16_t)std::lround(lunar::clamp(out.l,-.999f,.999f)*32767.f);
   int16_t r=(int16_t)std::lround(lunar::clamp(out.r,-.999f,.999f)*32767.f);
   integer(file,(uint16_t)l,2);integer(file,(uint16_t)r,2);
 }
}
int main(int argc,char** argv){
 for(int rate:{44100,48000,96000}){
   Parameters dry;dry.mix=0;
   auto m=evaluate(dry,rate,rate*2);
   std::printf("Dry %dHz rms=%.4f diff=%.8f peak=%.3f\n",rate,m.rms,m.diff,m.max);
   ensure(m.bad==0 && m.diff<1e-6,"dry passthrough should be bit-identical");
   Parameters p;p.memory=.72f;p.memoryFeel=1.6f;p.shatter=.51f;p.shatterFeel=2.2f;
   p.aura=.65f;p.auraFeel=.7f;p.dreamy=.70f;p.dreamyFeel=1.7f;
   m=evaluate(p,rate,rate*2);
   std::printf("All modes %dHz rms=%.4f diff=%.4f peak=%.3f\n",rate,m.rms,m.diff,m.max);
   ensure(m.bad==0 && m.max<2.2 && m.rms<1.f,"combined processors must remain finite and bounded");
 }
 for(int section=0;section<4;++section){
   for(int mode=0;mode<4;++mode){
     Parameters p;
     if(section==0){p.memory=.75f;p.memoryFeel=(float)mode;}
     if(section==1){p.shatter=.70f;p.shatterFeel=(float)mode;}
     if(section==2){p.aura=.72f;p.auraFeel=(float)mode;}
     if(section==3){p.dreamy=.70f;p.dreamyFeel=(float)mode;}
     auto m=evaluate(p,48000,48000*2);
     std::printf("%d/%d rms=%.4f diff=%.5f peak=%.3f\n",section,mode,m.rms,m.diff,m.max);
     ensure(m.bad==0 && m.max<2.0,"individual mode stability");
     ensure(m.diff>.0001,"each mode must change the signal audibly");
   }
 }
 // A realistic automation sweep should never emit NaN, infinity, or huge spikes.
 SoundEngine e;e.prepare(48000);Parameters p;p.dreamy=.66f;p.shatter=.30f;p.aura=.45f;
 for(int i=0;i<48000*3;++i){
   float pos=(float)i/(48000*3);
   p.memory=pos;p.memoryFeel=3.f*pos;
   p.dreamyFeel=3.f*pos;p.shatterFeel=3.f*pos;p.auraFeel=3.f*pos;
   auto y=e.process({tone(i,48000),tone(i,48000)*.8f},p);
   ensure(std::isfinite(y.l)&&std::isfinite(y.r)&&std::abs(y.l)<2.2f&&std::abs(y.r)<2.2f,"automation stability");
 }
 // Retain programme ambience once the input stops.
 e.prepare(48000);p=Parameters{};p.dreamy=.80f;p.dreamyFeel=0.f;e.resetParameters(p);
 double tail=0;
 for(int i=0;i<48000*2;++i){
   float impulse=(i==0)?1.f:0.f;
   auto y=e.process({impulse,impulse},p);
   if(i>48000/10&&i<48000)tail+=std::abs(y.l)+std::abs(y.r);
 }
 ensure(tail>.001,"reverb should create a persistent decaying tail");

 // Both the chorus width and spatial room modes must generate a stereo field
 // from an otherwise perfectly identical L/R signal.
 for(int mode=0;mode<4;++mode){
   Parameters wide;wide.aura=.92f;wide.auraFeel=(float)mode;
   SoundEngine spatial;spatial.prepare(48000.f);spatial.resetParameters(wide);
   double midPower=0,sidePower=0;
   for(int i=0;i<48000*3;++i){
     float x=.18f*std::sin(lunar::pi*2.f*211.f*i/48000.f)
             +.16f*std::sin(lunar::pi*2.f*397.f*i/48000.f);
     auto y=spatial.process({x,x},wide);
     if(i>24000){
       double m=.5*(y.l+y.r),s=.5*(y.l-y.r);
       midPower+=m*m;sidePower+=s*s;
     }
   }
   float ratio=std::sqrt(sidePower/(midPower+1.e-12));
   std::printf("Aura stereo mode %d side/mid %.4f\n",mode,ratio);
   ensure(ratio>.055,"Aura mode must produce true stereo width from mono");
 }
 {
   Parameters spacious;spacious.dreamy=.90f;spacious.dreamyFeel=0.f;
   SoundEngine s;s.prepare(48000.f);s.resetParameters(spacious);
   double m=0,diff=0;
   for(int i=0;i<48000*4;++i){
     float x=i<24000?.24f*std::sin(lunar::pi*2.f*420.f*i/48000.f):0.f;
     auto y=s.process({x,x},spacious);
     if(i>24000){m+=std::pow(.5*(y.l+y.r),2);diff+=std::pow(.5*(y.l-y.r),2);}
   }
   ensure(diff>m*.003,"Reverb must create a stereo tail");
 }

 if(argc>1){
   std::string dest=argv[1];
   Parameters dry;dry.mix=0;
   Parameters tape;tape.memory=.57f;tape.memoryFeel=.3f;tape.aura=.39f;tape.auraFeel=0.f;tape.dreamy=.28f;
   Parameters lush;lush.memory=.10f;lush.aura=.62f;lush.auraFeel=.12f;lush.dreamy=.75f;lush.dreamyFeel=1.f;
   Parameters grunge;grunge.memory=.30f;grunge.memoryFeel=2.1f;grunge.shatter=.52f;grunge.shatterFeel=2.3f;grunge.dreamy=.25f;grunge.dreamyFeel=2.f;
   renderWav(dest+"/01-Dry.wav",dry,48000);
   renderWav(dest+"/02-Tape-Garden.wav",tape,48000);
   renderWav(dest+"/03-Octave-Shimmer.wav",lush,48000);
   renderWav(dest+"/04-Fractured-Signal.wav",grunge,48000);
 }
 std::puts("PASS: engine stability, 16 modes, automation, reverb tail, and WAV renders.");
}

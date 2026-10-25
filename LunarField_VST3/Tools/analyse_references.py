"""Analyse public Nightfield Noctra dry/wet marketing demos.
No audio data is committed to the repository. Analysis is ephemeral on the CI runner.
"""
import json, math, os, subprocess, tempfile, urllib.request
import numpy as np

BASE="https://cdn.shopify.com/s/files/1/1043/6263/9690/files/"
FILES={
 "Pad":{"Dry":"PADTORR.mp3?v=1785687316","Regolith":"PADREG.mp3?v=1785687418","Glass Night":"PADGLA.mp3?v=1785687432"},
 "Guitar":{"Dry":"GUITARTORR.mp3?v=1785685877","Beautiful Ending":"GUITARBEA.mp3?v=1785685877","Blooming Stars":"GUITARBLO.mp3?v=1785685878"},
 "Saw":{"Dry":"LEADtorr.mp3?v=1787090387","VHS Bloom":"LEADvhs.mp3?v=1787090387","Silver Drift":"LEADsilver.mp3?v=1787090387"},
 "Bass":{"Dry":"BASSTORR.mp3?v=1785687264","Burnt Orbit":"BASSBUR.mp3?v=1785687322","Floating Midnight":"BASSFLO.mp3?v=1785687411"}
}
FS=48000

def load_audio(name,path):
 if not os.path.isfile(path):
  request=urllib.request.Request(BASE+name,headers={"User-Agent":"Mozilla/5.0"})
  data=urllib.request.urlopen(request,timeout=40).read()
  with open(path,"wb") as f:f.write(data)
 print("DOWNLOADED",name,os.stat(path).st_size)
 # Intentionally retain channel separation for width measurements.
 output=subprocess.check_output(["ffmpeg","-loglevel","error","-i",path,"-vn",
    "-ar",str(FS),"-ac","2","-c:a","pcm_f32le","-f","f32le","-"])
 return np.frombuffer(output,dtype="<f4").reshape((-1,2)).copy()

def measurements(x):
 n=len(x)
 rms=np.sqrt(np.mean(np.square(x.astype("float64"))))+1e-12
 pk=np.max(np.abs(x))
 mid=(x[:,0]+x[:,1])*0.5
 side=(x[:,0]-x[:,1])*0.5
 mr=np.sqrt(np.mean(np.square(mid.astype("float64"))))+1e-12
 sr=np.sqrt(np.mean(np.square(side.astype("float64"))))+1e-12
 # Average power spectrum in non-overlapping 8192-sample Hann windows.
 xx=x[:len(x)//8192*8192].reshape((-1,8192,2))
 f=np.fft.rfftfreq(8192,1/FS)
 fft=np.fft.rfft(xx*np.hanning(8192)[None,:,None],axis=1)
 power=np.mean(np.abs(fft)**2,axis=(0,2))
 power/=max(power.sum(),1e-30)
 band_edges=[20,150,500,2000,6000,20000]
 bands=[float(np.sum(power[(f>=a)&(f<b)])) for a,b in zip(band_edges[:-1],band_edges[1:])]
 corr=np.corrcoef(x[::10].T)[0,1] if n>48000 else 0.
 tail=x[-min(len(x),FS):]
 tail_rms=float(np.sqrt(np.mean(np.square(tail.astype("float64"))))+1e-12)
 return {"duration":round(n/FS,3),"rms_db":round(20*np.log10(rms),1),
 "peak_db":round(20*np.log10(max(pk,1e-12)),1),"crest_db":round(20*np.log10(max(pk,1e-12)/rms),1),
 "stereo_side_mid_db":round(20*np.log10(sr/mr),1),
 "stereo_correlation":round(float(corr),3),"band_power":bands,"end_1s_rms_db":round(20*np.log10(tail_rms),1)}

def summary(dry,fx):
 a,b=measurements(dry),measurements(fx)
 bands=lambda q:[round(10*np.log10(max(k,1e-10)),1) for k in q["band_power"]]
 base=bands(a);effect=bands(b)
 return {
 "dry_rms_db":a["rms_db"],"fx_rms_db":b["rms_db"],"loudness_rms_delta_db":round(b["rms_db"]-a["rms_db"],1),
 "dry_crest_db":a["crest_db"],"fx_crest_db":b["crest_db"],
 "dry_side_mid_db":a["stereo_side_mid_db"],"fx_side_mid_db":b["stereo_side_mid_db"],
 "dry_stereo_corr":a["stereo_correlation"],"fx_stereo_corr":b["stereo_correlation"],
 "bands_hz":["20-150","150-500","500-2000","2000-6000","6000-20000"],
 "relative_spectral_change_db":[round(v-u,1) for v,u in zip(effect,base)],
 "duration_dry_sec":a["duration"],"duration_fx_sec":b["duration"],
 "dry_end_rms_db":a["end_1s_rms_db"],"fx_end_rms_db":b["end_1s_rms_db"]}

with tempfile.TemporaryDirectory() as root:
 for category,contents in FILES.items():
  audio={}
  for label,name in contents.items():
   local=os.path.join(root,name.split("?")[0])
   audio[label]=load_audio(name,local)
  dry=audio["Dry"]
  for label,fx in audio.items():
   if label=="Dry":continue
   print("REFERENCE_COMPARISON",json.dumps({"instrument":category,"preset":label,**summary(dry,fx)},sort_keys=True))
print("REFERENCE_ANALYSIS_COMPLETED")

import html, re, urllib.request, urllib.parse
url='https://nightfieldaudio.com/'
req=urllib.request.Request(url,headers={'User-Agent':'Mozilla/5.0'})
page=urllib.request.urlopen(req,timeout=30).read().decode('utf-8','replace')
print('HTML SIZE',len(page))
matches=set()
for expression in [
  r"""(?:https?:)?//[^"'\s<>]+?\.(?:mp3|wav|ogg|m4a|flac|aac)(?:\?[^"'\s<>]*)?""",
  r"""(?:src|data-src|file|url|audio)[^>]{0,160}\.(?:mp3|wav|ogg|m4a|flac|aac)[^<]{0,100}""",
  r"""[^"'\s<>]{0,150}(?:dry|bloom|regolith|guitar|vhs|glass-night|silver-drift)[^"'\s<>]{0,200}"""]:
 for m in re.findall(expression,page,re.I):matches.add(html.unescape(m))
for m in sorted(matches):
 if len(m)<400:print('CANDIDATE',m)
print('found candidates:',len(matches))

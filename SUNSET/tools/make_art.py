"""Original procedural vector/palette artwork, embedded as row RLE, no assets."""
from pathlib import Path
from PIL import Image, ImageDraw
import math,random
ROOT=Path(__file__).resolve().parents[1]
def lerp(a,b,t):return tuple(round(x+(y-x)*t) for x,y in zip(a,b))
pal=[(0,0,0)]*256
for i in range(1,64):pal[i]=lerp((9,3,29),(139,22,107),((i-1)/62)**.8)
for i in range(64,112):
 t=(i-64)/47
 pal[i]=lerp((255,242,40),(255,62,140),t/.67) if t<.67 else lerp((255,62,140),(255,195,225),(t-.67)/.33)
for i in range(112,128):pal[i]=lerp((12,5,34),(66,12,78),(i-112)/15)
for i in range(128,144):pal[i]=lerp((40,18,95),(99,248,255),(i-128)/15)
for i in range(144,160):pal[i]=lerp((5,7,24),(16,7,43),(i-144)/15)
for i in range(160,176):pal[i]=lerp((7,28,69),(48,231,255),(i-160)/15)
for i in range(176,184):pal[i]=lerp((12,76,108),(58,224,247),(i-176)/7)
for i in range(184,192):pal[i]=lerp((83,23,119),(255,101,211),(i-184)/7)
car=[(5,5,18),(8,12,25),(17,18,39),(28,22,54),(41,27,71),(60,29,88),(81,32,109),(109,40,133),(145,56,166),(170,76,179),(47,72,104),(72,104,133),(104,155,183),(161,211,222),(97,40,135),(66,22,92),(30,50,86),(17,31,58),(59,112,154),(38,85,130),(192,76,211),(246,117,228),(106,190,221),(27,49,72),(245,127,242),(160,85,203),(119,67,170),(67,40,105),(20,9,38),(38,12,47),(56,15,52),(80,19,63)]
pal[192:224]=car
for i in range(224,240):pal[i]=lerp((237,23,64),(255,225,114),(i-224)/15)
pal[240:256]=[(20,113,171),(49,165,208),(89,231,255),(167,250,255),(255,58,194),(244,33,110),(175,34,143),(238,194,255),(255,249,236),(125,76,153),(60,32,82),(40,14,52),(32,9,45),(24,7,39),(18,6,34),(11,4,27)]
def dither_gradients(im):
 # Fixed 2x2 ordered thresholds blend adjacent palette levels. Only the
 # sky/sun ramps are eligible: all overlaid geometry and >=192 occlusion
 # indices remain exactly intact. This texture is baked once, never animated.
 w,h=im.size;sy=h/480;data=bytearray(im.tobytes())
 thresholds=((.125,.625),(.875,.375))
 def pair(value,ceiling,y):
  low=int(value);fraction=value-low
  return tuple(min(ceiling,low+(fraction>t)) for t in thresholds[y&1])
 for y in range(h):
  yy=y/sy
  sky=pair(1+62*min(1,yy/230),63,y)
  sun=pair(64+max(0,min(47,(yy-35)/196*47)),111,y)
  for x in range(w):
   at=y*w+x;c=data[at]
   if 1<=c<64:data[at]=sky[x&1]
   elif 64<=c<112:data[at]=sun[x&1]
 result=Image.frombytes('P',(w,h),bytes(data));result.putpalette(im.getpalette())
 return result
def make(w,h):
 sx=w/640;sy=h/480
 def pts(p):return [(round(x*sx),round(y*sy)) for x,y in p]
 im=Image.new('P',(w,h));im.putpalette(sum((list(c) for c in pal),[]));d=ImageDraw.Draw(im)
 def line(p,c,wide=1):d.line(pts(p),fill=c,width=max(1,round(wide*sx)))
 def poly(p,c):d.polygon(pts(p),fill=c)
 def ellipse(box,c):d.ellipse([round(box[0]*sx),round(box[1]*sy),round(box[2]*sx),round(box[3]*sy)],fill=c)
 hz=230
 for y in range(h):
  yy=y/sy;c=1+int(62*min(1,yy/hz)) if yy<hz else 144+int(15*(yy-hz)/(480-hz))
  d.line((0,y,w-1,y),fill=c)
 # Two subdued planets and pinprick stars.
 ellipse((-45,-90,153,96),118);ellipse((500,-68,635,49),120)
 for k in range(3):line([(474,39+k),(555,19+k),(642,-13+k)],126+k)
 rng=random.Random(1987)
 for k in range(70):
  x=rng.randrange(640);y=rng.randrange(10,176)
  if (x-320)**2+(y-143)**2<116**2:continue
  c=128+rng.randrange(6);line([(x,y),(x+(k%13==0),y)],c)
  if k%19==0:line([(x,y-1),(x,y+1)],c)
 # Row-coloured sun, with growing lower cutout stripes.
 for y in range(max(0,round(35*sy)),round(231*sy)):
  yy=y/sy;dy=yy-145
  if abs(dy)>110:continue
  if any(a<=yy<b for a,b in [(172,174),(186,189),(201,205),(218,224)]):continue
  dx=math.sqrt(110**2-dy*dy);c=64+min(47,int((yy-35)/196*47))
  d.line((round((320-dx)*sx),y,round((320+dx)*sx),y),fill=c)
 # Filled low-poly mountains with original triangular ridge geometry.
 nodes=[(-12,224),(35,190),(72,208),(125,158),(169,201),(206,187),(266,222),(311,230)]
 poly(nodes+[(311,235),(-12,235)],113)
 for tri,c in [([(35,190),(66,230),(125,158)],116),([(125,158),(146,229),(169,201)],118),([(169,201),(194,230),(206,187)],115),([(206,187),(235,230),(266,222)],117)]:poly(tri,c)
 line(nodes,135);line([(35,190),(66,230),(125,158),(146,230),(169,201),(194,230),(206,187),(235,230)],130)
 nodes2=[(640,220),(598,178),(557,210),(517,180),(480,208),(445,193),(393,224),(327,230)]
 poly(nodes2+[(327,235),(640,235)],114);line(nodes2,135)
 line([(598,178),(578,230),(557,210),(540,230),(517,180),(501,230),(480,208),(462,230),(445,193),(426,230)],131)
 for y,c in [(229,137),(230,140),(231,143),(232,141),(233,137),(234,134)]:line([(0,y),(640,y)],c)
 # Road and fixed perspective grid rays. Moving cross-lines are live 3D.
 for x in [-1100,-730,-430,-190,0,145,240,400,495,640,830,1070,1370,1740]:line([(320,231),(x,480)],163,1)
 poly([(306,231),(334,231),(513,480),(127,480)],144)
 for off,c,th in [(0,164,7),(0,170,3),(0,175,1)]:
  line([(306,233),(127,480)],c,th);line([(334,233),(513,480)],c,th)
 # Taillamp road reflections: deliberately low luminance, no black erasing.
 for p,c in [([(248,416),(276,417),(263,480),(216,480)],220),([(259,419),(274,419),(255,480),(240,480)],221),([(364,416),(392,416),(424,480),(377,480)],220),([(367,419),(382,419),(398,480),(383,480)],221)]:poly(p,c)
 ellipse((216,402,424,461),192)
 # Rear three-quarter-inspired hatchback silhouette and gradient body.
 silhouette=[(227,422),(236,380),(266,337),(280,328),(360,328),(375,337),(404,380),(413,422),(396,446),(245,446)]
 line(silhouette+[silhouette[0]],206,7)
 mask=Image.new('1',(w,h));md=ImageDraw.Draw(mask);md.polygon(pts(silhouette),fill=1)
 for y in range(round(328*sy),round(447*sy)):
  yy=y/sy;c=194+int(13*max(0,min(1,(yy-328)/119)))
  if yy>390:c=194+int(10*(445-yy)/55)
  row=mask.crop((0,y,w,y+1)).tobytes()
  # Discrete body bands compress efficiently and read as lit metal.
  for x in range(round(220*sx),round(420*sx)):
   if mask.getpixel((x,y)):im.putpixel((x,y),c)
 d=ImageDraw.Draw(im)
 line(silhouette+[silhouette[0]],209,2)
 poly([(272,341),(368,341),(391,382),(249,382)],192)
 line([(271,341),(368,341),(391,383)],212,2)
 for yy in [345,352,360,369,379]:
  span=(yy-341)*.52;line([(272-span,yy),(368+span,yy)],210 if yy<360 else 212,3)
 line([(277,333),(362,333)],216,2)
 # Rear body, glass divider and bright taillights.
 poly([(242,390),(398,390),(403,424),(237,424)],196)
 line([(243,389),(397,389)],213,2);line([(238,424),(403,424)],209,2)
 for y in range(397,418):
  c=224+int((417-y)/20*15)
  line([(244,y),(291,y)],c);line([(349,y),(396,y)],c)
 for x in [249,258,267,276,285,354,363,372,381,390]:line([(x,398),(x,417)],227)
 poly([(302,401),(338,401),(340,419),(300,419)],192)
 poly([(307,406),(333,406),(333,415),(307,415)],210)
 line([(310,409),(329,409)],202);line([(310,412),(325,412)],202)
 poly([(231,415),(242,425),(246,443),(234,438)],192);poly([(398,425),(410,415),(405,438),(394,443)],192)
 poly([(242,429),(398,429),(390,444),(250,444)],201)
 line([(246,435),(394,435)],242,2);line([(252,444),(388,444)],209,1)
 # No text or logo anywhere in the running scene.
 return dither_gradients(im)

def encode(im):
 w,h=im.size;blob=bytearray();offs=[]
 for y in range(h):
  offs.append(len(blob));row=im.crop((0,y,w,y+1)).tobytes();x=0
  while x<w:
   # Bit 15 marks alternating-colour runs; low 15 bits are PIXEL count.
   # Store both colours and allow an odd last pixel. The DOS decoder uses
   # REP STOSW plus one optional STOSB, keeping ordered dithering compact.
   if x+2<w and row[x]!=row[x+1]:
    j=x+2
    while j<w and row[j]==row[x+((j-x)&1)]:j+=1
    if j-x>=3:
     n=(j-x)|0x8000;blob.extend((n&255,n>>8,row[x],row[x+1]));x=j;continue
   c=row[x];j=x+1
   while j<w and row[j]==c:j+=1
   n=j-x;blob.extend((n&255,n>>8,c));x=j
 assert len(blob)<65536,(im.size,len(blob))
 return blob,offs
out=['/* Original procedural indexed artwork. Regenerate with tools/make_art.py. */',
     '/* RLE: count LE16 + colour; bit15=count alternating pixels + two colours. */']
for w,h in [(320,200),(640,480),(800,600)]:
 im=make(w,h);im.save(ROOT/f'STATIC{w}.png');blob,offs=encode(im)
 print(w,'RLE bytes',len(blob),'colours',len(im.getcolors(256)))
 out.append(f'static const unsigned char __far bg{w}[{len(blob)}]={{')
 for i in range(0,len(blob),30):out.append(','.join(str(v) for v in blob[i:i+30])+',')
 out.append('};');out.append(f'static const unsigned __far bgrow{w}[{h}]={{')
 for i in range(0,len(offs),16):out.append(','.join(str(v) for v in offs[i:i+16])+',')
 out.append('};')
out.append('static const unsigned char palette[768]={')
v=[round(x*63/255) for c in pal for x in c]
for i in range(0,len(v),24):out.append(','.join(str(q) for q in v[i:i+24])+',')
out.append('};')
(ROOT/'ART.H').write_text('\n'.join(out)+'\n')

# A real sixteen-entry VGA palette; ordered colour mixing is precomputed.
neon16=[(3,3,12),(15,4,36),(50,7,64),(96,14,90),
 (159,22,119),(244,66,159),(255,131,138),(255,204,49),
 (251,249,132),(7,28,58),(20,74,119),(25,138,173),
 (58,230,249),(104,147,184),(215,130,228),(240,237,247)]
# Quantization targets actual six-bit DAC values. Pair mixing is used only
# when it improves squared colour error enough to justify the texture.
npal=[tuple(round(c*63/255) for c in rgb) for rgb in neon16]
quant=[[] for _ in range(4)]
for idx,rgb in enumerate(pal):
 c=tuple(round(v*63/255) for v in rgb)
 def err(v):return sum((a-b)**2 for a,b in zip(c,v))
 best=min(range(16),key=lambda j:err(npal[j]));score=err(npal[best]);choice=(best,best,0)
 # Keep wireframe lines and car details crisp; gradients may mix colours.
 if idx<112:
  for a in range(16):
   for b in range(a+1,16):
    for n in (1,2,3):
     mix=tuple((npal[a][k]*(4-n)+npal[b][k]*n)/4 for k in range(3))
     penalty=sum((npal[a][k]-npal[b][k])**2 for k in range(3))*.025
     e=err(mix)+penalty
     if e<score:score=e;choice=(a,b,n)
 a,b,n=choice
 for phase,threshold in enumerate((0,2,3,1)):quant[phase].append(b if threshold<n else a)
lines=['/* Original 16-colour palette and precomputed 2x2 ordered mapping. */',
 'static const unsigned char neon_palette16[48]={'+','.join(str(v) for c in npal for v in c)+'};',
 'static const unsigned char __far quant16[4][256]={']
for q in quant:lines.append('{'+','.join(map(str,q))+'},')
lines.append('};')
(ROOT/'COLOR16.H').write_text('\n'.join(lines)+'\n')
for w,h in [(320,200),(640,480),(800,600)]:
 im=make(w,h);q=Image.new('P',(w,h));q.putpalette(sum((list(c) for c in neon16),[])+[0]*720)
 src=im.load();dst=q.load()
 for y in range(h):
  for x in range(w):dst[x,y]=quant[(y&1)*2+(x&1)][src[x,y]]
 q.save(ROOT/f'STATIC{w}-16.png')
# Fixed frond vectors are independent of depth and need not be recomputed
# using sine/multiplies every rendered frame on the V20.
lines=['/* Eight palms, three distance LODs (4/6/8 fronds), dx/dz/px/pz. */',
       'static const int __far frond_shape[8][3][8][4]={']
def sint(a):return int(round(math.sin((a&255)*math.tau/256)*16384)/64)
for ident in range(8):
 lines.append('{')
 for count in (4,6,8):
  row=[]
  for j in range(8):
   a=(j*256//count+ident*11)&255
   row.append([int(101*sint(a+64)/256),int(67*sint(a)/256),int(11*sint(a)/256),int(9*sint(a+64)/256)] if j<count else [0]*4)
  lines.append('{'+','.join('{'+','.join(map(str,v))+'}' for v in row)+'},')
 lines.append('},')
lines.append('};');(ROOT/'FRONDS.H').write_text('\n'.join(lines)+'\n')

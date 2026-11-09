"""Compile Atlas role families from this checkout's original Emerald graphics.
No external art, runtime conversion, or map/story import. IDs preserve Atlas's
existing terrain and grove semantics. Native primary animation references stay live.
"""
from pathlib import Path
import hashlib,json,struct
from PIL import Image,ImageDraw
R=Path(__file__).resolve().parents[1]
P=R/'data/tilesets/primary/general'
OUT=R/'build/atlas-native';OUT.mkdir(exist_ok=True)
ASSETS=R/'assets/atlas-native';ASSETS.mkdir(exist_ok=True)
tiles=Image.open(P/'tiles.png')
native=struct.unpack('<4096H',(P/'metatiles.bin').read_bytes())
palettes=[]
inputs=[P/'tiles.png',P/'metatiles.bin',P/'metatile_attributes.bin']
for i in range(16):
 f=P/f'palettes/{i:02d}.pal';inputs.append(f)
 palettes.append([tuple(int(v)//8 for v in s.split()) for s in f.read_text().splitlines()[3:19]])
def rgb(c):return tuple(v*255//31 for v in c)
def pixels(im):return list(im.get_flattened_data() if hasattr(im,'get_flattened_data') else im.getdata())
original=[list(p) for p in palettes]
# Derived native palette variants. No primary palette is modified.
palettes[6]=[(min(31,19+(r+g+b)//8),min(31,22+(r+g+b)//9),min(31,25+(r+g+b)//12)) for r,g,b in original[2]]
for bank,style in [(7,'sand'),(8,'rock'),(9,'snow')]:
 result=[]
 for r,g,b in original[3]:
  tone=(r+g+b)//3
  result.append((min(31,9+tone),min(31,7+tone),min(31,4+tone*3//4)) if style=='sand'
   else (tone,tone,tone) if style=='rock' else (min(31,15+tone//2),min(31,18+tone//2),min(31,21+tone//2)))
 palettes[bank]=result
def canonical(mask):
 c=mask&15
 for i,(a,b) in enumerate(((0,1),(1,2),(2,3),(3,0))):
  if mask&(1<<(i+4)) and c&(1<<a) and c&(1<<b):c|=1<<(i+4)
 return c
masks=sorted({canonical(m) for m in range(256)});assert len(masks)==47
lookup=[masks.index(canonical(m)) for m in range(256)]
raw=[bytes(32)];dedup={raw[0]:512};definitions=[];attrs=[];roles=[]
def meta(n):return list(native[n*8:n*8+8])
def flip(words,h=False,v=False):
 return [words[(i//4)*4+((i%4)^int(bool(h))^int(bool(v))*2)]^(1024 if h else 0)^(2048 if v else 0) for i in range(8)]
def remap(words,changes):return [(w&4095)|(changes.get(w>>12,w>>12)<<12) for w in words]
def add(name,words,behavior=0,layer=1,source=None):
 assert len(words)==8
 definitions.extend(words);attrs.append((layer<<12)|behavior)
 roles.append(dict(id=511+len(attrs),role=name,source=source))
def pixel(word,x,y):
 n=word&1023;pal=word>>12
 if n<512:c=tiles.getpixel(((n%16)*8+(7-x if word&1024 else x),(n//16)*8+(7-y if word&2048 else y)))
 else:
  data=raw[n-512];xx=7-x if word&1024 else x;yy=7-y if word&2048 else y
  c=(data[yy*4+xx//2]>>(4*(xx%2)))&15
 return c,rgb(palettes[pal][c])
def render(words):
 im=Image.new('RGBA',(16,16))
 for i,w in enumerate(words):
  for y in range(8):
   for x in range(8):
    c,color=pixel(w,x,y)
    if not c:continue
    im.putpixel(((i%2)*8+x,((i%4)//2)*8+y),color+(255,))
 return im
# Each tuple is N/W, N/E, S/W, S/E plus its corresponding diagonal.
corners=[(0,3,3),(0,1,0),(2,3,2),(2,1,1)]
def kind(mask,a,b,d):
 return 4 if mask&(1<<a) and mask&(1<<b) and mask&(1<<(d+4)) else 3 if mask&(1<<a) and mask&(1<<b) else 2 if mask&(1<<a) else 1 if mask&(1<<b) else 0
def water(mask,sea,material):
 base=meta(0x170 if sea else 0xA1)[:4];top=[]
 for q,(a,b,d) in enumerate(corners):
  k=kind(mask,a,b,d)
  if k==4:top.append(0);continue
  # Original Emerald pond banks. South corners/edges reuse hardware flips;
  # water bottoms keep their original animated primary tile references.
  if k==0:n=0xB0 if q%2==0 else 0xB2;sq=q%2;vertical=q>=2
  elif k==3:n=0xC0 if q%2==0 else 0xC2;sq=q%2;vertical=q>=2
  elif k==1:n=0xB1;sq=q%2;vertical=q>=2
  else:n=0xB8 if q%2==0 else 0xBA;sq=q;vertical=False
  w=meta(n)[4+sq]^(2048 if vertical else 0)
  if material and w>>12==3:w=(w&4095)|([0,7,8,9][material]<<12)
  top.append(w)
 return base+top
for sea in range(2):
 for material in range(4):
  for mask in masks:add(f'{"ocean" if sea else "freshwater"}_{material}_{mask:02x}',water(mask,sea,material),0x15 if sea else 0x10,source='General pond/ocean 0xA1/0x170 and 0xB0..0xC2 bank quarters')
for ground,n,changes,behavior in [('grass',1,{},0),('sand',0x124,{},0x21),('snow',1,{2:6},0)]:
 for variant in range(4):add(f'{ground}_{variant}',remap(flip(meta(n),variant&1,variant&2),changes),behavior,source=n)
add('marsh_ground_cover',meta(0x15),behavior=2,source='General 0x15 long-grass art, retaining existing short-grass behavior')
for i in range(1,15):add(f'cliff_catalog_{i}',meta(0x70+i),source=0x70+i)
# Adapt one native two-column tree to Atlas's existing 3x3 collision footprint.
# Ground colors become transparent so the unscaled native ground remains below.
tree=Image.new('RGBA',(32,32))
for y,row in enumerate([[0x1D4,0x1D5],[0x1E4,0x1E5]]):
 for x,n in enumerate(row):tree.paste(render(meta(n)),(x*16,y*16))
ground={color[:3] for color in pixels(render(meta(1))) if color[3]}
tree.putdata([p if p[:3] not in ground else (0,0,0,0) for p in pixels(tree)])
tree=tree.resize((48,48),Image.Resampling.NEAREST)
def subtile(im,bank):
 indices=[]
 for color in pixels(im):
  indices.append(0 if color[3]==0 else min(range(1,16),key=lambda i:sum((color[k]-rgb(palettes[bank][i])[k])**2 for k in range(3))))
 packed=bytes(indices[i]|indices[i+1]<<4 for i in range(0,64,2))
 if packed not in dedup:dedup[packed]=512+len(raw);raw.append(packed)
 return dedup[packed]|bank<<12
for y in range(3):
 for x in range(3):
  top=[subtile(tree.crop((x*16+(q%2)*8,y*16+(q//2)*8,x*16+(q%2)*8+8,y*16+(q//2)*8+8)),2) for q in range(4)]
  add(f'tree_{x}_{y}',meta(1)[:4]+top,layer=2,source='General 0x1D4/1D5/1E4/1E5; footprint-preserving nearest scaling')
def mountain(mask,snow):
 words=[0]*8
 for q,(a,b,d) in enumerate(corners):
  k=kind(mask,a,b,d)
  n=1 if k==4 else [0x70,0x72,0x78,0x7A][q] if k==0 else [0x94,0x95,0x96,0x97][q] if k==3 else (0x71 if q<2 else 0x79) if k==1 else (0x78 if q%2==0 else 0x7A)
  src=remap(meta(n),{2:6,3:9} if snow else {})
  words[q]=src[q];words[q+4]=src[q+4]
 return words
for snow in range(2):
 for mask in masks:add(f'mountain_{snow}_{mask:02x}',mountain(mask,snow),source='General rock wall quarters 0x70..0x97')
# Reuse the original water-rock foreground as small walkable surface pebbles.
# Mask its blue water fringe, then reduce it; never copy the water behavior.
stone=render([0]*4+meta(0x13B)[4:])
stone.putdata([p if max(p[:3])-min(p[:3])<=40 else (0,0,0,0) for p in pixels(stone)])
stone=stone.resize((8,8),Image.Resampling.NEAREST)
for mirrored in range(2):
 overlay=Image.new('RGBA',(16,16));overlay.paste(stone.transpose(Image.Transpose.FLIP_LEFT_RIGHT) if mirrored else stone,(6 if mirrored else 2,4))
 top=[subtile(overlay.crop(((q%2)*8,(q//2)*8,(q%2)*8+8,(q//2)*8+8)),1) for q in range(4)]
 for snow in range(2):add(f'pass_{snow}_{mirrored}',remap(flip(meta(1),mirrored),{2:6} if snow else {})[:4]+top,source='General 0x13B rock foreground, reduced and normalized to walkable ground')
for mirrored in range(2):add(f'flowers_{mirrored}',flip(meta(4),mirrored),source=4)
assert len(attrs)==512 and len(raw)<=512
lines=['/* Generated by dev_scripts/compile_atlas_native.py from native Emerald assets. */','#include "global.h"','#include "atlas_tiles.h"']
def array(t,n,size,values):
 lines.append(f'const {t} {n}[{size}] = {{')
 for i in range(0,len(values),12):lines.append('    '+', '.join(f'0x{x:X}' for x in values[i:i+12])+',')
 lines.append('};')
packed=b''.join(raw);array('u32','gAtlasNatureTiles',4096,struct.unpack('<'+'I'*(len(packed)//4),packed))
lines.append('const u16 gAtlasNaturePalettes[16][16] = {')
for pal in palettes:lines.append('    {'+', '.join(f'0x{r|(g<<5)|(b<<10):04X}' for r,g,b in pal)+'},')
lines.append('};')
array('u16','gAtlasNatureMetatiles',4096,definitions);array('u16','gAtlasNatureAttributes',512,attrs);array('u8','gAtlasNatureMaskIndex',256,lookup)
(R/'src/atlas_tiles_data.c').write_text('\n'.join(lines)+'\n')
(OUT/'roles.json').write_text(json.dumps(roles,indent=2)+'\n')
families=[dict(first=512+i*47,count=47,role=f'water_{i//4}_{i%4}') for i in range(8)]
families += [dict(first=a,count=b,role=c) for a,b,c in [(888,4,'grass'),(892,4,'sand'),(896,4,'snow'),(900,1,'marsh_ground_cover'),(901,14,'cliff_catalog'),(915,9,'grove'),(924,47,'mountains'),(971,47,'snow_mountains'),(1018,4,'passes'),(1022,2,'flowers')]]
manifest=dict(source='Original Emerald assets in this Expansion checkout; not open-source artwork',inputs={str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},raw_tiles=len(raw),metatiles=len(attrs),families=families)
(ASSETS/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
sheet=Image.new('RGB',(16*40,32*44),'#303238');draw=ImageDraw.Draw(sheet)
for i in range(512):
 im=render(definitions[i*8:i*8+8]);x=i%16*40;y=i//16*44
 sheet.paste(im.resize((32,32),Image.Resampling.NEAREST),(x,y),im.resize((32,32),Image.Resampling.NEAREST));draw.text((x,y+32),f'{i+512:03X}',fill='white')
sheet.save(OUT/'native-role-catalog.png')
print(f'PASS native terrain catalogue: {len(raw)}/512 secondary graphic tiles, {len(attrs)}/512 metatiles; original inputs hashed')

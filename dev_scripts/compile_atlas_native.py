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
LAYER_COVERED=1 # global.fieldmap.h: bottom + middle backgrounds
LAYER_SPLIT=2   # bottom + top; foliage covers actors
def add(name,words,behavior=0,layer=LAYER_COVERED,source=None):
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
def sand_edge(mask):
 words=[0]*8
 for q,(a,b,d) in enumerate(corners):
  vertical=bool(mask&(1<<a));horizontal=bool(mask&(1<<b))
  n=0x124 if vertical and horizontal else ([0x118,0x11A,0x128,0x12A][q] if not vertical and not horizontal else (0x120 if q%2==0 else 0x122) if vertical else (0x119 if q<2 else 0x129))
  src=meta(n);words[q]=src[q];words[q+4]=src[q+4]
 return words
for sea in range(2):
 for material in range(4):
  for mask in masks:
   if sea and material==3 and mask==255:
    add('sand_ground_edge_e',sand_edge(14),behavior=0x21,source='General 0x118..0x12A grass/sand quarters; reclaimed duplicate full-water role')
   else:add(f'{"ocean" if sea else "freshwater"}_{material}_{mask:02x}',water(mask,sea,material),0x15 if sea else 0x10,source='General pond/ocean 0xA1/0x170 and 0xB0..0xC2 bank quarters')
for ground,n,changes,behavior in [('grass',1,{},0),('sand',0x124,{},0x21),('snow',1,{2:6},0)]:
 for variant in range(4):add(f'{ground}_{variant}',remap(flip(meta(n),variant&1,variant&2),changes),behavior,source=n)
add('marsh_ground_cover',meta(0x15),behavior=7,source='General 0x15 long-grass art with MB_SHORT_GRASS; normal running/cycling, no tall-grass encounters')
for mask in range(14):add(f'sand_ground_edge_{mask:01x}',sand_edge(mask),behavior=0x21,source='General 0x118..0x12A grass/sand quarters, cardinal topology')
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
  add(f'tree_{x}_{y}',meta(1)[:4]+top,layer=LAYER_SPLIT,source='General 0x1D4/1D5/1E4/1E5; footprint-preserving nearest scaling')
def mountain(mask,snow):
 words=[0]*8
 for q,(a,b,d) in enumerate(corners):
  k=kind(mask,a,b,d)
  if k==3:
   # A concave corner exposes one native rounded wall quadrant. The old
   # 0x94..0x97 selection mixed opaque rock bases and directional jump ledges.
   src=meta(1);corner=meta(0x6E if q%2==0 else 0x6F)[7 if q%2==0 else 6]
   src[q+4]=corner^1024^(2048 if q<2 else 0)
  else:
   n=1 if k==4 else [0x70,0x72,0x78,0x7A][q] if k==0 else (0x71 if q<2 else 0x79) if k==1 else (0x78 if q%2==0 else 0x7A)
   src=meta(n)
  src=remap(src,{2:6,3:9} if snow else {})
  words[q]=src[q];words[q+4]=src[q+4]
 return words
for snow in range(2):
 for mask in masks:add(f'mountain_{snow}_{mask:02x}',mountain(mask,snow),source='General ordinary wall quarters 0x6E/0x6F and 0x70..0x7A')
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
# Atlas alone uses a primary catalogue clone. Reclaim unused building roles,
# preserving native primary graphics, palettes and animation tile bindings.
# Secondary water/grove IDs remain intact; other engine layouts keep General.
C=R/'data/tilesets/secondary/cave'
cave=Image.open(C/'tiles.png')
cave_words=struct.unpack('<'+'H'*((C/'metatiles.bin').stat().st_size//2),(C/'metatiles.bin').read_bytes())
cave_palette=[tuple(int(v)//8 for v in line.split()) for line in (C/'palettes/06.pal').read_text().splitlines()[3:19]]
inputs += [C/'tiles.png',C/'metatiles.bin',C/'palettes/06.pal']
def cave_floor(n):
 im=Image.new('RGBA',(16,16))
 for q,w in enumerate(cave_words[(n-512)*8:(n-511)*8]):
  if not w:continue
  assert w>>12==6 and (w&1023)>=512
  tile=(w&1023)-512
  for y in range(8):
   for x in range(8):
    index=cave.getpixel(((tile%16)*8+(7-x if w&1024 else x),(tile//16)*8+(7-y if w&2048 else y)))
    if q<4 or index:im.putpixel(((q%2)*8+x,((q%4)//2)*8+y),(*rgb(cave_palette[index]),255))
 return im
rock_floor=[]
for mirrored in range(4):
 im=cave_floor(0x201)
 if mirrored&1:im=im.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
 if mirrored&2:im=im.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
 rock_floor.append([subtile(im.crop(((q%2)*8,(q//2)*8,(q%2)*8+8,(q//2)*8+8)),8) for q in range(4)]+[0]*4)
primary=list(native)
primary_attrs=list(struct.unpack('<512H',(P/'metatile_attributes.bin').read_bytes()))
# A small native leafy shrub supplies walkable forest understory. Reclaim only
# Atlas's spare primary roles; keep native subtiles, palettes and ground scale.
for variant in range(2):
 primary[(367+variant)*8:(368+variant)*8]=flip(meta(0x1EC),variant,False)
 primary_attrs[367+variant]=0x0000 # Native NORMAL layer: leaves cover the actor feet.
def material_mountain(mask,ground,wall_bank):
 words=mountain(mask,False)
 # Rock walls carry opaque face texture in BOTH native layers. Only the
 # palette-2 grass base is a replaceable floor; preserve palette-3 wall bases.
 for q in range(4):
  if words[q]>>12==2:words[q]=ground[q]
 return remap(words,{3:wall_bank})
for i,mask in enumerate(masks):
 words=material_mountain(mask,rock_floor[0],8)
 primary[(384+i)*8:(385+i)*8]=words;primary_attrs[384+i]=0x1000
for i,words in enumerate(rock_floor):
 primary[(431+i)*8:(432+i)*8]=words;primary_attrs[431+i]=0x1000
# Walkable sand ridge faces reuse General wall quarters and native sand ground.
# Their map collision remains clear; no original cliff behavior is imported.
for i,mask in enumerate(masks):
 words=material_mountain(mask,meta(0x124),7)
 primary[(435+i)*8:(436+i)*8]=words;primary_attrs[435+i]=0x1021
# Ground transitions preserve native rounded sand-edge geometry. Only the
# material colors change; fully interior quarters keep the actual floor art.
used={pixel(w,x,y)[0] for w in rock_floor[0][:4] for y in range(8) for x in range(8)}
grays=sorted({palettes[8][i] for i in used if i},key=sum)
palettes[10]=list(original[5]);palettes[11]=list(original[5])
for i,shade in enumerate([grays[-1],grays[2*len(grays)//3],grays[len(grays)//3],grays[0]]):palettes[10][11+i]=shade
for i in range(4):palettes[11][11+i]=palettes[6][12+i]
for snow,first in [(False,482),(True,497)]:
 full=remap(meta(1),{2:6}) if snow else rock_floor[0]
 for mask in range(15):
  words=remap(sand_edge(mask),{5:11 if snow else 10})
  for q,(a,b,d) in enumerate(corners):
   if mask&(1<<a) and mask&(1<<b):words[q]=full[q];words[q+4]=full[q+4]
  primary[(first+mask)*8:(first+mask+1)*8]=words;primary_attrs[first+mask]=0x1000
# Rock/sand edges share the same native silhouette. Palette 12 is unused by
# the other Atlas roles and is loaded as a normal secondary terrain palette.
# Outside index 15 becomes the native sand floor's dominant color; interiors
# retain the textured Cave rock quarters. Only unused Atlas building roles change.
palettes[12]=list(palettes[10]);palettes[12][15]=original[5][12]
for mask in range(15):
 words=remap(sand_edge(mask),{5:12})
 for q,(a,b,d) in enumerate(corners):
  if mask&(1<<a) and mask&(1<<b):words[q]=rock_floor[0][q];words[q+4]=rock_floor[0][q+4]
 primary[(352+mask)*8:(353+mask)*8]=words;primary_attrs[352+mask]=0x1000
# Snowline edges retain native rock interiors and native snow texture outside.
# Palette12 indices1..4 are unused by rock/sand silhouettes (11..15 only).
# Compile the rounded silhouette once; no pixel conversion runs on the GBA.
for i in range(4):palettes[12][1+i]=palettes[6][12+i]
snow_ground=render(remap(meta(1),{2:6}))
for i in range(4):palettes[12][5+i]=original[5][11+i]
for mask in range(15):
 im=render(remap(sand_edge(mask),{5:10}))
 green=rgb(original[5][15])
 for y in range(16):
  for x in range(16):
   if im.getpixel((x,y))[:3]==green:im.putpixel((x,y),snow_ground.getpixel((x,y)))
 words=[]
 for q,(a,b,d) in enumerate(corners):
  if mask&(1<<a) and mask&(1<<b):words.append(rock_floor[0][q])
  else:words.append(subtile(im.crop(((q%2)*8,(q//2)*8,(q%2)*8+8,(q//2)*8+8)),12))
 words += [0]*4
 primary[(337+mask)*8:(338+mask)*8]=words;primary_attrs[337+mask]=0x1000
# Cold beaches use the same native rounded boundary with textured sand outside.
sand_ground=render(meta(0x124));snow_full=remap(meta(1),{2:6})
for mask in range(15):
 im=render(remap(sand_edge(mask),{5:11}));green=rgb(original[5][15])
 for y in range(16):
  for x in range(16):
   if im.getpixel((x,y))[:3]==green:im.putpixel((x,y),sand_ground.getpixel((x,y)))
 words=[]
 for q,(a,b,d) in enumerate(corners):
  if mask&(1<<a) and mask&(1<<b):words.append(snow_full[q])
  else:words.append(subtile(im.crop(((q%2)*8,(q//2)*8,(q%2)*8+8,(q//2)*8+8)),12))
 words += [0]*4
 primary[(322+mask)*8:(323+mask)*8]=words;primary_attrs[322+mask]=0x1000
# A second native broadleaf crown shares the exact existing 3x3 footprint.
# Append graphics after the established 79-tile bank so earlier indices stay
# byte-identical. No new palette or secondary metatile slot is needed.
alternate=Image.new('RGBA',(32,32))
for y,row in enumerate([[0x1D6,0x1D7],[0x1E6,0x1E7]]):
 for x,n in enumerate(row):alternate.paste(render(meta(n)),(x*16,y*16))
alternate.putdata([p if p[:3] not in ground else (0,0,0,0) for p in pixels(alternate)])
alternate=alternate.resize((48,48),Image.Resampling.NEAREST)
for y in range(3):
 for x in range(3):
  top=[subtile(alternate.crop((x*16+(q%2)*8,y*16+(q//2)*8,x*16+(q%2)*8+8,y*16+(q//2)*8+8)),2) for q in range(4)]
  role=369+y*3+x
  primary[role*8:(role+1)*8]=meta(1)[:4]+top;primary_attrs[role]=0x2000
# Climate bloom variants use only audited spare entries 1..9 of terrain
# palettes 10/11; cliff/snow edges retain entries 11..15. Reindex the native
# flower's nine colors, keeping original pixel geometry and grass beneath.
# The shared glyph strip follows the original animation in the Atlas callback.
flower_indices=[2,3,4,9,11,12,13,14,15]
for bank,center,petal in [(10,(27,20,31),(21,8,25)),(11,(31,29,14),(27,19,3))]:
 for i,index in enumerate(flower_indices,1):palettes[bank][i]=center if index==9 else petal if index==11 else original[2][index]
 im=render(meta(4))
 im.putdata([(*rgb(center if p[:3]==rgb(original[2][9]) else petal),p[3]) if p[:3] in [rgb(original[2][9]),rgb(original[2][11])] else p for p in pixels(im)])
 top=[subtile(im.crop(((q%2)*8,(q//2)*8,(q%2)*8+8,(q//2)*8+8)),bank) for q in range(4)]
 for mirrored in range(2):
  role=378+(bank-10)*2+mirrored
  primary[role*8:(role+1)*8]=flip(meta(1)[:4]+top,mirrored);primary_attrs[role]=0x1000
# Two mutable strips share the frames; mirrored plants use the opposite phase.
assert len(raw)==108
assert [w&1023 for w in primary[378*8+4:379*8]]==list(range(616,620))
raw.extend(raw[104:108])
for role in [379,381]:
 for q in range(4,8):primary[role*8+q]+=4
for data in raw[104:112]:
 if dedup.get(data) in range(616,620):del dedup[data]
flower_frames=[]
for frame in range(3):
 path=P/f'anim/flower/{frame}.png';inputs.append(path);im=Image.open(path)
 assert im.size==(16,16) and set(pixels(im))<=set(flower_indices)
 mapping={index:i for i,index in enumerate(flower_indices,1)}
 packed=[]
 for q in range(4):
  indices=[mapping[im.getpixel(((q%2)*8+x,(q//2)*8+y))] for y in range(8) for x in range(8)]
  packed.extend(indices[i]|indices[i+1]<<4 for i in range(0,64,2))
 flower_frames.append(bytes(packed))
assert flower_frames[0]==b''.join(raw[104:108])
# Sparse surface fragments reuse the native rock silhouette with the existing
# derived snow palette. Water beneath retains its native animation/behavior.
ice=render([0]*4+meta(0x13B)[4:])
ice=ice.crop(ice.getbbox()).resize((10,7),Image.Resampling.NEAREST)
ice.putdata([(160+sum(p[:3])*90//765,190+sum(p[:3])*60//765,215+sum(p[:3])*40//765,p[3]) for p in pixels(ice)])
overlay=Image.new('RGBA',(16,16));overlay.paste(ice,(1,2));overlay.paste(ice.resize((4,4),Image.Resampling.NEAREST),(10,10))
for sea in range(2):
 im=overlay.transpose(Image.Transpose.FLIP_LEFT_RIGHT) if sea else overlay
 top=[subtile(im.crop(((q%2)*8,(q//2)*8,(q%2)*8+8,(q//2)*8+8)),6) for q in range(4)]
 role=382+sea;primary[role*8:(role+1)*8]=meta(0x170 if sea else 0xA1)[:4]+top
 primary_attrs[role]=0x1015 if sea else 0x1010
# Atlas-only spare primary roles 0x100..0x11E hold diagonal sand/grass
# notches. Existing cardinal words stay byte-identical. Use native pond inner
# corner alpha as the rounded silhouette, with native sand/grass colors.
notch_tiles=[]
for q in range(4):
 im=render(meta(0x124)).crop(((q%2)*8,(q//2)*8,(q%2)*8+8,(q//2)*8+8))
 w=meta(0xC0 if q%2==0 else 0xC2)[4+q%2]^(2048 if q>=2 else 0)
 for y in range(8):
  for x in range(8):
   if pixel(w,x,y)[0]:im.putpixel((x,y),(*rgb(original[5][15]),255))
 notch_tiles.append(subtile(im,5))
notch_masks=[m for m in masks if any(kind(m,a,b,d)==3 for a,b,d in corners)]
assert len(notch_masks)==31
notch_lookup=[notch_masks.index(canonical(m)) if canonical(m) in notch_masks else 255 for m in range(256)]
for i,m in enumerate(notch_masks):
 words=sand_edge(m)
 for q,(a,b,d) in enumerate(corners):
  if kind(m,a,b,d)==3:words[q]=notch_tiles[q];words[q+4]=0
 primary[(256+i)*8:(257+i)*8]=words;primary_attrs[256+i]=0x1021
# The same native inner-corner silhouette overlays full native rock/snow
# texture, sharing four transparent glyphs without quantizing either floor.
grass_notches=[]
for q in range(4):
 im=Image.new('RGBA',(8,8));w=meta(0xC0 if q%2==0 else 0xC2)[4+q%2]^(2048 if q>=2 else 0)
 for y in range(8):
  for x in range(8):
   if pixel(w,x,y)[0]:im.putpixel((x,y),(*rgb(original[5][15]),255))
 grass_notches.append(subtile(im,5))
for first,edge,full in [(192,482,rock_floor[0]),(224,497,remap(meta(1),{2:6}))]:
 for i,m in enumerate(notch_masks):
  cardinal=m&15;words=list(full) if cardinal==15 else primary[(edge+cardinal)*8:(edge+cardinal+1)*8]
  for q,(a,b,d) in enumerate(corners):
   if kind(m,a,b,d)==3:words[q+4]=grass_notches[q]
  primary[(first+i)*8:(first+i+1)*8]=words;primary_attrs[first+i]=0x1000
# Blocking lowland sand slopes reuse native sand ridge art in their own roles;
# dune ridge behavior/IDs stay walkable and independent.
for i in range(47):
 primary[(128+i)*8:(129+i)*8]=primary[(435+i)*8:(436+i)*8]
 primary_attrs[128+i]=0x1021
assert len(attrs)==512 and len(raw)<=512
lines=['/* Generated by dev_scripts/compile_atlas_native.py from native Emerald assets. */','#include "global.h"','#include "atlas_tiles.h"']
def array(t,n,size,values):
 lines.append(f'const {t} {n}[{size}] = {{')
 for i in range(0,len(values),12):lines.append('    '+', '.join(f'0x{x:X}' for x in values[i:i+12])+',')
 lines.append('};')
packed=b''.join(raw);array('u32','gAtlasNatureTiles',4096,struct.unpack('<'+'I'*(len(packed)//4),packed))
array('u32','gAtlasBloomAnimFrames',96,struct.unpack('<96I',b''.join(flower_frames)))
lines.append('const u16 gAtlasNaturePalettes[16][16] = {')
for pal in palettes:lines.append('    {'+', '.join(f'0x{r|(g<<5)|(b<<10):04X}' for r,g,b in pal)+'},')
lines.append('};')
array('u16','gAtlasPrimaryMetatiles',4096,primary);array('u16','gAtlasPrimaryAttributes',512,primary_attrs)
array('u16','gAtlasNatureMetatiles',4096,definitions);array('u16','gAtlasNatureAttributes',512,attrs);array('u8','gAtlasNatureMaskIndex',256,lookup);array('u8','gAtlasGroundNotchIndex',256,notch_lookup)
(R/'src/atlas_tiles_data.c').write_text('\n'.join(lines)+'\n')
(OUT/'roles.json').write_text(json.dumps(roles,indent=2)+'\n')
families=[dict(first=512+i*47,count=46 if i==7 else 47,role=f'water_{i//4}_{i%4}') for i in range(8)]
families += [dict(first=a,count=b,role=c) for a,b,c in [(887,1,'sand_ground_edge_north'),(888,4,'grass'),(892,4,'sand'),(896,4,'snow'),(900,1,'marsh_ground_cover'),(901,14,'sand_ground_edges'),(915,9,'grove'),(924,47,'mountains'),(971,47,'snow_mountains'),(1018,4,'passes'),(1022,2,'flowers')]]
families += [dict(first=384,count=47,role='primary_rock_cliffs'),dict(first=431,count=4,role='primary_rock_floor'),dict(first=435,count=47,role='primary_walkable_sand_ridges')]
families += [dict(first=128,count=47,role='primary_lowland_sand_cliffs',source='Native sand ridge quarters, separate blocking terrain roles')]
families += [dict(first=192,count=31,role='primary_rock_inner_edges'),dict(first=224,count=31,role='primary_snow_inner_edges')]
families += [dict(first=256,count=31,role='primary_sand_inner_edges',source='General sand quarters and 0xC0/0xC2 inner-corner silhouettes')]
families += [dict(first=322,count=15,role='primary_snow_sand_edges')]
families += [dict(first=337,count=15,role='primary_rock_snow_edges')]
families += [dict(first=352,count=15,role='primary_rock_sand_edges')]
families += [dict(first=367,count=2,role='primary_leafy_undergrowth',source='General 0x1EC land plant; hardware flips, native NORMAL foreground and walkable ground')]
families += [dict(first=369,count=9,role='primary_alternate_broadleaf',source='General 0x1D6/1D7/1E6/1E7; existing 3x3 footprint and canopy layer')]
families += [dict(first=378,count=4,role='primary_climate_blooms',source='General 0x004/flower animation; shared reindexed purple/gold glyph strip')]
families += [dict(first=382,count=2,role='primary_surface_ice',source='General 0x13B silhouette, derived snow palette and native lake/sea water')]
families += [dict(first=482,count=15,role='primary_rock_ground_edges'),dict(first=497,count=15,role='primary_snow_ground_edges')]
manifest=dict(source='Original Emerald assets in this Expansion checkout; not open-source artwork',inputs={str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},raw_tiles=len(raw),metatiles=len(attrs),families=families,animations=dict(blooms=dict(first_tile=616,tiles=8,frames=3,sequence=[0,1,0,2],phase_offsets=[0,2],period_frames=16)))
(ASSETS/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
sheet=Image.new('RGB',(16*40,32*44),'#303238');draw=ImageDraw.Draw(sheet)
for i in range(512):
 im=render(definitions[i*8:i*8+8]);x=i%16*40;y=i//16*44
 sheet.paste(im.resize((32,32),Image.Resampling.NEAREST),(x,y),im.resize((32,32),Image.Resampling.NEAREST));draw.text((x,y+32),f'{i+512:03X}',fill='white')
sheet.save(OUT/'native-role-catalog.png')
rock_sheet=Image.new('RGB',(16*40,4*44),'#303238');draw=ImageDraw.Draw(rock_sheet)
for i in range(51):
 im=render(primary[(384+i)*8:(385+i)*8]);x=i%16*40;y=i//16*44
 rock_sheet.paste(im.resize((32,32),Image.Resampling.NEAREST),(x,y));draw.text((x,y+32),f'{384+i:03X}',fill='white')
rock_sheet.save(OUT/'rock-role-catalog.png')
dune_sheet=Image.new('RGB',(16*40,3*44),'#303238');draw=ImageDraw.Draw(dune_sheet)
for i in range(47):
 im=render(primary[(435+i)*8:(436+i)*8]);x=i%16*40;y=i//16*44
 dune_sheet.paste(im.resize((32,32),Image.Resampling.NEAREST),(x,y));draw.text((x,y+32),f'{435+i:03X}',fill='white')
dune_sheet.save(OUT/'dune-role-catalog.png')
edge_sheet=Image.new('RGB',(16*40,2*44),'#303238');draw=ImageDraw.Draw(edge_sheet)
for i in range(30):
 im=render(primary[(482+i)*8:(483+i)*8]);x=i%15*40;y=i//15*44
 edge_sheet.paste(im.resize((32,32),Image.Resampling.NEAREST),(x,y));draw.text((x,y+32),f'{482+i:03X}',fill='white')
edge_sheet.save(OUT/'material-edge-catalog.png')
print(f'PASS native terrain catalogue: {len(raw)}/512 secondary graphic tiles, {len(attrs)}/512 metatiles; original inputs hashed')

pair_sheet=Image.new('RGB',(15*40,44),'#303238');draw=ImageDraw.Draw(pair_sheet)
for i in range(15):
 im=render(primary[(352+i)*8:(353+i)*8]);pair_sheet.paste(im.resize((32,32),Image.Resampling.NEAREST),(i*40,0));draw.text((i*40,32),f'{352+i:03X}',fill='white')
pair_sheet.save(OUT/'rock-sand-catalog.png')

snowline_sheet=Image.new('RGB',(15*40,44),'#303238');draw=ImageDraw.Draw(snowline_sheet)
for i in range(15):
 im=render(primary[(337+i)*8:(338+i)*8]);snowline_sheet.paste(im.resize((32,32),Image.Resampling.NEAREST),(i*40,0));draw.text((i*40,32),f'{337+i:03X}',fill='white')
snowline_sheet.save(OUT/'rock-snow-catalog.png')

cold_beach_sheet=Image.new('RGB',(15*40,44),'#303238');draw=ImageDraw.Draw(cold_beach_sheet)
for i in range(15):
 im=render(primary[(322+i)*8:(323+i)*8]);cold_beach_sheet.paste(im.resize((32,32),Image.Resampling.NEAREST),(i*40,0));draw.text((i*40,32),f'{322+i:03X}',fill='white')
cold_beach_sheet.save(OUT/'snow-sand-catalog.png')

notch_sheet=Image.new('RGB',(8*64,4*76),'#303238');draw=ImageDraw.Draw(notch_sheet)
for i,m in enumerate(notch_masks):
 im=render(primary[(256+i)*8:(257+i)*8]).resize((64,64),Image.Resampling.NEAREST);x=i%8*64;y=i//8*76
 notch_sheet.paste(im,(x,y));draw.text((x,y+64),f'{m:02X}',fill='white')
notch_sheet.save(OUT/'sand-inner-corners.png')

for first,name in [(192,'rock'),(224,'snow')]:
 sheet=Image.new('RGB',(8*64,4*76),'#303238');draw=ImageDraw.Draw(sheet)
 for i,m in enumerate(notch_masks):
  im=render(primary[(first+i)*8:(first+i+1)*8]).resize((64,64),Image.Resampling.NEAREST);x=i%8*64;y=i//8*76
  sheet.paste(im,(x,y));draw.text((x,y+64),f'{m:02X}',fill='white')
 sheet.save(OUT/f'{name}-inner-corners.png')

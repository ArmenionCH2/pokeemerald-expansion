"""Deterministic CC0 Puny World -> stock Emerald secondary tileset compiler.
No runtime pixel processing. Re-run after editing this selection or its source.
"""
from pathlib import Path
import hashlib,json,struct
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1];E=ROOT;A=ROOT/'assets/punyworld';OUT=ROOT/'build/atlas';OUT.mkdir(exist_ok=True)
SOURCE_HASH='14d285bde8dee96cb0113947c8cce4922b86bf1d033b19123112145c7cef3a38'
raw=(A/'source.png').read_bytes();assert hashlib.sha256(raw).hexdigest()==SOURCE_HASH
source=Image.open(A/'source.png').convert('RGBA')
def pixels(im):return list(im.get_flattened_data() if hasattr(im,"get_flattened_data") else im.getdata())
def gba(c):return tuple((v>>3) for v in c)
def rgb(c):return tuple(round(v*255/31) for v in c)
def tile(x,y):return source.crop((x*16,y*16,x*16+16,y*16+16))
def canonical(mask):
 c=mask&15
 for i,(a,b) in enumerate(((0,1),(1,2),(2,3),(3,0))):
  if mask&(1<<(i+4)) and c&(1<<a) and c&(1<<b):c|=1<<(i+4)
 return c
masks=sorted({canonical(m) for m in range(256)});assert len(masks)==47
lookup=[masks.index(canonical(m)) for m in range(256)]
# Source coordinates: 3x3 river bank plus four concave corners.
def blob(mask,cliff=False):
 im=Image.new('RGBA',(16,16))
 for q,(a,b,corner,outer,edgeA,edgeB,inner) in enumerate([
  (0,3,3,(7,10),(8,10),(7,11),(10,10)),
  (0,1,0,(9,10),(8,10),(9,11),(11,10)),
  (2,3,2,(7,12),(8,12),(7,11),(10,11)),
  (2,1,1,(9,12),(8,12),(9,11),(11,11))]):
  ca=bool(mask&(1<<a));cb=bool(mask&(1<<b));diag=bool(mask&(1<<(corner+4)))
  pos=(8,11) if ca and cb and diag else inner if ca and cb else edgeB if ca else edgeA if cb else outer
  if cliff:
   pos=(pos[0]-7,pos[1]-6)
   # South faces occupy the complete authored wall tile, rather than only
   # its bottom quarter. This preserves the source's directional perspective.
   if not mask&4:
    pos=(0 if q%2==0 and not mask&8 else 2 if q%2==1 and not mask&2 else 1,6)
  x=q%2*8;y=q//2*8;im.paste(tile(*pos).crop((x,y,x+8,y+8)),(x,y))
 return im
# Slots 1..15: three ground shades, three bank shades, six water shades,
# one extra bank shade, one ground shade, one intermediate water shade.
base=[(0,0,0),(115,197,164),(65,180,131),(164,213,197),(98,65,82),(131,90,90),(189,148,139),(4,160,180),(14,129,140),(10,144,160),(35,192,191),(19,219,218),(129,229,239),(156,115,115),(134,205,175),(21,174,185)]
palettes=[[gba(c) for c in base] for _ in range(16)]
for bank,colors in [(7,[(222,205,131),(213,180,106),(238,230,164),(230,213,147)]),(8,[(189,189,172),(156,156,139),(222,222,205),(205,205,189)]),(9,[(230,238,246),(189,213,230),(255,255,255),(213,230,246)])]:
 for i,c in zip((1,2,3,14),colors):palettes[bank][i]=gba(c)
# Categorize water/ground/bank source colors before nearest-color conversion.
def bank_index(p):
 r,g,b,a=p
 if not a:return 0
 if b>100 and g>100 and r<140:allowed=(7,8,9,10,11,12,15)
 elif p[:3] in ((133,166,67),(126,158,63),(159,183,71),(150,177,70)):allowed=(1,2,3,14)
 else:allowed=(4,5,6,13)
 if p[:3]==(133,166,67):return 1
 if p[:3]==(126,158,63):return 2
 if p[:3]==(159,183,71):return 3
 if p[:3]==(150,177,70):return 14
 return min(allowed,key=lambda i:sum((p[k]-base[i][k])**2 for k in range(3)))
# Palette 10 for imported cliff pieces, 11 for tree pieces. Quantize to fixed
# stock 4bpp limits; transformed greens match Emerald's grass backdrop.
def option_palette(coords,bank):
 ims=[]
 for xy in coords:
  im=tile(*xy)
  data=[]
  for p in pixels(im):
   if not p[3]:data.append(base[1])
   elif p[:3] in ((133,166,67),(126,158,63),(159,183,71),(150,177,70)):data.append(base[bank_index(p)])
   else:data.append(p[:3])
  out=Image.new('RGB',(16,16));out.putdata(data);ims.append(out)
 atlas=Image.new('RGB',(16*len(ims),16))
 for i,im in enumerate(ims):atlas.paste(im,(i*16,0))
 q=atlas.quantize(colors=15,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
 cs=q.getpalette();palettes[bank]=[(0,0,0)]+[gba(tuple(cs[i*3:i*3+3])) for i in range(15)]
 return ims
cliff_coords=[(x,y) for y in range(4,7) for x in range(5)]
tree_coords=[(x,y) for y in range(7,10) for x in range(3)]
cliffs=option_palette(cliff_coords,10);trees=option_palette(tree_coords,11)
def index_option(im,bank):
 im=im.convert('RGB');pals=[rgb(c) for c in palettes[bank]]
 return [min(range(1,16),key=lambda i:sum((p[k]-pals[i][k])**2 for k in range(3))) for p in pixels(im)]
raw_tiles=[bytes(32)];tile_map={bytes(32):512};definitions=[];attrs=[];options=[];previews=[]
def add(name,indices,bank,behavior=0,layer=1,blocked=False,src=None):
 entries=[]
 for q in range(4):
  px=q%2*8;py=q//2*8;v=[indices[(py+y)*16+px+x] for y in range(8) for x in range(8)]
  packed=bytes(v[i]|v[i+1]<<4 for i in range(0,64,2))
  if packed not in tile_map:tile_map[packed]=512+len(raw_tiles);raw_tiles.append(packed)
  entries.append(tile_map[packed]|bank<<12)
 if layer == 2:
  grass=struct.unpack('<8H',(E/'data/tilesets/primary/general/metatiles.bin').read_bytes()[16:32])
  definitions.extend(list(grass[:4])+entries)
 else:definitions.extend(entries+[512]*4)
 attrs.append(layer<<12|behavior)
 im=Image.new('RGB',(16,16));im.putdata([rgb(palettes[bank][i]) for i in indices]);previews.append(im)
 options.append(dict(id=512+len(attrs)-1,name=name,palette=bank,behavior=behavior,layer=layer,blocked=blocked,source=src))
for form in ('freshwater','ocean'):
 for material in range(4):
  for mask in masks:add(f'{form}_{("grass","sand","rock","snow")[material]}_{mask:02x}',[bank_index(p) for p in pixels(blob(mask))],6+material,0x10 if form=='freshwater' else 0x15,blocked=True,src='river bank quadrants x7..11/y10..12')
water_count=len(attrs)
for bank in (6,7,9):
 for x,y in ((0,0),(1,0),(2,0),(1,1)):
  add(f'{("grass" if bank==6 else "sand" if bank==7 else "snow")}_floor_{x}_{y}',[bank_index(p) for p in pixels(tile(x,y))],bank,0x21 if bank==7 else 0,src=[x,y])
cliff_start=512+len(attrs)
for xy,im in zip(cliff_coords,cliffs):add(f'cliff_option_{xy[0]}_{xy[1]}',index_option(im,10),10,blocked=True,src=list(xy))
tree_start=512+len(attrs)
for xy,im in zip(tree_coords,trees):
 indices=index_option(im,11)
 if xy[1]<9:
  indices=[v if p[3] else 0 for v,p in zip(indices,pixels(tile(*xy)))]
 add(f'tree_option_{xy[0]}_{xy[1]}',indices,11,layer=2 if xy[1]<9 else 1,blocked=True,src=list(xy))
# Same 47-shape topology, authored cliff quadrants (not rotated wall art).
mountain_start=512+len(attrs)
for snowy in (False,True):
 bank=12 if snowy else 10
 if snowy:
  palettes[12]=list(palettes[10])
  for i,(r,g,b) in enumerate(palettes[12]):
   if g>r and g>b:palettes[12][i]=gba((230,238,246)) if g>18 else gba((189,213,230))
 for mask in masks:
  im=blob(mask,True)
  # Apply the same source-to-native grass adaptation as the catalog pieces.
  data=[base[bank_index(p)] if p[:3] in ((133,166,67),(126,158,63),(159,183,71),(150,177,70)) else p[:3] for p in pixels(im)]
  transformed=Image.new('RGB',(16,16));transformed.putdata(data)
  add(f'mountain_{"snow" if snowy else "grass"}_{mask:02x}',index_option(transformed,10),bank,blocked=mask!=255,src='cliff quadrants x0..4/y4..6')
# Sparse surface stones mark existing natural connector cuts. Composite the
# source's small stone cluster over its flat grass; retain the cliff palettes
# so grass/snow meets the adjacent rim without a new palette or rotation.
pass_start=512+len(attrs)
for mirrored in (False,True):
 im=Image.new('RGBA',(16,16),tuple(base[1])+(255,))
 stones=tile(1,26)
 if mirrored:stones=stones.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
 im.alpha_composite(stones)
 indices=index_option(im,10)
 for snowy in (False,True):
  add(f'pass_{"snow" if snowy else "grass"}_{int(mirrored)}',indices,12 if snowy else 10,
      src='small stones x1/y26 over flat grass x0/y0; snow uses palette 12')
assert len(raw_tiles)<=512 and len(attrs)<=512
# All raw pixels/metatiles fit stock Emerald. Full declared arrays prevent an
# uncompressed 512-tile VRAM upload reading past the object.
lines=['/* Generated by implementation/tools/compile_atlas_nature.py; Puny World by Shade, CC0. */','#include "global.h"','#include "atlas_tiles.h"']
def array(typ,name,size,values):
 lines.append(f'const {typ} {name}[{size}] = {{')
 for i in range(0,len(values),12):lines.append('    '+', '.join(f'0x{v:X}' for v in values[i:i+12])+',')
 lines.append('};')
packed=b''.join(raw_tiles);array('u32','gAtlasNatureTiles','4096',struct.unpack('<'+'I'*(len(packed)//4),packed))
lines.append('const u16 gAtlasNaturePalettes[16][16] = {')
for pal in palettes:lines.append('    {'+', '.join(f'0x{r|(g<<5)|(b<<10):04X}' for r,g,b in pal)+'},')
lines.append('};')
array('u16','gAtlasNatureMetatiles','4096',definitions);array('u16','gAtlasNatureAttributes','512',attrs);array('u8','gAtlasNatureMaskIndex','256',lookup)
(E/'src/atlas_tiles_data.c').write_text('\n'.join(lines)+'\n')
manifest=dict(author='Shade',license='CC0-1.0',source_url='https://opengameart.org/content/16x16-puny-world-tileset',source_sha256=SOURCE_HASH,raw_tiles=len(raw_tiles),metatiles=len(attrs),palette_slots=list(range(6,13)),water_count=water_count,mountain_start=mountain_start,cliff_start=cliff_start,tree_start=tree_start,pass_start=pass_start,pass_count=4,water_families=[dict(form=f,material=m,first=512+fi*188+mi*47,count=47) for fi,f in enumerate(("freshwater","ocean")) for mi,m in enumerate(("grass","sand","rock","snow"))],options=options[water_count:])
(A/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
header=f'''#ifndef GUARD_ATLAS_TILES_H
#define GUARD_ATLAS_TILES_H
#define ATLAS_NATURE_WATER_FIRST 0x200
#define ATLAS_NATURE_WATER_COUNT {water_count}
#define ATLAS_NATURE_GRASS_FIRST {512+water_count}
#define ATLAS_NATURE_SAND_FIRST {516+water_count}
#define ATLAS_NATURE_SNOW_FIRST {520+water_count}
#define ATLAS_NATURE_CLIFF_FIRST {cliff_start}
#define ATLAS_NATURE_TREE_FIRST {tree_start}
#define ATLAS_NATURE_OPTION_COUNT {len(attrs)}
#define ATLAS_NATURE_MOUNTAIN_FIRST {mountain_start}
#define ATLAS_NATURE_PASS_FIRST {pass_start}
#define ATLAS_NATURE_PASS_COUNT 4
struct AtlasCell;
extern const u32 gAtlasNatureTiles[4096];
extern const u16 gAtlasNaturePalettes[16][16];
extern const u16 gAtlasNatureMetatiles[4096], gAtlasNatureAttributes[512];
extern const u8 gAtlasNatureMaskIndex[256];
u16 AtlasTiles_Water(u8 mask, u8 material, u8 waterform);
u16 AtlasTiles_At(s32 x, s32 y, const struct AtlasCell *center);
u16 AtlasTerrain_BaseTile(s32 x, s32 y, const struct AtlasCell *cell, bool8 clearCover);
void AtlasTiles_Reset(void);
void AtlasTiles_FillRow(u16 *dest, s32 x, s32 y, u16 width);
#endif
'''
(E/'include/atlas_tiles.h').write_text(header)
contact=Image.new('RGB',(24*48,((len(previews)+23)//24)*62),(245,245,245));d=ImageDraw.Draw(contact)
for i,im in enumerate(previews):
 x=i%24*48;y=i//24*62;contact.paste(im.resize((48,48),Image.Resampling.NEAREST),(x,y));d.text((x,y+48),f'{i+512:03X}',fill='black')
contact.save(OUT/'AtlasNature-options.png')
print('PASS stock secondary capacity:',len(raw_tiles),'/512 raw tiles;',len(attrs),'/512 metatiles; palette slots 6..12')

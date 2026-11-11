"""Validate native provenance, hardware bounds, animation bindings and regeneration.
This checks asset data, not emulator frame performance or visual quality.
"""
from pathlib import Path
import hashlib,json,re,shutil,struct,subprocess,sys,tempfile
from PIL import Image
R=Path(__file__).resolve().parents[2]
manifest=json.loads((R/'assets/atlas-native/manifest.json').read_text())
assert not (R/'assets/punyworld').exists()
assert not (R/'dev_scripts/compile_atlas_nature.py').exists()
for path,digest in manifest['inputs'].items():
    assert path.startswith(('data/tilesets/primary/general/','data/tilesets/secondary/cave/'))
    assert hashlib.sha256((R/path).read_bytes()).hexdigest()==digest
source=(R/'src/atlas_tiles_data.c').read_text()
def array(name):
    text=re.search(r'\b'+name+r'\[[^;=]+\]\s*=\s*\{(.*?)\};',source,re.S)[1]
    return [int(v,16) for v in re.findall(r'0x[0-9a-fA-F]+',text)]
metatiles=array('gAtlasNatureMetatiles');attrs=array('gAtlasNatureAttributes')
assert len(metatiles)==4096 and len(attrs)==512 and manifest['metatiles']==512
assert all((w&1023)<512+manifest['raw_tiles'] and (w>>12)<13 for w in metatiles)
primary=array('gAtlasPrimaryMetatiles');primary_attrs=array('gAtlasPrimaryAttributes')
assert len(primary)==4096 and len(primary_attrs)==512
assert all((w&1023)<512+manifest['raw_tiles'] and (w>>12)<13 for w in primary)
native=struct.unpack('<4096H',(R/'data/tilesets/primary/general/metatiles.bin').read_bytes())
native_attrs=struct.unpack('<512H',(R/'data/tilesets/primary/general/metatile_attributes.bin').read_bytes())
for role in range(512):
    if 128<=role<175 or 192<=role<223 or 224<=role<255 or 256<=role<287 or 322<=role<512:continue
    assert primary[role*8:(role+1)*8]==list(native[role*8:(role+1)*8])
    assert primary_attrs[role]==native_attrs[role]
assert all(primary_attrs[i]==0x1000 for i in range(322,367))
assert all(primary_attrs[i]==0x1021 for i in range(256,287))
for i in range(47):
    assert primary[(128+i)*8:(129+i)*8]==primary[(435+i)*8:(436+i)*8]
    assert primary_attrs[128+i]==0x1021
assert all(primary_attrs[i]==0x1000 for first in [192,224] for i in range(first,first+31))
for variant in range(2):
    words=list(native[0x1EC*8:0x1ED*8]);h=variant;v=0
    expected=[words[(i//4)*4+((i%4)^h^(v*2))]^(1024 if h else 0)^(2048 if v else 0) for i in range(8)]
    assert primary[(367+variant)*8:(368+variant)*8]==expected
    assert primary_attrs[367+variant]==native_attrs[0x1EC]==0x0000
assert all(primary_attrs[i]==0x2000 for i in range(369,378))
assert all(primary_attrs[i]==0x1000 for i in range(378,382))
for role,original,behavior in [(382,0xA1,0x1010),(383,0x170,0x1015)]:
    words=primary[role*8:(role+1)*8]
    assert words[:4]==list(native[original*8:original*8+4])
    assert primary_attrs[role]==behavior
    assert all(w>>12==6 and ((w&1023)==512 or (w&1023)>=624) for w in words[4:])
    assert any((w&1023)>=624 for w in words[4:])
assert all(primary[i*8:i*8+4]==list(native[8:12]) for i in range(369,378))
assert any((w&1023)>=512+79 for w in primary[369*8:378*8])
# Preserve opaque native wall faces on both layers. Replacing every bottom
# quadrant with floor art used to erase these faces from rock/sand variants.
preserved_wall_bases=0
for i in range(47):
    original_wall=metatiles[(924-512+i)*8:(925-512+i)*8]
    for first,bank,floor_words in [(384,8,primary[431*8:432*8]),(435,7,list(native[0x124*8:0x125*8]))]:
        words=primary[(first+i)*8:(first+i+1)*8]
        for q,w in enumerate(original_wall):
            expected=floor_words[q] if q<4 and w>>12==2 else (w&4095)|((bank if w>>12==3 else w>>12)<<12)
            assert words[q]==expected
            if q<4 and w>>12==3:preserved_wall_bases+=1
assert preserved_wall_bases>0

# The land and underwater plant share leaves, not their ground backgrounds.
assert primary[367*8+4:368*8]==list(native[0x193*8+4:0x194*8])
assert all(w>>12==2 for w in primary[367*8:367*8+4])
# Ordinary concave wall quadrants, never the 0x95..0x97 jump-ledge variants.
canonical_masks=sorted({(m&15)|sum((1<<(i+4)) if m&(1<<(i+4)) and m&(1<<a) and m&(1<<b) else 0 for i,(a,b) in enumerate([(0,1),(1,2),(2,3),(3,0)])) for m in range(256)})
for index,mask in enumerate(canonical_masks):
    for q,(a,b,d) in enumerate([(0,3,3),(0,1,0),(2,3,2),(2,1,1)]):
        if not(mask&(1<<a) and mask&(1<<b)) or mask&(1<<(d+4)):continue
        corner_source=(0x6E if q%2==0 else 0x6F)*8+(7 if q%2==0 else 6)
        expected=native[corner_source]^1024^(2048 if q<2 else 0)
        words=metatiles[(924-512+index)*8:(925-512+index)*8]
        assert words[q]==native[8+q] and words[q+4]==expected
        snow=metatiles[(971-512+index)*8:(972-512+index)*8]
        assert snow[q]==(native[8+q]&4095)|6<<12
        assert snow[q+4]==(expected&4095)|9<<12

# The only newly used palette belongs to rock/sand edges. Existing terrain and
# native animation references retain all original palette words.
assert not any(w>>12==12 for w in metatiles)
assert not any(w>>12==12 for i,w in enumerate(primary) if not 322*8<=i<367*8)
pals=array('gAtlasNaturePalettes')
# Spare low entries must remain unreferenced by all pre-existing roles. New
# blossoms change only the native petal/center colors; grass and leaves match.
native_tiles=Image.open(R/'data/tilesets/primary/general/tiles.png')
raw=b''.join(struct.pack('<I',n) for n in array('gAtlasNatureTiles'))
assert raw[:32]==bytes(32) # transparent quadrant shared by sparse overlays
frames=b''.join(struct.pack('<I',n) for n in array('gAtlasBloomAnimFrames'))
assert len(frames)==384 and frames[:128]==raw[104*32:108*32]==raw[108*32:112*32]
lookup={index:i for i,index in enumerate([2,3,4,9,11,12,13,14,15],1)}
for frame in range(3):
    im=Image.open(R/f'data/tilesets/primary/general/anim/flower/{frame}.png');expected=bytearray()
    for q in range(4):
        values=[lookup[im.getpixel(((q%2)*8+x,(q//2)*8+y))] for y in range(8) for x in range(8)]
        expected.extend(values[i]|values[i+1]<<4 for i in range(0,64,2))
    assert frames[frame*128:(frame+1)*128]==bytes(expected)
assert len({frames[i*128:(i+1)*128] for i in range(3)})==3
for role in range(512):
    if 378<=role<382:continue
    assert all(not 616<=(w&1023)<624 for w in primary[role*8:(role+1)*8])
assert all(not 616<=(w&1023)<624 for w in metatiles)
headers=(R/'src/data/tilesets/headers.h').read_text()
assert '.callback = InitTilesetAnim_AtlasGeneral' in re.search(r'gTileset_AtlasPrimary\s*=\s*\{(.*?)\};',headers,re.S)[1]

def index_at(w,x,y):
    tile=w&1023;x=7-x if w&1024 else x;y=7-y if w&2048 else y
    if tile<512:return native_tiles.getpixel(((tile%16)*8+x,(tile//16)*8+y))
    value=raw[(tile-512)*32+y*4+x//2];return (value>>(4*(x%2)))&15
for role in range(512):
    if 378<=role<382:continue
    for w in primary[role*8:(role+1)*8]:
        if w>>12 in [10,11]:assert all(index_at(w,x,y)==0 or index_at(w,x,y)>=11 for y in range(8) for x in range(8))
for role in [382,383]:
    words=primary[role*8+4:(role+1)*8]
    colors=[index_at(w,x,y) for w in words for y in range(8) for x in range(8)]
    assert 50<=sum(bool(i) for i in colors)<=100 # solid fragments, not a hollow outline
    assert len(set(colors)-{0})>=2

def visible_color(words,x,y):
    q=(y//8)*2+x//8;color=0
    for w in [words[q],words[q+4]]:
        i=index_at(w,x%8,y%8)
        if i:color=pals[(w>>12)*16+i]
    return color
for w in metatiles:
    if w>>12 in [10,11]:assert all(index_at(w,x,y)==0 or index_at(w,x,y)>=11 for y in range(8) for x in range(8))
for role in range(378,382):
    center,petal=((27,20,31),(21,8,25)) if role<380 else ((31,29,14),(27,19,3))
    center=center[0]|center[1]<<5|center[2]<<10;petal=petal[0]|petal[1]<<5|petal[2]<<10
    for y in range(16):
        for x in range(16):
            old=visible_color(native[4*8:5*8],15-x if role&1 else x,y)
            expected=center if old==pals[2*16+9] else petal if old==pals[2*16+11] else old
            assert visible_color(primary[role*8:(role+1)*8],x,y)==expected

# Every corner role must be reachable and change only its intended quadrant.
notch_lookup=array('gAtlasGroundNotchIndex')
assert len(notch_lookup)==256 and set(notch_lookup)==set(range(31))|{255}
for mask,index in enumerate(notch_lookup):
    missing=[bool(mask&(1<<q) and mask&(1<<((q+1)%4)) and not mask&(1<<(q+4))) for q in range(4)]
    assert (index!=255)==any(missing)
    if index==255:continue
    words=primary[(256+index)*8:(257+index)*8];cardinal=mask&15
    oldrole=892 if cardinal==15 else 887 if cardinal==14 else 901+cardinal
    oldwords=metatiles[(oldrole-512)*8:(oldrole-511)*8]
    # Runtime bit order NE,SE,SW,NW differs from image NW,NE,SW,SE.
    for q,diagonal in enumerate([3,0,2,1]):
        if not missing[diagonal]:
            assert [words[q],words[q+4]]==[oldwords[q],oldwords[q+4]]
            continue
        inner=native[(0xC0 if q%2==0 else 0xC2)*8+4+q%2]^(2048 if q>=2 else 0)
        for y in range(8):
            for x in range(8):
                xx=(q%2)*8+x;yy=(q//2)*8+y
                expected=pals[5*16+15] if index_at(inner,x,y) else visible_color(native[0x124*8:0x125*8],xx,yy)
                assert visible_color(words,xx,yy)==expected

# Rock/snow retain exact native interior pixels; only the grass mask overlays.
for first,edge,floorrole in [(192,482,431),(224,497,896)]:
    for mask,index in enumerate(notch_lookup):
        if index==255:continue
        cardinal=mask&15;oldrole=floorrole if cardinal==15 else edge+cardinal
        oldwords=metatiles[(oldrole-512)*8:(oldrole-511)*8] if oldrole>=512 else primary[oldrole*8:(oldrole+1)*8];words=primary[(first+index)*8:(first+index+1)*8]
        assert words[:4]==oldwords[:4]
        for q,(a,b,diagonal) in enumerate([(0,3,3),(0,1,0),(2,3,2),(2,1,1)]):
            missing=mask&(1<<a) and mask&(1<<b) and not mask&(1<<(diagonal+4))
            if not missing:assert words[q+4]==oldwords[q+4]
            inner=native[(0xC0 if q%2==0 else 0xC2)*8+4+q%2]^(2048 if q>=2 else 0)
            for y in range(8):
                for x in range(8):
                    xx=(q%2)*8+x;yy=(q//2)*8+y
                    expected=pals[5*16+15] if missing and index_at(inner,x,y) else visible_color(oldwords,xx,yy)
                    assert visible_color(words,xx,yy)==expected

assert pals[12*16+1:12*16+5]==pals[6*16+12:6*16+16]
assert pals[12*16+5:12*16+9]==pals[5*16+11:5*16+15]
assert pals[12*16+15]==pals[5*16+12] # dominant native sand floor outside
assert pals[12*16+11:12*16+15]==pals[10*16+11:10*16+15] # rock inside
for mask in range(15):
    assert any(w>>12==12 for w in primary[(352+mask)*8:(353+mask)*8])
assert all(primary_attrs[i]==0x1000 for i in range(384,435))
assert all(primary_attrs[i]==0x1021 for i in range(435,482))
assert all(primary_attrs[i]==0x1000 for i in range(482,512))
assert (R/'data/layouts/layouts.json').read_text().count('gTileset_AtlasPrimary')==1
for role,original in [(888,1),(892,0x124),(558,0xA1),(746,0x170),(1022,4),(900,0x15)]:
    assert metatiles[(role-512)*8:(role-511)*8]==list(native[original*8:(original+1)*8])
assert attrs[900-512]&255==7
for role in [887,*range(901,915)]:
    assert attrs[role-512]==attrs[892-512]==0x1021
assert any((w&1023)!=0 for w in metatiles[(887-512)*8:(888-512)*8])
assert (R/'data/layouts/layouts.json').read_text().count('gTileset_AtlasNative')==1
assert 'gTileset_AtlasNature' not in (R/'src/data/tilesets/headers.h').read_text()
# Run the compiler in an isolated source fixture, never rewriting a live build.
with tempfile.TemporaryDirectory(prefix='atlas-native-art-') as directory:
    stage=Path(directory)
    for d in ['dev_scripts','src','assets','build']:(stage/d).mkdir()
    for path in manifest['inputs']:
        dest=stage/path;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(R/path,dest)
    shutil.copy2(R/'dev_scripts/compile_atlas_native.py',stage/'dev_scripts/compile_atlas_native.py')
    subprocess.run([sys.executable,str(stage/'dev_scripts/compile_atlas_native.py')],check=True,stdout=subprocess.PIPE)
    for path in ['src/atlas_tiles_data.c','assets/atlas-native/manifest.json']:
        assert (stage/path).read_bytes()==(R/path).read_bytes(),path
print('PASS native asset provenance, primary references, animated water/flowers, 4bpp bounds and byte-exact isolated regeneration')

"""Validate native provenance, hardware bounds, animation bindings and regeneration.
This checks asset data, not emulator frame performance or visual quality.
"""
from pathlib import Path
import hashlib,json,re,shutil,struct,subprocess,sys,tempfile
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
    if 384<=role<435:continue
    assert primary[role*8:(role+1)*8]==list(native[role*8:(role+1)*8])
    assert primary_attrs[role]==native_attrs[role]
assert all(primary_attrs[i]==0x1000 for i in range(384,435))
assert (R/'data/layouts/layouts.json').read_text().count('gTileset_AtlasPrimary')==1
for role,original in [(888,1),(892,0x124),(558,0xA1),(746,0x170),(1022,4),(900,0x15)]:
    assert metatiles[(role-512)*8:(role-511)*8]==list(native[original*8:(original+1)*8])
assert attrs[900-512]&255==2
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

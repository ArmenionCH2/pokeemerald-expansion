"""Compile the live animation callbacks with a bounded recording DMA adapter."""
from pathlib import Path
import re,subprocess,tempfile,os
R=Path(__file__).resolve().parents[2]
source=(R/'src/tileset_anims.c').read_text()
def function(name):
    match=re.search(r'\b'+name+r'\s*\([^;{}]*\)\s*\{',source);assert match,name
    start=source.rfind('\n',0,match.start())+1;at=source.index('{',match.start());depth=1;end=at+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
harness=r'''
#include "global.h"
#include "atlas_tiles.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define BG_VRAM ((uintptr_t)0x06000000u)
#define TILE_OFFSET_4BPP(n) ((n)*32u)
#define TILE_SIZE_4BPP 32
static u16 sPrimaryTilesetAnimCounter,sPrimaryTilesetAnimCounterMax;
static void (*sPrimaryTilesetAnimCallback)(u16);
struct Event {unsigned tag,timer,size;const void *src,*dest;};
static struct Event events[20];static unsigned count;
static void Native(unsigned tag,u16 timer){assert(count<20);events[count++]=(struct Event){.tag=tag,.timer=timer};}
static void QueueAnimTiles_General_Flower(u16 t){Native(0,t);}
static void QueueAnimTiles_General_Water(u16 t){Native(1,t);}
static void QueueAnimTiles_General_SandWaterEdge(u16 t){Native(2,t);}
static void QueueAnimTiles_General_Waterfall(u16 t){Native(3,t);}
static void QueueAnimTiles_General_LandWaterEdge(u16 t){Native(4,t);}
static void AppendTilesetAnimToBuffer(const u16 *src,u16 *dest,u16 size){assert(count<20);events[count++]=(struct Event){.tag=9,.src=src,.dest=dest,.size=size};}
static void TilesetAnim_General(u16);
'''
harness+='\n'.join(function(n) for n in ['InitTilesetAnim_General','TilesetAnim_General','TilesetAnim_AtlasGeneral','InitTilesetAnim_AtlasGeneral'])
harness+=r'''
int main(void){
static const unsigned sequence[]={0,1,0,2};
for(unsigned timer=0;timer<256;timer++){
 struct Event baseline[20];count=0;TilesetAnim_General(timer);unsigned old=count;memcpy(baseline,events,sizeof(events));
 count=0;TilesetAnim_AtlasGeneral(timer);assert(count==old+(timer%16==0?2:0));
 for(unsigned i=0;i<old;i++)assert(events[i].tag==baseline[i].tag && events[i].timer==baseline[i].timer);
 if(timer%16==0)for(unsigned strip=0;strip<2;strip++){
  struct Event *e=&events[old+strip];unsigned frame=sequence[(timer/16+strip*2)%4];
  assert(e->tag==9 && e->src==gAtlasBloomAnimFrames+frame*32 && e->size==128);
  assert((uintptr_t)e->dest==BG_VRAM+(ATLAS_BLOOM_TILE_FIRST+strip*4)*32u);
 }
}
sPrimaryTilesetAnimCounter=123;InitTilesetAnim_AtlasGeneral();assert(!sPrimaryTilesetAnimCounter && sPrimaryTilesetAnimCounterMax==256 && sPrimaryTilesetAnimCallback==TilesetAnim_AtlasGeneral);
InitTilesetAnim_General();assert(!sPrimaryTilesetAnimCounter && sPrimaryTilesetAnimCounterMax==256 && sPrimaryTilesetAnimCallback==TilesetAnim_General);
count=0;sPrimaryTilesetAnimCallback(0);assert(count==1 && events[0].tag==0);
puts("PASS live bloom scheduler: full 256-frame native schedule preserved, two bounded 128-byte transfers, opposite phases and native callback restoration");
}
'''
with tempfile.TemporaryDirectory(prefix='bloom-schedule-',dir=R/'build') as tmp:
    test=Path(tmp)/'test.c';test.write_text(harness);exe=Path(tmp)/'test'
    subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-Itests/atlas/mocks','-Iinclude',str(test),'src/atlas_tiles_data.c','-o',str(exe)],cwd=R,check=True)
    subprocess.run([str(exe)],cwd=R,check=True)

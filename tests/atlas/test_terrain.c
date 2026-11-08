#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static u16 a[1192],b[1190],c[1190];
int main(void) {
 s32 x,y,dx,dy,i,spawnX,spawnY;u32 biomes[9]={0};struct AtlasCell p,q;
 assert(AtlasPlan_Begin(0x41544C33));
 while(!AtlasPlan_Step(128));
 assert(gAtlasPlanReady && gAtlasPlanStats.land && gAtlasPlanStats.continents);
 assert(AtlasTerrain_FloorDiv(-1,20)==-1);
 for(y=-ATLAS_WORLD_HALF_HEIGHT+12345;y<ATLAS_WORLD_HALF_HEIGHT;y+=167413)
 for(x=0;x<ATLAS_WORLD_WIDTH;x+=197333) {
  AtlasTerrain_Sample(x,y,&p);AtlasTerrain_Sample(x+ATLAS_WORLD_WIDTH,y,&q);
  assert(!memcmp(&p,&q,sizeof p));biomes[p.biome]++;
  AtlasTerrain_Sample(123,456,&q);AtlasTerrain_Sample(x,y,&q);
  assert(!memcmp(&p,&q,sizeof p)); /* lattice cache never defines the world */
 }
 for(i=0;i<1200;i++) {
  struct AtlasPlanSample warm,cold;
  s32 px=(u32)i*33187%ATLAS_WORLD_WIDTH, py=(s32)((u32)i*7331%(2*ATLAS_WORLD_HALF_HEIGHT))-ATLAS_WORLD_HALF_HEIGHT;
  AtlasPlan_SampleDetail(px,py,&warm);
  AtlasPlan_SampleDetail((px+1733)%ATLAS_WORLD_WIDTH,py+1<ATLAS_WORLD_HALF_HEIGHT?py+1:py,&warm);
  AtlasPlan_ResetSample();
  AtlasPlan_SampleDetail((px+1733)%ATLAS_WORLD_WIDTH,py+1<ATLAS_WORLD_HALF_HEIGHT?py+1:py,&cold);
  assert(!memcmp(&warm,&cold,sizeof warm));
 }
 for(i=0;i<9;i++){printf("biome %d samples %u\n",i,biomes[i]);assert(biomes[i]);}
 assert(AtlasTerrain_FindSpawn(&spawnX,&spawnY));
 printf("spawn %d,%d chunk %d,%d local %d,%d\n",spawnX,spawnY,AtlasTerrain_FloorDiv(spawnX,20),AtlasTerrain_FloorDiv(spawnY,20),spawnX%20,spawnY%20);
 for(dy=-4;dy<=4;dy++)for(dx=-4;dx<=4;dx++)assert(!(AtlasTerrain_Tile(spawnX+dx,spawnY+dy)&0xC00));
 a[0]=0xCAFE;a[1191]=0xBEEF;
 assert(AtlasTerrain_Fill(a+1,35,34,AtlasTerrain_FloorDiv(spawnX,20),AtlasTerrain_FloorDiv(spawnY,20)));
 assert(AtlasTerrain_Fill(b,35,34,AtlasTerrain_FloorDiv(spawnX,20)+1,AtlasTerrain_FloorDiv(spawnY,20)));
 assert(AtlasTerrain_Fill(c,35,34,AtlasTerrain_FloorDiv(spawnX,20),AtlasTerrain_FloorDiv(spawnY,20)+1));
 for(y=0;y<34;y++)for(x=0;x<15;x++)assert(a[1+y*35+x+20]==b[y*35+x]);
 for(y=0;y<14;y++)for(x=0;x<35;x++)assert(a[1+(y+20)*35+x]==c[y*35+x]);
 for(y=0;y<34;y++)for(x=0;x<35;x++)
  assert(a[1+y*35+x]==AtlasTerrain_Tile(AtlasTerrain_FloorDiv(spawnX,20)*20-7+x,AtlasTerrain_FloorDiv(spawnY,20)*20-7+y));
 assert(a[0]==0xCAFE&&a[1191]==0xBEEF);
 assert(AtlasTerrain_Fill(b,35,34,-1,-1));assert(AtlasTerrain_Fill(c,35,34,0,-1));
 for(y=0;y<34;y++)for(x=0;x<15;x++)assert(b[y*35+x+20]==c[y*35+x]);
 assert(AtlasTerrain_Fill(b,35,34,ATLAS_WORLD_WIDTH/20-1,0));assert(AtlasTerrain_Fill(c,35,34,0,0));
 for(y=0;y<34;y++)for(x=0;x<15;x++)assert(b[y*35+x+20]==c[y*35+x]);
 for(i=0;i<1192;i++)a[i]=0xCAFE;
 assert(!AtlasTerrain_Fill(a+1,34,34,0,0));assert(!AtlasTerrain_Fill(a+1,35,34,0,ATLAS_WORLD_HALF_HEIGHT));
 for(i=0;i<1192;i++)assert(a[i]==0xCAFE);
 assert(AtlasTerrain_Tile(0,ATLAS_WORLD_HALF_HEIGHT)&0xC00);
 AtlasStream_Reset();
 assert(AtlasStream_Load(b,AtlasTerrain_FloorDiv(spawnX,20),AtlasTerrain_FloorDiv(spawnY,20)));
 assert(gAtlasCacheMisses==1);
 for(i=0;i<40;i++)AtlasStream_Tick(AtlasTerrain_FloorDiv(spawnX,20),AtlasTerrain_FloorDiv(spawnY,20),18,10);
 assert(AtlasStream_Load(c,AtlasTerrain_FloorDiv(spawnX,20)+1,AtlasTerrain_FloorDiv(spawnY,20)));
 assert(gAtlasCacheHits==1&&gAtlasCacheMisses==1);
 for(i=0;i<40;i++) {
  assert(AtlasStream_Load(c,AtlasTerrain_FloorDiv(spawnX,20),AtlasTerrain_FloorDiv(spawnY,20)));
  assert(AtlasStream_Load(c,AtlasTerrain_FloorDiv(spawnX,20)+1,AtlasTerrain_FloorDiv(spawnY,20)));
 }
 assert(gAtlasCacheMisses==1);
 puts("PASS: nine biomes, repeatable fields, safe spawn, grid bounds, seams, longitude wrap and repeated cached crossings");
}

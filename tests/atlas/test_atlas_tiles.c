#include "global.h"
#include "atlas_world.h"
#include "atlas_tiles.h"
#include <assert.h>
#include <stdio.h>
static int fixture;
u32 gAtlasWorldSeed;
s32 AtlasTerrain_FloorDiv(s32 n,s32 d){s32 q=n/d;return n%d<0?q-1:q;}
u8 AtlasTerrain_ReliefNoise(s32 x,s32 y,bool8 broad){return 128;}
void AtlasTerrain_Sample(s32 x,s32 y,struct AtlasCell*c){
 if(fixture>=2){memset(c,0,sizeof(*c));c->landform=ATLAS_COAST;c->elevation=125;c->biome=fixture==2?ATLAS_WETLAND:ATLAS_DESERT;return;}
 x%=ATLAS_WORLD_WIDTH;if(x<0)x+=ATLAS_WORLD_WIDTH;
 memset(c,0,sizeof(*c));c->waterform=((x%13<7)^(y%9<4)^fixture)?ATLAS_RIVER:ATLAS_DRY;
 c->biome=(y%5==0)?ATLAS_DESERT:ATLAS_GRASSLAND;c->elevation=130;
}
u16 AtlasTerrain_BaseTile(s32 x,s32 y,const struct AtlasCell*c,bool8 clearCover){return 0x3001;}
int main(void){
 u16 row[35],a[35],b[35];struct AtlasCell c;unsigned m,mat,form,i;int x,y;
 for(form=1;form<=3;form++)for(mat=0;mat<4;mat++)for(m=0;m<256;m++){
  u16 id=AtlasTiles_Water(m,mat,form);assert(id>=512&&id<512+ATLAS_NATURE_WATER_COUNT);
  assert((gAtlasNatureAttributes[id-512]&255)==(form==ATLAS_SEA?0x15:0x10));
 }
 for(i=0;i<ATLAS_NATURE_OPTION_COUNT*8;i++){
  u16 entry=gAtlasNatureMetatiles[i];assert((entry&1023)<1024);
  assert((entry>>12)<=12); /* includes native grass references for canopy bases */
 }
 for(m=0;m<256;m++){
  static const unsigned neighbor[8]={1,5,7,3,2,8,6,0};
  u16 cells[9]={0};
  cells[4]=0x30;
  for(i=0;i<8;i++)if(m&(1u<<i))cells[neighbor[i]]=0x30;
  assert(!!(AtlasTiles_Render(0x3001,cells)&0xC00)==(m!=255));
  cells[4]|=0x80;assert(!(AtlasTiles_Render(0x3001,cells)&0xC00));
 }
 /* All cardinal borders preserve the floor's movement/elevation bits and
  * native sand behavior. Diagonal geography does not change this first-stage
  * edge catalogue. Water/snow/rock continue sand instead of adding green. */
 for(m=0;m<16;m++)for(mat=0;mat<4;mat++)for(form=0;form<4;form++){
  static const unsigned sides[4]={1,5,7,3};
  u16 cells[9]={0},floor=0x3000|(ATLAS_NATURE_SAND_FIRST+mat),expected;
  cells[4]=0x7C04;
  for(i=0;i<4;i++)cells[sides[i]]=(m&(1<<i))?(form?form:4):0;
  expected=m==15?(floor&1023):m==14?ATLAS_NATURE_SAND_EDGE_NORTH:ATLAS_NATURE_SAND_EDGE_FIRST+m;
  u16 result=AtlasTiles_Render(floor,cells);
  assert((result&1023)==expected && (result&0xFC00)==(floor&0xFC00));
  assert(gAtlasNatureAttributes[expected-512]==0x1021);
  cells[4]|=0x80;assert(AtlasTiles_Render(floor,cells)==result);
  for(i=0;i<4;i++)cells[sides[i]]=12;
  assert(AtlasTiles_Render(floor,cells)==floor); /* no green snow fringe */
  for(i=0;i<4;i++)cells[sides[i]]=8;
  assert(AtlasTiles_Render(floor,cells)==floor); /* no green rock fringe */
 }
 for(y=-8;y<8;y++)for(x=-1;x<2;x++){
  AtlasTiles_FillRow(row,x,y,35);
  for(i=0;i<35;i++){
   AtlasTerrain_Sample(x+i,y,&c);
   assert(row[i]==AtlasTiles_At(x+i,y,&c));
   assert(!!(row[i]&0xC00)==!!c.waterform);
  }
 }
 AtlasTiles_FillRow(a,-7,19,35);AtlasTiles_FillRow(b,13,19,35);
 for(i=0;i<15;i++)assert(a[i+20]==b[i]);
 AtlasTiles_FillRow(a,ATLAS_WORLD_WIDTH-7,-1,35);AtlasTiles_FillRow(b,-7,-1,35);
 assert(!memcmp(a,b,sizeof(a)));
 fixture=1;AtlasTiles_Reset();AtlasTiles_FillRow(row,0,0,35);
 for(i=0;i<35;i++){AtlasTerrain_Sample(i,0,&c);assert(row[i]==AtlasTiles_At(i,0,&c));}
 u16 ignored;
 fixture=2;assert((AtlasTiles_Describe(0,0,&ignored)&12)==0); /* marsh bank matches green floor */
 fixture=3;assert((AtlasTiles_Describe(0,0,&ignored)&12)==4); /* dry coast keeps sand bank */
 puts("PASS: all masks/material/water behaviors, row/point equivalence, blocked water, cache reset, overlaps and X wrap");
}

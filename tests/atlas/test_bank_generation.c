#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include <assert.h>
#include <stdio.h>
static struct {s32 x,y,offset;unsigned transect;struct AtlasCell cell;} samples[2048];
static u16 map[1190];
static void InstallLake(void)
{
    for(unsigned i=0;i<ATLAS_PLAN_COUNT;i++)
        gAtlasPlan[i]=(struct AtlasPlanNode){i,0,1,0,160,190,210,160};
    unsigned id=24*ATLAS_PLAN_WIDTH+48;
    gAtlasPlan[id].height=150;gAtlasPlan[id].water=165;gAtlasPlan[id].basin=ATLAS_LAKE_FLAG|1;
    AtlasPlan_ResetSample();AtlasWater_Reset();
}
static void Generate(unsigned version)
{
    assert(AtlasWorld_SetGeneration(version));assert(AtlasPlan_Begin(0));while(!AtlasPlan_Step(256));
}
int main(void)
{
    setbuf(stdout,NULL);
    Generate(8);u32 checksum=gAtlasPlanStats.checksum,habitat=gAtlasPlanStats.habitatChecksum;
    unsigned count=0,nodes=0;
    for(unsigned id=0;id<ATLAS_PLAN_COUNT && nodes<32;id++)
    {
        struct AtlasPlanNode *n=&gAtlasPlan[id];
        if(n->height<ATLAS_PLAN_SEA || n->runoff<600 || n->parent>=ATLAS_PLAN_COUNT || n->parent==id
         || gAtlasPlan[n->parent].height<ATLAS_PLAN_SEA || (gAtlasPlan[n->parent].basin&ATLAS_LAKE_FLAG))continue;
        s32 ax,ay,bx,by,x,y;u16 h;
        AtlasWater_Point(id,1,&ax,&ay,&h);AtlasWater_Point(id,3,&bx,&by,&h);AtlasWater_Point(id,2,&x,&y,&h);
        unsigned width=24+n->runoff/128;if(width>400)width=400;
        s32 dx=bx-ax,dy=by-ay;bool8 horizontal=(dx<0?-dx:dx)>(dy<0?-dy:dy);
        for(int side=-1;side<=1;side+=2)for(unsigned d=0;d<=width+96;d+=4)
        {
            if(count==2048)break;
            s32 wx=x+(horizontal?0:side*(s32)d),wy=y+(horizontal?side*(s32)d:0);
            wx%=ATLAS_WORLD_WIDTH;if(wx<0)wx+=ATLAS_WORLD_WIDTH;
            if(wy<=-ATLAS_WORLD_HALF_HEIGHT || wy>=ATLAS_WORLD_HALF_HEIGHT)continue;
            samples[count].x=wx;samples[count].y=wy;samples[count].offset=side*(s32)d;samples[count].transect=nodes;
            AtlasTerrain_Sample(wx,wy,&samples[count].cell);count++;
        }
        nodes++;
    }
    Generate(9);assert(gAtlasPlanStats.checksum==checksum && gAtlasPlanStats.habitatChecksum==habitat);
    unsigned lowered=0,moister=0,first=0;
    FILE *survey=fopen("build/generation9-bank-samples.csv","w");assert(survey);
    fprintf(survey,"transect,offset,old_height,new_height,old_moisture,new_moisture,water\n");
    for(unsigned i=0;i<count;i++)
    {
        struct AtlasCell c;AtlasTerrain_Sample(samples[i].x,samples[i].y,&c);
        struct AtlasCell *old=&samples[i].cell;
        fprintf(survey,"%u,%d,%u,%u,%u,%u,%u\n",samples[i].transect,samples[i].offset,old->elevation,c.elevation,old->moisture,c.moisture,c.waterform);
        assert(c.waterform==old->waterform);
        if(c.waterform)assert(c.elevation==old->elevation);
        else
        {
            assert(c.elevation<=old->elevation);
            if(c.elevation<old->elevation){first=i;lowered++;}
            moister+=c.moisture>old->moisture;
            if(old->landform!=ATLAS_COAST && c.elevation<128)assert(c.landform!=ATLAS_COAST);
        }
    }
    fclose(survey);
    printf("Bank survey: count=%u lowered=%u moister=%u\n",count,lowered,moister);
    assert(count>100 && lowered>10 && moister>10);
    s32 x=samples[first].x,y=samples[first].y,cx=AtlasTerrain_FloorDiv(x,20),cy=AtlasTerrain_FloorDiv(y,20),oldCX=cx;
    AtlasStream_Reset();assert(AtlasStream_Load(map,cx,cy));
    for(unsigned step=0;step<240;step++)
    {
        s32 wx=x+(step<120?(s32)step:239-(s32)step);cx=AtlasTerrain_FloorDiv(wx,20);
        if(cx!=oldCX){assert(AtlasStream_Load(map,cx,cy));oldCX=cx;}
        for(unsigned frame=0;frame<8;frame++)AtlasStream_Tick(cx,cy,wx-cx*20,y-cy*20);
        for(s32 dy=-4;dy<=4;dy++)for(s32 dx=-6;dx<=6;dx++)
            assert(map[(y+dy-cy*20+7)*35+wx+dx-cx*20+7]==AtlasTerrain_Tile(wx+dx,y+dy));
    }
    assert(gAtlasCacheMisses==1 && !gAtlasStreamWaits);
    for(unsigned i=0;i<ATLAS_PLAN_COUNT;i++)gAtlasPlan[i].height=125;
    assert(!AtlasPlan_HasSea(0,0));
    gAtlasPlan[23*ATLAS_PLAN_WIDTH].height=100;
    assert(AtlasPlan_HasSea(0,0) && AtlasPlan_HasSea(ATLAS_WORLD_WIDTH,0));
    assert(AtlasPlan_HasSea(-1,0) && AtlasPlan_HasSea(ATLAS_WORLD_WIDTH-1,0));
    assert(!AtlasPlan_HasSea(ATLAS_WORLD_WIDTH/2,0));
    assert(!AtlasPlan_HasSea(0,ATLAS_WORLD_HALF_HEIGHT));
    Generate(8);InstallLake();unsigned shores=0;
    for(s32 dy=-240000;dy<=240000;dy+=16000)for(s32 dx=-240000;dx<=240000;dx+=16000)
    {
        s32 wx=ATLAS_WORLD_WIDTH/2+dx,wy=24*(2*ATLAS_WORLD_HALF_HEIGHT/(ATLAS_PLAN_HEIGHT-1))-ATLAS_WORLD_HALF_HEIGHT+dy;
        struct AtlasPlanSample p;AtlasPlan_SampleDetail(wx,wy,&p);
        if(p.lake!=2 || p.height<p.water || p.height>p.water+3)continue;
        assert(shores<2048);samples[shores].x=wx;samples[shores].y=wy;
        AtlasTerrain_Sample(wx,wy,&samples[shores].cell);shores++;
    }
    assert(shores);
    Generate(9);InstallLake();unsigned marshes=0;
    for(unsigned i=0;i<shores;i++)
    {
        struct AtlasCell c;AtlasTerrain_Sample(samples[i].x,samples[i].y,&c);
        assert(c.waterform==samples[i].cell.waterform && c.elevation==samples[i].cell.elevation);
        assert(c.moisture>=samples[i].cell.moisture);
        marshes+=c.waterform==ATLAS_DRY && c.biome==ATLAS_WETLAND && c.elevation>132;
    }
    assert(marshes);printf("PASS %u elevated dry lake-bank wetland samples follow lake-relative height, not sea-level altitude\n",marshes);
    printf("PASS generation9 %u river transect samples: %u lowered, %u moister, unchanged macro atlas/water, inland banks not beaches; 240 streamed camera steps agree with direct terrain\n",count,lowered,moister);
}

#include "global.h"
#include "atlas_plan.h"
#include "atlas_world.h"
#include <assert.h>
#include <stdio.h>
static s32 Abs(s32 n){return n<0?-n:n;}
static s32 Wrap(s32 n){n%=ATLAS_WORLD_WIDTH;return n<0?n+ATLAS_WORLD_WIDTH:n;}
static unsigned samples,rises,dry;
static bool8 Minor(u16 id)
{
    struct AtlasPlanNode *n=&gAtlasPlan[id],*p=&gAtlasPlan[n->parent];
    return n->height>=ATLAS_PLAN_SEA && n->runoff>=240 && n->runoff<600
        && n->parent!=id && p->height>=ATLAS_PLAN_SEA && p->runoff>=600;
}
static void Check(s32 x,s32 y,u8 *previous,u32 seed,u16 node)
{
    struct AtlasCell c;AtlasTerrain_Sample(Wrap(x),y,&c);samples++;
    if(c.waterform==ATLAS_DRY)
    {if(dry++<3)printf("Missing channel seed=%u node=%u at %d,%d\n",seed,node,x,y);*previous=255;return;}
    if(c.elevation>*previous)
    {if(rises++<3)printf("Rise seed=%u junction=%u at %d,%d: %u -> %u\n",seed,node,x,y,*previous,c.elevation);}
    *previous=c.elevation;
}
int main(void)
{
    const u32 seeds[]={0,4220706268u,1,123456789};unsigned junctions=0,branches=0,minor=0;
    setbuf(stdout,NULL);
    assert(!AtlasWater_IsTributary(ATLAS_PLAN_COUNT));
    assert(!AtlasWater_IsTributary(0xFFFF));
    gAtlasPlan[0].parent=0xFFFF;assert(!AtlasWater_IsTributary(0));
    for(unsigned s=0;s<sizeof(seeds)/sizeof(seeds[0]);s++)
    {
        assert(AtlasPlan_Begin(seeds[s]));while(!AtlasPlan_Step(256));
        for(u16 id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            u16 child[4],count=0;unsigned col=id%ATLAS_PLAN_WIDTH,row=id/ATLAS_PLAN_WIDTH;
            u16 adjacent[4]={row*ATLAS_PLAN_WIDTH+(col+1)%ATLAS_PLAN_WIDTH,row*ATLAS_PLAN_WIDTH+(col+ATLAS_PLAN_WIDTH-1)%ATLAS_PLAN_WIDTH,row? id-ATLAS_PLAN_WIDTH:id,row+1<ATLAS_PLAN_HEIGHT?id+ATLAS_PLAN_WIDTH:id};
            assert(AtlasWater_IsTributary(id)==Minor(id));
            if(Minor(id))
            {
                u8 previous=255;minor++;
                for(unsigned part=0;part<4;part++)
                {
                    s32 ax,ay,bx,by;u16 unused;
                    AtlasWater_Point(id,part,&ax,&ay,&unused);AtlasWater_Point(id,part+1,&bx,&by,&unused);
                    for(unsigned step=0;step<64;step++)Check(ax+(s64)(bx-ax)*step/64,ay+(s64)(by-ay)*step/64,&previous,seeds[s],id);
                }
            }
            if(gAtlasPlan[id].height<ATLAS_PLAN_SEA || gAtlasPlan[id].runoff<600 || gAtlasPlan[id].parent==id)continue;
            for(unsigned k=0;k<4;k++)
            {u16 n=adjacent[k];if(n!=id && gAtlasPlan[n].parent==id && (gAtlasPlan[n].runoff>=600 || Minor(n)) && gAtlasPlan[n].height>=ATLAS_PLAN_SEA)child[count++]=n;}
            if(count<2)continue;
            junctions++;
            for(unsigned k=0;k<count;k++)
            {
                s32 ax,ay,bx,by,dx,dy,length;u16 unused;u8 previous=255;
                AtlasWater_Point(child[k],3,&ax,&ay,&unused);AtlasWater_Point(child[k],4,&bx,&by,&unused);
                dx=bx-ax;dy=by-ay;length=Abs(dx)>Abs(dy)?Abs(dx):Abs(dy);assert(length);
                for(int d=512;d>=0;d--)Check(bx-(s64)dx*d/length,by-(s64)dy*d/length,&previous,seeds[s],id);
                AtlasWater_Point(id,0,&ax,&ay,&unused);AtlasWater_Point(id,1,&bx,&by,&unused);
                dx=bx-ax;dy=by-ay;length=Abs(dx)>Abs(dy)?Abs(dx):Abs(dy);assert(length);
                for(int d=1;d<=512;d++)Check(ax+(s64)dx*d/length,ay+(s64)dy*d/length,&previous,seeds[s],id);
                branches++;
            }
        }
    }
    printf("Junctions=%u incoming branches=%u minor channels=%u samples=%u rises=%u dry=%u\n",junctions,branches,minor,samples,rises,dry);
    assert(junctions && branches && minor && !rises && !dry);
    puts("PASS connected incoming/outgoing confluence paths remain level or descending at tile scale");
}

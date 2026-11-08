#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include <assert.h>
#include <stdio.h>
static s32 Wrap(s32 x){x%=ATLAS_WORLD_WIDTH;return x<0?x+ATLAS_WORLD_WIDTH:x;}
int main(void)
{
    const u32 seeds[]={0,4220706268u,1,123456789};
    unsigned edges=0,checks=0,outlets=0,outletSamples=0,wrong=0;
    for(unsigned s=0;s<sizeof(seeds)/sizeof(seeds[0]);s++)
    {
        assert(AtlasPlan_Begin(seeds[s]));while(!AtlasPlan_Step(256));
        for(unsigned id=0;id<ATLAS_PLAN_COUNT;id++)
        {
            struct AtlasPlanNode *n=&gAtlasPlan[id],*p=&gAtlasPlan[n->parent];
            if(n->basin&ATLAS_LAKE_FLAG)
            {
                /* Flooded lake nodes discharge along a level first edge to
                 * the spill approach; lowering that edge would drain the plane. */
                assert(n->water==p->water);outlets++;
                if(n->runoff>=600)
                    for(unsigned part=0;part<5;part++)
                    {
                        s32 x,y;u16 unused;struct AtlasCell c;
                        AtlasWater_Point(id,part,&x,&y,&unused);AtlasTerrain_Sample(Wrap(x),y,&c);
                        assert(c.waterform!=ATLAS_DRY && c.elevation==n->water);outletSamples++;
                    }
            }
            if(n->runoff<600 || n->height<ATLAS_PLAN_SEA || !(p->basin&ATLAS_LAKE_FLAG) || n->water<=p->water)continue;
            edges++;bool8 entered=FALSE;u8 previous=255;
            for(unsigned part=0;part<4;part++)
            {
                s32 ax,ay,bx,by;u16 unused;
                AtlasWater_Point(id,part,&ax,&ay,&unused);AtlasWater_Point(id,part+1,&bx,&by,&unused);
                for(unsigned step=0;step<64;step++)
                {
                    s32 x=Wrap(ax+(s64)(bx-ax)*step/64),y=ay+(s64)(by-ay)*step/64;
                    struct AtlasPlanSample plan;struct AtlasCell c;
                    u8 ground=AtlasTerrain_GroundHeight(x,y,&plan);
                    bool8 receiving=plan.lake==1 && plan.water==p->water && ground+1<plan.water;
                    /* Same quarter-endpoint bracket contract as runtime fit;
                     * tiny earlier inlets between endpoints are not promised. */
                    if(!step && receiving)entered=TRUE;
                    AtlasTerrain_Sample(x,y,&c);
                    assert(c.waterform!=ATLAS_DRY && c.elevation<=previous);previous=c.elevation;
                    if(entered && receiving)
                    {
                        checks++;
                        if(c.waterform!=ATLAS_LAKE || c.elevation!=p->water)
                        {if(wrong++<3)printf("Wrong inlet seed=%u node=%u level=%u actual=%u type=%u\n",seeds[s],id,p->water,c.elevation,c.waterform);}
                    }
                }
            }
        }
    }
    printf("Lake inlet edges=%u receiving-plane samples=%u level outlet edges=%u wet outlet samples=%u wrong=%u\n",edges,checks,outlets,outletSamples,wrong);
    assert(edges && checks && outlets && !wrong);
    puts("PASS lake inlet planes, descending connected centerlines and level lake-node outlet approaches");
}

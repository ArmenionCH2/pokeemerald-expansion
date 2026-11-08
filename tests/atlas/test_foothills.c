#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    struct AtlasCell c;
    unsigned seed,i,e,counts[4][4]={{0}},lowRaised=0;
    u8 levels[4]={175,180,190,220};
    memset(&c,0,sizeof(c));c.valley=255;
    for(seed=0;seed<2;seed++)
    {
        AtlasTerrain_SetSeed(seed?4220706268u:0);
        for(i=0;i<4096;i++)
        {
            s32 x=(s32)((u32)i*9787%ATLAS_WORLD_WIDTH),y=(s32)i*73-150000;
            unsigned previous=0;
            for(e=155;e<=230;e++)
            {
                unsigned tier;
                c.elevation=e;tier=(AtlasMountain_Relief(x,y,&c)>>4)&7;
                assert(tier>=previous && tier<=previous+1 && tier<=3);
                if(e==155)assert(!tier);
                previous=tier;
            }
            for(e=0;e<4;e++)
            {
                c.elevation=levels[e];
                u8 a=AtlasMountain_Relief(x,y,&c),b=AtlasMountain_Relief(x+ATLAS_WORLD_WIDTH,y,&c);
                assert(a==b);counts[e][(a>>4)&7]++;
                if(e==0 && (a&0x70))lowRaised++;
            }
        }
    }
    assert(lowRaised);assert(counts[1][0]&&counts[1][1]&&!counts[1][2]&&!counts[1][3]);
    assert(counts[2][2]&&!counts[2][3]);assert(counts[3][0]&&counts[3][1]&&counts[3][2]&&counts[3][3]);
    for(e=0;e<4;e++)printf("RELIEF elevation=%u tiers=%u,%u,%u,%u\n",levels[e],counts[e][0],counts[e][1],counts[e][2],counts[e][3]);
    c.elevation=175;c.valley=0;assert(!(AtlasMountain_Relief(123,456,&c)&0x70));
    for(i=1;i<4;i++){c.waterform=i;c.elevation=220;assert(!AtlasMountain_Relief(123,456,&c));}
    puts("PASS: foothills below former cutoff, gradual monotone envelope, one-tier elevation steps, periodic detail, valley floor and protected water");
    return 0;
}

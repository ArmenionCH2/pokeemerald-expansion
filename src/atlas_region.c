#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_region.h"

u8 AtlasRegion_Classify(const u8 h[9])
{
    unsigned i,low=255,high=0;
    if (!h) return ATLAS_REGION_UNKNOWN;
    if (h[4]<ATLAS_PLAN_SEA) return ATLAS_REGION_OCEAN;
    for (i=0;i<9;i++)
    {
        if (h[i]<ATLAS_PLAN_SEA) return ATLAS_REGION_COAST;
        if (h[i]<low) low=h[i];
        if (h[i]>high) high=h[i];
    }
    for (i=0;i<4;i++)
    {
        static const u8 axes[4][4]={{3,5,1,7},{1,7,3,5},{0,8,2,6},{2,6,0,8}};
        s32 alongA=(s32)h[axes[i][2]]-h[4],alongB=(s32)h[axes[i][3]]-h[4];
        /* An open valley/ridge can descend along its axis. Fit the center
         * against that longitudinal profile instead of demanding a flat
         * floor. A pit or isolated summit lies off the fitted profile. */
        if (alongA+alongB< -16 || alongA+alongB>16
         || alongA-alongB< -64 || alongA-alongB>64) continue;
        if (h[axes[i][0]]>=h[4]+10 && h[axes[i][1]]>=h[4]+10) return ATLAS_REGION_VALLEY;
        if (h[axes[i][0]]+12<=h[4] && h[axes[i][1]]+12<=h[4]) return ATLAS_REGION_RIDGE;
    }
    if (h[4]>=170 && high-low<=18) return ATLAS_REGION_PLATEAU;
    if (h[4]>=185 || high-low>=40) return ATLAS_REGION_MOUNTAINS;
    return high-low>18 || h[4]>155?ATLAS_REGION_HILLS:ATLAS_REGION_PLAIN;
}
static u8 Height(s32 x,s32 y)
{
    // Raw interpolated macro height is enough for regional relief. Avoid the
    // detailed lake-floor fitter and all water-cache work in this stencil.
    u32 sx=(u32)x*ATLAS_PLAN_WIDTH,sy=(u32)(y+ATLAS_WORLD_HALF_HEIGHT)*(ATLAS_PLAN_HEIGHT-1);
    u32 col=sx/ATLAS_WORLD_WIDTH,row=sy/(2*ATLAS_WORLD_HALF_HEIGHT);
    u32 fx=(sx%ATLAS_WORLD_WIDTH)/(ATLAS_WORLD_WIDTH/256);
    u32 fy=(sy%(2*ATLAS_WORLD_HALF_HEIGHT))/(2*ATLAS_WORLD_HALF_HEIGHT/256);
    u32 next=(col+1)%ATLAS_PLAN_WIDTH,lower=row+1<ATLAS_PLAN_HEIGHT?row+1:row;
    if (fx>255) fx=255;
    if (fy>255) fy=255;
    u32 top=gAtlasPlan[row*ATLAS_PLAN_WIDTH+col].height*(256-fx)+gAtlasPlan[row*ATLAS_PLAN_WIDTH+next].height*fx;
    u32 bottom=gAtlasPlan[lower*ATLAS_PLAN_WIDTH+col].height*(256-fx)+gAtlasPlan[lower*ATLAS_PLAN_WIDTH+next].height*fx;
    return (top*(256-fy)+bottom*fy)>>16;
}
u8 AtlasRegion_At(s32 x,s32 y)
{
    u8 heights[9];unsigned i;
    if (!gAtlasPlanReady || y< -ATLAS_WORLD_HALF_HEIGHT || y>=ATLAS_WORLD_HALF_HEIGHT)
        return ATLAS_REGION_UNKNOWN;
    x%=ATLAS_WORLD_WIDTH;if (x<0) x+=ATLAS_WORLD_WIDTH;
    for (i=0;i<9;i++)
    {
        s32 sx=x+((s32)(i%3)-1)*200000,sy=y+((s32)(i/3)-1)*200000;
        sx%=ATLAS_WORLD_WIDTH;if (sx<0) sx+=ATLAS_WORLD_WIDTH;
        if (sy< -ATLAS_WORLD_HALF_HEIGHT) sy= -ATLAS_WORLD_HALF_HEIGHT;
        if (sy>=ATLAS_WORLD_HALF_HEIGHT) sy=ATLAS_WORLD_HALF_HEIGHT-1;
        heights[i]=Height(sx,sy);
    }
    return AtlasRegion_Classify(heights);
}

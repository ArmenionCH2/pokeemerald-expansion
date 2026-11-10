#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_ocean.h"

static u32 Mix(u32 n)
{
    n^=n>>16;n*=0x7FEB352Du;n^=n>>15;n*=0x846CA68Bu;return n^(n>>16);
}
static u32 Grain(u32 x,u32 y,u32 salt,u32 mask)
{
    return Mix(gAtlasWorldSeed^salt^x*0x9E3779B1u^y*0x85EBCA77u)&mask;
}
static unsigned Noise(unsigned x,unsigned y,u32 salt,u32 mask)
{
    unsigned gx=x/8,gy=y/8,next=(gx+1)%(ATLAS_PLAN_WIDTH/8),fx=(x&7)*32,fy=(y&7)*32;
    unsigned top=Grain(gx,gy,salt,mask)*(256-fx)+Grain(next,gy,salt,mask)*fx;
    unsigned bottom=Grain(gx,gy+1,salt,mask)*(256-fx)+Grain(next,gy+1,salt,mask)*fx;
    return (top*(256-fy)+bottom*fy)>>16;
}
static bool8 Land(s32 x,s32 y)
{
    if(y<0 || y>=ATLAS_PLAN_HEIGHT)return FALSE;
    if(x<0)x+=ATLAS_PLAN_WIDTH;else if(x>=ATLAS_PLAN_WIDTH)x-=ATLAS_PLAN_WIDTH;
    return gAtlasPlan[y*ATLAS_PLAN_WIDTH+x].height>=ATLAS_PLAN_SEA;
}
static unsigned Ridge(s32 x,s32 y)
{
    unsigned best=0;
    s32 dy=y-ATLAS_PLAN_HEIGHT/2;
    for(unsigned i=0;i<3;i++)
    {
        u32 h=Mix(gAtlasWorldSeed^0x52494447u^i*0x9E3779B1u);
        s32 center=(s32)(i*(ATLAS_PLAN_WIDTH/3)+(h%(ATLAS_PLAN_WIDTH/3)));
        center+=dy*((h&1)?1:-1)/3+(dy*dy/64)*((h&2)?1:-1);
        center%=ATLAS_PLAN_WIDTH;if(center<0)center+=ATLAS_PLAN_WIDTH;
        s32 dx=x-center;
        if(dx>ATLAS_PLAN_WIDTH/2)dx-=ATLAS_PLAN_WIDTH;
        if(dx< -ATLAS_PLAN_WIDTH/2)dx+=ATLAS_PLAN_WIDTH;
        if(dx<0)dx=-dx;
        s32 uplift=44-dx*11;
        if(uplift>(s32)best)best=uplift;
    }
    return best;
}
u8 AtlasOcean_Height(u16 node)
{
    if(node>=ATLAS_PLAN_COUNT)return 0;
    if(gAtlasPlan[node].height>=ATLAS_PLAN_SEA)return gAtlasPlan[node].height;
    s32 x=node%ATLAS_PLAN_WIDTH,y=node/ATLAS_PLAN_WIDTH;
    unsigned noise=Noise(x,y,0x42415448u,31);
    for(s32 radius=1;radius<=2;radius++)
        for(s32 dy=-radius;dy<=radius;dy++)for(s32 dx=-radius;dx<=radius;dx++)
        {
            if(dx!=-radius && dx!=radius && dy!=-radius && dy!=radius)continue;
            if(Land(x+dx,y+dy))return radius==1?110+noise%9:75+noise%24;
        }
    return 6+noise+Ridge(x,y);
}
u8 AtlasOcean_MacroNoise(u16 node,u32 salt)
{
    if(node>=ATLAS_PLAN_COUNT)return 0;
    return Noise(node%ATLAS_PLAN_WIDTH,node/ATLAS_PLAN_WIDTH,salt,255);
}
bool8 AtlasOcean_IslandCandidate(u16 node)
{
    if(node>=ATLAS_PLAN_COUNT)return FALSE;
    unsigned x=node%ATLAS_PLAN_WIDTH,y=node/ATLAS_PLAN_WIDTH;
    if(Ridge(x,y)<22)return FALSE;
    return Mix(gAtlasWorldSeed^0x49534C41u^(y/4)*0x9E3779B1u^(x/16)*0x85EBCA77u)%3==0
        && Mix(gAtlasWorldSeed^node^0x43484149u)%3==0;
}

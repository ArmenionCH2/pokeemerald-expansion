#ifndef ATLAS_TEST_STREAM_WALK_H
#define ATLAS_TEST_STREAM_WALK_H
static u16 map[1190];
static struct {s32 x,y;u16 tiles[5];} observed[640];
static const s8 view[5][2]={{0,0},{-7,-5},{8,-5},{-7,6},{8,6}};
static void Walk(s32 x,s32 y,bool8 vertical)
{
    s32 cx=AtlasTerrain_FloorDiv(x,20),cy=AtlasTerrain_FloorDiv(y,20),oldX=cx,oldY=cy;
    AtlasWater_Reset();AtlasStream_Reset();assert(AtlasStream_Load(map,cx,cy));
    for(unsigned frame=0;frame<80;frame++)AtlasStream_Tick(cx,cy,x-cx*20,y-cy*20);
    for(unsigned step=0;step<640;step++)
    {
        bool8 forward=step<160 || step>=480;
        u8 direction=vertical?(forward?1:2):(forward?4:3);
        if(!AtlasStream_CanStep(direction))fprintf(stderr,"terrain frontier wait at %d,%d vertical=%u step=%u\n",x,y,vertical,step);
        assert(!gAtlasStreamWaits);
        if(vertical)y+=forward?1:-1;else x+=forward?1:-1;
        cx=AtlasTerrain_FloorDiv(x,20);cy=AtlasTerrain_FloorDiv(y,20);
        if(cx!=oldX || cy!=oldY){assert(AtlasStream_Load(map,cx,cy));oldX=cx;oldY=cy;}
        observed[step].x=x;observed[step].y=y;
        for(unsigned i=0;i<5;i++)
            observed[step].tiles[i]=map[(y-cy*20+7+view[i][1])*35+x-cx*20+7+view[i][0]];
        for(unsigned frame=0;frame<8;frame++)AtlasStream_Tick(cx,cy,x-cx*20,y-cy*20);
    }
    assert(!gAtlasStreamWaits && gAtlasCacheMisses==1 && gAtlasCacheHits>=32);
    /* Compare only after the timed walk. A direct geography oracle must never
     * warm hydrology ahead of streaming and hide an unready frontier. */
    for(unsigned step=0;step<640;step++)for(unsigned i=0;i<5;i++)
        assert(observed[step].tiles[i]==AtlasTerrain_Tile(observed[step].x+view[i][0],observed[step].y+view[i][1]));
}
#endif

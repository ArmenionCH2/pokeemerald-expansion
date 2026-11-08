#include "global.h"
#include "atlas_world.h"
#include "atlas_plan.h"
#include "atlas_geo.h"
#include <assert.h>
#include <stdio.h>

#define SIDE 384
#define COUNT (SIDE*SIDE)
static u16 cells[COUNT];
static u8 open[COUNT],visited[COUNT];
static u32 queue[COUNT];
static unsigned Adj(unsigned id,unsigned d)
{
    return d==0?id-1:d==1?id+1:d==2?id-SIDE:id+SIDE;
}
static bool8 WalkConnects(bool8 connectors)
{
    unsigned id,x,y,dx,dy,j;
    memset(open,0,sizeof(open));memset(visited,0,sizeof(visited));
    for(y=1;y<SIDE-1;y++)for(x=1;x<SIDE-1;x++)
    {
        u16 nine[9];
        for(dy=0;dy<3;dy++)for(dx=0;dx<3;dx++)
        {u16 v=cells[(y+dy-1)*SIDE+x+dx-1];nine[dy*3+dx]=connectors?v:v&~0x80;}
        open[y*SIDE+x]=!(AtlasTiles_Render(0x3001,nine)&0xC00);
    }
    for(id=0;id<COUNT;id++)if(open[id]&&!visited[id])
    {
        unsigned head=0,tail=1,low=7,high=0;queue[0]=id;visited[id]=1;
        while(head<tail)
        {
            unsigned at=queue[head++],tier=(cells[at]>>4)&7;
            if(tier<low)low=tier;
            if(tier>high)high=tier;
            for(j=0;j<4;j++)
            {unsigned next=Adj(at,j);if(open[next]&&!visited[next]){visited[next]=1;queue[tail++]=next;}}
        }
        if(low==0&&high==3)return TRUE;
    }
    return FALSE;
}
int main(void)
{
    u32 seeds[]={0,4220706268u};unsigned seed,x,y,i;
    struct AtlasCell c;memset(&c,0,sizeof(c));c.elevation=220;c.valley=255;
    setbuf(stdout,NULL);
    for(seed=0;seed<2;seed++)
    {
        s32 ox,oy;bool8 longRidge=FALSE;
        assert(AtlasPlan_Begin(seeds[seed]));while(!AtlasPlan_Step(256));
        AtlasGeo_Begin(ATLAS_GEO_MOUNTAIN);for(i=0;i<8000&&!AtlasGeo_Step();i++);
        assert(gAtlasGeoStatus==1);ox=gAtlasGeoResult.x-SIDE/2;oy=gAtlasGeoResult.y-SIDE/2;
        for(y=0;y<SIDE;y++)for(x=0;x<SIDE;x++)
        {
            unsigned tier;
            cells[y*SIDE+x]=0xDC00|AtlasMountain_Relief(ox+x,oy+y,&c);
            tier=(cells[y*SIDE+x]>>4)&7;
            if(x){unsigned p=(cells[y*SIDE+x-1]>>4)&7;assert(tier<=p+1&&p<=tier+1);}
            if(y){unsigned p=(cells[(y-1)*SIDE+x]>>4)&7;assert(tier<=p+1&&p<=tier+1);}
        }
        /* A connected crest crosses multiple 128-tile interpolation cells,
         * rather than ending at every old 256-tile mound owner boundary. */
        memset(visited,0,sizeof(visited));
        for(i=0;i<COUNT;i++)if(!visited[i]&&((cells[i]>>4)&7)==3)
        {
            unsigned head=0,tail=1,minX=i%SIDE,maxX=minX,minY=i/SIDE,maxY=minY;
            queue[0]=i;visited[i]=1;
            while(head<tail)
            {
                unsigned at=queue[head++],d,xx=at%SIDE,yy=at/SIDE;
                if(xx<minX)minX=xx;
                if(xx>maxX)maxX=xx;
                if(yy<minY)minY=yy;
                if(yy>maxY)maxY=yy;
                for(d=0;d<4;d++)
                {
                    unsigned next;
                    if((d==0&&!xx)||(d==1&&xx==SIDE-1)||(d==2&&!yy)||(d==3&&yy==SIDE-1))continue;
                    next=Adj(at,d);
                    if(!visited[next]&&((cells[next]>>4)&7)==3){visited[next]=1;queue[tail++]=next;}
                }
            }
            if(maxX-minX>=128||maxY-minY>=128)longRidge=TRUE;
        }
        assert(longRidge);assert(WalkConnects(TRUE));assert(!WalkConnects(FALSE));
        for(i=0;i<4000;i++)
        {
            s32 xx=(i&1)?(s32)i-2000:ATLAS_WORLD_WIDTH-1000+(s32)i;
            s32 yy=(s32)i*17-32000;
            u8 a=AtlasMountain_Relief(xx,yy,&c),b=AtlasMountain_Relief(xx+ATLAS_WORLD_WIDTH,yy,&c);
            assert(a==b);
        }
        printf("PASS connected relief seed %u: cross-cell crests, bounded slopes, pass-to-top walking graph and periodic seam\n",seeds[seed]);
    }
    for(i=1;i<4;i++){c.waterform=i;assert(!AtlasMountain_Relief(0,0,&c));}
    c.waterform=0;c.valley=0;assert(!(AtlasMountain_Relief(0,0,&c)&0x70));
    puts("PASS protected water and carved valley floor");
    return 0;
}

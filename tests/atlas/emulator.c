/* Headless interaction test. Symbol addresses are supplied from the built ELF.
 * Uses genuine emulation and keys; never patches gameplay or save memory. */
#include <mgba/flags.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static color_t pixels[240*160];
static struct mCore *core;
static const char *symbolFile;
static unsigned elapsedFrames;
static uint32_t rosterAddr, objectsAddr, spritesAddr;
static unsigned weatherMask;
static uint32_t creationStateAddr,planStageAddr;
static unsigned generationProfile[14],profilingGeneration;
static void featureColdContinue(unsigned type);
struct ActorView { uint32_t generation[3]; uint16_t x[3], y[3]; unsigned mask; };

/* Pinned ARM layouts: AtlasActorAdmissionSlot=28, ObjectEvent=0x24,
 * Sprite=0x44. These checks inspect the running ROM, not host mock objects. */
static struct ActorView actors(void)
{
    struct ActorView view = {0};
    for (unsigned i=0;i<3;i++)
    {
        uint32_t slot=rosterAddr+i*28;
        unsigned state=core->busRead8(core,slot+25);
        assert(state<=2);
        if (state!=2) continue;
        unsigned objectId=core->busRead8(core,slot+20),spriteId=core->busRead8(core,slot+21);
        assert(core->busRead8(core,slot+26) && objectId<16 && spriteId<64);
        uint32_t object=objectsAddr+objectId*0x24,sprite=spritesAddr+spriteId*0x44;
        assert(core->busRead8(core,object)&1);
        assert(!(core->busRead8(core,object+2)&1)); // never the player
        assert(core->busRead8(core,object+8)==core->busRead8(core,slot+24));
        assert(core->busRead8(core,object+9)==core->busRead8(core,slot+22));
        assert(core->busRead8(core,object+10)==core->busRead8(core,slot+23));
        assert(core->busRead8(core,object+0x23)==spriteId);
        assert(core->busRead8(core,sprite+0x3e)&1);
        assert(core->busRead16(core,sprite+0x2e)==objectId);
        assert(core->busRead16(core,object+4)==(0x4000|core->busRead16(core,slot+12)));
        view.generation[i]=core->busRead32(core,slot+16);
        view.x[i]=core->busRead16(core,object+0x10);
        view.y[i]=core->busRead16(core,object+0x12);
        view.mask|=1u<<i;
    }
    return view;
}
static void quiet(struct mLogger *l,int cat,enum mLogLevel level,const char *fmt,va_list args) {(void)l;(void)cat;(void)level;(void)fmt;(void)args;}
static struct mLogger logger={quiet,0};
static uint32_t symbol(const char *name)
{
    FILE *f=fopen(symbolFile,"r");unsigned addr;char type,word[200];assert(f);
    while (fscanf(f,"%x %c %199s",&addr,&type,word)==3)
        if (!strcmp(word,name)) {fclose(f);return addr;}
    fprintf(stderr,"missing symbol %s\n",name);abort();
}
static void frames(unsigned keys,unsigned count)
{
    core->setKeys(core,keys);
    while(count--)
    {
        core->runFrame(core);elapsedFrames++;
        if(profilingGeneration)
        {
            unsigned state=core->busRead8(core,creationStateAddr);
            unsigned slot=state==1?core->busRead8(core,planStageAddr):state==2?12:13;
            if(state && slot<14)generationProfile[slot]++;
        }
    }
}
static void printGenerationProfile(const char *label)
{
    printf("GENERATION PROFILE %s macro stages:",label);
    for(unsigned i=0;i<12;i++)printf(" %u",generationProfile[i]);
    printf("; finish=%u field=%u real GBA frames\n",generationProfile[12],generationProfile[13]);
}
static void shot(const char *path);
static unsigned settle(void)
{
    unsigned start=elapsedFrames;
    uint32_t main=symbol("gMain"),cb1=symbol("CB1_Overworld"),cb2=symbol("CB2_Overworld");
    uint32_t fade=symbol("gPaletteFade"),lock=symbol("sLockFieldControls");
    /* Expansion ARM PaletteFadeControl.active is bit 31 at offset 12. */
    while ((core->busRead32(core,main)!=(cb1|1) || core->busRead32(core,main+4)!=(cb2|1)
         || (core->busRead32(core,fade+12)&0x80000000u) || core->busRead8(core,lock))
         && elapsedFrames-start<600) frames(0,1);
    if (elapsedFrames-start>=600)
    {
        printf("SETTLE failure cb1=%08x cb2=%08x lock=%u fade=%08x\n",core->busRead32(core,main),core->busRead32(core,main+4),core->busRead8(core,lock),core->busRead32(core,fade+12));
        shot("build/atlas-emulator/settle-failure.ppm");
    }
    assert(elapsedFrames-start<600);
    return elapsedFrames-start;
}
static void press(unsigned keys) {frames(keys,2);frames(0,30);}
static void saveGame(void)
{
    uint32_t callback=symbol("sSaveDialogCallback"),counter=symbol("gSaveCounter");
    uint32_t cursor=symbol("sStartMenuCursorPos"),before=core->busRead32(core,counter);
    unsigned overwrite=0,defaultNo=0,waited;
    press(8);
    for(unsigned i=0;core->busRead8(core,cursor)!=1 && i<8;i++)press(128);
    assert(core->busRead8(core,cursor)==1);press(1);
    for(waited=0;waited<1800;waited++)
    {
        uint32_t step=core->busRead32(core,callback);
        if(core->busRead32(core,counter)!=before &&
           (step==(symbol("SaveSuccessCallback")|1) || step==(symbol("SaveReturnSuccessCallback")|1)))break;
        if(step==(symbol("SaveConfirmInputCallback")|1))press(1);
        else if(step==(symbol("SaveOverwriteInputCallback")|1))
        {
            overwrite=1;
            if(core->busRead8(core,symbol("gDifferentSaveFile"))){defaultNo=1;press(64);}
            press(1);
        }
        else frames(0,1);
    }
    assert(waited<1800);frames(0,180);settle();
    printf("PASS native save completion counter %u -> %u, overwrite=%u default_no=%u\n",before,core->busRead32(core,counter),overwrite,defaultNo);
}
static void shot(const char *path)
{
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 160\n255\n");
    for(unsigned i=0;i<240*160;i++) {unsigned char rgb[3]={pixels[i]&255,(pixels[i]>>8)&255,(pixels[i]>>16)&255};fwrite(rgb,1,3,f);} fclose(f);
}
static void bytes(uint32_t addr,void *out,unsigned size) {for(unsigned i=0;i<size;i++)((uint8_t*)out)[i]=core->busRead8(core,addr+i);}
static void nativeAnimations(void)
{
    unsigned char water[960],flowers[128],after[960];unsigned wet=0,bloom=0;
    uint32_t definitions=symbol("gAtlasNatureMetatiles");
    for (unsigned q=0;q<4;q++)
    {
        unsigned tile=core->busRead16(core,definitions+(746-512)*16+q*2)&1023;
        assert(tile>=432 && tile<462); // full ocean role references native animated tiles
        assert((core->busRead16(core,definitions+(1022-512)*16+8+q*2)&1023)==508+q);
    }
    bytes(0x06000000+432*32,water,sizeof(water));
    bytes(0x06000000+508*32,flowers,sizeof(flowers));
    for (unsigned i=0;i<4;i++)
    {
        frames(0,17);bytes(0x06000000+432*32,after,sizeof(water));wet|=!!memcmp(water,after,sizeof(water));
        bytes(0x06000000+508*32,after,sizeof(flowers));bloom|=!!memcmp(flowers,after,sizeof(flowers));
    }
    assert(wet && bloom);
    printf("PASS native ocean/flower roles reference animated primary graphics; both VRAM animations advance\n");
}
static unsigned fieldWeather(void)
{
    // ARM Weather: 512-byte sprite union, currWeather at 528; rain and snow
    // counts at 538/548. Validate real effect ownership alongside the actors.
    uint32_t weather=symbol("gWeather");
    unsigned current=core->busRead8(core,weather+528),next=core->busRead8(core,weather+529);
    assert(current==2 || current==3 || current==4 || current==6);
    assert(next==2 || next==3 || next==4 || next==6);
    weatherMask|=1u<<current;
    if (current==3 || current==4 || current==6)
    {
        unsigned count=core->busRead8(core,weather+(current==3?538:current==4?548:571));
        assert(count<=(current==3?24:current==4?16:1));
        if (current==6) count*=20;
        for (unsigned i=0;i<count;i++)
        {
            uint32_t sprite=core->busRead32(core,weather+(current==4?96:0)+i*4);
            assert(sprite>=spritesAddr && sprite<spritesAddr+64*0x44 && !((sprite-spritesAddr)%0x44));
            assert(core->busRead8(core,sprite+0x3e)&1);
            // Weather effects never borrow a live wildlife sprite.
            for (unsigned j=0;j<3;j++)
                if (core->busRead8(core,rosterAddr+j*28+25)==2)
                    assert(sprite!=spritesAddr+core->busRead8(core,rosterAddr+j*28+21)*0x44);
        }
    }
    printf("PASS native regional field weather %u -> %u and particle ownership\n",current,next);
    return current;
}
static void placeNames(void)
{
    uint32_t popup=symbol("sAtlasLocationPopup");
    char previous[21]={0};
    unsigned names=0;
    for (unsigned tick=0;tick<800 && names<3;tick++)
    {
        char name[21];unsigned n;
        frames(0,1);
        for (n=0;n<20;n++)
        {
            unsigned c=core->busRead8(core,popup+n);
            if (c==255) break;
            name[n]=c>=0xbb && c<=0xd4 ? 'A'+c-0xbb : c>=0xd5 && c<=0xee ? 'a'+c-0xd5 : ' ';
        }
        name[n]=0;
        if (!name[0] || !strcmp(name,previous)) continue;
        assert(!strstr(name,"Route"));
        if (names==0) assert(strstr(name,"Continent") || strstr(name,"Island"));
        if (names==1) assert(strstr(name,"Basin"));
        strcpy(previous,name);printf("PLACE %u: %s\n",names,name);
        frames(0,50);
        char path[80];snprintf(path,sizeof(path),"build/atlas-emulator/place-%u.ppm",names++);shot(path);
    }
    assert(names==3);
    printf("PASS native physical location breadcrumbs: landmass, catchment and local terrain\n");
}
static unsigned nextMapKey(uint32_t save,unsigned shore)
{
    uint32_t layout=symbol("gBackupMapLayout"),map=core->busRead32(core,layout+8);
    int width=core->busRead32(core,layout),height=core->busRead32(core,layout+4);
    int queue[1190],parent[1190],head=0,tail=0;
    unsigned keys[1190];
    assert(width==35 && height==34);
    int x=(int16_t)core->busRead16(core,save)+7,y=(int16_t)core->busRead16(core,save+2)+7;
    assert(x>=0 && x<width && y>=0 && y<height);
    int start=y*width+x;
    for (int i=0;i<1190;i++) parent[i]=-1;
    queue[tail++]=start;parent[start]=start;
    static const int dx[]={1,-1,0,0},dy[]={0,0,1,-1};
    static const unsigned direction[]={16,32,128,64};
    while (head<tail)
    {
        int at=queue[head++],cx=at%width,cy=at/width;
        unsigned goal=!shore && at!=start && (cx<7 || cx>=27 || cy<7 || cy>=27);
        if (shore)
            for (unsigned d=0;d<4;d++) for (int distance=1;distance<=3;distance++)
            {
                int nx=cx+dx[d]*distance,ny=cy+dy[d]*distance;
                if (nx<0 || ny<0 || nx>=width || ny>=height) continue;
                unsigned id=core->busRead16(core,map+(ny*width+nx)*2)&1023;
                if (id>=512 && id<888 && ((id-512>=188)==(shore==2))) goal=1;
            }
        if (goal)
        {
            if (at==start) return 1024; // already on a reachable bank
            while (parent[at]!=start) at=parent[at];
            return keys[at];
        }
        for (unsigned d=0;d<4;d++)
        {
            int nx=cx+dx[d],ny=cy+dy[d],next=ny*width+nx;
            if (nx<0 || ny<0 || nx>=width || ny>=height || parent[next]>=0) continue;
            unsigned tile=core->busRead16(core,map+next*2);
            if ((tile>>10)&3) continue;
            unsigned elevation=tile>>12,from=core->busRead16(core,map+at*2)>>12;
            if (elevation && from && elevation!=from) continue;
            parent[next]=at;keys[next]=direction[d];queue[tail++]=next;
        }
    }
    return 0;
}
static void cachedCrossing(uint32_t savePtr)
{
    uint32_t hits=symbol("gAtlasCacheHits"),miss=symbol("gAtlasCacheMisses"),wait=symbol("gAtlasStreamWaits");
    unsigned initial=core->busRead32(core,hits),misses=core->busRead32(core,miss),waits=core->busRead32(core,wait);
    for (unsigned step=0;step<80 && core->busRead32(core,hits)==initial;step++)
    {
        unsigned key=nextMapKey(core->busRead32(core,savePtr),0);
        if (!key) break;
        frames(key,8);frames(0,20);actors();
    }
    if (core->busRead32(core,hits)>initial)
    {
        assert(core->busRead32(core,miss)==misses && core->busRead32(core,wait)==waits);
        printf("PASS native chunk crossing after globe inspection uses cached terrain, no new synchronous miss or readiness wait\n");
    }
    else printf("NOTE no reachable chunk exit in this spawn viewport; crossing coverage not exercised\n");
}
static void weatherMapRoundTrip(void)
{
    unsigned char before[1456],after[1456];
    uint32_t cursor=symbol("sStartMenuCursorPos"),save=symbol("gSaveBlock3Ptr");
    struct ActorView previous=actors();
    press(8);
    bytes(core->busRead32(core,save),before,sizeof(before));
    for (unsigned i=0;core->busRead8(core,cursor)!=0 && i<8;i++) press(64);
    assert(core->busRead8(core,cursor)==0);press(1);frames(0,120);
    assert(core->busRead8(core,symbol("gAtlasMapActive")));
    bytes(core->busRead32(core,save),after,sizeof(after));
    assert(!memcmp(before,after,sizeof(before)));
    press(2);frames(0,180);press(2);frames(0,120);
    struct ActorView current=actors();fieldWeather();
    for (unsigned i=0;i<3;i++)
        if (previous.mask&(1u<<i)) assert((current.mask&(1u<<i)) && current.generation[i]==previous.generation[i]);
    shot("build/atlas-emulator/field-weather-after-map.ppm");
    printf("PASS native weather/map round trip preserves journal, effect ownership and wildlife generations\n");
}
static void biomeTour(uint32_t savePointer)
{
    uint32_t main=symbol("gMain"),geo=symbol("GeoMenu_Run"),choice=symbol("sGeoChoice");
    uint32_t cursor=symbol("sStartMenuCursorPos"),status=symbol("gAtlasGeoStatus"),result=symbol("gAtlasGeoResult");
    unsigned mapChecked=0;
    for (unsigned visit=0;visit<12;visit++)
    {
        // Additional genuine destinations exercise weather without patching
        // the clock, save, climate or effect state. Coverage may vary by seed.
        unsigned type=visit<4?12+visit:visit<8?14:12;
        if (visit>=4 && (weatherMask&(1u<<(type==14?4:3)))) continue;
        press(8);
        for (unsigned i=0;core->busRead8(core,cursor)!=3 && i<8;i++) press(128);
        assert(core->busRead8(core,cursor)==3);press(1);
        assert(core->busRead32(core,main+4)==(geo|1));
        for (unsigned i=0;core->busRead8(core,choice)!=type && i<26;i++) press(128);
        assert(core->busRead8(core,choice)==type);
        if (type==12) shot("build/atlas-emulator/geo-biome-menu.ppm");
        press(1);
        unsigned waited=0;
        while (core->busRead32(core,main+4)==(geo|1) && core->busRead8(core,status)<2 && waited++<6000) frames(0,1);
        if(waited>=6000)fprintf(stderr,"GEO timeout type=%u phase=%u visited=%u trial=%u node=%u\n",type,core->busRead8(core,symbol("sPhase")),core->busRead16(core,symbol("sVisited")),core->busRead16(core,symbol("sTrial")),core->busRead16(core,symbol("sNode")));
        assert(waited<6000);
        if (core->busRead8(core,status)==2)
        {
            printf("NOTE biome TP type %u has no safe destination in this seed\n",type);
            frames(0,30); // status is published before the failure pane finishes drawing
            press(2);settle();continue;
        }
        assert(core->busRead8(core,status)==1 && core->busRead8(core,result+12)==type);
        settle();frames(0,120);actors();unsigned weather=fieldWeather();
        if (weather!=2)
        {
            char path[80];snprintf(path,sizeof(path),"build/atlas-emulator/field-weather-%u.ppm",weather);shot(path);
        }
        if (type==13 || type==14)
        {
            char label[21];unsigned n;
            uint32_t text=symbol("sAtlasLocationPopup");
            for (n=0;n<20;n++)
            {
                unsigned c=core->busRead8(core,text+n);if (c==255) break;
                label[n]=c>=0xbb && c<=0xd4?'A'+c-0xbb:c>=0xd5 && c<=0xee?'a'+c-0xd5:' ';
            }
            label[n]=0;
            assert(strcmp(label,"Forest")); // never drain the previous biome's pending label
            if (type==14) assert(strcmp(label,"Desert"));
        }
        uint32_t save=core->busRead32(core,savePointer),layout=symbol("gBackupMapLayout");
        uint32_t map=core->busRead32(core,layout+8);
        int x=(int16_t)core->busRead16(core,save)+7,y=(int16_t)core->busRead16(core,save+2)+7;
        for (int dy=-1;dy<=1;dy++) for (int dx=-1;dx<=1;dx++)
            assert(!(core->busRead16(core,map+((y+dy)*35+x+dx)*2)&0xC00));
        char path[80];snprintf(path,sizeof(path),"build/atlas-emulator/geo-biome-%u.ppm",type);shot(path);
        printf("PASS native biome teleport type %u, third menu page and safe 3x3 landing\n",type);
        if (weather!=2 && !mapChecked) {weatherMapRoundTrip();mapChecked=1;}
    }
    printf("Weather effects observed (native IDs): mask 0x%x; rain=3, snow=4, fog=6\n",weatherMask);
    if (!(weatherMask&(1u<<4))) printf("NOTE no natural snow front encountered during this seed's bounded cold-region tour\n");
}
static void shoreTour(uint32_t savePointer)
{
    uint32_t main=symbol("gMain"),geo=symbol("GeoMenu_Run"),choice=symbol("sGeoChoice");
    uint32_t cursor=symbol("sStartMenuCursorPos"),status=symbol("gAtlasGeoStatus");
    static const unsigned targets[]={2,3,4,16,17,1,7,8,9,18,18,19,5,20,11,21,22,23,24,25};
    for (unsigned visit=0;visit<sizeof(targets)/sizeof(targets[0]);visit++)
    {
        unsigned type=targets[visit],isShore=(type>=2 && type<=4) || type==20 || type==11 || type==22;
        press(8);
        for (unsigned i=0;core->busRead8(core,cursor)!=3 && i<8;i++) press(128);
        assert(core->busRead8(core,cursor)==3);press(1);
        assert(core->busRead32(core,main+4)==(geo|1));
        for (unsigned i=0;core->busRead8(core,choice)!=type && i<26;i++) press(128);
        assert(core->busRead8(core,choice)==type);press(1);
        unsigned waited=0;
        while (core->busRead32(core,main+4)==(geo|1) && core->busRead8(core,status)<2 && waited++<6000) frames(0,1);
        if(waited>=6000)fprintf(stderr,"GEO timeout type=%u phase=%u visited=%u trial=%u node=%u\n",type,core->busRead8(core,symbol("sPhase")),core->busRead16(core,symbol("sVisited")),core->busRead16(core,symbol("sTrial")),core->busRead16(core,symbol("sNode")));
        assert(waited<6000);
        if (core->busRead8(core,status)==2) {printf("NOTE inspection type %u found no safe destination within this search\n",type);frames(0,30);press(2);settle();continue;}
        assert(core->busRead8(core,status)==1);settle();frames(0,60);
        unsigned reached=!isShore;
        for (unsigned step=0;step<40 && isShore;step++)
        {
            unsigned key=nextMapKey(core->busRead32(core,savePointer),type==4?2:1);
            if (key==1024) {reached=1;break;}
            if (!key) break;
            frames(key,8);frames(0,20);actors();
        }
        frames(0,300);actors();fieldWeather();
        if (reached)
        {
            if (type==17)
            {
                uint32_t map=core->busRead32(core,symbol("gBackupMapLayout")+8);
                unsigned trees=0;
                for (unsigned at=0;at<1190;at++)
                {
                    unsigned tile=core->busRead16(core,map+at*2),id=tile&1023;
                    if (id<915 || id>=924) continue;
                    trees++;assert(tile&0x400);
                    int ax=(int)(at%35)-(int)((id-915)%3),ay=(int)(at/35)-(int)((id-915)/3);
                    if (ax<0 || ay<0 || ax+2>=35 || ay+2>=34) continue;
                    for (unsigned part=0;part<9;part++)
                        assert((core->busRead16(core,map+((ay+part/3)*35+ax+part%3)*2)&1023)==915+part);
                }
                assert(trees>=9);printf("PASS native visible complete forest groves: %u blocked tree tiles\n",trees);
            }
            else if (isShore || type==16)
            {
                unsigned expected=type==22?19:type==16?13:type==2?2:(type==3 || type==20 || type==11)?1:3;
                assert(core->busRead8(core,symbol("sAtlasPlaceTracker")+4)==expected);
                if(type==11)
                {
                    unsigned node=core->busRead16(core,symbol("gAtlasGeoResult")+10);
                    unsigned runoff=core->busRead16(core,symbol("gHeap")+60672+node*12+2);
                    assert(runoff>=240 && runoff<600);
                    printf("PASS native connected small-river bank: node %u runoff %u and safe landing\n",node,runoff);
                }
                if(type==22)
                {
                    uint32_t buckets=symbol("sBuckets"),pieces=symbol("sPieceBank");unsigned creeks=0;
                    for(unsigned b=0;b<4;b++)for(unsigned p=0;p<core->busRead8(core,buckets+b*64+30);p++)
                        creeks+=!!(core->busRead16(core,pieces+(b*128+p)*16+14)&0x8000);
                    assert(creeks);printf("PASS native validated local tributary creek: %u resident creek pieces, Creekbank name and safe landing\n",creeks);
                }
                if(type==20)
                {
                    uint32_t buckets=symbol("sBuckets"),pieces=symbol("sPieceBank");unsigned tapers=0;
                    for(unsigned b=0;b<4;b++)for(unsigned p=0;p<core->busRead8(core,buckets+b*64+30);p++)
                    {
                        unsigned tip=core->busRead16(core,pieces+(b*128+p)*16+14);
                        tapers+=!(tip&0x8000) && tip>core->busRead16(core,pieces+(b*128+p)*16+12);
                    }
                    assert(tapers);printf("PASS native flared river outlet: %u resident taper pieces and safe flared river bank landing\n",tapers);
                }
            }
            else
            {
                uint32_t map=core->busRead32(core,symbol("gBackupMapLayout")+8);
                uint32_t save=core->busRead32(core,savePointer);
                int px=(int16_t)core->busRead16(core,save)+7,py=(int16_t)core->busRead16(core,save+2)+7;
                unsigned walls=0,edges=0,water=0,dunes=0,rockEdges=0,snowEdges=0;
                for (int dy=-4;dy<=4;dy++) for (int dx=-6;dx<=6;dx++)
                {
                    unsigned tile=core->busRead16(core,map+((py+dy)*35+px+dx)*2),id=tile&1023;
                    dunes+=id>=435 && id<482;
                    rockEdges+=id>=482 && id<497;snowEdges+=id>=497 && id<512;
                    walls+=(id>=924 && id<1018) || (id>=384 && id<431);
                    edges+=id==887 || (id>=901 && id<915);
                    water+=id>=512 && id<887;
                    if (dx>=-1 && dx<=1 && dy>=-1 && dy<=1) assert(!(tile&0xC00));
                }
                if(type==18){assert(edges);printf("PASS native sand/grass boundary: %u visible sand borders and safe 3x3 landing\n",edges);}
                else if(type==5){
                    unsigned ground=core->busRead16(core,map+(py*35+px)*2)&1023;
                    assert((ground>=431 && ground<435) || (ground>=896 && ground<900) || ground==1019 || ground==1021);
                    printf("PASS native volcanic ground material role %u\n",ground);
                    assert(walls);assert(core->busRead8(core,symbol("sAtlasPlaceTracker")+4)==16);printf("PASS native dormant volcanic cone: %u cliff tiles, physical name and safe radial gap landing\n",walls);}
                else if(type==19)
                {
                    unsigned component=core->busRead16(core,symbol("gAtlasGeoResult")+8),cells=0;
                    uint32_t plan=symbol("gHeap")+60672;
                    for(unsigned i=0;i<4608;i++)cells+=core->busRead16(core,plan+i*12+4)==component;
                    assert(cells && cells<32);printf("PASS native island inspection: %u-cell landmass and safe 3x3 landing\n",cells);
                }
                else if(type==24 || type==25){assert(type==24?rockEdges:snowEdges);printf("PASS native %s/grass boundary: %u visible edges, safe 3x3 patch and fifth inspector page\n",type==24?"rock":"snow",type==24?rockEdges:snowEdges);}
                else if(type==23){assert(dunes);assert(core->busRead8(core,symbol("sAtlasPlaceTracker")+4)==20);printf("PASS native sand dunes: %u walkable ridge faces, Dunes name and safe 3x3 patch\n",dunes);}
                else if(type==21){fprintf(stderr,"Floodplain name=%u at %d,%d seed=%u\n",core->busRead8(core,symbol("sAtlasPlaceTracker")+4),(int32_t)core->busRead32(core,symbol("gAtlasGeoResult")),(int32_t)core->busRead32(core,symbol("gAtlasGeoResult")+4),core->busRead32(core,symbol("gAtlasWorldSeed")));assert(core->busRead8(core,symbol("sAtlasPlaceTracker")+4)==18);printf("PASS native named floodplain and safe dry soil landing\n");}
                else if(type==8){assert(water);printf("PASS native valley inspection: %u visible water tiles and safe carved bank landing\n",water);}
                else{assert(walls);printf("PASS native terrain inspection type %u: %u visible cliff tiles and safe 3x3 landing\n",type,walls);}
            }
            printf("PASS native physical location for inspection type %u (plateau or dry shore)\n",type);
        }
        else printf("NOTE shore TP type %u has no reachable bank in the test viewport\n",type);
        if(type==5)
        {
            uint32_t popup=symbol("sAtlasLocationPopup");unsigned named=0;
            for(unsigned tick=0;tick<900 && !named;tick++)
            {
                char name[21];unsigned n;
                for(n=0;n<20;n++)
                {
                    unsigned c=core->busRead8(core,popup+n);if(c==255)break;
                    name[n]=c>=0xbb && c<=0xd4?'A'+c-0xbb:c>=0xd5 && c<=0xee?'a'+c-0xd5:' ';
                }
                name[n]=0;named=!strcmp(name,"Volcanic Cone");
                if(!named)frames(0,1);
            }
            assert(named);
        }
        char path[80];snprintf(path,sizeof(path),"build/atlas-emulator/shore-%u.ppm",type);shot(path);
        if(type==5 || type==20 || type==21 || type==23)
        {
            unsigned char before[1456],after[1456];
            press(8);
            for(unsigned i=0;core->busRead8(core,cursor)!=0 && i<8;i++)press(128);
            press(1);frames(0,60);
            assert(core->busRead8(core,symbol("gAtlasMapActive")));
            uint32_t journal=core->busRead32(core,symbol("gSaveBlock3Ptr"));
            bytes(journal,before,sizeof(before));
            if(!core->busRead8(core,symbol("sAdminView")))press(4);
            unsigned targetZoom=type==5?4:3,targetLayer=type==5?1:type==21?4:type==23?8:0;
            for(unsigned i=0;core->busRead8(core,symbol("sAtlasMapZoom"))!=targetZoom && i<8;i++)press(core->busRead8(core,symbol("sAtlasMapZoom"))<targetZoom?256:512);
            for(unsigned i=0;core->busRead8(core,symbol("sAtlasMapLayer"))!=targetLayer && i<9;i++)press(8);
            unsigned painted=0;
            while(core->busRead8(core,symbol("sAtlasMapRow"))<25 && painted++<1800)frames(0,1);
            assert(core->busRead8(core,symbol("sAtlasMapRow"))==25);
            shot(type==5?"build/atlas-emulator/volcano-elevation.ppm":type==21?"build/atlas-emulator/floodplain-moisture.ppm":type==23?"build/atlas-emulator/dune-relief.ppm":"build/atlas-emulator/estuary-geography.ppm");
            bytes(journal,after,sizeof(after));assert(!memcmp(before,after,sizeof(before)));
            press(2);frames(0,180);press(2);frames(0,60);
            printf("PASS native local feature %u atlas completes in %u frames and leaves journal unchanged\n",type,painted);
            cachedCrossing(savePointer);
        }
        if (type==17 || type==18 || type==11 || type==22) cachedCrossing(savePointer);
        if(type==24 || type==25)cachedCrossing(savePointer);
        if(type==22 || type==23 || type==25)featureColdContinue(type);
    }
}
static void cancelGeoSearch(uint32_t savePointer)
{
    uint32_t main=symbol("gMain"),geo=symbol("GeoMenu_Run"),cursor=symbol("sStartMenuCursorPos");
    uint32_t choice=symbol("sGeoChoice"),save=core->busRead32(core,savePointer);
    unsigned x=core->busRead16(core,save),y=core->busRead16(core,save+2);
    press(8);
    for (unsigned i=0;core->busRead8(core,cursor)!=3 && i<8;i++) press(128);
    assert(core->busRead8(core,cursor)==3);press(1);
    for (unsigned i=0;core->busRead8(core,choice)!=17 && i<26;i++) press(128);
    assert(core->busRead8(core,choice)==17);press(1);
    if (core->busRead32(core,main+4)!=(geo|1)) {settle();printf("NOTE grove search completed before cancellation\n");return;}
    press(2);frames(0,30);
    if (core->busRead32(core,main+4)==(geo|1)) press(2);
    settle();save=core->busRead32(core,savePointer);
    assert(core->busRead16(core,save)==x && core->busRead16(core,save+2)==y);
    actors();printf("PASS native geographic search cancel returns to unlocked field without teleporting\n");
}
static void featureColdContinue(unsigned type)
{
    const char *feature=type==22?"creek":type==23?"dune":"snow";
    char label[80],path[120];
    unsigned char before[1456],after[1456];uint16_t tiles[117];
    uint32_t savePointer=symbol("gSaveBlock1Ptr"),journalPointer=symbol("gSaveBlock3Ptr");
    saveGame();
    uint32_t save=core->busRead32(core,savePointer),journal=core->busRead32(core,journalPointer);
    uint16_t x=core->busRead16(core,save),y=core->busRead16(core,save+2);
    uint32_t map=core->busRead32(core,symbol("gBackupMapLayout")+8);
    for(int dy=-4;dy<=4;dy++)for(int dx=-6;dx<=6;dx++)
        tiles[(dy+4)*13+dx+6]=core->busRead16(core,map+(((int)y+7+dy)*35+(int)x+7+dx)*2);
    bytes(journal,before,sizeof(before));
    core->reset(core);unsigned began=0,frame;
    memset(generationProfile,0,sizeof(generationProfile));profilingGeneration=1;
    for(frame=0;frame<18000;frame++)
    {
        unsigned keys=0;if(frame<1200 && frame%60<2)keys=(frame%120<60)?8:1;
        frames(keys,1);
        if(!began && core->busRead8(core,creationStateAddr)==1)began=elapsedFrames;
        if(frame>120 && core->busRead8(core,creationStateAddr)==3)break;
    }
    assert(frame<18000 && began);settle();
    unsigned ready=elapsedFrames-began;profilingGeneration=0;
    snprintf(label,sizeof(label),"%s Continue",feature);printGenerationProfile(label);
    if(ready>=1791)fprintf(stderr,"%s Continue exceeded budget: %u frames %.3fs\n",feature,ready,ready/59.72750057);
    assert(ready<1791);
    save=core->busRead32(core,savePointer);journal=core->busRead32(core,journalPointer);
    assert(core->busRead16(core,save)==x && core->busRead16(core,save+2)==y);
    bytes(journal,after,sizeof(after));assert(!memcmp(before,after,8));
    assert(!memcmp(before+12,after+12,2) && !memcmp(before+15,after+15,sizeof(before)-15));
    map=core->busRead32(core,symbol("gBackupMapLayout")+8);
    for(int dy=-4;dy<=4;dy++)for(int dx=-6;dx<=6;dx++)
        assert(tiles[(dy+4)*13+dx+6]==core->busRead16(core,map+(((int)y+7+dy)*35+(int)x+7+dx)*2));
    uint32_t buckets=symbol("sBuckets"),pieces=symbol("sPieceBank");unsigned creeks=0;
    for(unsigned b=0;b<4;b++)for(unsigned p=0;p<core->busRead8(core,buckets+b*64+30);p++)
        creeks+=!!(core->busRead16(core,pieces+(b*128+p)*16+14)&0x8000);
    if(type==22)assert(creeks);
    actors();snprintf(path,sizeof(path),"build/atlas-emulator/%s-continue.ppm",feature);shot(path);
    snprintf(path,sizeof(path),"build/atlas-emulator/%s-continue-timing.json",feature);
    FILE *file=fopen(path,"w");assert(file);
    fprintf(file,"{\"frames\":%u,\"seconds\":%.4f,\"phone_measured\":false}\n",ready,ready/59.72750057);fclose(file);
    printf("PASS native cold %s-area Continue: %.3fs, identical visible terrain/position/seed/discovery\n",feature,ready/59.72750057);
}
int main(int argc,char **argv)
{
    unsigned frame;
    uint32_t stage,sav1,sav3,statePtr,savePtr,mapActive;
    unsigned char before[1456],after[1456];
    assert(argc==3 || argc==4);setbuf(stdout,NULL);mLogSetDefaultLogger(&logger);symbolFile=argv[2];
    stage=symbol("gAtlasCreationState");sav1=symbol("gSaveBlock1Ptr");sav3=symbol("gSaveBlock3Ptr");mapActive=symbol("gAtlasMapActive");
    creationStateAddr=stage;planStageAddr=symbol("sStage");profilingGeneration=1;
    rosterAddr=symbol("sAtlasActorRoster");objectsAddr=symbol("gObjectEvents");spritesAddr=symbol("gSprites");
    core=mCoreFind(argv[1]);assert(core&&core->init(core));mCoreInitConfig(core,NULL);
    core->setVideoBuffer(core,pixels,240);assert(mCoreLoadFile(core,argv[1]));assert(mCoreAutoloadSave(core));core->reset(core);
    unsigned generating=0;
    for(frame=0;frame<18000;frame++)
    {
        unsigned keys=0;
        if(frame<1200 && frame%60<2)keys=(frame%120<60)?8:1;
        frames(keys,1);
        if (!generating && core->busRead8(core,stage)==1) generating=elapsedFrames;
        if(core->busRead8(core,stage)==3)break;
        if(frame%1200==0)printf("frame %u creation stage %u\n",frame,core->busRead8(core,stage));
    }
    shot("build/atlas-emulator/startup.ppm");
    assert(frame<18000 && generating);settle();
    unsigned newGameFrames=elapsedFrames-generating;
    profilingGeneration=0;printGenerationProfile("New Game");
    assert(newGameFrames<7167); // two minutes at GBA speed, including field readiness
    frames(0,60);
    savePtr=core->busRead32(core,sav1);statePtr=core->busRead32(core,sav3);
    assert(savePtr>=0x02000000&&savePtr<0x02040000);
    assert(core->busRead32(core,statePtr)==0x41544C53);
    assert(core->busRead8(core,statePtr+15)>0);
    shot("build/atlas-emulator/world.ppm");
    printf("PASS native world entry at frame %u, position %d,%d, visited chunks %u\n",frame,(int16_t)core->busRead16(core,savePtr),(int16_t)core->busRead16(core,savePtr+2),core->busRead8(core,statePtr+15));
    if (argc==4)
    {
        FILE *f=fopen(argv[3],"wb");assert(f);
        bytes(statePtr,before,sizeof(before));assert(fwrite(before,1,sizeof(before),f)==sizeof(before));fclose(f);
        printf("SNAPSHOT schema=%u seed=%08x position=%u,%u\n",core->busRead16(core,statePtr+12),core->busRead32(core,statePtr+4),core->busRead16(core,savePtr),core->busRead16(core,savePtr+2));
        printf("SNAPSHOT generation=%u ready_seconds=%.4f\n",core->busRead16(core,symbol("sAtlasGenerationVersion")),newGameFrames/59.72750057);
        uint32_t stats=symbol("gAtlasPlanStats");
        printf("CHECKSUM %08x HABITAT %08x\n",core->busRead32(core,stats+4),core->busRead32(core,stats+24));
        core->deinit(core);return 0;
    }
    assert(core->busRead16(core,symbol("sAtlasGenerationVersion"))==19);
    assert(core->busRead16(core,symbol("gAtlasPlanStats")+8)==1345);
    printf("PASS native generation 19 macro atlas: 1345/4608 land cells (29.19 percent)\n");
    fieldWeather();
    placeNames();
    nativeAnimations();
    struct ActorView initial=actors();
    for (unsigned i=0;i<600&&!initial.mask;i++) {frames(0,1);initial=actors();}
    // Some seeds legitimately contain no eligible candidate in this viewport.
    // Validate movement when populated, without forcing wildlife into the world.
    unsigned moved=0;
    for (unsigned tick=0;tick<1800;tick++)
    {
        frames(0,1);
        struct ActorView current=actors();
        for (unsigned i=0;i<3;i++)
            if ((initial.mask&current.mask&(1u<<i)) && initial.generation[i]==current.generation[i]
             && (initial.x[i]!=current.x[i] || initial.y[i]!=current.y[i])) moved=1;
    }
    if (initial.mask) assert(moved);
    else printf("NOTE no eligible wildlife at this seed's spawn; movement coverage not exercised\n");
    shot("build/atlas-emulator/wildlife.ppm");
    struct ActorView beforeMap=actors();
    if (initial.mask) printf("PASS native species sprites wander with valid actor/object/sprite bindings (mask %u)\n",beforeMap.mask);
    press(8); // START
    bytes(statePtr,before,sizeof(before));frames(0,120);
    bytes(statePtr,after,sizeof(after));assert(!memcmp(before,after,sizeof(before)));
    printf("PASS start menu pauses the Atlas clock and journal\n");
    press(1);frames(0,90); // WORLD MAP
    assert(core->busRead8(core,mapActive));
    bytes(statePtr,before,sizeof(before));shot("build/atlas-emulator/player-map.ppm");
    press(4);frames(0,180);shot("build/atlas-emulator/admin-map.ppm"); // SELECT admin
    for (unsigned zoom=0;zoom<7;zoom++) {press(256);assert(core->busRead8(core,symbol("sAtlasMapZoom"))==zoom+1);} // whole longitude span
    frames(0,420); // finish coarse raster and resumable macro-river overlay
    assert(core->busRead16(core,symbol("sAtlasMapRiver"))==4608);
    shot("build/atlas-emulator/atlas-world-rivers.ppm");
    for (unsigned layer=1;layer<9;layer++)
    {
        press(8);frames(0,240);
        if (layer==7)
        {
            unsigned row=core->busRead8(core,symbol("sAtlasMapRow"));
            press(16); // short pan must be accepted while regional rendering runs
            assert(core->busRead8(core,symbol("sAtlasMapRow"))<row);
            unsigned waited=0;
            while (core->busRead8(core,symbol("sAtlasMapRow"))<25 && waited++<1200) frames(0,1);
            printf("Regional relief raster completed after %u additional frames; short pan accepted\n",waited);
        }
        assert(core->busRead8(core,symbol("sAtlasMapLayer"))==layer);
        assert(core->busRead8(core,symbol("sAtlasMapRow"))==25);
        char path[80];snprintf(path,sizeof(path),"build/atlas-emulator/admin-layer-%u.ppm",layer);shot(path);
    }
    bytes(statePtr,after,sizeof(after));assert(!memcmp(before,after,sizeof(before)));
    printf("PASS map/admin/zoom reads leave full journal and clock unchanged\n");
    press(2);assert(!core->busRead8(core,mapActive));frames(0,180);press(2);frames(0,60);
    struct ActorView afterMap=actors();
    for (unsigned i=0;i<3;i++)
        if (beforeMap.mask&(1u<<i)) assert((afterMap.mask&(1u<<i)) && afterMap.generation[i]==beforeMap.generation[i]);
    printf("PASS map return, with wildlife generation checks for mask %u\n",beforeMap.mask);
    frames(16,40);frames(0,30);actors();shot("build/atlas-emulator/movement.ppm");
    cachedCrossing(sav1);
    // Follow the real confirmation states, including an existing-file prompt.
    saveGame();
    shot("build/atlas-emulator/save.ppm");
    savePtr=core->busRead32(core,sav1);statePtr=core->busRead32(core,sav3);
    uint16_t x=core->busRead16(core,savePtr),y=core->busRead16(core,savePtr+2);
    bytes(statePtr,before,sizeof(before));
    core->reset(core);generating=0;memset(generationProfile,0,sizeof(generationProfile));profilingGeneration=1;
    for(frame=0;frame<18000;frame++)
    {
        unsigned keys=0;
        if(frame<1200&&frame%60<2)keys=(frame%120<60)?8:1;
        frames(keys,1);
        if (!generating && core->busRead8(core,stage)==1) generating=elapsedFrames;
        if(frame>120&&core->busRead8(core,stage)==3)break;
    }
    assert(frame<18000 && generating);settle();
    unsigned continueFrames=elapsedFrames-generating;
    profilingGeneration=0;printGenerationProfile("Continue");
    if(continueFrames>=1791)fprintf(stderr,"Continue exceeded budget: %u frames (%.3fs)\n",continueFrames,continueFrames/59.72750057);
    assert(continueFrames<1791); // thirty seconds from entering Atlas restoration
    frames(0,60);actors();shot("build/atlas-emulator/continue.ppm");
    fieldWeather();
    savePtr=core->busRead32(core,sav1);statePtr=core->busRead32(core,sav3);
    bytes(statePtr,after,sizeof(after));
    printf("restored position %d,%d; saved %d,%d; seeds %08x/%08x\n",core->busRead16(core,savePtr),core->busRead16(core,savePtr+2),x,y,*(uint32_t*)(before+4),*(uint32_t*)(after+4));
    assert(core->busRead16(core,savePtr)==x && core->busRead16(core,savePtr+2)==y);
    assert(!memcmp(before+12,after+12,2)); // schema
    assert(!memcmp(before+15,after+15,sizeof(before)-15)); // discovery, not running clock
    assert(!memcmp(before,after,8)); // seed identity
    printf("PASS native save/reset/Continue preserves seed, position and discovery\n");
    FILE *timing=fopen("build/atlas-emulator/timing.json","w");assert(timing);
    fprintf(timing,"{\"new_game_frames\":%u,\"new_game_seconds\":%.4f,\"continue_frames\":%u,\"continue_seconds\":%.4f,\"phone_measured\":false,\"timing_origin\":\"Atlas generation screen entry\"}\n",newGameFrames,newGameFrames/59.72750057,continueFrames,continueFrames/59.72750057);fclose(timing);
    printf("PASS ready-to-walk timing: New Game %.3f s; Continue %.3f s at full GBA speed (phones unmeasured)\n",newGameFrames/59.72750057,continueFrames/59.72750057);
    biomeTour(sav1);
    cancelGeoSearch(sav1);
    shoreTour(sav1);
    core->deinit(core);return 0;
}

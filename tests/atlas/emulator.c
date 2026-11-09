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
static void frames(unsigned keys,unsigned count) {core->setKeys(core,keys);while(count--) {core->runFrame(core);elapsedFrames++;}}
static unsigned settle(void)
{
    unsigned start=elapsedFrames;
    uint32_t main=symbol("gMain"),cb1=symbol("CB1_Overworld"),cb2=symbol("CB2_Overworld");
    uint32_t fade=symbol("gPaletteFade"),lock=symbol("sLockFieldControls");
    /* Expansion ARM PaletteFadeControl.active is bit 31 at offset 12. */
    while ((core->busRead32(core,main)!=(cb1|1) || core->busRead32(core,main+4)!=(cb2|1)
         || (core->busRead32(core,fade+12)&0x80000000u) || core->busRead8(core,lock))
         && elapsedFrames-start<600) frames(0,1);
    assert(elapsedFrames-start<600);
    return elapsedFrames-start;
}
static void press(unsigned keys) {frames(keys,2);frames(0,30);}
static void shot(const char *path)
{
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n240 160\n255\n");
    for(unsigned i=0;i<240*160;i++) {unsigned char rgb[3]={pixels[i]&255,(pixels[i]>>8)&255,(pixels[i]>>16)&255};fwrite(rgb,1,3,f);} fclose(f);
}
static void bytes(uint32_t addr,void *out,unsigned size) {for(unsigned i=0;i<size;i++)((uint8_t*)out)[i]=core->busRead8(core,addr+i);}
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
static unsigned nextExitKey(uint32_t save)
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
        if (at!=start && (cx<7 || cx>=27 || cy<7 || cy>=27))
        {
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
        unsigned key=nextExitKey(core->busRead32(core,savePtr));
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
static void biomeTour(uint32_t savePointer)
{
    uint32_t main=symbol("gMain"),geo=symbol("GeoMenu_Run"),choice=symbol("sGeoChoice");
    uint32_t cursor=symbol("sStartMenuCursorPos"),status=symbol("gAtlasGeoStatus"),result=symbol("gAtlasGeoResult");
    for (unsigned type=12;type<16;type++)
    {
        press(8);
        for (unsigned i=0;core->busRead8(core,cursor)!=3 && i<8;i++) press(128);
        assert(core->busRead8(core,cursor)==3);press(1);
        assert(core->busRead32(core,main+4)==(geo|1));
        for (unsigned i=0;core->busRead8(core,choice)!=type && i<16;i++) press(128);
        assert(core->busRead8(core,choice)==type);
        if (type==12) shot("build/atlas-emulator/geo-biome-menu.ppm");
        press(1);
        unsigned waited=0;
        while (core->busRead32(core,main+4)==(geo|1) && core->busRead8(core,status)<2 && waited++<6000) frames(0,1);
        assert(waited<6000);
        if (core->busRead8(core,status)==2)
        {
            printf("NOTE biome TP type %u has no safe destination in this seed\n",type);press(2);settle();continue;
        }
        assert(core->busRead8(core,status)==1 && core->busRead8(core,result+12)==type);
        settle();frames(0,60);actors();
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
    }
}
int main(int argc,char **argv)
{
    unsigned frame;
    uint32_t stage,sav1,sav3,statePtr,savePtr,mapActive;
    unsigned char before[1456],after[1456];
    assert(argc==3 || argc==4);setbuf(stdout,NULL);mLogSetDefaultLogger(&logger);symbolFile=argv[2];
    stage=symbol("gAtlasCreationState");sav1=symbol("gSaveBlock1Ptr");sav3=symbol("gSaveBlock3Ptr");mapActive=symbol("gAtlasMapActive");
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
        uint32_t stats=symbol("gAtlasPlanStats");
        printf("CHECKSUM %08x HABITAT %08x\n",core->busRead32(core,stats+4),core->busRead32(core,stats+24));
        core->deinit(core);return 0;
    }
    assert(core->busRead16(core,symbol("sAtlasGenerationVersion"))==4);
    assert(core->busRead16(core,symbol("gAtlasPlanStats")+8)==1345);
    printf("PASS native generation 4 macro atlas: 1345/4608 land cells (29.19 percent)\n");
    placeNames();
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
    frames(0,180);
    for (unsigned layer=1;layer<7;layer++)
    {
        press(8);frames(0,240);
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
    // Save is second Atlas menu entry. Confirm overwrite and save prompts.
    press(8);press(128);press(1);frames(0,60);press(1);frames(0,180);press(1);frames(0,900);
    shot("build/atlas-emulator/save.ppm");
    savePtr=core->busRead32(core,sav1);statePtr=core->busRead32(core,sav3);
    uint16_t x=core->busRead16(core,savePtr),y=core->busRead16(core,savePtr+2);
    bytes(statePtr,before,sizeof(before));
    core->reset(core);generating=0;
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
    assert(continueFrames<1791); // thirty seconds from entering Atlas restoration
    frames(0,60);actors();shot("build/atlas-emulator/continue.ppm");
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
    core->deinit(core);return 0;
}

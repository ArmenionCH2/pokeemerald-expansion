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
int main(int argc,char **argv)
{
    unsigned frame;
    uint32_t stage,sav1,sav3,statePtr,savePtr,mapActive;
    unsigned char before[1456],after[1456];
    assert(argc==3);setbuf(stdout,NULL);mLogSetDefaultLogger(&logger);symbolFile=argv[2];
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
    struct ActorView initial=actors();
    for (unsigned i=0;i<600&&!initial.mask;i++) {frames(0,1);initial=actors();}
    assert(initial.mask); // this reproducible key-driven world must have wildlife
    unsigned moved=0;
    for (unsigned tick=0;tick<1800;tick++)
    {
        frames(0,1);
        struct ActorView current=actors();
        for (unsigned i=0;i<3;i++)
            if ((initial.mask&current.mask&(1u<<i)) && initial.generation[i]==current.generation[i]
             && (initial.x[i]!=current.x[i] || initial.y[i]!=current.y[i])) moved=1;
    }
    assert(moved);shot("build/atlas-emulator/wildlife.ppm");
    struct ActorView beforeMap=actors();
    printf("PASS native species sprites wander with valid actor/object/sprite bindings (mask %u)\n",beforeMap.mask);
    press(8); // START
    bytes(statePtr,before,sizeof(before));frames(0,120);
    bytes(statePtr,after,sizeof(after));assert(!memcmp(before,after,sizeof(before)));
    printf("PASS start menu pauses the Atlas clock and journal\n");
    press(1);frames(0,90); // WORLD MAP
    assert(core->busRead8(core,mapActive));
    bytes(statePtr,before,sizeof(before));shot("build/atlas-emulator/player-map.ppm");
    press(4);frames(0,180);shot("build/atlas-emulator/admin-map.ppm"); // SELECT admin
    press(256);frames(0,180); // R zoom out
    bytes(statePtr,after,sizeof(after));assert(!memcmp(before,after,sizeof(before)));
    printf("PASS map/admin/zoom reads leave full journal and clock unchanged\n");
    press(2);assert(!core->busRead8(core,mapActive));frames(0,180);press(2);frames(0,60);
    struct ActorView afterMap=actors();
    for (unsigned i=0;i<3;i++)
        if (beforeMap.mask&(1u<<i)) assert((afterMap.mask&(1u<<i)) && afterMap.generation[i]==beforeMap.generation[i]);
    printf("PASS map return preserves live wildlife generations and restores owned sprites\n");
    frames(16,40);frames(0,30);actors();shot("build/atlas-emulator/movement.ppm");
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
    core->deinit(core);return 0;
}

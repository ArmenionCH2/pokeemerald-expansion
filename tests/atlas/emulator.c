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
static void quiet(struct mLogger *l,int cat,enum mLogLevel level,const char *fmt,va_list args) {(void)l;(void)cat;(void)level;(void)fmt;(void)args;}
static struct mLogger logger={quiet,0};
static void frames(unsigned keys,unsigned count) {core->setKeys(core,keys);while(count--)core->runFrame(core);}
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
    assert(argc==6);setbuf(stdout,NULL);mLogSetDefaultLogger(&logger);
    stage=strtoul(argv[2],NULL,16);sav1=strtoul(argv[3],NULL,16);sav3=strtoul(argv[4],NULL,16);mapActive=strtoul(argv[5],NULL,16);
    core=mCoreFind(argv[1]);assert(core&&core->init(core));mCoreInitConfig(core,NULL);
    core->setVideoBuffer(core,pixels,240);assert(mCoreLoadFile(core,argv[1]));assert(mCoreAutoloadSave(core));core->reset(core);
    for(frame=0;frame<18000;frame++)
    {
        unsigned keys=0;
        if(frame<1200 && frame%60<2)keys=(frame%120<60)?8:1;
        frames(keys,1);
        if(core->busRead8(core,stage)==3)break;
        if(frame%1200==0)printf("frame %u creation stage %u\n",frame,core->busRead8(core,stage));
    }
    shot("build/atlas-emulator/startup.ppm");
    assert(frame<18000);frames(0,180);
    savePtr=core->busRead32(core,sav1);statePtr=core->busRead32(core,sav3);
    assert(savePtr>=0x02000000&&savePtr<0x02040000);
    assert(core->busRead32(core,statePtr)==0x41544C53);
    assert(core->busRead8(core,statePtr+15)>0);
    shot("build/atlas-emulator/world.ppm");
    printf("PASS native world entry at frame %u, position %d,%d, visited chunks %u\n",frame,(int16_t)core->busRead16(core,savePtr),(int16_t)core->busRead16(core,savePtr+2),core->busRead8(core,statePtr+15));
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
    press(2);assert(!core->busRead8(core,mapActive));frames(0,180);press(2);frames(0,60);frames(16,40);frames(0,30);shot("build/atlas-emulator/movement.ppm");
    // Save is second Atlas menu entry. Confirm overwrite and save prompts.
    press(8);press(128);press(1);frames(0,60);press(1);frames(0,180);press(1);frames(0,900);
    shot("build/atlas-emulator/save.ppm");
    savePtr=core->busRead32(core,sav1);statePtr=core->busRead32(core,sav3);
    uint16_t x=core->busRead16(core,savePtr),y=core->busRead16(core,savePtr+2);
    bytes(statePtr,before,sizeof(before));
    core->reset(core);
    for(frame=0;frame<18000;frame++)
    {
        unsigned keys=0;
        if(frame<1200&&frame%60<2)keys=(frame%120<60)?8:1;
        frames(keys,1);
        if(frame>120&&core->busRead8(core,stage)==3)break;
    }
    frames(0,120);shot("build/atlas-emulator/continue.ppm");
    assert(frame<18000);
    savePtr=core->busRead32(core,sav1);statePtr=core->busRead32(core,sav3);
    bytes(statePtr,after,sizeof(after));
    printf("restored position %d,%d; saved %d,%d; seeds %08x/%08x\n",core->busRead16(core,savePtr),core->busRead16(core,savePtr+2),x,y,*(uint32_t*)(before+4),*(uint32_t*)(after+4));
    assert(core->busRead16(core,savePtr)==x && core->busRead16(core,savePtr+2)==y);
    assert(!memcmp(before+12,after+12,2)); // schema
    assert(!memcmp(before+15,after+15,sizeof(before)-15)); // discovery, not running clock
    assert(!memcmp(before,after,8)); // seed identity
    printf("PASS native save/reset/Continue preserves seed, position and discovery\n");
    core->deinit(core);return 0;
}

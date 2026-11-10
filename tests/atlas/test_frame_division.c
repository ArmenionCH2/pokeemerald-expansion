#include "../../src/atlas_plan.c"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    u32 random=0xdeadbeef;unsigned checked=0;
    for(u32 divisor=8;divisor<=400;divisor++)
    {
        u32 inverse=0xffffffffu/divisor;
        u32 edges[]={0,1,divisor-1,divisor,divisor+1,0x7fffffffu,0x80000000u,0xffffffffu};
        for(unsigned k=0;k<sizeof(edges)/sizeof(edges[0]);k++)assert(FrameDivide(edges[k],divisor,inverse)==edges[k]/divisor);
        for(unsigned k=0;k<2048;k++)
        {
            random=random*1664525u+1013904223u;
            assert(FrameDivide(random,divisor,inverse)==random/divisor);
            s32 signedValue=(s32)random;
            assert(FrameSignedDivide(signedValue,divisor,inverse)==signedValue/(s32)divisor);checked++;
        }
    }
    printf("PASS %u full-width signed/unsigned reciprocal quotients plus divisor boundary values match exact C division\n",checked);
}

#include "map_input.h"
#include <assert.h>
#include <stdio.h>

enum { A=1, B=2, L3=16, R3=32, UP=8192, DOWN=32768 };

static unsigned step(unsigned pad, unsigned physical, unsigned remap, int available, uint64_t time, unsigned expected) {
    uint64_t output=pad;
    int32_t lx=50, ly=0, rx=0, ry=40;
    runtime_map_input_physical(physical,remap,available);
    runtime_map_filter_input(pad,&output,&lx,&ly,&rx,&ry,time);
    assert(output==expected);
    return runtime_map_input_events();
}
int main(void) {
    assert(step(R3,R3,R3,1,0,R3)==0);
    assert(step(0,0,R3,1,100,0)==0);
    runtime_map_input_context(1,0);
    assert(step(R3,R3,R3,1,200,0)==0);
    assert(step(0,0,R3,1,200000000,0)==MAP_PAD_OPEN);
    runtime_map_input_context(1,1);
    assert(step(UP,UP,R3,1,210000000,0)==MAP_PAD_UP);
    assert(step(A,A,R3,1,220000000,0)==MAP_PAD_SELECT);
    assert(step(B,B,R3,1,230000000,0)==MAP_PAD_CLOSE);
    runtime_map_input_context(1,0);
    assert(step(B,B,R3,1,240000000,0)==0);
    assert(step(0,0,R3,1,250000000,0)==0);
    assert(step(A,A,R3,1,260000000,A)==0);
    assert(step(0,0,R3,1,270000000,0)==0);
    assert(step(A,R3,A,1,300000000,0)==0);
    assert(step(A,R3,A,1,1000000000,0)==MAP_PAD_LAYER);
    assert(step(A,R3,A,1,1200000000,0)==0);
    assert(step(0,0,A,1,1300000000,0)==0);
    assert(step(R3,R3,R3,1,1400000000,0)==0);
    assert(step(L3|R3,L3|R3,R3,1,1450000000,L3)==0);
    assert(step(0,0,R3,1,1500000000,0)==0);
    assert(step(DOWN,DOWN,R3,1,1600000000,DOWN)==0);
    assert(step(R3,R3,R3,1,1700000000,0)==0);
    assert(step(0,R3|L3,R3,0,1750000000,0)==0);
    assert(step(0,0,R3,0,1800000000,0)==0);
    assert(step(R3,R3,R3,1,1900000000,0)==0);
    runtime_map_input_context(0,0);
    assert(step(0,0,R3,1,1950000000,0)==0);
    puts("PASS: controller passthrough, short/long R3, remap, navigation, release and settings chord");
}

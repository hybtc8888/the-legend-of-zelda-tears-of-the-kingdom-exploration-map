#include "map_points.h"
#include "runtime.h"
#include <math.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t map_branch(uint64_t from,uint64_t to){
 return 0x14000000u|((uint32_t)(((int64_t)to-(int64_t)from)/4)&0x03ffffffu);
}

static _Atomic unsigned char hide_native_koroks;
static _Atomic uint32_t native_reveals;
static _Atomic unsigned installed_reveals;

unsigned runtime_map_set_native_reveals(unsigned requested) {
 unsigned active=requested&atomic_load_explicit(&installed_reveals,memory_order_acquire);
 atomic_store_explicit(&native_reveals,active,memory_order_release);
 return active;
}

void runtime_map_hide_native_koroks(int hide) {
 atomic_store_explicit(&hide_native_koroks,hide != 0,memory_order_release);
}

void runtime_map_install_korok_filter(Module *module) {
 const uint64_t tail=((module->file_size+15)&~15ULL)+1632;
 // This predicate is specific to collected Korok map icons. Return its normal
 // false result when the overlay owns Korok visibility.
 const uint64_t site=0x20fc7a4, skip=0x20fc724;
 const uint32_t original=0x39407688;
 const uint32_t guard[]={0x54fffc28,original,0x7100051f,0x540002c0,0x35fffba8};
 const uint32_t reject[]={0x2a1f03e0,0x6d42a3e9};
 uint32_t body[]={
  0xa9bf2be9,0x58000129,0x08dffd2a,0x3500008a, // save x9/x10; read host flag
  0xa8c12be9,original,0,                         // run the original predicate
  0xa8c12be9,0,0xd503201f,0,0                    // or jump to its false return
 };
 uint8_t *base=module->base;
 uint64_t limit=((module->file_size+0x3fff)&~0x3fffULL)-256;
 if(site+16>module->file_size||tail<module->patch_off||tail+sizeof body>limit||
    memcmp(base+site-4,guard,sizeof guard)||memcmp(base+skip,reject,sizeof reject))return;
 for(size_t i=0;i<sizeof body;i++)if(base[tail+i])return;
 body[6]=map_branch(tail+24,site+4);
 body[8]=map_branch(tail+32,skip);
 uint64_t pointer=(uintptr_t)&hide_native_koroks;
 memcpy(body+10,&pointer,8);
 memcpy(base+tail,body,sizeof body);
 uint32_t jump=map_branch(site,tail);
 memcpy(base+site,&jump,4);
}

typedef struct {
 uint32_t site, original, previous, next;
 uint32_t reject, accept;
 unsigned char mode, context, indirect;
} NativeMapReveal;

enum { native_slot_size=144, native_fairy_mode=9 };

static void install_fairy_predicate(Module *module,uint64_t slot) {
 // FairyFountainGlobalSetting's four HaveKnownFlag hashes. The shared
 // visibility predicate also handles treasure clues and the horse god.
 const uint32_t flags[]={0x95dfbc64,0xce69eee8,0xe8c15c64,0x603bb97b};
 uint32_t body[]={
  0xb9410269,                    /* ldr w9,[x19,#0x100] */
  0,0x6b0a013f,0x54000140,      /* ldr w10, flag; cmp; b.eq restore */
  0,0x6b0a013f,0x540000e0,
  0,0x6b0a013f,0x54000080,
  0,0x6b0a013f,0,               /* b.ne original rejection */
  0xa8c12be9,0                 /* restore x9/x10; original success */
 };
 const uint64_t start=slot+64, literals=start+sizeof body;
 for(unsigned i=0;i<sizeof flags/sizeof *flags;++i){
  unsigned load=1+i*3;
  body[load]=0x1800000au|((uint32_t)((literals+i*4-start-load*4)/4)<<5);
 }
 int64_t reject_words=((int64_t)(slot+44)-(int64_t)(start+48))/4;
 body[12]=0x54000001u|(((uint32_t)reject_words&0x7ffffu)<<5);
 body[14]=map_branch(start+56,slot+40);
 memcpy(module->base+start,body,sizeof body);
 memcpy(module->base+literals,flags,sizeof flags);
}

/* Preserve the original result for discovered markers. For an undiscovered
   marker, reveal it only while drawing the map. The context's cursor-query
   flag is set when the game decides what can be selected. */
static void install_native_reveal(Module *module,uint64_t slot,const NativeMapReveal *p) {
 uint32_t body[]={
  0x34000048, /* cbz w8, +8; preserve original success when nonzero */
  0,
  0xa9bf2be9, /* stp x9,x10,[sp,#-16]! */
  0,
  0x39414529, /* ldrb w9,[x9,#0x51] */
  0x370000c9, /* tbnz w9,#0,restore and reject */
  0x58000109, /* ldr x9, literal at slot+56 */
  0x88dffd2a, /* ldar w10,[x9] */
  0x3600006a, /* tbz w10,mode,restore and reject */
  0xa8c12be9, /* ldp x9,x10,[sp],#16 */
  0,
  0xa8c12be9,
  0,0,0,0
 };
 body[1]=map_branch(slot+4,p->accept);
 body[3]=p->indirect ? 0xf9401809u|(p->context<<5) : 0xaa0003e9u|(p->context<<16);
 body[8]|=(uint32_t)p->mode<<19;
 body[10]=map_branch(slot+40,p->accept);
 body[12]=map_branch(slot+48,p->reject);
 if(p->mode==native_fairy_mode){
  body[9]=map_branch(slot+36,slot+64);
  install_fairy_predicate(module,slot);
 }
 uint64_t pointer=(uintptr_t)&native_reveals;
 memcpy(body+14,&pointer,8);
 memcpy(module->base+slot,body,sizeof body);
 uint32_t jump=map_branch(p->site,slot);
 memcpy(module->base+p->site,&jump,4);
}

void runtime_map_install_native_reveals(Module *module) {
 const NativeMapReveal sites[]={
  {0x20f6ff0,0x34000248,0x394033e8,0x39414668,0x20f7038,0x20f6ff4,0,19,0},
  {0x736614, 0x340002c8,0x394073a8,0xf9401a68,0x73666c, 0x736618, 1,19,1},
  {0x20f9f9c,0x340000e8,0x385fc3a8,0xf9400688,0x20f9fb8,0x20f9fa0,2,20,0},
  {0x73644c, 0x35000328,0x394073e8,0x2a1f03e0,0x736450, 0x7364b0, 3,19,0},
  {0x7364fc, 0x34fffaa8,0x394063e8,0x39414668,0x736450, 0x736500, 3,19,0},
  {0x736ce4, 0x35000328,0x394073a8,0x2a1f03e0,0x736ce8, 0x736d48, 4,19,1},
  {0x20fd158,0x34fffbe8,0x394033e8,0xf9401a68,0x20fd0d4,0x20fd15c,5,19,1},
  {0x20fdc68,0x34fffb48,0x394023e8,0xf9401a68,0x20fdbd0,0x20fdc6c,6,19,1},
  {0x20fcce4,0x34fffa88,0x394033e8,0xf9401a68,0x20fcc34,0x20fcce8,7,19,1},
  {0x20f8068,0x34000988,0x394063a8,0xf9401a68,0x20f8198,0x20f806c,8,19,1},
  {0x20fe2bc,0x34fffba8,0x385fc3a8,0xf9401a68,0x20fe230,0x20fe2c0,native_fairy_mode,19,1},
 };
 const uint64_t first=((module->file_size+15)&~15ULL)+1696;
 const uint64_t limit=((module->file_size+0x3fff)&~0x3fffULL)-256;
 if(strcmp(module->name,"main")||first<module->patch_off||first+native_slot_size*(sizeof sites/sizeof *sites)>limit)return;
 unsigned installed=0;
 for(unsigned mode=0;mode<=native_fairy_mode;++mode){
  int usable=1;
  for(size_t i=0;i<sizeof sites/sizeof *sites;++i){
   const NativeMapReveal *p=&sites[i];
   if(p->mode!=mode)continue;
   uint32_t expected[]={p->previous,p->original,p->next};
   if(p->site+8>module->file_size||memcmp(module->base+p->site-4,expected,sizeof expected)){
    usable=0;
    break;
   }
   for(size_t j=0;j<native_slot_size;++j)if(module->base[first+i*native_slot_size+j])usable=0;
  }
  if(!usable)continue;
  for(size_t i=0;i<sizeof sites/sizeof *sites;++i)
   if(sites[i].mode==mode)install_native_reveal(module,first+i*native_slot_size,&sites[i]);
  installed|=1u<<mode;
 }
 atomic_store_explicit(&installed_reveals,installed,memory_order_release);
 fprintf(stderr,"[exploration-map] native display hooks %#x\n",installed);
}

#ifndef MAP_KOROK_FILTER_ONLY
static uint8_t *main_base;
static int ready;

/* Copy the map widget's current center, zoom, layer and filter flags to
   host storage. The same widget runs in the background with flag bit 0 set. */
static _Atomic uint64_t map_camera[3];
void runtime_map_install(Module *module){
 const char *path=getenv("TOTK_MAP_POINTS");
 if(!path||!*path||!strcmp(path,"0")||strcmp(module->name,"main"))return;
 main_base=module->base;
 const uint64_t site=0x20f6f40;
 const uint32_t original=0xf9401428;
 if(site+4>module->file_size)return;
 uint32_t before, current;
 memcpy(&before, main_base+site-4,4);
 memcpy(&current,main_base+site,4);
 if(current!=original||before!=0x540007e1)return;
 const uint64_t tail=((module->file_size+15)&~15ULL)+1536;
 if(tail<module->patch_off||tail+96>((module->file_size+0x3fff)&~0x3fffULL)-256)return;
 for(size_t i=0;i<96;i++)if(main_base[tail+i])return;
 uint32_t body[]={
  0xa9be2be9,0xa90133eb,                /* save x9-x12 */
  0xf9401429,0xf940082a,0xf9400129,0xb940014a, /* center and zoom */
  0xf9400c2b,0xb940016b,0x3941442c,0x2a0c216b,0xaa0b814a, /* layer and flags */
  0x5800016b,0x9100416c,0xd2800028,
  0xf8e80188,0xa9002969,0xd2800028,0xf868019f, /* odd sequence, publish, even */
  0xa94133eb,0xa8c22be9,original,0,0,0
 };
 body[21]=map_branch(tail+84,site+4);
 uint64_t pointer=(uintptr_t)map_camera;
 memcpy(body+22,&pointer,8);
 memcpy(main_base+tail,body,sizeof body);
 uint32_t jump=map_branch(site,tail);
 memcpy(main_base+site,&jump,4);
 runtime_map_install_korok_filter(module);
 runtime_map_install_native_reveals(module);
 ready=1;
}

int runtime_map_point_view(MapPointView *out){
 uint64_t a=atomic_load_explicit(map_camera+2,memory_order_acquire);
 if(!a||(a&1))return 0;
 uint64_t xy=atomic_load_explicit(map_camera,memory_order_relaxed);
 uint64_t scale_layer=atomic_load_explicit(map_camera+1,memory_order_relaxed);
 atomic_thread_fence(memory_order_acquire);
 if(atomic_load_explicit(map_camera+2,memory_order_relaxed)!=a)return 0;
 uint32_t words[]={ (uint32_t)xy,(uint32_t)(xy>>32),(uint32_t)scale_layer };
 float values[3];memcpy(values,words,sizeof values);
 unsigned layer=(unsigned)(scale_layer>>32)&255,flags=(unsigned)(scale_layer>>40)&255;
 if(layer>2||!isfinite(values[0])||!isfinite(values[1])||!isfinite(values[2])||
    values[2]<=0||values[2]>16||fabsf(values[0])>8000||fabsf(values[1])>8000)return 0;
 *out=(MapPointView){values[0],values[1],values[2],layer,a,flags==0};
 return 1;
}

/* GameDataMgr for the verified 1.4.3 rewrite. Copy only tables the native
   getters use, never call guest code or keep guest pointers across samples. */
typedef struct { uint32_t count, pad; uintptr_t values, hashes; uint32_t used, slots; } MapBoolTable;
typedef struct { uint32_t hash, index; } MapBoolIndex;
typedef struct { uint64_t key; uint32_t reset; uint8_t value, save, flags, extra; } MapGuidEntry;
typedef struct { uint32_t count, pad; uintptr_t entries; } MapGuidTable;

static int map_read(uintptr_t address, void *output, size_t size){
 mach_vm_size_t read=0;
 return address>=0x100000000ULL && address<(1ULL<<39) && size<=(1ULL<<39)-address &&
  mach_vm_read_overwrite(mach_task_self(),address,size,(uintptr_t)output,&read)==KERN_SUCCESS && read==size;
}

static unsigned map_key_index(const MapBoolTable *table,const MapBoolIndex *hashes,uint32_t key){
 if(!key)return table->count;
 unsigned index=key%table->slots;
 for(unsigned n=0;n<table->slots;n++){
  MapBoolIndex entry=hashes[index];
  if(!entry.hash)break;
  if(entry.hash==key&&entry.index<table->count)return entry.index;
  if(++index==table->slots)index=0;
 }
 return table->count;
}

static unsigned char map_guid_value(const MapGuidEntry *entries,uint32_t count,MapProgressPoint point){
 unsigned lo=0,hi=count;
 while(lo<hi){
  unsigned mid=lo+(hi-lo)/2;
  if(entries[mid].key<point.key)lo=mid+1;
  else hi=mid;
 }
 if(lo==count||entries[lo].key!=point.key||entries[lo].value!=1)return 0;
 uint32_t reset=entries[lo].reset;
 return point.kind==MAP_PROGRESS_RESPAWN ? (reset==88||reset==208) : reset==80;
}

int runtime_map_progress(MapProgressPoint *points,size_t count){
 if(!points||!count||!ready)return 0;
 uintptr_t manager,vtable;
 MapBoolTable bools,after_bools,enums,after_enums;
 MapGuidTable guids,after_guids;
 if(!map_read((uintptr_t)main_base+0x3ad66b0,&manager,sizeof manager)||
    !map_read(manager,&vtable,sizeof vtable)||vtable!=(uintptr_t)main_base+0x39403b0||
    !map_read(manager+0x10,&bools,sizeof bools)||!map_read(manager+0x250,&enums,sizeof enums)||
    !map_read(manager+0xc58,&guids,sizeof guids)||enums.count!=1696||enums.used!=1696||enums.slots!=3392||
    !bools.count||bools.count>30000||bools.used!=bools.count||
    bools.slots<bools.count||bools.slots>60000||guids.count<100000||guids.count>150000)return 0;
 size_t hash_bytes=bools.slots*sizeof(MapBoolIndex),value_bytes=bools.count*24;
 size_t guid_bytes=guids.count*sizeof(MapGuidEntry);
 size_t enum_hash_bytes=enums.slots*sizeof(MapBoolIndex),enum_value_bytes=enums.count*56;
 uint8_t *snapshot=malloc(hash_bytes+value_bytes+guid_bytes+enum_hash_bytes+enum_value_bytes);
 if(!snapshot)return 0;
 MapBoolIndex *hashes=(void *)snapshot;
 uint8_t *values=snapshot+hash_bytes;
 MapGuidEntry *entries=(void *)(values+value_bytes);
 MapBoolIndex *enum_hashes=(void *)((uint8_t *)entries+guid_bytes);
 uint8_t *enum_values=(uint8_t *)enum_hashes+enum_hash_bytes;
 uintptr_t after_manager=0;
 int ok=map_read(bools.hashes,hashes,hash_bytes)&&map_read(bools.values,values,value_bytes)&&
  map_read(guids.entries,entries,guid_bytes)&&map_read(enums.hashes,enum_hashes,enum_hash_bytes)&&
  map_read(enums.values,enum_values,enum_value_bytes)&&map_read(manager+0x250,&after_enums,sizeof after_enums)&&
  map_read(manager+0x10,&after_bools,sizeof after_bools)&&map_read(manager+0xc58,&after_guids,sizeof after_guids)&&
  map_read((uintptr_t)main_base+0x3ad66b0,&after_manager,sizeof after_manager)&&after_manager==manager&&
  !memcmp(&bools,&after_bools,sizeof bools)&&!memcmp(&guids,&after_guids,sizeof guids)&&
  !memcmp(&enums,&after_enums,sizeof enums);
 if(ok)for(size_t i=0;i<count;i++){
  MapProgressPoint point=points[i];
  points[i].done=0;
  if(point.kind==MAP_PROGRESS_BOOL){
   unsigned index=map_key_index(&bools,hashes,(uint32_t)point.key);
   points[i].done=index<bools.count && values[index*24+22]==1;
  }else if(point.kind==MAP_PROGRESS_ENUM){
   unsigned index=map_key_index(&enums,enum_hashes,(uint32_t)point.key);
   if(index<enums.count){
    uint32_t value;
    memcpy(&value,enum_values+index*56+28,sizeof value);
    for(unsigned j=0;j<point.value_count&&j<3;j++)
     if(value==point.values[j])points[i].done=1;
   }
  }else{
   points[i].done=map_guid_value(entries,guids.count,point);
  }
 }
 free(snapshot);
 return ok;
}

#endif

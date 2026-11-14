#include "map_points.h"
#include "runtime.h"
#include <math.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static uint32_t map_branch(uint64_t from,uint64_t to){
 return 0x14000000u|((uint32_t)(((int64_t)to-(int64_t)from)/4)&0x03ffffffu);
}

static _Atomic unsigned char hide_native_koroks;

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

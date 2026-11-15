#ifndef TOTK_MAP_POINTS_H
#define TOTK_MAP_POINTS_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    float x, north, scale;
    unsigned layer;
    uint64_t sequence;
    int visible;
} MapPointView;

int runtime_map_point_view(MapPointView *out);
void runtime_map_hide_native_koroks(int hide);

enum {
    MAP_NATIVE_SHRINES = 1 << 0,
    MAP_NATIVE_TOWERS = 1 << 1,
    MAP_NATIVE_PLACES = 1 << 2,
    MAP_NATIVE_ENTRANCES = 1 << 3,
    MAP_NATIVE_TOWNS = 1 << 4,
    MAP_NATIVE_TEARS = 1 << 5,
    MAP_NATIVE_OLD_MAPS = 1 << 6,
    MAP_NATIVE_DISPENSERS = 1 << 7,
    MAP_NATIVE_TEMPLES = 1 << 8,
    MAP_NATIVE_FAIRIES = 1 << 9
};
unsigned runtime_map_set_native_reveals(unsigned requested);

typedef enum {
    MAP_PROGRESS_BOOL,
    MAP_PROGRESS_GUID,
    MAP_PROGRESS_RESPAWN,
    MAP_PROGRESS_ENUM
} MapProgressKind;

typedef struct {
    uint64_t key;
    MapProgressKind kind;
    unsigned char done;
    unsigned char value_count;
    uint32_t values[3];
} MapProgressPoint;

int runtime_map_progress(MapProgressPoint *points, size_t count);

#endif

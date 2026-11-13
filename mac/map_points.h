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

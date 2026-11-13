#ifndef TOTK_MAP_INPUT_H
#define TOTK_MAP_INPUT_H

#include <stdint.h>

enum {
    MAP_PAD_OPEN = 1u << 0, MAP_PAD_LAYER = 1u << 1,
    MAP_PAD_UP = 1u << 2, MAP_PAD_DOWN = 1u << 3,
    MAP_PAD_LEFT = 1u << 4, MAP_PAD_RIGHT = 1u << 5,
    MAP_PAD_SELECT = 1u << 6, MAP_PAD_CLOSE = 1u << 7
};

/* Set and drain on the UI thread. Filter on the serialized controller reader. */
void runtime_map_input_context(int map_active, int panel_open);
unsigned runtime_map_input_events(void);
void runtime_map_input_physical(uint64_t physical, uint16_t mapped_r3, int available);
void runtime_map_filter_input(uint64_t pad, uint64_t *buttons,
                              int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry,
                              uint64_t now);

#endif

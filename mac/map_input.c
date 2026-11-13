#include "map_input.h"

#include <stdbool.h>
#include <stdatomic.h>

enum {
    PAD_A = 1u << 0, PAD_B = 1u << 1, PAD_L3 = 1u << 4, PAD_R3 = 1u << 5,
    PAD_LEFT = 1u << 12, PAD_UP = 1u << 13,
    PAD_RIGHT = 1u << 14, PAD_DOWN = 1u << 15
};

static _Atomic unsigned context;
static _Atomic unsigned events;
static _Atomic uint64_t physical_buttons;

void runtime_map_input_context(int map_active, int panel_open) {
    atomic_store(&context, map_active ? (panel_open ? 2u : 1u) : 0u);
}

unsigned runtime_map_input_events(void) {
    return atomic_exchange(&events, 0);
}

void runtime_map_input_physical(uint64_t physical, uint16_t mapped_r3, int available) {
    atomic_store(&physical_buttons, (physical & (PAD_R3 | PAD_L3)) |
                 ((uint64_t)mapped_r3 << 32) | (available ? (1ULL << 63) : 0));
}

void runtime_map_filter_input(uint64_t pad, uint64_t *buttons,
                              int32_t *lx, int32_t *ly, int32_t *rx, int32_t *ry,
                              uint64_t now) {
    static uint64_t previous, started, blocked;
    static bool tracking, long_press;
    const unsigned mode = atomic_load(&context);
    const uint64_t physical = atomic_load(&physical_buttons);
    const uint64_t input = (pad & ~(uint64_t)(PAD_R3 | PAD_L3)) |
                           (physical & (PAD_R3 | PAD_L3));
    const uint64_t pressed = input & ~previous;
    const bool r3 = (input & PAD_R3) != 0;
    const bool settings_chord = (input & (PAD_R3 | PAD_L3)) == (PAD_R3 | PAD_L3);
    unsigned actions = 0;

    if (!mode || !(physical >> 63) || settings_chord) {
        tracking = false;
    } else if (pressed & PAD_R3) {
        started = now;
        tracking = true;
        long_press = false;
    }
    if (tracking && !long_press && now - started >= 700000000ULL) {
        actions |= MAP_PAD_LAYER;
        long_press = true;
    }
    if (tracking && !r3) {
        if (!long_press) actions |= mode == 2 ? MAP_PAD_CLOSE : MAP_PAD_OPEN;
        tracking = false;
    }

    if (mode == 2) {
        blocked |= *buttons;
        *buttons = 0;
        *lx = *ly = *rx = *ry = 0;
        if (pressed & PAD_B) actions |= MAP_PAD_CLOSE;
        else {
            if (pressed & PAD_UP) actions |= MAP_PAD_UP;
            if (pressed & PAD_DOWN) actions |= MAP_PAD_DOWN;
            if (pressed & PAD_LEFT) actions |= MAP_PAD_LEFT;
            if (pressed & PAD_RIGHT) actions |= MAP_PAD_RIGHT;
            if (pressed & PAD_A) actions |= MAP_PAD_SELECT;
        }
    } else {
        blocked &= *buttons;
        *buttons &= ~blocked;
        if (mode && r3) *buttons &= ~(uint64_t)(uint16_t)(physical >> 32);
    }
    previous = input;
    if (actions) atomic_fetch_or(&events, actions);
}

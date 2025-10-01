// rom_glitcher_types.h

#ifndef _ROM_GLITCHER_TYPES_H_
#define _ROM_GLITCHER_TYPES_H_

#include "rom_glitcher_defines.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t address;
    uint8_t initial_value;
    uint8_t mod_value;
} rom_glitcher_glitch_t;

typedef struct {
    rom_glitcher_glitch_t* glitch;
    uint32_t glitch_count;
    uint32_t range_start;
    uint32_t range_size;
    uint32_t step_count;
    uint32_t seed;
    bool localizing;
    bool launch_done;
    bool init_done;
    uint8_t range_size_adj;
} rom_glitcher_main_t;

typedef struct {
    bool was_pressed;
    bool is_processed;
} rom_glitcher_button_state_t;

typedef struct {
    uint16_t count;
    uint16_t enabled_count;
    uint16_t current_page;
    uint16_t total_pages;
    uint32_t virt_address[RG_MAX_FOUND_GLITCH_SLOTS];
    uint32_t real_address[RG_MAX_FOUND_GLITCH_SLOTS];
    uint8_t initial_value[RG_MAX_FOUND_GLITCH_SLOTS];
    uint8_t mod_value[RG_MAX_FOUND_GLITCH_SLOTS];
    bool enabled[RG_MAX_FOUND_GLITCH_SLOTS];
} rom_glitcher_found_glitches_t;

typedef struct {
    bool record;
    bool play;
    uint16_t play_count;
    uint16_t length;
    int16_t sequence[RG_MAX_REPLAY_GAMEPADS][RG_MAX_REPLAY_FRAMES];
    int16_t hook_mask[RG_MAX_REPLAY_GAMEPADS];
    //retro_input_state_t input_cb_copy;
} rom_glitcher_input_replay_t;

typedef struct {
    uint8_t* data;
    int width;
    int height;
    int pitch;
    int vwidth;
    int vheight;
} rom_glitcher_bitmap_t;

typedef struct {
    uint32_t part1;
    uint32_t part2;
} rom_glitcher_dhash64_t;

typedef struct {
    rom_glitcher_glitch_t* glitch;
    uint16_t glitch_count;
    rom_glitcher_dhash64_t hash;
} rom_glitcher_bug_range_t;

typedef struct {
    uint32_t address[UINT8_MAX];
    uint8_t initial_value[UINT8_MAX];
    uint8_t mod_value[UINT8_MAX];
    uint8_t count;
} rom_glitcher_bug_glitches_t;

#endif
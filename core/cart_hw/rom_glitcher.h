// rom_glitcher.h

#ifndef _ROM_GLITCHER_H_
#define _ROM_GLITCHER_H_

#define RG_DEBUG 1
#define RG_VERSION_BASE "v0.3.1"
#define RG_DISABLED_KEY                 -2  // random number that is not gamepad button
#define RG_MAX_BACKUP_SLOTS             1   // 100 slots * 10000 candidates = ~7 Mb RAM
#define RG_MAX_FOUND_GLITCH_SLOTS       256
#define RG_PATH_SIZE                    512
#define RG_MAX_REPLAY_FRAMES            7200    // 60 FPS * 120 sec
#define RG_MAX_REPLAY_GAMEPADS          2       // port 0 and 1

#if RG_DEBUG
    #define RG_VERSION RG_VERSION_BASE "-dev"
#else
    #define RG_VERSION RG_VERSION_BASE
#endif

#include "shared.h"
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
    retro_input_state_t input_cb_copy;
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

extern const struct retro_game_info* rg_last_game; // info to reload current game
extern int32_t rg_menu_button;
extern bool rg_swap_buttons;
extern bool rg_menu_visible;
extern bool rg_found_glitches_modified;
extern rom_glitcher_found_glitches_t rg_found_glitches;
extern rom_glitcher_input_replay_t rg_input_replay;
extern rom_glitcher_main_t rg_main;
extern rom_glitcher_bug_glitches_t rg_bug_glitches;
extern uint8_t rg_backup_count;
extern uint32_t rg_total_glitch_count;
extern rom_glitcher_button_state_t rg_button_states[7];
extern uint16_t rg_fps;
extern uint8_t rg_pause_effect;
extern bool rg_rom_in_mdx;
extern bool rg_rom_is_byte_swapped;
extern bool rg_rom_has_header;
extern bool rg_rom_was_deinterleaved;
extern bool rg_clear_bug_range;
//extern char rg_log[1024];

extern bool libretro_supports_bitmasks;
extern char g_rom_dir[256];
extern char g_rom_name[256];
extern retro_log_printf_t log_cb;
extern retro_video_refresh_t video_cb;
extern retro_input_poll_t input_poll_cb;
extern retro_input_state_t input_state_cb;
extern retro_environment_t environ_cb;

void rg_init(uint8_t* rom_data, uint32_t size);
void rg_deinit(void);
void rg_force_stop_glitcher(void);
void rg_game_reset(void);
void rg_game_save_state(void);
void rg_game_load_state(void);
void rg_launch_glitcher(void);
uint32_t rg_virt_to_real_rom_offset(uint32_t address);
uint32_t rg_real_to_virt_rom_offset(uint32_t address);
void rg_handle_input(const t_bitmap* bitmap, const int* vwidth, const int* vheight);

#endif
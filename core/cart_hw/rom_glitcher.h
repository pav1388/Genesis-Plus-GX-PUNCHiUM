// rom_glitcher.h

#ifndef _ROM_GLITCHER_H_
#define _ROM_GLITCHER_H_

#define RG_VERSION                      "v0.2.4b"
#define RG_DISABLED_KEY                 -2   // random number that is not gamepad button
#define RG_MAX_BACKUP_SLOTS             33   // 100 slots * 10000 candidates = ~7 Mb RAM
#define RG_MAX_FOUND_GLITCH_SLOTS       240
#define RG_MAX_FOUND_GLITCH_PER_PAGE    6
#define RG_MAX_REPLAY_FRAMES            3600 // 60 FPS * 60 sec
#define RG_MAX_REPLAY_GAMEPAD           2

#define RG_MSG_INFO        1
#define RG_MSG_ERROR       2
#define RG_MSG_FOUND       3
#define RG_MSG_REPLAY_REC  4
#define RG_MSG_REPLAY_PLAY 5

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    uint32_t address;
    uint8_t initial_value;
    uint8_t mod_value;
} rom_glitch_t;

typedef struct {
    rom_glitch_t* glitches;
    uint32_t glitch_count;
    uint32_t range_start;
    uint32_t range_size;
    uint32_t step_count;
    uint32_t seed;
    bool localizing;
    bool launch;
    bool init_done;
} rom_glitcher_t;

typedef struct {
    bool was_pressed;
    bool is_processed;
} rom_glitcher_button_state_t;

typedef struct {
    uint8_t count;
    uint8_t enabled_count;
    uint8_t current_page;
    uint8_t total_pages;
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
    int16_t sequence[RG_MAX_REPLAY_GAMEPAD][RG_MAX_REPLAY_FRAMES];
    int16_t hook_mask[RG_MAX_REPLAY_GAMEPAD];
    retro_input_state_t input_cb_copy;
} rom_glitcher_input_replay_t;

extern const struct retro_game_info* rg_last_game; // info to reload current game
extern int32_t rg_menu_button;
extern bool rg_swap_buttons;
extern bool rg_menu_visible;
extern bool rg_found_glitches_modified;
extern rom_glitcher_found_glitches_t rg_found_glitches;
extern rom_glitcher_input_replay_t rg_input_replay;
extern rom_glitcher_t rg_main;
extern rom_glitcher_t rg_backup[RG_MAX_BACKUP_SLOTS];
extern rom_glitcher_t rg_backup_before_local;
extern uint8_t rg_backup_index;
extern uint8_t rg_backup_count;
extern uint32_t rg_total_glitch_count;
extern rom_glitcher_button_state_t rg_button_states[7];
extern uint16_t rg_fps;
extern uint8_t rg_pause_effect;
extern bool rg_rom_in_mdx;
extern bool rg_rom_is_byte_swapped;
extern bool rg_rom_has_header;
extern bool rg_rom_was_deinterleaved;
extern char rg_log[1024];

extern bool libretro_supports_bitmasks;
extern char g_rom_dir[256];
extern char g_rom_name[256];
extern retro_log_printf_t log_cb;
extern retro_input_poll_t input_poll_cb;
extern retro_input_state_t input_state_cb;
extern retro_environment_t environ_cb;

void rg_create_step_backup(void);
void rg_instructions_restore(void);
void rg_instructions_shuffle(void);
void rg_instructions_inversion(void);
void rg_init(uint8_t* rom_data, uint32_t size);
void rg_deinit(void);
void rg_force_stop_glitcher(void);
void rg_input_processing(void);
void rg_current_search_end(void);
void rg_game_reset(void);
void rg_game_save_state(void);
void rg_game_load_state(void);
uint8_t rg_save_found_to_file(uint32_t virt_address,
    uint32_t real_address, uint8_t intial_value, uint8_t mod_value);
uint32_t rg_virt_to_real_rom_offset(uint32_t address);
uint32_t rg_real_to_virt_rom_offset(uint32_t address);
void rg_handle_pause_frame(void** pause_frame, int* pause_frame_width, int* pause_frame_height,
    const void* bitmap_data, int vwidth, int vheight, int bitmap_pitch, int bitmap_width);

#ifndef RANDOM_SEED
#warning RANDOM_SEED not defined
#endif
#ifndef COMPRESSED_OPCODE_TABLE
#warning COMPRESSED_OPCODE_TABLE not defined
#endif

#endif
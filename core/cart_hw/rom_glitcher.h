// rom_glitcher.h
// perfect_genius - glitcher idea, pav13 - implementation

#ifndef _ROM_GLITCHER_H_
#define _ROM_GLITCHER_H_

#define RG_DISABLED_KEY 128 // random number that is not gamepad button

#include "shared.h"

typedef struct {
    retro_log_printf_t log_cb;
    retro_environment_t environ_cb;
    retro_input_poll_t input_poll_cb;
    retro_input_state_t input_state_cb;
} rom_glitcher_callbacks_t;

extern rom_glitcher_callbacks_t rg_cbs;

extern const struct retro_game_info* rg_last_game; // info to reload current game
extern uint8_t rg_menu_button; // menu call button

static const char* get_label_list_of_found_glitches(void);
static const char* get_label_after_step(void);
static const char* get_label_step_back(void);
static void menu_item_activate_selected_glitch(void);
static void menu_item_open_options(void);
static void menu_item_0_launch_glitcher(void);
static void menu_item_1_bug_not_understand(void);
static void menu_item_2_glitch_not_found(void);
static void menu_item_3_glitch_found(void);
static void menu_item_4_step_back(void);
static void menu_item_after_step(void);
static void menu_item_open_list_of_found_glitches(void);
static void menu_hide(void);
static void menu_show(void);

static void apply_glitches(void);
static void create_search_backup(void);
static void restore_instructions(void);
static void shuffle_instructions(void);
//static void remove_current_range(void);
//static void keep_only_current_range(void);
static void show_notification(const char* s, uint8_t context);
static void game_reset(void);
static void game_save_state_to_ram(void);
static void game_load_state_from_ram(void);
static uint32_t xorshift(uint32_t* state);
static uint16_t get_rom_checksum(uint8* rom, int length);
static uint32_t ram_to_rom_offset(uint32_t address);
static uint8_t add_glitch_as_cheat_to_file(uint32_t virt_address,
    uint32_t real_address, uint8_t intial_value, uint8_t mod_value);
static void rg_reset(void);
//static uint32_t get_checksum_for_file(const uint8_t* data, size_t size);
//static bool game_save_state_to_file(const char* path);
//static bool game_load_state_from_file(const char* path);

void rg_init(uint8_t* rom_data, uint32_t size);
void rg_deinit(void);
void rg_input_processing(void);
void rg_set_rom_in_mdx(bool value);
void rg_set_rom_is_byte_swapped(bool value);
void rg_set_rom_has_header(bool value);
void rg_set_rom_was_interleaved(bool value);
bool rg_get_menu_visible(void);

#endif
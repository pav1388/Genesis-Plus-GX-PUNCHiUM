// rom_glitcher_menu.h

#ifndef _ROM_GLITCHER_MENU_H_
#define _ROM_GLITCHER_MENU_H_

#include "rom_glitcher_defines.h"
#include "rom_glitcher_types.h"
#include "rom_glitcher.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char* (*get_label)(uint8_t index);
    void (*action)(void);
} rom_glitcher_menu_item_t;

typedef struct {
    rom_glitcher_menu_item_t* items;
    uint8_t item_count;
    uint8_t selected_index;
} rom_glitcher_menu_t;

typedef struct {
    rom_glitcher_menu_t* current;
    rom_glitcher_menu_t launch;
    rom_glitcher_menu_t search;
    rom_glitcher_menu_t options;
    rom_glitcher_menu_t found;
    rom_glitcher_menu_t command;
    rom_glitcher_menu_t inst_bcc;
    rom_glitcher_menu_t inst_scc;
    rom_glitcher_menu_t inst_dbcc;
    rom_glitcher_menu_t inst_add_sub;
    rom_glitcher_menu_t inst_div_mul;
} rom_glitcher_menu_manager_t;

extern const char* rg_instr_mnemonic[];
extern rom_glitcher_button_state_t rg_button_states[7];
extern rom_glitcher_menu_manager_t rg_menu;
extern rom_glitcher_found_glitches_t rg_found_glitches;
extern rom_glitcher_input_replay_t rg_input_replay;
extern rom_glitcher_main_t rg_main;
extern rom_glitcher_bug_glitches_t rg_bug_glitches;
extern uint32_t rg_total_glitch_count;
extern uint32_t rg_inst_allowed;
extern uint16_t rg_fps;
extern uint8_t rg_pause_effect;
extern bool rg_clear_bug_range;
extern bool rg_found_glitches_modified;
extern bool rg_menu_visible;

void rg_menu_show(void);
void rg_menu_hide(void);
void rg_msg(uint8_t context, const char* format, ...);

#endif

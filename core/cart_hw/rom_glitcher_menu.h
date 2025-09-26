// rom_glitcher_menu.h

#ifndef _ROM_GLITCHER_MENU_H_
#define _ROM_GLITCHER_MENU_H_

#define RG_FOUND_GLITCH_PER_PAGE    6
#define RG_MSG_DEBUG        0
#define RG_MSG_INFO         1
#define RG_MSG_ERROR        2
#define RG_MSG_FOUND        3
#define RG_MSG_REPLAY_REC   4
#define RG_MSG_REPLAY_PLAY  5

#include <stdarg.h>
#include <stdint.h>

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
} rom_glitcher_menu_manager_t;

extern rom_glitcher_menu_manager_t rg_menu;
extern uint32_t rg_inst_allowed;

void rg_menu_show(void);
void rg_menu_hide(void);
void rg_msg(uint8_t context, const char* format, ...);

#endif

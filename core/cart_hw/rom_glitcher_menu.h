// rom_glitcher_menu.h

#ifndef _ROM_GLITCHER_MENU_H_
#define _ROM_GLITCHER_MENU_H_

typedef struct {
    const char* label;
    const char* (*get_label)(void);
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
    rom_glitcher_menu_t main;
    rom_glitcher_menu_t options;
    rom_glitcher_menu_t list;
    rom_glitcher_menu_t branch;
} rom_glitcher_menu_manager_t;

extern rom_glitcher_menu_manager_t rg_menu;
extern uint8_t rg_branch_allowed;

void rg_menu_show(void);
void rg_menu_hide(void);
void rg_msg(const char* s, uint8_t context);

#endif

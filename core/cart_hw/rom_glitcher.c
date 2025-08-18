// rom_glitcher.c 
// perfect_genius - glitcher idea, pav13 - implementation

#define RG_VERSION "Launch Glitcher v0.0.7"
//#define RG_STATE_SIGNATURE "RGI"
//#define RG_STATE_VERSION 1
#define RG_LOAD_STATE 0
#define RG_HARD_RESET 1
#define RG_MSG_INFO 1
#define RG_MSG_ERROR 2
#define RG_MSG_FOUND 3
#define RG_BACKUP_SLOTS_MAX 9 // любое количество
#define RG_GLITCH_SLOTS_MAX 7 // <= 7 !
#define PATH_MAX 512

#include "rom_glitcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

typedef struct {
    uint32_t address;
    uint8_t initial_value;
    uint8_t mod_value;
} rom_glitch_t;

typedef struct {
    rom_glitch_t* glitches;
    uint32_t glitch_count;
    uint32_t capacity;
    uint32_t range_start;
    uint32_t range_size;
    uint32_t total_glitch_count;
    uint32_t step_count;
    uint32_t bug_step_count;
    bool localizing;
    bool launch;
    bool init_done;
} rom_glitcher_t;

static rom_glitcher_t rg_main = {   // основная структура для работы с глитчами
    .glitches = NULL,
    .glitch_count = 0,
    .capacity = 0,
    .range_start = 0,
    .range_size = 0,
    .total_glitch_count = 0,
    .step_count = 0,
    .bug_step_count = 0,
    .localizing = false,
    .launch = false,
    .init_done = false
};

typedef struct {
    rom_glitcher_t backup;
    rom_glitch_t* glitches_backup;
} rom_glitcher_backup_t;

static rom_glitcher_backup_t rg_backups[RG_BACKUP_SLOTS_MAX];
static int rg_backup_index = 0;
static int rg_backup_count = 0;

typedef struct {
    uint8_t count;
    uint32_t virt_address[RG_GLITCH_SLOTS_MAX];
    uint32_t real_address[RG_GLITCH_SLOTS_MAX];
    uint8_t initial_value[RG_GLITCH_SLOTS_MAX];
    uint8_t mod_value[RG_GLITCH_SLOTS_MAX];
    bool activate[RG_GLITCH_SLOTS_MAX];
} found_glitch_list_t;

static found_glitch_list_t found_glitches = { 0 };

uint8_t rg_menu_button = RG_DISABLED_KEY;
rom_glitcher_callbacks_t rg_cbs;

static char log_text[512];
static uint8_t game_state_buffer[STATE_SIZE];   // буфер для save state игры размером 0xFD000 (1 036 288) байт
static uint8_t action_after = RG_LOAD_STATE;    // действие после выбора следующего шага 
static bool need_load_state = false;
static uint32_t final_percent = 0;

static bool rom_in_mdx = false;             // rom был в формате MDX?
static bool rom_is_byte_swapped = false;    // ROM был изначально байт-свапнут?
static bool rom_has_header = false;         // был удалён заголовок?
static bool rom_was_deinterleaved = false;  // к ROM применялся деинтерлив?

static bool menu_visible = false;           // отображение меню на экране 

// ************************************************************* MENU

typedef struct {
    const char* label; // статическая метка
    const char* (*get_label)(void); // функция для создания динамической метки
    void (*action)(void); // действие при активации пункта меню
} rom_glitcher_menu_items_t;

typedef struct {
    rom_glitcher_menu_items_t* items;
    uint8_t item_count;
    uint8_t selected_index;
} rg_menu;

static rom_glitcher_menu_items_t menu_launch[] = {
    { RG_VERSION, NULL, menu_item_0_launch_glitcher },
    { "Save state", NULL, game_save_state_to_ram },
    { "After step", get_label_after_step, menu_item_after_step },
    { "List of found glitches", NULL, menu_item_open_list_of_found_glitches }
};

static rom_glitcher_menu_items_t menu_main[] = {
    { "Options", NULL, menu_item_open_options },
    { "BUG", NULL, menu_item_1_bug_not_understand },
    { "NOT FOUND", NULL, menu_item_2_glitch_not_found },
    { "FOUND", NULL, menu_item_3_glitch_found },
    { "Step back", get_label_step_back, menu_item_4_step_back },
    { "Reset Glitcher", NULL, rg_reset }
};

static rom_glitcher_menu_items_t menu_settings[] = {
    { "After step", get_label_after_step, menu_item_after_step },
    { "Load state", NULL, game_load_state_from_ram },
    { "Save state", NULL, game_save_state_to_ram },
    { "Reset game", NULL, game_reset },
    { "List of found glitches", NULL, menu_item_open_list_of_found_glitches }
};

static rom_glitcher_menu_items_t menu_list[] = {
    { "0x0", get_label_list_of_found_glitches, menu_item_activate_selected_glitch },
    { "0x1", get_label_list_of_found_glitches, menu_item_activate_selected_glitch },
    { "0x2", get_label_list_of_found_glitches, menu_item_activate_selected_glitch },
    { "0x3", get_label_list_of_found_glitches, menu_item_activate_selected_glitch },
    { "0x4", get_label_list_of_found_glitches, menu_item_activate_selected_glitch },
    { "0x5", get_label_list_of_found_glitches, menu_item_activate_selected_glitch },
    { "0x6", get_label_list_of_found_glitches, menu_item_activate_selected_glitch }
};

static struct {
    rg_menu* current;
    rg_menu launch;
    rg_menu main;
    rg_menu settings;
    rg_menu list;
} menu = {
    .launch = { menu_launch, ARRAY_SIZE(menu_launch), 0 },
    .main = { menu_main, ARRAY_SIZE(menu_main), 0 },
    .settings = { menu_settings, ARRAY_SIZE(menu_settings), 0 },
    .list = { menu_list, ARRAY_SIZE(menu_list), 0 }
};

static char dyn_list[RG_GLITCH_SLOTS_MAX][14];
static const char* get_label_list_of_found_glitches(void) { // пункты для меню найденных глитчей
    static uint8_t i = 0;
    uint8_t j = i;
    i = (i + 1) % RG_GLITCH_SLOTS_MAX;

    if (found_glitches.real_address[j]) {
        snprintf(dyn_list[j], sizeof(dyn_list[j]),
            "0x%06X >%u",
            found_glitches.real_address[j],
            found_glitches.activate[j]);
    }
    else
        snprintf(dyn_list[j], sizeof(dyn_list[j]), "empty");

    return dyn_list[j];
}

static char dyn_label2[32];
static const char* get_label_after_step(void) { // выбор действия после шага
    snprintf(dyn_label2, sizeof(dyn_label2), "After step > %s",
        (action_after == RG_LOAD_STATE) ? "Load state" :
        (action_after == RG_HARD_RESET) ? "Hard reset" : "!eRRoR1");
    return dyn_label2;
}

static char dyn_label1[32];
static const char* get_label_step_back(void) {
    snprintf(dyn_label1, sizeof(dyn_label1), "Step back:%u", rg_backup_count);
    return dyn_label1;
}

// активировать/деактивировать выбранный глитч
static void menu_item_activate_selected_glitch(void) {
    if (!found_glitches.virt_address[menu.current->selected_index]) {
        show_notification("Empty slot", RG_MSG_ERROR);
        return;
    }

    found_glitches.activate[menu.current->selected_index] = !found_glitches.activate[menu.current->selected_index];

    if(found_glitches.activate[menu.current->selected_index])
        show_notification("Selected glitch is ENABLED", RG_MSG_INFO);
    else
        show_notification("Selected glitch is DISABLED", RG_MSG_INFO);

    game_reset();
}

static void menu_item_open_options(void) { // открыть меню настроек
    menu.current = &menu.settings;
    menu_show();
}

static void menu_item_open_list_of_found_glitches(void) { // открыть список найденных глитчей
    if (!found_glitches.virt_address[0]) {
        show_notification("Glitches NOT found ... yet", RG_MSG_ERROR);
        return;
    }
    menu.current = &menu.list;
    menu_show();
}

static void menu_item_after_step(void) { // выбор действия после шага
    action_after = (action_after + 1) % 2;
    menu_show();
}

static uint32_t xorshift(uint32_t* seed) { // замена для rand()
    uint32_t x = *seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (*seed = x);
}

// перемешивание инструкций
static void shuffle_instructions(void) {
    const uint32_t min = 2;
    const uint32_t max = 51;
    static uint32_t seed = 19881029;

    seed ^= m68k_get_reg(M68K_REG_PC); // регистр m68k
    seed ^= rg_main.glitch_count;
    seed ^= rg_main.step_count;
    seed ^= rg_main.bug_step_count;
    seed ^= found_glitches.count;

    // расчёт максимального процента изменяемых инструкций
    uint32_t progress = rg_main.glitch_count * 100 / rg_main.total_glitch_count;
    uint32_t base = max - ((progress * (max - min)) / 100);
    uint32_t penalty = (rg_main.bug_step_count > (max / 2)) ? max : rg_main.bug_step_count * 2;
    final_percent = (base > penalty) ? (base - penalty) : min;

    final_percent = (final_percent < min) ? min : (final_percent > max) ? max : final_percent;

    rg_main.range_start = 0; // log
    for (uint32_t i = 0; i < rg_main.glitch_count; i++)
        if ((xorshift(&seed) % 100) < final_percent) {
            rg_main.glitches[i].mod_value = rg_main.glitches[i].initial_value ^ 1;
            rg_main.range_start++; // log
        }

    // равномерное перераспределение
    //for (uint32_t i = 0; i < rg_main.glitch_count - 1; i++) {
    //    uint32_t j = i + xorshift(&seed) % (rg_main.glitch_count - i);
    //    uint8_t tmp = rg_main.glitches[i].initial_value;
    //    rg_main.glitches[i].mod_value = rg_main.glitches[j].initial_value;
    //    rg_main.glitches[j].mod_value = tmp;
    //}
        //rg_main.glitches[i].mod_value = rg_main.glitches[i].initial_value ^ 1;

    // как в оригинале
    /*for (uint32_t i = 0; i < rg_main.glitch_count; i++)
        rg_main.glitches[i] = rg_main.glitches[rand() % rg_main.glitch_count];*/
}

static void menu_item_0_launch_glitcher(void) { // действие 0 "Launch Glitcher"
    if (rg_main.init_done) {
        rg_main.launch = true;
        menu.current = &menu.main;
        create_search_backup();
        shuffle_instructions();
        rg_main.step_count++;
        game_reset();
    }
    else
        show_notification("Candidates NOT FOUND", RG_MSG_ERROR);
}

static void menu_item_1_bug_not_understand(void) { // действие 1 "Bug"
    create_search_backup();
    restore_instructions();
    shuffle_instructions();
    rg_main.step_count++;
    rg_main.bug_step_count++;
    game_reset();
    
    //if (rg_main.init_done && !rg_main.localizing) { // действие bug/not understand
    //create_search_backup();
    //restore_instructions();
    
    /*if (rg_main.range_size == 1 || rg_main.glitch_count == 1) {
        // обработка окончания
        rg_reset();
        show_notification("RG: Instruction NOT FOUND. Try again.", RG_MSG_ERROR);
        return;
    }

    //rg_main.range_start += rg_main.range_size;
    //if (rg_main.range_start >= rg_main.glitch_count) {
    if ((rg_main.range_start + rg_main.range_size) >= rg_main.glitch_count) {
        rg_main.range_start = 0;
        rg_main.range_size /= 2; // уменьшаем окно, если дошли до конца
        if (!rg_main.localizing && rg_main.range_size > 1) {
            shuffle_instructions();
        }
    }
    else
        rg_main.range_start += rg_main.range_size;
    */

    // как в оригинале
    //if (rg_main.range_start + rg_main.range_size >= rg_main.glitch_count) {
    //    if (rg_main.range_size == 1) {
    //        show_notification("RG ERROR: Instruction NOT FOUND. Try again.", RG_MSG_ERROR);
    //        //rg_reset();
    //        return;
    //    }
    //    else {
    //        rg_main.range_start = 0;
    //        rg_main.range_size /= 2;
    //    }
    //}
    //else
    //    rg_main.range_start += rg_main.range_size;

    //if (!rg_main.localizing)
    //    shuffle_instructions();
}

// удаление текущего диапазона инструкций
//static void remove_current_range(void) {
//    if (!rg_main.glitches || rg_main.glitch_count == 0) return;
//    if (rg_main.range_start >= rg_main.glitch_count) return;
//
//    // корректируем размер, если выходим за границы
//    uint32_t remove_size = rg_main.range_size;
//    if (rg_main.range_start + remove_size > rg_main.glitch_count) {
//        remove_size = rg_main.glitch_count - rg_main.range_start;
//    }
//    if (remove_size == 0) return;
//
//    // удаляем диапазон
//    memmove(&rg_main.glitches[rg_main.range_start],
//        &rg_main.glitches[rg_main.range_start + remove_size],
//        (rg_main.glitch_count - rg_main.range_start - remove_size) * sizeof(rom_glitch_t));
//
//    rg_main.glitch_count -= remove_size;
//
//    if (rg_main.range_start >= rg_main.glitch_count) {
//        rg_main.range_start = 0;
//    }
//}

static void menu_item_2_glitch_not_found(void) { // действие 2 "Not found"
    if (rg_main.glitch_count > rg_main.total_glitch_count) {
        show_notification("(#2)", RG_MSG_ERROR);
        rg_reset();
        return;
    }
    
    rg_main.range_size = rg_main.glitch_count; // log
    uint32_t i = 0;

    // удаление всех изменённых кандидатов
    for (uint32_t j = 0; j < rg_main.glitch_count; j++) {
        if (rg_main.glitches[j].mod_value == rg_main.glitches[j].initial_value) {
            if (i != j) {
                rg_main.glitches[i] = rg_main.glitches[j];
                rg_main.range_size--; // log
            }
            i++;
        }
    }
    rg_main.glitch_count = i;

    if (rg_main.glitch_count == 0) {
        show_notification("Glitch NOT found. Try again (#4)", RG_MSG_ERROR);
        rg_reset();
        return;
    }

    create_search_backup();
    restore_instructions();
    shuffle_instructions();
    rg_main.step_count++;
    rg_main.bug_step_count = 0;
    game_reset();
    
    //create_search_backup();
    //restore_instructions();

    //if (rg_main.range_size < 2 || rg_main.glitch_count < 2) {
    //    // обработка окончания
    //    rg_reset();
    //    show_notification("RG: Instruction NOT FOUND. Try again.", RG_MSG_ERROR);
    //    return;
    //}
    //
    //if ((rg_main.range_start + rg_main.range_size) >= rg_main.glitch_count) {
    //    rg_main.range_start = 0;
    //    rg_main.range_size /= 2;
    //    if (!rg_main.localizing)
    //        shuffle_instructions();
    //}
    //else {
    //    // удаляем инструкции внутри окна 
    //    remove_current_range();
    //    rg_main.range_size /= 2;
    //}

    // как в оригинале
    //if (rg_main.range_start + rg_main.range_size >= rg_main.glitch_count) {
    //    if (rg_main.range_size == 1 || rg_main.range_start == 0) {
    //        show_notification("RG ERROR: Instruction NOT FOUND. Try again.", RG_MSG_ERROR);
    //        //rg_reset();
    //        return;
    //    }
    //    else {
    //        // ??? инструкции_на_инвертирование.resize(окно_начало);
    //        rg_main.range_start = 0;
    //        rg_main.range_size /= 2;
    //    }
    //}
    //else {
    //    remove_current_range();
    //    if (rg_main.localizing)
    //        rg_main.range_size /= 2;
    //}

    //if (!rg_main.localizing)
    //    shuffle_instructions();
}

// оставить только указанный диапазон
//static void keep_only_current_range(void) {
//    if (!rg_main.glitches || rg_main.glitch_count == 0) {
//        rg_main.range_start = 0;
//        rg_main.range_size = 0;
//        return;
//    }
//
//    // Корректируем размер, если выходим за границы
//    uint32_t keep_size = rg_main.range_size;
//    if (rg_main.range_start + keep_size > rg_main.glitch_count) {
//        keep_size = rg_main.glitch_count - rg_main.range_start;
//    }
//    if (keep_size == 0) {
//        rg_main.glitch_count = 0;
//        rg_main.range_start = 0;
//        rg_main.range_size = 0;
//        return;
//    }
//
//    // Переносим нужный диапазон в начало массива
//    memmove(rg_main.glitches,
//        &rg_main.glitches[rg_main.range_start],
//        keep_size * sizeof(rom_glitch_t));
//
//    rg_main.glitch_count = keep_size;
//    rg_main.range_start = 0;
//
//    // Если окно больше, чем количество оставшихся инструкций — обрезаем
//    if (rg_main.range_size > rg_main.glitch_count) {
//        rg_main.range_size = rg_main.glitch_count;
//    }
//}

static void menu_item_3_glitch_found(void) { // действие 3 "Found"
    if (rg_main.glitch_count > rg_main.total_glitch_count) {
        show_notification("(#3)", RG_MSG_ERROR);
        rg_reset();
        return;
    }

    rg_main.localizing = true;

    if (rg_main.glitch_count == 1) {
        // глитч найден
        uint8_t idx;

        // поиск дубликата в уже найденных глитчах
        for (int i = 1; i < RG_GLITCH_SLOTS_MAX; i++) {
            if (rg_main.glitches[0].address == found_glitches.virt_address[i]) {
                show_notification("Duplicate. Glitch has already been found", RG_MSG_INFO);
                rg_reset();
                return;
            }
        }

        // выбор слота для добавления глитча в список найденных
        if (found_glitches.count < RG_GLITCH_SLOTS_MAX) {
            idx = found_glitches.count;
            found_glitches.count++;
        }
        else {
            for (int i = 1; i < RG_GLITCH_SLOTS_MAX; i++) {
                found_glitches.virt_address[i - 1] = found_glitches.virt_address[i];
                found_glitches.initial_value[i - 1] = found_glitches.initial_value[i];
                found_glitches.real_address[i - 1] = found_glitches.real_address[i];
                found_glitches.activate[i - 1] = found_glitches.activate[i];
            }

            idx = RG_GLITCH_SLOTS_MAX - 1;
            show_notification("Slots are full. Glitch #1 removed", RG_MSG_INFO);
        }
        
        found_glitches.initial_value[idx] = rg_main.glitches[0].initial_value;
        found_glitches.mod_value[idx] = rg_main.glitches[0].mod_value;
        found_glitches.activate[idx] = false;
        found_glitches.virt_address[idx] = rg_main.glitches[0].address;
        found_glitches.real_address[idx] = ram_to_rom_offset(found_glitches.virt_address[idx]);
        add_glitch_as_cheat_to_file(found_glitches.virt_address[idx], found_glitches.real_address[idx],
            found_glitches.initial_value[idx], found_glitches.mod_value[idx]);

        char temp[84];
        snprintf(temp, sizeof(temp), "Glitch #%u. Steps %u, Real ROM '0x%06X' (VirtROM %06X)",
            found_glitches.count, rg_main.step_count, found_glitches.real_address[found_glitches.count - 1],
            found_glitches.virt_address[found_glitches.count - 1]);
        show_notification(temp, RG_MSG_FOUND);
        rg_reset();
        return;
    }

    rg_main.range_size = rg_main.glitch_count; // log
    uint32_t i = 0;

    // удаление всех неизменённых кандидатов
    for (uint32_t j = 0; j < rg_main.glitch_count; j++) {
        if (rg_main.glitches[j].mod_value != rg_main.glitches[j].initial_value) {
            if (i != j) {
                rg_main.glitches[i] = rg_main.glitches[j];
                rg_main.range_size--; // log
            }
            i++;
        }
    }
    rg_main.glitch_count = i;

    if (rg_main.glitch_count == 0) {
        show_notification("Glitch NOT found. Try again (#5)", RG_MSG_ERROR);
        rg_reset();
        return;
    }

    create_search_backup();
    restore_instructions();
    shuffle_instructions();
    rg_main.step_count++;
    rg_main.bug_step_count = 0;
    game_reset();

    /*if (rg_main.range_size < 2 || rg_main.glitch_count < 2) {
        // !!! добавить обработку окончания
        found_glitch_virt_address = rg_main.glitches[rg_main.range_start].address;
        found_glitch_real_address = ram_to_rom_offset(found_glitch_virt_address);
        rg_reset();
        show_notification("RG: Instruction FOUND", RG_MSG_FOUND);
        return;
    }*/

    // как в оригинале
    //if (rg_main.range_size == 1 || rg_main.glitch_count == 1) {
    //    found_glitch_virt_address = rg_main.glitches[rg_main.range_start].address;
    //    //found_glitch_real_address = ram_to_rom_offset(found_glitch_virt_address);
    //    show_notification("RG: Instruction FOUND", RG_MSG_FOUND);
    //    //rg_reset();
    //    return;
    //}

    //create_search_backup();
    //restore_instructions();
    //keep_only_current_range();
    //rg_main.range_size /= 2;
}

static void menu_item_4_step_back(void) { // действие 4 "Step back"
    if (rg_backup_count == 0) {
        show_notification("No previous search states", RG_MSG_ERROR);
        return;
    }

    // Берём предыдущий индекс
    rg_backup_index = (rg_backup_index - 1 + RG_BACKUP_SLOTS_MAX) % RG_BACKUP_SLOTS_MAX;
    rg_backup_count--;

    rom_glitcher_backup_t* slot = &rg_backups[rg_backup_index];

    // Чистим текущий список глитчей
    if (rg_main.glitches) {
        free(rg_main.glitches);
        rg_main.glitches = NULL;
    }

    // Восстанавливаем состояние
    rg_main = slot->backup;

    // Копируем массив глитчей
    if (slot->glitches_backup && slot->backup.glitch_count > 0) {
        rg_main.glitches = malloc(sizeof(rom_glitch_t) * rg_main.glitch_count);
        if (rg_main.glitches) {
            memcpy(rg_main.glitches, slot->glitches_backup,
                sizeof(rom_glitch_t) * rg_main.glitch_count);
        }
        else {
            rg_main.glitches = NULL;
            rg_main.glitch_count = 0;
            show_notification("Step restore failed (memory)", RG_MSG_ERROR);
        }
    }
    else {
        rg_main.glitches = NULL;
        show_notification("Step restore failed (no backup)", RG_MSG_ERROR);
    }

    game_reset();
}

// "Отрисовка" меню
static void menu_show(void) {
    if (!menu_visible || !menu.current || !rg_cbs.environ_cb) 
        return;

    char menu_text[128] = {0}; // <= 128 !

    for (int i = 0; i < menu.current->item_count; i++) {
        const char* label = menu.current->items[i].get_label ? menu.current->items[i].get_label() : menu.current->items[i].label;

        if (label[0]) {
            char buf[64];
            snprintf(buf, sizeof(buf), " %s %s\n", (i == menu.current->selected_index) ? "<>" : " .  ", label);
            strcat(menu_text, buf);
        }
    }

    struct retro_message_ext msg = {
        .msg = menu_text,                   // текст сообщения <= 128 !
        .duration = 40000,                  // время отображения в мс
        .priority = 2,                      // приоритет очереди отображения
        .level = RETRO_LOG_DEBUG,           // уровень сообщения
        .target = RETRO_MESSAGE_TARGET_OSD, // только на экран
        .type = RETRO_MESSAGE_TYPE_STATUS,  // тип (в каком месте экрана выводится)
        .progress = -1                      // прогресс бар
    };

    rg_cbs.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);

    // строка с логами внизу экрана
    snprintf(log_text, sizeof(log_text), "Steps:%u (%s)  |  Candidates:%u/%u  |  %u%%  |  Range swap:%u rem:%u",
        rg_main.step_count, rg_main.localizing ? "local" : "search", rg_main.glitch_count,
        rg_main.total_glitch_count, final_percent, rg_main.range_start, rg_main.range_size);

    struct retro_message msg_under = { log_text, 2400 }; // {текст, время отображения в кадрах} (2400 = 40 сек при 60 Гц)
    rg_cbs.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg_under);
    memset(log_text, 0, sizeof(log_text));
}

// cкрыть меню
static void menu_hide(void) {
    menu_visible = false;
    if (rg_cbs.environ_cb) {
        struct retro_message_ext clear_msg = {
            .msg = "", .duration = 1, .priority = 10, .level = RETRO_LOG_DEBUG,
            .target = RETRO_MESSAGE_TARGET_OSD, .type = RETRO_MESSAGE_TYPE_STATUS,
            .progress = -1 };
        rg_cbs.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &clear_msg);
    }
}

// опрос кнопок геймпада и обработка ввода
void rg_input_processing(void)
{   
    if (need_load_state) {
        need_load_state = false;
        game_load_state_from_ram();
        return;
    }

    if (!rg_cbs.input_poll_cb || !rg_cbs.input_state_cb)
        return;

    rg_cbs.input_poll_cb();

    static struct {
        bool was_pressed;
        bool is_processed;
    } button_states[5] = { 0 }; // 0:Menu, 1:Prev, 2:Next, 3:Confirm, 4:Cancel

    bool menu_key = rg_cbs.input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, rg_menu_button);

    if (!menu_key && button_states[0].was_pressed && !button_states[0].is_processed) {
        menu_visible = !menu_visible;
        if (!menu_visible)
            menu_hide();
        else {
            menu.current = rg_main.launch ? &menu.main : &menu.launch;
            menu_show();
        }

        button_states[0].is_processed = true;
    }
    button_states[0].was_pressed = menu_key;
    if (menu_key) button_states[0].is_processed = false;

    if (menu_visible) {
        bool prev_key = rg_cbs.input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
        bool next_key = rg_cbs.input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
        bool confirm_key = rg_cbs.input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
        bool cancel_key = rg_cbs.input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);

        if (!prev_key && button_states[1].was_pressed && !button_states[1].is_processed) {
            if (menu.current) {
                menu.current->selected_index =
                    (menu.current->selected_index - 1 + menu.current->item_count) % menu.current->item_count;
            }
            button_states[1].is_processed = true;
            menu_show();
        }
        button_states[1].was_pressed = prev_key;
        if (prev_key) button_states[1].is_processed = false;

        if (!next_key && button_states[2].was_pressed && !button_states[2].is_processed) {
            if (menu.current) {
                menu.current->selected_index = (menu.current->selected_index + 1) % menu.current->item_count;
            }
            button_states[2].is_processed = true;
            menu_show();
        }
        button_states[2].was_pressed = next_key;
        if (next_key) button_states[2].is_processed = false;

        if (!confirm_key && button_states[3].was_pressed && !button_states[3].is_processed) {
            if (menu.current) {
                rom_glitcher_menu_items_t* item = &menu.current->items[menu.current->selected_index];
                if (item->action) item->action();
            }
            button_states[3].is_processed = true;
            menu_show();
        }
        button_states[3].was_pressed = confirm_key;
        if (confirm_key) button_states[3].is_processed = false;

        if (!cancel_key && button_states[4].was_pressed && !button_states[4].is_processed) {
            if (menu.current == &menu.settings || menu.current == &menu.list) {
                menu.current = rg_main.launch ? &menu.main : &menu.launch;
            }
            else
                menu_hide();

            button_states[4].is_processed = true;
            menu_show();
        }
        button_states[4].was_pressed = cancel_key;
        if (cancel_key) button_states[4].is_processed = false;
    }
    else {
        for (int i = 1; i < 5; i++) {
            button_states[i].was_pressed = false;
            button_states[i].is_processed = false;
        }
    }
}
// ************************************************************* MENU END

static uint16_t get_rom_checksum(uint8* rom, int length)
{
    uint16_t checksum = 0;

    for (int i = 0; i < length; i += 2)
        checksum += ((rom[i] << 8) + rom[i + 1]);

    return checksum;
}

static void apply_glitches(void) {
    // изменения в ROM при отсеивании кандидатов
    for (uint32_t i = 0; i < rg_main.glitch_count; i++)
        cart.rom[rg_main.glitches[i].address] = rg_main.glitches[i].mod_value;

    // применение активированных глитчей из списка найденных
    for (uint8_t i = 0; i < RG_GLITCH_SLOTS_MAX; i++)
        if (found_glitches.activate[i] && found_glitches.virt_address[i])
            cart.rom[found_glitches.virt_address[i]] = found_glitches.mod_value[i];

    if (action_after == RG_LOAD_STATE)
        need_load_state = true;
    else if (action_after == RG_HARD_RESET)
        show_notification("Hard reset", RG_MSG_INFO);

    // если заголовок не был удалён эмулятором
    if (!rom_has_header) {
        // пересчёт контрольной суммы в заголовке игры после применения глитчей
        uint16_t real_checksum = get_rom_checksum(((uint8*)cart.rom) + 0x200, cart.romsize - 0x200);
        cart.rom[0x18E] = (real_checksum >> 8) & 0xFF;
        cart.rom[0x18F] = real_checksum & 0xFF;
    }
}

// Инициализация глитчера (поиск всех BEQ(0x67)/BNE(0x66) с проверками)
void rg_init(uint8_t* rom_data, uint32_t size) {
    if (rg_main.init_done) {
        apply_glitches();
        return;
    }

    // если заголовок не был удалён эмулятором
    if (!rom_has_header) {
        uint16 header_checksum = (cart.rom[0x18E] << 8) | cart.rom[0x18F];
        uint16 real_checksum = get_rom_checksum(((uint8*)cart.rom) + 0x200, cart.romsize - 0x200);
        // обновление контрольной суммы в заголовке
        if (header_checksum != real_checksum) {
            cart.rom[0x18E] = (real_checksum >> 8) & 0xFF;
            cart.rom[0x18F] = real_checksum & 0xFF;
            show_notification("Checksum in header has been corrected", RG_MSG_INFO);
        }
    }

    for (int i = 0; i < RG_BACKUP_SLOTS_MAX; i++) {
        if (rg_backups[i].glitches_backup) {
            free(rg_backups[i].glitches_backup);
            rg_backups[i].glitches_backup = NULL;
        }
    }
    rg_backup_index = 0;
    rg_backup_count = 0;

    if (rg_main.glitches) {
        free(rg_main.glitches);
        rg_main.glitches = NULL;
    }

    rg_main.init_done = false;
    rg_main.glitch_count = 0;
    rg_main.capacity = 1024;
    rg_main.glitches = malloc(rg_main.capacity * sizeof(rom_glitch_t));

    uint32_t trim = rom_has_header ? 0 : 0x200;
    if (size <= trim) {
        show_notification("ROM too small", RG_MSG_ERROR);
        return;
    }

    // Читаем нормализованный ROM
    for (uint32_t byte_addr = trim; byte_addr + 1 < size; byte_addr += 2) {
        uint8_t high_byte = 0;
        uint8_t low_byte = 0;
        int32_t target = 0;

        high_byte = rom_data[byte_addr];
        low_byte = rom_data[byte_addr + 1];

        // Ищем только BEQ(0x67) и BNE(0x66)
        if (high_byte != 0x66 && high_byte != 0x67) 
            continue;

        // Простая проверка адреса смещения на чётность (как в оригинале)
        //if ((low_byte & 1) != 0) continue;

        // Дополнительная проверка целевого адреса назначения инструкции
        if ((low_byte & 1) == 0) {
            // Чётное смещение - короткая форма инструкции
            int8_t disp8 = (int8_t)low_byte;
            uint32_t pc_next = byte_addr + 2;
            target = (int32_t)pc_next + (int32_t)disp8;
        }
        else {
            // Нечётное смещение - расширенная форма
            if (byte_addr + 3 >= size)
                continue;

            uint8_t ext_hi, ext_lo;
            ext_hi = rom_data[byte_addr + 2];
            ext_lo = rom_data[byte_addr + 3];
            int16_t disp16 = (int16_t)((ext_hi << 8) | ext_lo);
            uint32_t pc_next = byte_addr + 4;
            target = (int32_t)pc_next + (int32_t)disp16;
        }

        // Проверка целевого адреса на попадание в ROM и чётность
        if (target < (int32_t)trim || target >= (int32_t)size || (target & 1)) 
            continue;

        // Правдоподобная BEQ/BNE инструкция
        if (rg_main.glitch_count >= rg_main.capacity) {
            rg_main.capacity *= 2;
            rg_main.glitches = realloc(rg_main.glitches,
                rg_main.capacity * sizeof(rom_glitch_t));
        }

        rg_main.glitches[rg_main.glitch_count].address = byte_addr;
        rg_main.glitches[rg_main.glitch_count].initial_value = high_byte;
        rg_main.glitches[rg_main.glitch_count].mod_value = high_byte;
        rg_main.glitch_count++;
    }

    final_percent = 0;
    need_load_state = false;
    menu.current = &menu.launch;
    rg_main.total_glitch_count = rg_main.glitch_count;
    rg_main.step_count = 0;
    rg_main.bug_step_count = 0;
    rg_main.localizing = false;
    rg_main.launch = false;
    rg_main.range_start = 0;
    rg_main.range_size = 0;
    rg_main.init_done = rg_main.glitch_count ? true : false;

    //выбор размера стартового окна в зависимости от общего количества найденных команд,
    //чтобы изначально "мягче" ломать игру с большим количеством команд
    //if (rg_main.total_glitch_count < 512) // !!! значение наугад, потом надо поправить
    //    rg_main.range_size = (rg_main.total_glitch_count + 1) / 2; // 1/2
    //else if (rg_main.total_glitch_count < 1024) // !!! значение наугад, потом надо поправить
    //    rg_main.range_size = (rg_main.total_glitch_count + 3) / 4; // 1/4
    //else if (rg_main.total_glitch_count < 2048) // !!! значение наугад, потом надо поправить
    //    rg_main.range_size = (rg_main.total_glitch_count + 7) / 8; // 1/8
    //else
    //    rg_main.range_size = (rg_main.total_glitch_count + 15) / 16; // 1/16
    
    char s[64];
    snprintf(s, sizeof(s), "Candidates %u%s", rg_main.glitch_count, rg_main.init_done ? "" : " NOT FOUND");
    show_notification(s, rg_main.init_done ? RG_MSG_INFO : RG_MSG_ERROR);
}

// сохранение предыдущего состояния отсеивания кандидатов
static void create_search_backup(void) {
    rom_glitcher_backup_t* slot = &rg_backups[rg_backup_index];

    if (slot->glitches_backup) {
        free(slot->glitches_backup);
        slot->glitches_backup = NULL;
    }

    slot->backup = rg_main;

    if (rg_main.glitch_count > 0 && rg_main.glitches) {
        slot->glitches_backup = malloc(sizeof(rom_glitch_t) * rg_main.glitch_count);
        if (slot->glitches_backup) {
            memcpy(slot->glitches_backup, rg_main.glitches,
                sizeof(rom_glitch_t) * rg_main.glitch_count);

            slot->backup.glitches = slot->glitches_backup;
        }
        else {
            slot->backup.glitches = NULL;
            slot->backup.glitch_count = 0;
            show_notification("Previous step NOT saved (memory)", RG_MSG_ERROR);
        }
    }
    else {
        slot->glitches_backup = NULL;
        show_notification("Previous step NOT saved (no data)", RG_MSG_ERROR);
    }

    rg_backup_index = (rg_backup_index + 1) % RG_BACKUP_SLOTS_MAX;
    if (rg_backup_count < RG_BACKUP_SLOTS_MAX)
        rg_backup_count++;
}

// восстановить оригинальные значения инструкций
static void restore_instructions(void) {
    if (!rg_main.glitches) return;

    for (uint32_t i = 0; i < rg_main.glitch_count; i++)
        rg_main.glitches[i].mod_value = rg_main.glitches[i].initial_value;
}

// сброс состояния глитчера
static void rg_reset(void) { 
    restore_instructions();
    rg_main.init_done = false;
    game_reset();
}

// очистка памяти при закрытии ядра
void rg_deinit(void) {
    if (rg_main.glitches) {
        free(rg_main.glitches);
        rg_main.glitches = NULL;
    }

    for (int i = 0; i < RG_BACKUP_SLOTS_MAX; i++) {
        if (rg_backups[i].glitches_backup) {
            free(rg_backups[i].glitches_backup);
            rg_backups[i].glitches_backup = NULL;
        }
    }
    rg_backup_index = 0;
    rg_backup_count = 0;

    memset(&rg_main, 0, sizeof(rg_main));
    action_after = RG_LOAD_STATE;
    need_load_state = false;
    final_percent = 0;
    found_glitches.count = 0;
    memset(found_glitches.virt_address, 0, sizeof(found_glitches.virt_address));
    memset(found_glitches.real_address, 0, sizeof(found_glitches.real_address));
    memset(found_glitches.initial_value, 0, sizeof(found_glitches.initial_value));
    menu_hide();
    menu.current = NULL;
}

// Преобразование найденного адреса глитча из
// виртуального ROM эмулятора в реальный ROM адрес
static uint32_t ram_to_rom_offset(uint32_t address) {
    uint32_t file_offset = address;

    if (rom_was_deinterleaved) { // если к ROM применялся деинтерлив
        uint32_t block = address / 0x4000;
        uint32_t offset = address % 0x4000;

        if ((offset % 2) == 0)
            file_offset = block * 0x4000 + 0x2000 + (offset / 2);
        else
            file_offset = block * 0x4000 + (offset / 2);
    }

    if (rom_has_header) // если у ROM был удалён заголовок
        file_offset += 512;

    if (rom_is_byte_swapped) // если ROM был изначально байт-свапнут
        file_offset ^= 1;

    if (rom_in_mdx) // если ROM был в формате MDX
        file_offset += 4;

    return file_offset;
}

// ищет папку с читами для сохранения файла (если не находит, то использует папку с ROM)
static uint8_t add_glitch_as_cheat_to_file(uint32_t virt_address, uint32_t real_address, uint8_t initial_value, uint8_t mod_value) {
    char cheats_path[PATH_MAX] = { 0 };
    RFILE* f_cht = NULL;

#if defined(_WIN32)
    char slash = '\\';
#else
    char slash = '/';
#endif

    // ищем путь к файлу читов
    /*for (uint8_t try = 1; try < 4; try++) {
        if (try == 1) {
            RFILE* f = filestream_open("retroarch.cfg",
                RETRO_VFS_FILE_ACCESS_READ,
                RETRO_VFS_FILE_ACCESS_HINT_NONE);
            if (f) {
                char* line = NULL;
                while ((line = filestream_getline(f))) {
                    if (strncmp(line, "cheat_database_path", 19) == 0) {
                        char* val = strchr(line, '=');
                        if (!val) { free(line); continue; }
                        val++;
                        while (*val == ' ' || *val == '\t') val++;
                        if (*val == '\"') val++;
                        char* end = strchr(val, '\"');
                        if (end) *end = '\0';

                        if (val[0] == ':' && (val[1] == '/' || val[1] == '\\'))
                            snprintf(cheats_path, sizeof(cheats_path), "%s", val + 2);
                        else
                            snprintf(cheats_path, sizeof(cheats_path), "%s", val);

                        free(line);
                        break;
                    }
                    free(line);
                }
                filestream_close(f);

                if (cheats_path[0] && rg_last_game && rg_last_game->path) {
                    char rom_name[PATH_MAX] = { 0 };
                    const char* base_name = strrchr(rg_last_game->path, '/');
                    if (!base_name) base_name = strrchr(rg_last_game->path, '\\');
                    if (base_name) base_name++; else base_name = rg_last_game->path;
                    strncpy(rom_name, base_name, sizeof(rom_name) - 1);
                    char* ext = strrchr(rom_name, '.');
                    if (ext) *ext = '\0';

                    size_t len = strlen(cheats_path);
                    if (len && (cheats_path[len - 1] != '/' && cheats_path[len - 1] != '\\'))
                        strncat(cheats_path, &slash, 1);

                    strncat(cheats_path, rom_name, sizeof(cheats_path) - strlen(cheats_path) - 1);
                    strncat(cheats_path, "_RGI.cht", sizeof(cheats_path) - strlen(cheats_path) - 1);
                }
                else
                    continue;
            }
            else
                continue;
        }
        else if (try == 2) {
            if (rg_last_game && rg_last_game->path) {
                char rom_dir[PATH_MAX] = { 0 };
                char rom_name[PATH_MAX] = { 0 };

                strncpy(rom_dir, rg_last_game->path, sizeof(rom_dir) - 1);
                char* base = strrchr(rom_dir, '/');
                if (!base) base = strrchr(rom_dir, '\\');
                if (base) *base = '\0';

                const char* base_name = strrchr(rg_last_game->path, '/');
                if (!base_name) base_name = strrchr(rg_last_game->path, '\\');
                if (base_name) base_name++; else base_name = rg_last_game->path;
                strncpy(rom_name, base_name, sizeof(rom_name) - 1);
                char* ext = strrchr(rom_name, '.');
                if (ext) *ext = '\0';

                snprintf(cheats_path, sizeof(cheats_path),
                    "%s%c%s_RGI.cht", rom_dir, slash, rom_name);
            }
            else
                continue;
        }
        else if (try == 3) {
            show_notification("Failed to open cheat file", RG_MSG_ERROR);
            return 1;
        }

        f_cht = filestream_open(cheats_path,
            RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
        break;
    }*/
    
    if (!rg_last_game || !rg_last_game->path) {
        show_notification("No last game info", RG_MSG_ERROR);
        return 3;
    }

    char rom_dir[PATH_MAX] = { 0 };
    char rom_name[PATH_MAX] = { 0 };

    strncpy(rom_dir, rg_last_game->path, sizeof(rom_dir) - 1);
    char* base = strrchr(rom_dir, '/');
    if (!base) 
        base = strrchr(rom_dir, '\\');

    if (base) 
        *base = '\0';

    const char* base_name = strrchr(rg_last_game->path, '/');
    if (!base_name) 
        base_name = strrchr(rg_last_game->path, '\\');

    if (base_name) 
        base_name++; 
    else 
        base_name = rg_last_game->path;

    strncpy(rom_name, base_name, sizeof(rom_name) - 1);
    char* ext = strrchr(rom_name, '.');
    if (ext) *ext = '\0';

    snprintf(cheats_path, sizeof(cheats_path),
        "%s%c%s_RGI.cht", rom_dir, slash, rom_name);

    f_cht = filestream_open(cheats_path,
        RETRO_VFS_FILE_ACCESS_READ,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);

    // читаем файл
    char* file_content = NULL;
    int32_t size = 0;
    if (f_cht) {
        filestream_seek(f_cht, 0, RETRO_VFS_SEEK_POSITION_END);
        size = filestream_tell(f_cht);
        filestream_rewind(f_cht);

        if (size > 0) {
            file_content = (char*)calloc(1, size + 1);
            filestream_read(f_cht, file_content, size);
        }
        filestream_close(f_cht);
    }

    if (!file_content) {
        file_content = strdup("cheats = \"0\"\n");
        size = strlen(file_content);
    }

    // проверка на дубликат
    char search[64];
    snprintf(search, sizeof(search), "address = \"%u\"", real_address);
    if (strstr(file_content, search)) {
        free(file_content);
        return 2; // уже есть такой глитч в файле
    }

    // парсим количество читов
    int cheats_count = 0;
    char* cheats_pos = strstr(file_content, "cheats = ");
    if (cheats_pos) {
        char* num_start = strchr(cheats_pos, '"');
        if (num_start) {
            num_start++;
            cheats_count = atoi(num_start);
        }
    }

    // новый блок
    char new_entries[512];
    char desc[64];
    snprintf(desc, sizeof(desc),
        "Glitch #%u (0x%02X->0x%02X)", cheats_count, initial_value, mod_value);

    snprintf(new_entries, sizeof(new_entries),
        "cheat%d_desc = \"%s\"\n"
        "cheat%d_code = \"%06X:%02X\"\n"
        "cheat%d_enable = \"false\"\n\n",
        cheats_count, desc,
        cheats_count, real_address, mod_value,
        cheats_count);

    // собираем новый файл
    int new_count = cheats_count + 1;
    char* new_file = (char*)calloc(1, size + strlen(new_entries) + 64);

    if (cheats_pos) {
        char* line_end = strchr(cheats_pos, '\n');
        if (!line_end) line_end = cheats_pos + strlen(cheats_pos);

        *cheats_pos = '\0';

        snprintf(new_file, size + strlen(new_entries) + 64,
            "%scheats = \"%d\"\n%s%s",
            file_content,
            new_count,
            line_end + 1,
            new_entries);
    }
    else {
        snprintf(new_file, size + strlen(new_entries) + 64,
            "%scheats = \"%d\"\n%s",
            file_content,
            new_count,
            new_entries);
    }

    // сохраняем
    f_cht = filestream_open(cheats_path,
        RETRO_VFS_FILE_ACCESS_WRITE,
        RETRO_VFS_FILE_ACCESS_HINT_NONE);
    if (!f_cht) {
        free(file_content);
        free(new_file);
        return 4;
    }

    filestream_write(f_cht, new_file, strlen(new_file));
    filestream_close(f_cht);
    free(file_content);
    free(new_file);

    // уведомление
    //const char* path_to_show = cheats_path;
    //size_t len = strlen(cheats_path);
    //if (len > 128)
        //path_to_show = cheats_path + (len - 128);

    char tmp[164];
    //snprintf(tmp, sizeof(tmp), "Glitch saved to \"%s\"", path_to_show);
    snprintf(tmp, sizeof(tmp), "Glitch saved to \"%s\"", cheats_path);
    show_notification(tmp, RG_MSG_INFO);

    return 0;
}

// вывод информационных сообщений на экран
static void show_notification(const char* s, uint8_t context) {
    char msg_text[256];
    uint32_t duration = 5000;
    // 0, 1, 2, 3 ... 255, обычно от 0 до 3
    uint8_t priority = 1; 
    // RETRO_LOG_DEBUG, RETRO_LOG_INFO, RETRO_LOG_WARN, RETRO_LOG_ERROR
    uint8_t level = RETRO_LOG_DEBUG;
    // RETRO_MESSAGE_TYPE_NOTIFICATION, RETRO_MESSAGE_TYPE_NOTIFICATION_ALT, RETRO_MESSAGE_TYPE_STATUS, RETRO_MESSAGE_TYPE_PROGRESS
    //uint8_t type = RETRO_MESSAGE_TYPE_NOTIFICATION;

    if (context == RG_MSG_INFO) {
        snprintf(msg_text, sizeof(msg_text), "RG: %s.", s);
        priority = 1;
        level = RETRO_LOG_INFO;
    }
    else if (context == RG_MSG_ERROR) {
        snprintf(msg_text, sizeof(msg_text), "RG ERROR: %s.", s);
        priority = 4;
        level = RETRO_LOG_ERROR;
    }
    else if (context == RG_MSG_FOUND) {
        snprintf(msg_text, sizeof(msg_text), "RG FOUND: %s.", s);
        duration = 20000;
        priority = 5;
        level = RETRO_LOG_INFO;
    }

    struct retro_message_ext msg = {
        .msg = msg_text,                         // текст сообщения
        .duration = duration,                    // время отображения в мс
        .priority = priority,                    // приоритет очереди отображения
        .level = level,                          // уровень сообщения (иконка сообщения)
        .target = RETRO_MESSAGE_TARGET_OSD,      // только на экран
        .type = RETRO_MESSAGE_TYPE_NOTIFICATION, // тип
        .progress = -1                     // прогресс бар
    };

    rg_cbs.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);
}

// сброс игры
static void game_reset(void) {
    menu_hide();
    menu.current = NULL;
    retro_unload_game();
    retro_load_game(rg_last_game);
}

// сохранить save state игры в памяти для работы с глитчером
static void game_save_state_to_ram(void) {
    if (retro_serialize(game_state_buffer, sizeof(game_state_buffer)))
        show_notification("State saved to RAM", RG_MSG_INFO);
    else
        show_notification("State NOT saved", RG_MSG_ERROR);
}

// загрузить save state игры из памяти для работы с глитчером
static void game_load_state_from_ram(void) {
    menu_hide();

    if (retro_unserialize(game_state_buffer, sizeof(game_state_buffer))) {
        //show_notification("State loaded from RAM", RG_MSG_INFO);
    }
    else
        show_notification("State NOT loaded", RG_MSG_ERROR);
}

bool rg_get_menu_visible(void) { return menu_visible; }
void rg_set_rom_in_mdx(bool value) { rom_in_mdx = value; }
void rg_set_rom_is_byte_swapped(bool value) { rom_is_byte_swapped = value; }
void rg_set_rom_has_header(bool value) { rom_has_header = value; }
void rg_set_rom_was_interleaved(bool value) { rom_was_deinterleaved = value; }

// подсчёт контрольной суммы (при сохранении и загрузке файла)
//static uint32_t get_checksum_for_file(const uint8_t* data, size_t size) {
//    uint32_t sum = 0;
//    for (size_t i = 0; i < size; i++)
//        sum += data[i]; // !!! заменить на что-то другое
//    return sum;
//}

// сохранить save state игры в файл на диск (не используется)
//static bool game_save_state_to_file(const char* path) {
//    state_save(game_state_buffer);
//
//    FILE* f = fopen(path, "wb");
//    if (!f)
//        return false;
//
//    fwrite(RG_STATE_SIGNATURE, 1, 4, f);
//    fputc(RG_STATE_VERSION, f);
//
//    uint32_t size = STATE_SIZE;
//    fwrite(&size, sizeof(size), 1, f);
//
//    uint32_t checksum = get_checksum_for_file(game_state_buffer, STATE_SIZE);
//    fwrite(&checksum, sizeof(checksum), 1, f);
//
//    fwrite(game_state_buffer, 1, STATE_SIZE, f);
//    fclose(f);
//
//    return true;
//}

// загрузить save state игры из файла на диске (не используется)
//static bool game_load_state_from_file(const char* path) {
//    FILE* f = fopen(path, "rb");
//    if (!f)
//        return false;
//
//    char sig[4];
//    fread(sig, 1, 4, f);
//    if (memcmp(sig, RG_STATE_SIGNATURE, 4) != 0) {
//        fclose(f);
//        return false;
//    }
//
//    int version = fgetc(f);
//    if (version != RG_STATE_VERSION) {
//        fclose(f);
//        return false;
//    }
//
//    uint32_t size = 0;
//    fread(&size, sizeof(size), 1, f);
//    if (size != STATE_SIZE) {
//        fclose(f);
//        return false;
//    }
//
//    uint32_t checksum_file = 0;
//    fread(&checksum_file, sizeof(checksum_file), 1, f);
//
//    fread(game_state_buffer, 1, STATE_SIZE, f);
//    fclose(f);
//
//    uint32_t checksum_calc = get_checksum_for_file(game_state_buffer, STATE_SIZE);
//    if (checksum_calc != checksum_file)
//        return false;
//
//    return state_load(game_state_buffer);
//}

/*
// logs
if (rg_cbs.log_cb) {
    snprintf(log_text, sizeof(log_text), "");
    rg_cbs.log_cb(RETRO_LOG_INFO, "%s\n", log_text);
}

if (rg_cbs.log_cb) {
            snprintf(log_text, sizeof(log_text),
                "%u(0x%02X)  ",
                rg_main.glitches[rg_main.glitch_count].address,
                rg_main.glitches[rg_main.glitch_count].initial_value);
            rg_cbs.log_cb(RETRO_LOG_DEBUG, "%s", log_text);
}
*/
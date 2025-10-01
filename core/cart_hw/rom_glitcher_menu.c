// rom_glitcher_menu.c

#include "rom_glitcher_menu.h"
#include "rom_glitcher_translation.h"

static const char* get_label_launch_glitcher(uint8_t index);
static const char* get_label_stop_glitcher(uint8_t index);
static const char* get_label_options(uint8_t index);
static const char* get_label_game_save_state(uint8_t index);
static const char* get_label_pause_effect(uint8_t index);
static const char* get_label_search(uint8_t index);
static const char* get_label_list_of_found(uint8_t index);
static const char* get_label_inst_allowed(uint8_t index);
static const char* get_label_inst_group(uint8_t index);
static const char* get_label_menu_list_of_found(uint8_t index);
static const char* get_label_menu_inst_allowed(uint8_t index);
static void menu_item_pause_effect(void);
static void menu_item_toggle_found_instruction(void);
static void menu_item_inst_allowed(void);
static void menu_item_prev_page(void);
static void menu_item_next_page(void);
static void menu_item_open_options(void);
static void menu_item_open_list_of_found(void);
static void menu_item_open_command_allowed(void);
static void menu_item_open_inst_bcc(void);
static void menu_item_open_inst_scc(void);
static void menu_item_open_inst_dbcc(void);
static void menu_item_open_inst_add_sub(void);
static void menu_item_game_save_state(void);

static bool refresh_menu_log = true;

static rom_glitcher_menu_item_t menu_launch[] = {
    { get_label_launch_glitcher, rg_launch_glitcher},           // Launch Glitcher
    { get_label_list_of_found, menu_item_open_list_of_found },  // List of found
    { get_label_inst_allowed, menu_item_open_command_allowed }  // Instruction Filter
};

static rom_glitcher_menu_item_t menu_search[] = {
    { get_label_search, NULL },                                 // BUG NOT_FOUND FOUND Step_back
    { get_label_list_of_found, menu_item_open_list_of_found },  // List of found
    { get_label_stop_glitcher, rg_force_stop },                 // Stop Glitcher
    { get_label_options, menu_item_open_options }               // Options
};

static rom_glitcher_menu_item_t menu_options[] = {
    { get_label_game_save_state, menu_item_game_save_state },   // Save state
    { get_label_pause_effect, menu_item_pause_effect }          // Pause effect
};

static rom_glitcher_menu_item_t menu_found[] = {
    { get_label_menu_list_of_found, menu_item_prev_page },             // cur/prev
    { get_label_menu_list_of_found, menu_item_toggle_found_instruction }, // found 1
    { get_label_menu_list_of_found, menu_item_toggle_found_instruction }, // found 2
    { get_label_menu_list_of_found, menu_item_toggle_found_instruction }, // found 3
    { get_label_menu_list_of_found, menu_item_toggle_found_instruction }, // found 4
    { get_label_menu_list_of_found, menu_item_toggle_found_instruction }, // found 5
    { get_label_menu_list_of_found, menu_item_toggle_found_instruction }, // found 6
    { get_label_menu_list_of_found, menu_item_next_page }              // next
};

static rom_glitcher_menu_item_t menu_command[] = {
    { get_label_inst_group, menu_item_open_inst_bcc },      // Bcc
    { get_label_inst_group, menu_item_open_inst_scc },      // Scc
    { get_label_inst_group, menu_item_open_inst_dbcc },     // DBcc
    { get_label_inst_group, menu_item_open_inst_add_sub }   // Arithmetic '+-'
};

static rom_glitcher_menu_item_t menu_inst_bcc[] = {
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // BHI/BLS
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // BCC/BCS
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // BNE/BEQ
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // BVC/BVS
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // BPL/BMI
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // BGE/BLT
    { get_label_menu_inst_allowed, menu_item_inst_allowed }    // BGT/BLE
};

static rom_glitcher_menu_item_t menu_inst_scc[] = {
    { get_label_menu_inst_allowed, NULL },                     //  -skip-
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // SHI/SLS
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // SCC/SCS
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // SNE/SEQ
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // SVC/SVS
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // SPL/SMI
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // SGE/SLT
    { get_label_menu_inst_allowed, menu_item_inst_allowed }    // SGT/SLE
};

static rom_glitcher_menu_item_t menu_inst_dbcc[] = {
    { get_label_menu_inst_allowed, NULL },                     // -skip-
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // DBHI/DBLS
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // DBCC/DBCS
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // DBNE/DBEQ
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // DBVC/DBVS
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // DBPL/DBMI
    { get_label_menu_inst_allowed, menu_item_inst_allowed },   // DBGE/DBLT
    { get_label_menu_inst_allowed, menu_item_inst_allowed }    // DBGT/DBLE
};

static rom_glitcher_menu_item_t menu_inst_add_sub[] = {
    { get_label_menu_inst_allowed, menu_item_inst_allowed},   // ADD/SUB
    { get_label_menu_inst_allowed, menu_item_inst_allowed},   // ADDX/SUBX
    { get_label_menu_inst_allowed, menu_item_inst_allowed},   // ADDA/SUBA
    { get_label_menu_inst_allowed, menu_item_inst_allowed},   // ADDI/SUBI
    { get_label_menu_inst_allowed, menu_item_inst_allowed}    // ADDQ/SUBQ
};

rom_glitcher_menu_manager_t rg_menu = {
    .current = NULL,
    .launch = { menu_launch, ARRAY_SIZE(menu_launch), 0 },
    .search = { menu_search, ARRAY_SIZE(menu_search), 0 },
    .options = { menu_options, ARRAY_SIZE(menu_options), 0 },
    .found = { menu_found, ARRAY_SIZE(menu_found), 1 },
    .command = { menu_command, ARRAY_SIZE(menu_command), 0 },
    .inst_bcc = { menu_inst_bcc, ARRAY_SIZE(menu_inst_bcc), 2 },
    .inst_scc = { menu_inst_scc, ARRAY_SIZE(menu_inst_scc), 0 },
    .inst_dbcc = { menu_inst_dbcc, ARRAY_SIZE(menu_inst_dbcc), 0 },
    .inst_add_sub = { menu_inst_add_sub, ARRAY_SIZE(menu_inst_add_sub), 0 }
};

static const char* get_label_launch_glitcher(uint8_t index) { return TR(RG_TR_LAUNCH_GLITCHER); }
static const char* get_label_stop_glitcher(uint8_t index) { return TR(RG_TR_STOP_GLITCHER); }
static const char* get_label_options(uint8_t index) { return TR(RG_TR_OPTIONS); }
static const char* get_label_game_save_state(uint8_t index) { return TR(RG_TR_SAVE_STATE); }
static const char* get_label_pause_effect(uint8_t index) { return TR(RG_TR_PAUSE_EFFECT); }

static const char* get_label_search(uint8_t index) {
    static char buf[96];
    snprintf(buf, sizeof(buf), TR(RG_TR_SEARCH_HEADER),
        rg_button_states[4].was_pressed ? "#" : "B",
        rg_button_states[5].was_pressed ? "#" : "X",
        rg_button_states[3].was_pressed ? "#" : "A",
        rg_button_states[6].was_pressed ? "#" : "Y");
    return buf;
}

static const char* get_label_list_of_found(uint8_t index) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "%s:%u/%u", TR(RG_TR_LIST_OF_FOUND), 
        rg_found_glitches.enabled_count, 
        rg_found_glitches.count);
    return buf;
}

static const char* get_label_inst_allowed(uint8_t index) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "%s:%u/%u", TR(RG_TR_INSTRUCTION_FILTER), 
        (uint8_t)__builtin_popcount(rg_inst_allowed),
        TOTAL_INST_BITS_USED);
    return buf;
}

static const char* get_label_inst_group(uint8_t index) {
    static char buf[8][64];
    const char* label = "";
    uint32_t mask = 0;
    uint8_t total_count = 0;

    if (index == 0) {
        label = TR(RG_TR_INSTRUCTION_BRANCHING);
        // Условные переходы (Bcc) - биты 0-6
        mask = (1 << INST_BHI_BLS) | (1 << INST_BCC_BCS) | (1 << INST_BNE_BEQ) |
            (1 << INST_BVC_BVS) | (1 << INST_BPL_BMI) | (1 << INST_BGE_BLT) |
            (1 << INST_BGT_BLE);
        total_count = rg_menu.inst_bcc.item_count;
    }
    else if (index == 1) {
        label = TR(RG_TR_INSTRUCTION_SETTING);
        // Установка (Scc) - биты 9-15
        mask = (1 << INST_SHI_SLS) | (1 << INST_SCC_SCS) | (1 << INST_SNE_SEQ) |
            (1 << INST_SVC_SVS) | (1 << INST_SPL_SMI) | (1 << INST_SGE_SLT) |
            (1 << INST_SGT_SLE);
        total_count = rg_menu.inst_scc.item_count;
    }
    else if (index == 2) {
        label = TR(RG_TR_INSTRUCTION_LOOPING);
        // Циклы (DBcc) - биты 17-23
        mask = (1 << INST_DBHI_DBLS) | (1 << INST_DBCC_DBCS) | (1 << INST_DBNE_DBEQ) |
            (1 << INST_DBVC_DBVS) | (1 << INST_DBPL_DBMI) | (1 << INST_DBGE_DBLT) |
            (1 << INST_DBGT_DBLE);
        total_count = rg_menu.inst_dbcc.item_count;
    }
    else if (index == 3) {
        label = TR(RG_TR_INSTRUCTION_ARITHMETIC);
        // Арифметические - биты 24-28
        mask = (1 << INST_ADD_SUB) | (1 << INST_ADDX_SUBX) | (1 << INST_ADDA_SUBA) |
            (1 << INST_ADDI_SUBI) | (1 << INST_ADDQ_SUBQ);
        total_count = rg_menu.inst_add_sub.item_count;
    }


    uint8_t active_count = (uint8_t)__builtin_popcount(rg_inst_allowed & mask);
    snprintf(buf[index], sizeof(buf[0]), "%s:%u/%u", label, active_count, total_count);
    return buf[index];
}

static const char* get_label_menu_list_of_found(uint8_t index) {
    static char buf[8][20];

    if (index == 0) { // prev page
        uint8_t prev_page = (rg_found_glitches.total_pages == 0) ? 0 :
            (rg_found_glitches.current_page + rg_found_glitches.total_pages - 1) % rg_found_glitches.total_pages;

        snprintf(buf[index], sizeof(buf[index]),
            TR(RG_TR_PREV_PAGE), rg_found_glitches.current_page + 1,
            (rg_found_glitches.total_pages > 0) ? (prev_page + 1) : 0);
        return buf[index];
    }

    if (index >= 1 && index <= RG_FOUND_GLITCH_PER_PAGE) {
        uint16_t i = rg_found_glitches.current_page * RG_FOUND_GLITCH_PER_PAGE + (index - 1);
        if (i < rg_found_glitches.count && rg_found_glitches.real_address[i]) {
            snprintf(buf[index], sizeof(buf[index]), "%06X-%s",
                rg_found_glitches.real_address[i],
                rg_found_glitches.enabled[i] ? TR(RG_TR_ON) : TR(RG_TR_OFF));
        }
        else {
            snprintf(buf[index], sizeof(buf[index]), " - - -");
        }

        return buf[index];
    }

    if (index == 7) { // next page
        uint8_t next_page = (rg_found_glitches.total_pages == 0) ? 0 :
            (rg_found_glitches.current_page + 1) % rg_found_glitches.total_pages;

        snprintf(buf[index], sizeof(buf[index]),
            TR(RG_TR_NEXT_PAGE), (rg_found_glitches.total_pages > 0) ? (next_page + 1) : 0);
        return buf[index];
    }

    return "";
}

static const char* get_label_menu_inst_allowed(uint8_t index) {
    static char buf[8][20];
    uint8_t new_index = 0;

    if (rg_menu.current == &rg_menu.inst_bcc)
        new_index = index;
    else if (rg_menu.current == &rg_menu.inst_scc)
        new_index = index + 8;
    else if (rg_menu.current == &rg_menu.inst_dbcc)
        new_index = index + 16;
    else if (rg_menu.current == &rg_menu.inst_add_sub)
        new_index = index + 24;

    snprintf(buf[index], sizeof(buf[index]), "%s-%s", rg_instr_mnemonic[new_index],
        (rg_inst_allowed & (1 << new_index)) ? TR(RG_TR_ON) : TR(RG_TR_OFF));
    return buf[index];
}

static void menu_item_pause_effect(void) {
    rg_pause_effect = (rg_pause_effect + 1) % 5;
}

static void menu_item_toggle_found_instruction(void) {
    int index = rg_found_glitches.current_page * RG_FOUND_GLITCH_PER_PAGE
        + (rg_menu.current->selected_index - 1);

    if (rg_found_glitches.count == 0 || index >= rg_found_glitches.count)
        return;

    rg_found_glitches.enabled[index] = !rg_found_glitches.enabled[index];
    rg_found_glitches.enabled_count = 0;
    
    for (int i = 0; i < rg_found_glitches.count; i++)
        if (rg_found_glitches.enabled[i])
            rg_found_glitches.enabled_count++;

    // Обновляем файл с читами
    char cheats_path[RG_PATH_SIZE] = { 0 };
    RFILE* f_cht = NULL;

#if defined(_WIN32)
    char slash = '\\';
#else
    char slash = '/';
#endif

    snprintf(cheats_path, sizeof(cheats_path), "%s%c%s_RG.cht", g_rom_dir, slash, g_rom_name);
    f_cht = filestream_open(cheats_path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

    if (!f_cht) {
        rg_found_glitches_modified = true;
        return; // Файл не существует
    }

    filestream_seek(f_cht, 0, RETRO_VFS_SEEK_POSITION_END);
    int32_t size = filestream_tell(f_cht);
    filestream_rewind(f_cht);

    if (size <= 0) {
        filestream_close(f_cht);
        rg_found_glitches_modified = true;
        return; // Пустой файл
    }

    char* file_content = (char*)calloc(1, size + 1);
    filestream_read(f_cht, file_content, size);
    filestream_close(f_cht);

    // Ищем адрес в файле
    char search[32];
    snprintf(search, sizeof(search), "%06X:%02X", rg_found_glitches.real_address[index], rg_found_glitches.mod_value[index]);
    char* found_pos = strstr(file_content, search);

    if (found_pos) {
        // Ищем начало блока с этим читом (ищем номер чита)
        char* block_start = found_pos;
        while (block_start > file_content) {
            if (strstr(block_start, "cheat") && strstr(block_start, "_code")) {
                break;
            }
            block_start--;
        }

        // Извлекаем номер чита
        if (block_start > file_content && strstr(block_start, "cheat")) {
            char* num_start = strstr(block_start, "cheat") + 5;
            int cheat_num = atoi(num_start);

            if (cheat_num > 0) {
                // Ищем строку enable для этого чита
                char enable_pattern[32];
                snprintf(enable_pattern, sizeof(enable_pattern), "cheat%d_enable = \"", cheat_num);
                char* enable_pos = strstr(file_content, enable_pattern);

                if (enable_pos) {
                    char* value_start = enable_pos + strlen(enable_pattern);
                    char* value_end = strchr(value_start, '"');

                    if (value_end) {
                        // Создаем новый файл с измененным значением
                        size_t prefix_len = value_start - file_content;
                        size_t suffix_len = strlen(value_end + 1);

                        char* new_file = (char*)malloc(prefix_len + 6 + suffix_len + 1);

                        memcpy(new_file, file_content, prefix_len);
                        if (rg_found_glitches.enabled[index])
                            strcpy(new_file + prefix_len, "true");
                        else
                            strcpy(new_file + prefix_len, "false");

                        strcpy(new_file + prefix_len + (rg_found_glitches.enabled[index] ? 4 : 5), value_end);

                        // Сохраняем измененный файл
                        f_cht = filestream_open(cheats_path, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
                        if (f_cht) {
                            filestream_write(f_cht, new_file, strlen(new_file));
                            filestream_close(f_cht);
                        }

                        free(new_file);
                    }
                }
            }
        }
    }

    free(file_content);
    rg_found_glitches_modified = true;
}

static void menu_item_inst_allowed(void) {
    rom_glitcher_menu_t* saved_menu = rg_menu.current; 
    uint8_t index = 0;

    if (rg_menu.current == &rg_menu.inst_bcc)
        index = 0;
    else if (rg_menu.current == &rg_menu.inst_scc)
        index += 8;
    else if (rg_menu.current == &rg_menu.inst_dbcc)
        index += 16;
    else if (rg_menu.current == &rg_menu.inst_add_sub)
        index += 24;

    rg_inst_allowed ^= (1 << (rg_menu.current->selected_index + index));
    rg_game_save_state();
    rg_main.init_done = false;
    rg_clear_bug_range = true;
    rg_game_reset();
    rg_game_load_state();
    rg_menu_visible = true;
    rg_menu.current = saved_menu;
}

static void menu_item_prev_page(void) {
    rg_found_glitches.current_page = (rg_found_glitches.current_page +
        rg_found_glitches.total_pages - 1) % rg_found_glitches.total_pages;
}

static void menu_item_next_page(void) {
    rg_found_glitches.current_page = (rg_found_glitches.current_page + 1) % rg_found_glitches.total_pages;
}

static void menu_item_open_options(void) { rg_menu.current = &rg_menu.options; }
static void menu_item_open_list_of_found(void) { rg_menu.current = &rg_menu.found; }
static void menu_item_open_command_allowed(void) { rg_menu.current = &rg_menu.command; }
static void menu_item_open_inst_bcc(void) { rg_menu.current = &rg_menu.inst_bcc; }
static void menu_item_open_inst_scc(void) { rg_menu.current = &rg_menu.inst_scc; }
static void menu_item_open_inst_dbcc(void) { rg_menu.current = &rg_menu.inst_dbcc; }
static void menu_item_open_inst_add_sub(void) { rg_menu.current = &rg_menu.inst_add_sub; }

static void menu_item_game_save_state(void) {
    rg_game_save_state();
    rg_input_replay.record = true;
    rg_input_replay.play = false;
    rg_input_replay.length = 0;
}

// "Отрисовка" меню
void rg_menu_show(void) {
    static uint8_t frame_skip;

    if (!rg_menu_visible || !rg_menu.current || !environ_cb || frame_skip % 4 != 0) {
        frame_skip++;
        return;
    }
    frame_skip++;

    char menu_text[128] = { 0 };

    for (uint8_t i = 0; i < rg_menu.current->item_count; i++) {
        const char* label = rg_menu.current->items[i].get_label(i);
        char buf[128];

        snprintf(buf, sizeof(buf), "%s %s\n", 
            (i == rg_menu.current->selected_index) ? "<>" : " . ", 
            (label && label[0]) ? label : "");
        strncat(menu_text, buf, sizeof(menu_text) - strlen(menu_text) - 1);
    }

    uint8_t len = strlen(menu_text);
    while (len < 126)
        menu_text[len++] = ' '; // чтобы меню было по левому краю

    struct retro_message_ext msg = {
        .msg = menu_text,                   // текст сообщения < 128 !
        .duration = 124,                    // время отображения в мс
        .priority = 2,                      // приоритет очереди отображения
        .level = RETRO_LOG_INFO,            // уровень сообщения
        .target = RETRO_MESSAGE_TARGET_OSD, // только на экран
        .type = RETRO_MESSAGE_TYPE_STATUS,  // тип (в каком месте экрана выводится)
        .progress = -1                      // прогресс бар
    };

    environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg);

    if (refresh_menu_log) {
        refresh_menu_log = false;
        char log_text[164];
        uint32_t percent = (rg_main.range_size * 1000) / rg_main.glitch_count;
        char filter[256] = { 0 };

        for (int i = 0; i < TOTAL_INST_BITS; i++)
            if (rg_inst_allowed & (1 << i))
                snprintf(filter + strlen(filter), sizeof(filter) - strlen(filter), "%s ", rg_instr_mnemonic[i]);

        snprintf(log_text, sizeof(log_text), "%s %u (%s)  |  %s %u/%u  |  %u.%u%%  |  %s %u:%u  |  %s",
            TR(RG_TR_STEP),
            rg_main.step_count, rg_bug_glitches.count ? TR(RG_TR_BUG) : rg_main.localizing ? TR(RG_TR_LOCAL) : TR(RG_TR_SEARCH),
            TR(RG_TR_CANDIDATES),
            rg_main.glitch_count, rg_total_glitch_count,
            (rg_main.range_size > 0) ? percent / 10 : 0, (rg_main.range_size > 0) ? percent % 10 : 0,
            TR(RG_TR_RANGE),
            rg_main.range_start, rg_main.range_size, filter);
        struct retro_message msg_log = { log_text, 100 * rg_fps }; // {текст, время отображения в кадрах}
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg_log);
    }
}

// cкрыть меню
void rg_menu_hide(void) {
    if (environ_cb) {
        struct retro_message clear_msg_log = { " ", 1 };
        environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &clear_msg_log);
    }

    rg_menu_visible = false;
    refresh_menu_log = true;
}

// вывод информационных сообщений на экран
void rg_msg(uint8_t context, const char* raw_msg, ...) {
    char raw_msg_next[256];
    char msg[256];
    uint32_t duration = 4000;
    uint8_t priority = 1; // 0, 1, 2, 3 ... 255, обычно от 0 до 3
    uint8_t level = RETRO_LOG_INFO; // RETRO_LOG_DEBUG, RETRO_LOG_INFO, RETRO_LOG_WARN, RETRO_LOG_ERROR
    // RETRO_MESSAGE_TYPE_NOTIFICATION, RETRO_MESSAGE_TYPE_NOTIFICATION_ALT, RETRO_MESSAGE_TYPE_STATUS, RETRO_MESSAGE_TYPE_PROGRESS
    uint8_t type = RETRO_MESSAGE_TYPE_NOTIFICATION;
    int8_t progress = -1;

    va_list args;
    va_start(args, raw_msg);
    vsnprintf(raw_msg_next, sizeof(raw_msg_next), raw_msg, args);
    va_end(args);

    switch (context) {
        case RG_MSG_REPLAY_PLAY: {
            snprintf(msg, sizeof(msg), TR(RG_TR_RG_PLAYBACK),
                rg_main.step_count, rg_bug_glitches.count ? TR(RG_TR_B) : rg_main.localizing ? TR(RG_TR_L) : TR(RG_TR_S));
            duration = 33;
            priority = 6;
            type = RETRO_MESSAGE_TYPE_PROGRESS;
            progress = atoi(raw_msg_next);
            break;
        }
        case RG_MSG_INFO: {
            snprintf(msg, sizeof(msg), TR(RG_TR_RG_INFO), raw_msg_next);
            break;
        }
        case RG_MSG_ERROR: {
            snprintf(msg, sizeof(msg), TR(RG_TR_RG_ERROR), raw_msg_next);
            priority = 4;
            level = RETRO_LOG_ERROR;
            break;
        }
        case RG_MSG_FOUND: {
            snprintf(msg, sizeof(msg), TR(RG_TR_RG_FOUND), raw_msg_next);
            duration = 10000;
            priority = 5;
            break;
        }
        case RG_MSG_REPLAY_REC: {
            snprintf(msg, sizeof(msg), TR(RG_TR_RG_RECORD));
            duration = 33;
            priority = 6;
            type = RETRO_MESSAGE_TYPE_PROGRESS;
            progress = atoi(raw_msg_next);
            break;
        }
        case RG_MSG_DEBUG: {
#if !RG_DEBUG
            return;
#endif
            snprintf(msg, sizeof(msg), TR(RG_TR_RG_DEBUG), raw_msg_next);
            level = RETRO_LOG_DEBUG; 
            break;
        }
        default:
            break;
    }

    struct retro_message_ext out_msg = {
        .msg = msg,                         // текст сообщения
        .duration = duration,                    // время отображения в мс
        .priority = priority,                    // приоритет очереди отображения
        .level = level,                          // уровень сообщения (иконка сообщения)
        .target = RETRO_MESSAGE_TARGET_OSD,      // только на экран
        .type = type,                            // тип
        .progress = progress                     // прогресс бар
    };

    environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &out_msg);
}

// rom_glitcher.c
// perfect_genius - glitcher idea, pav13 - implementation
// https://www.emu-land.net/forum/index.php/topic,88982.msg1652059.html#msg1652059

#include "shared.h"

rom_glitcher_t rg_main = {
    .glitches = NULL,
    .glitch_count = 0,
    .range_start = 0,
    .range_size = 0,
    .step_count = 0,
    .seed = 19881029,
    .localizing = false,
    .launch = false,
    .init_done = false
};

rom_glitcher_found_glitches_t rg_found_glitches = {
    .count = 0,
    .enabled_count = 0,
    .current_page = 0,
    .total_pages = 0
};

rom_glitcher_input_replay_t rg_input_replay = {
    .record = false,
    .play = false,
    .play_count = 0,
    .length = 0
};

rom_glitcher_t rg_backup[RG_MAX_BACKUP_SLOTS];
rom_glitcher_t rg_backup_before_local;
uint8_t rg_backup_index = 0;
uint8_t rg_backup_count = 0;
rom_glitcher_button_state_t rg_button_states[7] = { 0 }; // Key 0:Menu, 1:Prev, 2:Next, 3:Confirm/Found, 4:Cancel/Bug, 5:NotFound, 6:StepBack
uint8_t rg_branch_allowed = 0b00001000; // default only BEQ/BNE
int32_t rg_menu_button = RG_DISABLED_KEY;
bool rg_swap_buttons = false;
bool rg_found_glitches_modified = false;
bool rg_menu_visible = false;
bool rg_rom_in_mdx = false;
bool rg_rom_is_byte_swapped = false;
bool rg_rom_has_header = false;
bool rg_rom_was_deinterleaved = false;
uint8_t rg_pause_effect = 0;
uint16_t rg_fps = 60;
uint32_t rg_total_glitch_count = 0;
char rg_log[1024];

static bool need_load_state = false;
static bool load_step_before_local = false;
static uint8_t game_state_buffer[STATE_SIZE];

static uint32_t xorshift(uint32_t* seed) {
    uint32_t x = *seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (*seed = x);
}

void rg_instructions_shuffle(void) {
#ifdef RANDOM_SEED
    rg_main.seed ^= m68k_get_reg(M68K_REG_PC);
    rg_main.seed ^= m68k_get_reg(M68K_REG_IR);
    rg_main.seed ^= m68k_get_reg(M68K_REG_D2);
#endif // RANDOM_SEED

    //равномерное перемешивание
    for (uint32_t i = 0; i < rg_main.glitch_count - 1; i++) {
        uint32_t j = i + (xorshift(&rg_main.seed) % (rg_main.glitch_count - i));
        rom_glitch_t temp = rg_main.glitches[i];
        rg_main.glitches[i] = rg_main.glitches[j];
        rg_main.glitches[j] = temp;
    }
}

void rg_instructions_restore(void) {
    if (!rg_main.glitches)
        return;

    for (uint32_t i = rg_main.range_start; 
            i < rg_main.range_start + rg_main.range_size && i < rg_main.glitch_count; i++)
        rg_main.glitches[i].mod_value = rg_main.glitches[i].initial_value;
}

void rg_instructions_inversion(void) {
    if (!rg_main.glitches)
        return;
    
    for (uint32_t i = rg_main.range_start;
            i < rg_main.range_start + rg_main.range_size && i < rg_main.glitch_count; i++)
        rg_main.glitches[i].mod_value = rg_main.glitches[i].initial_value ^ 1;
}

// загрузка бэкапа до локализации и удаление найденной инструкции
static void load_step_back_before_local(void) {
    rom_glitcher_t* slot = &rg_backup_before_local;

    if (rg_main.glitches) {
        free(rg_main.glitches);
        rg_main.glitches = NULL;
    }

    rg_main = *slot;

    if (slot->glitch_count > 0 && slot->glitches) {
        rg_main.glitches = malloc(sizeof(rom_glitch_t) * slot->glitch_count);
        if (rg_main.glitches) {
            memcpy(rg_main.glitches, slot->glitches, sizeof(rom_glitch_t) * slot->glitch_count);

            for (uint32_t i = 0; i < rg_main.glitch_count; i++) {
                if (rg_main.glitches[i].address == rg_found_glitches.virt_address[rg_found_glitches.count - 1]) {
                    for (uint32_t j = i; j < rg_main.glitch_count - 1; j++)
                        rg_main.glitches[j] = rg_main.glitches[j + 1];

                    rg_main.glitch_count--;

                    rom_glitch_t* tmp = realloc(rg_main.glitches,
                        sizeof(rom_glitch_t) * rg_main.glitch_count);

                    if (tmp || rg_main.glitch_count == 0)
                        rg_main.glitches = tmp;

                    break;
                }
            }

            rg_msg("Continue searching ...", RG_MSG_INFO);
        }
        else {
            rg_main.glitches = NULL;
            rg_main.glitch_count = 0;
            rg_msg("Step before local restore failed (memory)", RG_MSG_ERROR);
        }
    }
    else {
        rg_main.glitches = NULL;
        rg_msg("Step before local restore failed (no backup)", RG_MSG_ERROR);
    }

    rg_game_reset();
}

static int16_t hook_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (device == RETRO_DEVICE_JOYPAD) {
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            return rg_input_replay.hook_mask[port];
        if (id <= RETRO_DEVICE_ID_JOYPAD_R3)
            return (rg_input_replay.hook_mask[port] >> id) & 1;
    }
    return 0;
}

// опрос кнопок геймпада и обработка ввода
void rg_input_processing(void) {   
    if (!input_poll_cb || !input_state_cb)
        return; 
    
    if (need_load_state) {
        need_load_state = false;
        rg_game_load_state();
        if (rg_input_replay.length > 0) {
            rg_input_replay.play = true;
            rg_input_replay.play_count = 0;
            rg_input_replay.input_cb_copy = input_state_cb;
            input_state_cb = hook_input_state_cb;
        }

        return;
    }

    input_poll_cb();

    int16_t current_mask[RG_MAX_REPLAY_GAMEPAD] = { 0 };

    // ----------------- rg_input_replay.record -----------------
    if (rg_input_replay.record) {
        if (rg_input_replay.length >= RG_MAX_REPLAY_FRAMES) {
            rg_input_replay.record = false;
            rg_game_reset();
            return;
        }

        if (libretro_supports_bitmasks)
            for (uint8_t port = 0; port < RG_MAX_REPLAY_GAMEPAD; port++) {
                current_mask[port] = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
                rg_input_replay.sequence[port][rg_input_replay.length++] = current_mask[port];
            }
        else
            for (uint8_t port = 0; port < RG_MAX_REPLAY_GAMEPAD; port++)
                for (int id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; id++)
                    if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, id)) {
                        current_mask[port] |= (1 << id);
                        rg_input_replay.sequence[port][rg_input_replay.length++] = current_mask[port];
                    }
        
        char tmp[6];
        snprintf(tmp, sizeof(tmp), "%d", 100 - (rg_input_replay.length * 100) / RG_MAX_REPLAY_FRAMES);
        rg_msg(tmp, RG_MSG_REPLAY_REC);
    }
    // ----------------- rg_input_replay.play -----------------
    else if (rg_input_replay.play && rg_input_replay.length > 0) {
        if (libretro_supports_bitmasks)
            current_mask[0] = rg_input_replay.input_cb_copy(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
        else
            for (int id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; id++)
                if (rg_input_replay.input_cb_copy(0, RETRO_DEVICE_JOYPAD, 0, id))
                    current_mask[0] |= (1 << id);
        
        if (rg_input_replay.play_count < rg_input_replay.length) {
            for (uint8_t port = 0; port < RG_MAX_REPLAY_GAMEPAD; port++)
                rg_input_replay.hook_mask[port] = rg_input_replay.sequence[port][rg_input_replay.play_count++];
            
            char tmp[6];
            snprintf(tmp, sizeof(tmp), "%d", (rg_input_replay.play_count * 100) / rg_input_replay.length);
            rg_msg(tmp, RG_MSG_REPLAY_PLAY);
        }
        else {
            rg_input_replay.play = false;
            input_state_cb = rg_input_replay.input_cb_copy;
            rg_menu_visible = true;
            rg_menu.current = &rg_menu.main;
            rg_menu.current->selected_index = 0;
        }
    }
    // ----------------- normal -----------------
    else {
        if (libretro_supports_bitmasks)
            current_mask[0] = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
        else
            for (int id = 0; id <= RETRO_DEVICE_ID_JOYPAD_R3; id++)
                if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, id))
                    current_mask[0] |= (1 << id);
    }

    bool menu_key = ((current_mask[0] & rg_menu_button) == rg_menu_button);

    // ----------------- menu_key -----------------
    if (!menu_key && rg_button_states[0].was_pressed && !rg_button_states[0].is_processed) {
        if (rg_input_replay.record) {
            rg_input_replay.record = false;
            rg_game_reset();
        }
        else if (rg_input_replay.play) {
            rg_input_replay.play = false;
            input_state_cb = rg_input_replay.input_cb_copy;
            rg_menu_visible = true;
            rg_menu.current = &rg_menu.main;
            rg_menu.current->selected_index = 0;
        }
        else {
            rg_menu_visible = !rg_menu_visible;
            if (rg_menu_visible) {
                rg_menu.current = rg_main.launch ? &rg_menu.main : &rg_menu.launch;
                rg_menu.current->selected_index = 0;
            }
            else {
                rg_menu_hide();

                if (load_step_before_local) {
                    load_step_before_local = false;
                    rg_found_glitches_modified = false;
                    load_step_back_before_local();
                }
                else if (rg_found_glitches_modified) {
                    rg_found_glitches_modified = false;
                    rg_msg("Selected glitches are modified", RG_MSG_INFO);
                    rg_game_reset();
                }
            }
        }

        rg_button_states[0].is_processed = true;
    }
    rg_button_states[0].was_pressed = menu_key;
    if (menu_key) rg_button_states[0].is_processed = false;

    if (rg_menu_visible || rg_input_replay.play) {
        bool prev_key = (current_mask[0] >> RETRO_DEVICE_ID_JOYPAD_UP) & 1;
        bool next_key = (current_mask[0] >> RETRO_DEVICE_ID_JOYPAD_DOWN) & 1;
        bool confirm_found_key = (current_mask[0] >>
            (rg_swap_buttons ? RETRO_DEVICE_ID_JOYPAD_B : RETRO_DEVICE_ID_JOYPAD_A)) & 1;
        bool cancel_bug_key = (current_mask[0] >>
            (rg_swap_buttons ? RETRO_DEVICE_ID_JOYPAD_A : RETRO_DEVICE_ID_JOYPAD_B)) & 1;
        bool not_found_key = (current_mask[0] >>
            (rg_swap_buttons ? RETRO_DEVICE_ID_JOYPAD_Y : RETRO_DEVICE_ID_JOYPAD_X)) & 1;
        bool step_back_key = (current_mask[0] >>
            (rg_swap_buttons ? RETRO_DEVICE_ID_JOYPAD_X : RETRO_DEVICE_ID_JOYPAD_Y)) & 1;

        // ----------------- prev_key -----------------
        if (!prev_key && rg_button_states[1].was_pressed && !rg_button_states[1].is_processed) {
            if (rg_input_replay.play)
                ;
            else if (rg_menu.current) {
                rg_menu.current->selected_index =
                    (rg_menu.current->selected_index - 1 + rg_menu.current->item_count) % rg_menu.current->item_count;
            }
            rg_button_states[1].is_processed = true;
        }
        rg_button_states[1].was_pressed = prev_key;
        if (prev_key) rg_button_states[1].is_processed = false;

        // ----------------- next_key -----------------
        if (!next_key && rg_button_states[2].was_pressed && !rg_button_states[2].is_processed) {
            if (rg_input_replay.play)
                ;
            else if (rg_menu.current) {
                rg_menu.current->selected_index = (rg_menu.current->selected_index + 1) % rg_menu.current->item_count;
            }
            rg_button_states[2].is_processed = true;
        }
        rg_button_states[2].was_pressed = next_key;
        if (next_key) rg_button_states[2].is_processed = false;

        // ----------------- confirm_found_key -----------------
        if (!confirm_found_key && rg_button_states[3].was_pressed && !rg_button_states[3].is_processed) {
            if (rg_input_replay.play) {
                rg_input_replay.play = false;
                input_state_cb = rg_input_replay.input_cb_copy;
                menu_item_3_found();
            }
            else if (rg_menu.current == &rg_menu.main && rg_menu.current->selected_index == 0)
                menu_item_3_found();
            else if (rg_menu.current) {
                rom_glitcher_menu_item_t* item = &rg_menu.current->items[rg_menu.current->selected_index];
                if (item->action)
                    item->action();
            }
            rg_button_states[3].is_processed = true;
        }
        rg_button_states[3].was_pressed = confirm_found_key;
        if (confirm_found_key) rg_button_states[3].is_processed = false;

        // ----------------- cancel_bug_key -----------------
        if (!cancel_bug_key && rg_button_states[4].was_pressed && !rg_button_states[4].is_processed) {
            if (rg_input_replay.play) {
                rg_input_replay.play = false;
                input_state_cb = rg_input_replay.input_cb_copy;
                menu_item_1_bug();
            }
            else if (rg_menu.current != &rg_menu.main && rg_menu.current != &rg_menu.launch)
                rg_menu.current = rg_main.launch ? &rg_menu.main : &rg_menu.launch;
            else if (rg_menu.current == &rg_menu.main && rg_menu.current->selected_index == 0)
                 menu_item_1_bug();
            else
                rg_menu_hide();

            if (load_step_before_local) {
                load_step_before_local = false;
                rg_found_glitches_modified = false;
                rg_menu_hide();
                load_step_back_before_local();
            }
            else if (rg_found_glitches_modified) {
                rg_found_glitches_modified = false;
                rg_msg("Selected glitches are modified", RG_MSG_INFO);
                rg_game_reset();
            }

            rg_button_states[4].is_processed = true;
        }
        rg_button_states[4].was_pressed = cancel_bug_key;
        if (cancel_bug_key) rg_button_states[4].is_processed = false;
        
        // ----------------- not_found_key -----------------
        if (!not_found_key && rg_button_states[5].was_pressed && !rg_button_states[5].is_processed) {
            if (rg_input_replay.play) {
                rg_input_replay.play = false;
                input_state_cb = rg_input_replay.input_cb_copy;
                menu_item_2_not_found();
            }
            else if (rg_menu.current == &rg_menu.main && rg_menu.current->selected_index == 0)
                menu_item_2_not_found();

            rg_button_states[5].is_processed = true;
        }
        rg_button_states[5].was_pressed = not_found_key;
        if (not_found_key) rg_button_states[5].is_processed = false;

        // ----------------- step_back_key -----------------
        if (!step_back_key && rg_button_states[6].was_pressed && !rg_button_states[6].is_processed) {
            if (rg_input_replay.play) {
                rg_input_replay.play = false;
                input_state_cb = rg_input_replay.input_cb_copy;
                menu_item_4_step_back();
            }
            else if (rg_menu.current == &rg_menu.main && rg_menu.current->selected_index == 0)
                menu_item_4_step_back();

            rg_button_states[6].is_processed = true;
        }
        rg_button_states[6].was_pressed = step_back_key;
        if (step_back_key) rg_button_states[6].is_processed = false;
    }
    else
        for (int i = 1; i < ARRAY_SIZE(rg_button_states); i++)
            rg_button_states[i].was_pressed = rg_button_states[i].is_processed = false;
    
    rg_menu_show();
}

static uint16_t get_rom_checksum(uint8_t* rom, uint32_t size) {
    uint16_t checksum = 0;

    for (int i = 0; i < size; i += 2) {
        uint8_t next_byte = (i + 1 < size) ? rom[i + 1] : 0;
        checksum += ((rom[i] << 8) + next_byte);
    }

    return checksum;
}

// применение к ROM глитчей из списка найденных
static void apply_found_glitches(void) {
    rg_found_glitches.enabled_count = 0;

    for (uint8_t i = 0; i < rg_found_glitches.count; i++) {
        if (rg_found_glitches.enabled[i]) {
            cart.rom[rg_found_glitches.virt_address[i]] = rg_found_glitches.mod_value[i];
            rg_found_glitches.enabled_count++;
        }
        else
            cart.rom[rg_found_glitches.virt_address[i]] = rg_found_glitches.initial_value[i];
    }
}

// изменение ROM при отсеивании кандидатов
static void apply_glitches(void) {
    for (uint32_t i = 0; i < rg_main.glitch_count; i++)
        cart.rom[rg_main.glitches[i].address] = rg_main.glitches[i].mod_value;

    apply_found_glitches();

    // если заголовок не был удалён эмулятором
    if (!rg_rom_has_header) {
        // пересчёт контрольной суммы в заголовке игры
        uint16_t real_checksum = get_rom_checksum(((uint8*)cart.rom) + 0x200, cart.romsize - 0x200);
        cart.rom[0x18E] = (real_checksum >> 8) & 0xFF;
        cart.rom[0x18F] = real_checksum & 0xFF;
    }

    need_load_state = true;
}

void rg_init(uint8_t* rom_data, uint32_t rom_size) {
    if (rg_main.init_done) {
        apply_glitches();
        return;
    }

    // если заголовок не был удалён эмулятором
    if (!rg_rom_has_header) {
        uint16_t header_checksum = (cart.rom[0x18E] << 8) | cart.rom[0x18F];
        uint16_t real_checksum = get_rom_checksum(((uint8*)cart.rom) + 0x200, cart.romsize - 0x200);
        // обновление контрольной суммы в заголовке
        if (header_checksum != real_checksum) {
            cart.rom[0x18E] = (real_checksum >> 8) & 0xFF;
            cart.rom[0x18F] = real_checksum & 0xFF;
            rg_msg("Checksum in header has been corrected", RG_MSG_INFO);
        }
    }

    for (int i = 0; i < RG_MAX_BACKUP_SLOTS; i++) {
        if (rg_backup[i].glitches) {
            free(rg_backup[i].glitches);
            rg_backup[i].glitches = NULL;
            memset(&rg_backup[i], 0, sizeof(rg_backup[i]));
        }
    }
    rg_backup_index = 0;
    rg_backup_count = 0;

    if (rg_main.glitches) {
        free(rg_main.glitches);
        rg_main.glitches = NULL;
    }
    
    uint32_t capacity = 10000;
    rg_main.init_done = false;
    rg_main.glitch_count = 0;
    rg_main.glitches = malloc(capacity * sizeof(rom_glitch_t));

    // Читаем нормализованный ROM
    uint8_t high_byte = 0;
    uint8_t offset8 = 0;
    int32_t target_addr = 0;
    uint32_t trim = rg_rom_has_header ? 0 : 0x200;

#ifdef COMPRESSED_OPCODE_TABLE
    rg_m68k_opcode_valid_init();
#endif // COMPRESSED_OPCODE_TABLE

    for (uint32_t byte_addr = trim; byte_addr + 1 < rom_size; byte_addr += 2) {
        high_byte = rom_data[byte_addr];

        // Ищем все branch инструкции
        if (high_byte < 0x60 || high_byte > 0x6f)
            continue;

        // Проверка битов на разрешение поиска конкретной пары инструкций
        // 7:0x6E/6F, 6:0x6C/6D, 5:0x6A/6B, 4:0x68/69, 3:0x66/67, 2:0x64/65, 1:0x62/63, 0:0x60/61
        if (!(rg_branch_allowed & (1 << ((high_byte - 0x60) >> 1))))
            continue;

        // Проверка целевого адреса на чётность и попадание в ROM
        offset8 = rom_data[byte_addr + 1];
        
        if (offset8 != 0 && (offset8 & 1) == 0) {
            // Короткое смещение
            
            // Проверка опкода после предпологаемой инструкции
            if (byte_addr + 3 >= rom_size)
                continue;

#ifdef COMPRESSED_OPCODE_TABLE
            if (!rg_m68k_opcode_valid((rom_data[byte_addr + 2] << 8) | rom_data[byte_addr + 3]))
                continue;
#else
            if (!rg_m68k_opcode_valid_table[(rom_data[byte_addr + 2] << 8) | rom_data[byte_addr + 3]])
                continue;
#endif // COMPRESSED_OPCODE_TABLE

            target_addr = byte_addr + 2 + (int8_t)offset8;
        }
        else {
            // Длинное смещение
            if (offset8 != 0)
                continue;

            // Проверка опкода после предпологаемой инструкции
            if (byte_addr + 5 >= rom_size)
                continue;

#ifdef COMPRESSED_OPCODE_TABLE
            if (!rg_m68k_opcode_valid((rom_data[byte_addr + 4] << 8) | rom_data[byte_addr + 5]))
                continue;
#else
            if (!rg_m68k_opcode_valid_table[(rom_data[byte_addr + 4] << 8) | rom_data[byte_addr + 5]])
                continue;
#endif // COMPRESSED_OPCODE_TABLE

            uint16_t offset16 = (rom_data[byte_addr + 2] << 8) | rom_data[byte_addr + 3];

            if ((offset16 & 1) != 0 || offset16 == 0)
                continue;
             
            target_addr = byte_addr + 2 + (int16_t)offset16;
        }

        if (target_addr < (int32_t)trim || (target_addr + 1) >= (int32_t)rom_size) 
            continue;

        // Проверка данных по целевому адресу на легальность для M68K
#ifdef COMPRESSED_OPCODE_TABLE
        if (!rg_m68k_opcode_valid((rom_data[target_addr] << 8) | rom_data[target_addr + 1]))
            continue;
#else
        if (!rg_m68k_opcode_valid_table[(rom_data[target_addr] << 8) | rom_data[target_addr + 1]])
            continue;
#endif // COMPRESSED_OPCODE_TABLE

        // Пропуск ранее сохранённой найденной инструкции
        bool skip = false;
        for (uint8_t i = 0; i < rg_found_glitches.count; i++)
            if (byte_addr == rg_found_glitches.virt_address[i]) {
                skip = true;
                break;
            }

        if (skip)
            continue;

        // Правдоподобная branch инструкция
        if (rg_main.glitch_count >= capacity) {
            uint32_t temp_capacity = capacity * 2;
            rom_glitch_t* temp_glitches = realloc(rg_main.glitches, temp_capacity * sizeof(rom_glitch_t));

            if (!temp_glitches) {
                rg_msg("Memory not allocated for storage (#5)", RG_MSG_ERROR);
                free(rg_main.glitches);
                rg_main.glitches = NULL;
                return;
            }
            
            rg_main.glitches = temp_glitches;
            capacity = temp_capacity;
        }

        rg_main.glitches[rg_main.glitch_count].address = byte_addr;
        rg_main.glitches[rg_main.glitch_count].initial_value = high_byte;
        rg_main.glitches[rg_main.glitch_count].mod_value = high_byte;
        rg_main.glitch_count++;
    }

    if (rg_main.glitch_count > 0 && capacity > rg_main.glitch_count) {
        rom_glitch_t* temp_glitches = realloc(rg_main.glitches, rg_main.glitch_count * sizeof(rom_glitch_t));

        if (temp_glitches)
            rg_main.glitches = temp_glitches;

        rg_main.init_done = true;
    }
    else if (rg_main.glitch_count == 0) {
        if (rg_main.glitches) {
            free(rg_main.glitches);
            rg_main.glitches = NULL;
        }
        rg_main.init_done = false;
    }

    apply_found_glitches();

#ifndef RANDOM_SEED
    rg_main.seed = 19881029;
#endif // RANDOM_SEED
    struct retro_system_av_info av_info;
    retro_get_system_av_info(&av_info);
    rg_fps = (uint8_t)(av_info.timing.fps + 0.5);
    rg_fps = rg_fps > 0 ? rg_fps : 60;
    need_load_state = false;
    load_step_before_local = false;
    rg_found_glitches.current_page = 0;
    rg_found_glitches_modified = false;
    rg_found_glitches.total_pages = 
        rg_found_glitches.count ? ((rg_found_glitches.count + RG_MAX_FOUND_GLITCH_PER_PAGE - 1) 
            / RG_MAX_FOUND_GLITCH_PER_PAGE) : 1;
    rg_input_replay.record = false;
    rg_input_replay.play = false;
    rg_input_replay.length = 0;
    rg_main.step_count = 0;
    rg_main.localizing = false;
    rg_main.launch = false;
    rg_main.range_start = 0;
    rg_main.range_size = (rg_main.glitch_count + 15) / 16; // 6% от всех кандидатов
    //rg_main.range_size = (rg_main.glitch_count + 31) / 32; // 3% от всех кандидатов
    rg_total_glitch_count = rg_main.glitch_count;

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "Candidates %u%s", rg_main.glitch_count, rg_main.init_done ? "" : ", NOT found");
    rg_msg(tmp, rg_main.init_done ? RG_MSG_INFO : RG_MSG_ERROR);

    
}

// сохранение предыдущего состояния отсеивания кандидатов
void rg_create_step_backup(void) {
    rom_glitcher_t* slot = &rg_backup[rg_backup_index];

    if (slot->glitches) {
        free(slot->glitches);
        slot->glitches = NULL;
    }

    *slot = rg_main;

    if (rg_main.glitch_count > 0 && rg_main.glitches) {
        slot->glitches = malloc(sizeof(rom_glitch_t) * rg_main.glitch_count);
        if (slot->glitches) {
            memcpy(slot->glitches, rg_main.glitches,  sizeof(rom_glitch_t) * rg_main.glitch_count);
        }
        else {
            slot->glitches = NULL;
            slot->glitch_count = 0;
            rg_msg("Previous step NOT saved (memory)", RG_MSG_ERROR);
            return;
        }
    }
    else {
        slot->glitches = NULL;
        rg_msg("Previous step NOT saved (no data)", RG_MSG_ERROR);
        return;
    }

    rg_backup_index = (rg_backup_index + 1) % RG_MAX_BACKUP_SLOTS;
    if (rg_backup_count < RG_MAX_BACKUP_SLOTS)
        rg_backup_count++;
}

// завершие текущего поиска
void rg_current_search_end(void) {
    rg_menu_visible = true;
    rg_menu.current = &rg_menu.list;
    
    for (int i = 0; i < rg_found_glitches.count; i++) {
        if (rg_main.glitches[0].address == rg_found_glitches.virt_address[i]) {
            rg_menu.current->selected_index = 1;
            rg_found_glitches.current_page = 0;
            load_step_before_local = true;
            return;
        }
    }

    if (rg_found_glitches.count > 0) {
        for (int i = 0; i < rg_found_glitches.count; i++) {
            if (rg_main.glitches[0].address == rg_found_glitches.virt_address[i]) {
                rg_menu.current->selected_index = i % RG_MAX_FOUND_GLITCH_PER_PAGE + 1;
                rg_found_glitches.current_page = i / RG_MAX_FOUND_GLITCH_PER_PAGE;
                load_step_before_local = true;
                return;
            }
        }

        rg_menu.current->selected_index = (rg_found_glitches.count - 1) % RG_MAX_FOUND_GLITCH_PER_PAGE + 1;
        rg_found_glitches.current_page = (rg_found_glitches.count - 1) / RG_MAX_FOUND_GLITCH_PER_PAGE;
    }
    else {
        rg_menu.current->selected_index = 1;
        rg_found_glitches.current_page = 0;
    }

    load_step_before_local = true;
}

// принудительная остановка 
void rg_force_stop_glitcher(void) { 
    rg_instructions_restore();
    rg_main.init_done = false;
    rg_game_reset();
    rg_game_load_state();
}

// очистка памяти при закрытии ядра
void rg_deinit(void) {
    rg_menu_hide();
    
    if (rg_main.glitches) {
        free(rg_main.glitches);
        rg_main.glitches = NULL;
    }

    if (rg_backup_before_local.glitches) {
        free(rg_backup_before_local.glitches);
        rg_backup_before_local.glitches = NULL;
    }

    for (int i = 0; i < RG_MAX_BACKUP_SLOTS; i++) {
        if (rg_backup[i].glitches) {
            free(rg_backup[i].glitches);
            rg_backup[i].glitches = NULL;
        }
    }
}

// Преобразование найденного адреса глитча из
// виртуального ROM эмулятора в реальный ROM адрес
uint32_t rg_virt_to_real_rom_offset(uint32_t address) {
    uint32_t file_offset = address;

    if (rg_rom_was_deinterleaved) {
        uint32_t block = address / 0x4000;
        uint32_t offset = address % 0x4000;

        if ((offset % 2) == 0)
            file_offset = block * 0x4000 + 0x2000 + (offset / 2);
        else
            file_offset = block * 0x4000 + (offset / 2);
    }

    if (rg_rom_has_header)
        file_offset += 512;

    if (rg_rom_is_byte_swapped)
        file_offset ^= 1;

    if (rg_rom_in_mdx)
        file_offset += 4;

    return file_offset;
}

// Преобразование найденного адреса глитча из
// реального ROM в виртуальный ROM эмулятора
uint32_t rg_real_to_virt_rom_offset(uint32_t address) {
    uint32_t virt_offset = address;

    if (rg_rom_in_mdx)
        virt_offset -= 4;

    if (rg_rom_is_byte_swapped)
        virt_offset ^= 1;

    if (rg_rom_has_header)
        virt_offset -= 512;

    if (rg_rom_was_deinterleaved) {
        uint32_t block = virt_offset / 0x4000;
        uint32_t offset = virt_offset % 0x4000;

        if (offset < 0x2000)
            virt_offset = block * 0x4000 + (offset * 2 + 1);
        else
            virt_offset = block * 0x4000 + ((offset - 0x2000) * 2);
    }

    return virt_offset;
}


// сохранения файла с читами в папку с ROM
uint8_t rg_save_found_to_file(uint32_t virt_address, uint32_t real_address, uint8_t initial_value, uint8_t mod_value) {
    char cheats_path[512] = { 0 };
    RFILE* f_cht = NULL;

#if defined(_WIN32)
    char slash = '\\';
#else
    char slash = '/';
#endif

    if (!rg_last_game || !rg_last_game->path) {
        rg_msg("No last game info", RG_MSG_ERROR);
        return 3;
    }

    snprintf(cheats_path, sizeof(cheats_path),
        "%s%c%s_RGI.cht", g_rom_dir, slash, g_rom_name);

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
    snprintf(search, sizeof(search), "code = \"%06X:%02X\"", real_address, mod_value);
    if (strstr(file_content, search)) {
        free(file_content);
        rg_msg("Duplicate. Glitch is already saved in cht-file", RG_MSG_INFO);
        return 2;
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
        "Glitch %u, steps %u (0x%02X->0x%02X) ", cheats_count, initial_value, mod_value, rg_main.step_count);

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
    f_cht = filestream_open(cheats_path, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
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
    size_t len = strlen(cheats_path);
    char tmp[128];

    if (len > 90) {
        const char* start_pos = cheats_path + (len - 87);
        snprintf(tmp, sizeof(tmp), "Glitch saved to \"...%s\"", start_pos);
    }
    else
        snprintf(tmp, sizeof(tmp), "Glitch saved to \"%s\"", cheats_path);

    rg_msg(tmp, RG_MSG_FOUND);

    return 0;
}

// сброс игры
void rg_game_reset(void) {
    rg_menu_hide();
    retro_unload_game();
    retro_load_game(rg_last_game);
}

// сохранить save state игры в памяти для работы с глитчером
void rg_game_save_state(void) {
    rg_menu_hide();

    if (!retro_serialize(game_state_buffer, sizeof(game_state_buffer)))
        rg_msg("State NOT saved", RG_MSG_ERROR);
}

// загрузить load state игры из памяти для работы с глитчером
void rg_game_load_state(void) {
    rg_menu_hide();

    if (!retro_unserialize(game_state_buffer, sizeof(game_state_buffer)))
        rg_msg("State NOT loaded", RG_MSG_ERROR);
}

// применение эффекта к последнему кадру игры при вызове меню глитчера
// (возможная будущая основа для "нового" меню)
void rg_handle_pause_frame(void** pause_frame, int* pause_frame_width, int* pause_frame_height,
    const void* bitmap_data, int vwidth, int vheight, int bitmap_pitch, int bitmap_width)
{
    static uint8_t old_pause_effect = 255;

    if (bitmap_data && (!*pause_frame || vwidth != *pause_frame_width || vheight != *pause_frame_height)) {
        if (*pause_frame) {
            free(*pause_frame);
            *pause_frame = NULL;
        }

        const int pixel_size = (bitmap_pitch / bitmap_width) > 2 ? 4 : 2;
        *pause_frame = malloc(vwidth * vheight * pixel_size);
        *pause_frame_width = vwidth;
        *pause_frame_height = vheight;

        if (!*pause_frame)
            return;

        // RGB565 (16 bit)
        if (pixel_size == 2) {
            const uint16_t* src_frame = (const uint16_t*)bitmap_data;
            uint16_t* dst_frame = (uint16_t*)*pause_frame;

            for (int y = 0; y < vheight; y++)
            {
                for (int x = 0; x < vwidth; x++)
                {
                    int src_idx = y * (bitmap_pitch / 2) + x;
                    int dst_idx = y * vwidth + x;
                    uint16_t pixel = src_frame[src_idx];

                    uint8_t r = (pixel >> 11) & 0x1F;
                    uint8_t g = (pixel >> 5) & 0x3F;
                    uint8_t b = pixel & 0x1F;

                    switch (rg_pause_effect) {
                        case 0: // dark
                            r >>= 2; g >>= 2; b >>= 2;
                            break;

                        case 1: // red glitch
                            r = (r + 31) / 3;
                            g >>= 2; b >>= 2;
                            break;

                        case 2: { // sepia
                            uint8_t gray = (r + g + b) / 4;
                            r = gray;
                            g = (gray - (gray >> 2));
                            b = (gray >> 1);
                            break;
                        }

                        case 3: // yellow glitch
                            r = r * 3 / 4;
                            g = r;
                            b = 0;
                            break;

                        case 4: { // color static
                            uint8_t noise = xorshift(&rg_main.seed) % 25;
                            r = (r > noise) ? (r - noise) * 3 / 4 : 1;
                            g = (g > noise) ? (g - noise) * 3 / 4 : 1;
                            b = (b > noise) ? (b - noise) * 3 / 4 : 1;
                            break;
                        }
                    }

                    dst_frame[dst_idx] = (r << 11) | (g << 5) | b;
                }
            }
        }
    }

    if (old_pause_effect != rg_pause_effect) {
        old_pause_effect = rg_pause_effect;
        *pause_frame_width = 0; // котыль для мгновенной смены эффекта
    }
}

/*if (log_cb) {
                int offset = 0;
                for (int i = 0; i < rg_input_replay.length; i++)
                    offset += snprintf(rg_log + offset, sizeof(rg_log) - offset, " %u", rg_input_replay.sequence[i]);

                log_cb(RETRO_LOG_INFO, "\n\n%s\n\n\n", rg_log);
}*/
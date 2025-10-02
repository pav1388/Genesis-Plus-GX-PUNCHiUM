// rom_glitcher_defines.h

#ifndef _ROM_GLITCHER_DEFINES_H_
#define _ROM_GLITCHER_DEFINES_H_

// Версия и отладка
#define RG_DEBUG 0
#define RG_VERSION_BASE "v0.3.4"
#if RG_DEBUG
    #define RG_VERSION RG_VERSION_BASE "-dev"
#else
    #define RG_VERSION RG_VERSION_BASE
#endif

// Ключи и настройки
#define RG_DISABLED_KEY                 -2
#define RG_PATH_SIZE                    512
#define RG_ROM_HEADER_SIZE              512

// Лимиты для структур
#define RG_MAX_FOUND_GLITCH_SLOTS       256
#define RG_MAX_REPLAY_FRAMES            7200    // 60 FPS * 120 sec
#define RG_MAX_REPLAY_GAMEPADS          2       // port 0 and 1
#define RG_MAX_BACKUP_SLOTS             1

// Настройки меню
#define RG_FOUND_GLITCH_PER_PAGE        6
#define RG_MSG_DEBUG        0
#define RG_MSG_INFO         1
#define RG_MSG_ERROR        2
#define RG_MSG_FOUND        3
#define RG_MSG_REPLAY_REC   4
#define RG_MSG_REPLAY_PLAY  5

// Битовая маска фильтра инструкций rg_inst_allowed:
#define TOTAL_INST_BITS_USED 26

typedef enum {
    // -- Условные переходы (Bcc)
    INST_BHI_BLS = 0,   // Branch if Higher/Lower or Same
    INST_BCC_BCS,       // Branch if Carry Clear/Carry Set
    INST_BNE_BEQ,       // Branch if Not Equal/Equal
    INST_BVC_BVS,       // Branch if Overflow Clear/Overflow Set
    INST_BPL_BMI,       // Branch if Plus/Minus
    INST_BGE_BLT,       // Branch if Greater or Equal/Less Than
    INST_BGT_BLE,       // Branch if Greater Than/Less or Equal
    INST_SKIP_7,        // (заглушка)

    // -- Установка условий (Scc)
    INST_SKIP_8 = 8,        // (заглушка)
    INST_SHI_SLS,       // Set if Higher/Lower or Same
    INST_SCC_SCS,       // Set if Carry Clear/Carry Set
    INST_SNE_SEQ,       // Set if Not Equal/Equal
    INST_SVC_SVS,       // Set if Overflow Clear/Overflow Set
    INST_SPL_SMI,       // Set if Plus/Minus
    INST_SGE_SLT,       // Set if Greater or Equal/Less Than
    INST_SGT_SLE,       // Set if Greater Than/Less or Equal

    // -- Циклы (DBcc)
    INST_SKIP_16 = 16,       // (заглушка)
    INST_DBHI_DBLS,     // Decrement and Branch if Higher/Lower or Same
    INST_DBCC_DBCS,     // Decrement and Branch if Carry Clear/Carry Set
    INST_DBNE_DBEQ,     // Decrement and Branch if Not Equal/Equal
    INST_DBVC_DBVS,     // Decrement and Branch if Overflow Clear/Overflow Set
    INST_DBPL_DBMI,     // Decrement and Branch if Plus/Minus
    INST_DBGE_DBLT,     // Decrement and Branch if Greater or Equal/Less Than
    INST_DBGT_DBLE,     // Decrement and Branch if Greater Than/Less or Equal

    // -- Арифметические
    INST_ADD_SUB = 24,       // ADD/SUB   (Addition/Subtraction)
    INST_ADDX_SUBX,     // ADDX/SUBX (Addition/Subtraction with Expansion)
    INST_ADDA_SUBA,     // ADDA/SUBA (Addition/Subtraction of Addresses)
    INST_ADDI_SUBI,     // ADDI/SUBI (Addition/Subtraction of Immediate Values)
    INST_ADDQ_SUBQ,     // ADDQ/SUBQ (Fast Addition/Subtraction)
    INST_SKIP_29,       // (заглушка)
    INST_SKIP_30,       // (заглушка)
    INST_SKIP_31,       // (заглушка)

    TOTAL_INST_BITS = 32
} InstructionBit;

enum {
    BCC_MASK = (1 << INST_BHI_BLS) | (1 << INST_BCC_BCS) | (1 << INST_BNE_BEQ) |
        (1 << INST_BVC_BVS) | (1 << INST_BPL_BMI) | (1 << INST_BGE_BLT) |
        (1 << INST_BGT_BLE),

    SCC_MASK = (1 << INST_SHI_SLS) | (1 << INST_SCC_SCS) | (1 << INST_SNE_SEQ) |
        (1 << INST_SVC_SVS) | (1 << INST_SPL_SMI) | (1 << INST_SGE_SLT) |
        (1 << INST_SGT_SLE),

    DBCC_MASK = (1 << INST_DBHI_DBLS) | (1 << INST_DBCC_DBCS) | (1 << INST_DBNE_DBEQ) |
        (1 << INST_DBVC_DBVS) | (1 << INST_DBPL_DBMI) | (1 << INST_DBGE_DBLT) |
        (1 << INST_DBGT_DBLE),

    ARITH_MASK = (1 << INST_ADD_SUB) | (1 << INST_ADDX_SUBX) | (1 << INST_ADDA_SUBA) |
        (1 << INST_ADDI_SUBI) | (1 << INST_ADDQ_SUBQ)
};

#define VIRT_TO_REAL true
#define REAL_TO_VIRT false

#endif
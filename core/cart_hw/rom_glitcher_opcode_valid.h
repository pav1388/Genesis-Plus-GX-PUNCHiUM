// rom_glitcher_opcode_valid.h

#ifndef _ROM_GLITCHER_OPCODE_VALID_H_
#define _ROM_GLITCHER_OPCODE_VALID_H_

#ifdef COMPRESSED_OPCODE_TABLE
	void rg_m68k_opcode_valid_init(void);
	bool rg_m68k_opcode_valid(uint16_t opcode);
#else
	extern const bool rg_m68k_opcode_valid_table[0x10000];
#endif // COMPRESSED_OPCODE_TABLE

#endif
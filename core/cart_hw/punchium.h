// punchium.h
#pragma once
/*
Project Little Man
*/

// edit by pav13

#define DEBUG_CHEAT 0
#define DEBUG_MODE 0
#define DEBUG_SPRITE 0

#ifdef _WIN32
    #define FASTCALL __fastcall
    #include <windows.h>
    #include <stdio.h>
    #include <sys/stat.h>
#elif __vita__
    #define FASTCALL 
    #include <psp2/io/stat.h>
	// #include <psp2/io/fcntl.h>
#else
    #define FASTCALL
    #include <stdio.h>
    #include <sys/stat.h>
#endif

#define m68k_read_immediate_16(address) *(uint16_t *)(m68k.memory_map[((address)>>16)&0xff].base + ((address) & 0xffff))
#define m68k_read_immediate_32(address) (m68k_read_immediate_16(address) << 16) | (m68k_read_immediate_16(address+2))

#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#ifdef __arm__ // rand/srand for ARM
static uint32_t rng_state;

#define rand() ({ \
	uint32_t x = rng_state; \
	x ^= x << 13; \
	x ^= x >> 17; \
	x ^= x << 5; \
	rng_state = x; \
	x >> 16; \
})

#define srand(seed) (rng_state = (uint32_t)(seed))
#endif

extern char g_rom_dir[256]; // путь к папке с ROM
extern T_SRAM sram;
extern uint8_t punchium_audio_track_format;
extern retro_log_printf_t log_cb;
static char error_str[1024];

static uint8 punchium_obj_ram[0x80000];
// static uint8 punchium_wave_ram[0x180000];
static int skip_boot1 = 1;

static int punchium_cmd_count = 0;
static int punchium_tmss = 1;
int fast_dma_hack = 0; // ипс. в core/vdp_ctrl.c

static int punchium_music_ptr;
static int punchium_wave_ptr;
static int punchium_sfx_ptr;
static int punchium_tile_ptr;
static int punchium_sprite_ptr;

typedef struct punchium_voice_t {
	int volume, panning, flags, type; // Основные параметры
	int size, ptr, tick, loop; // Управление воспроизведением
	int echo, program, count, time; // Эффекты и обработка
	int start, num, decay, decay2, release, sustain; // Огибающая (envelope)
	int duration, velocity, keyon, key_type; // Параметры ноты
	int pitch, cents, modulation; // Тон и модуляции
} punchium_voice_t;

struct punchium_t
{
	uint8 ram[0x10000];		  // Основная RAM
	uint8 decoder_ram[0x10000];   // Буфер декомпрессии
	uint8 scaler_ram[0x1000];	 // Графический буфер
	uint8 music_ram[0x8000];	  // Музыкальные данные
	uint8 exps_ram[14*8];		 // Данные экспаншенов
	punchium_voice_t sfx[8];		 // SFX-каналы
	punchium_voice_t music[26];	  // Музыкальные каналы (не используются)
	// Состояние аудио
	int music_section, audio_tick, music_segment;
	int out_l, out_r; // Выходной сигнал
	int audio_flags, sfx_volume, music_volume;
	// Декомпрессия/графика
	int decoder_mode, decoder_ptr, decoder_size;
	int draw_src, draw_dst; // Графические буферы
	int obj[0x31]; // Объекты
	// Эффекты
	int echo_l[48000/4], echo_r[48000/4]; // Эхо-буферы
	int echo_ptr, echo_pan; // Параметры эхо
	
	// Управление треками
	int8_t music_track;			 // Номер трека
	int music_tick;				 // Тик музыки
	int music_pos;				  // Позиция воспроизведения
} punchium_s;

struct punchium_track_t {
	int sync_tick;				// Коэффициент синхронизации
	int8_t track_last;			  // Текущий трек
	// Форматы аудио
	int8_t file_type;			   // mp3/wav/ogg
	int16_t *buffer;				// PCM-данные (L/R)
	uint16_t sample_rate;		   // 48000, 44100 и т.д.
	uint16_t channels;			  // 1 (моно) или 2 (стерео)
	uint32_t total_samples;		 // Всего сэмплов (на канал)
	// Декодеры
	mp3dec_t mp3;				   // MP3-декодер
	mp3dec_file_info_t mp3_info;	// Инфо MP3
	drwav wav;					  // WAV-декодер
	stb_vorbis *vorbis;			 // OGG-декодер
	stb_vorbis_info vorbis_info;	// Инфо OGG
	int samples_read;			   // Прочитано сэмплов
} punchium_track;

uint8 punchium_volume_table[256] = // Логарифмическая кривая уровня громкости
{
	0x00, 0x03, 0x07, 0x0B, 0x0E, 0x12, 0x15, 0x18, 0x1B, 0x1E, 0x21, 0x24, 0x27, 0x29, 0x2C, 0x2F, 
	0x31, 0x34, 0x36, 0x38, 0x3B, 0x3D, 0x3F, 0x41, 0x44, 0x46, 0x48, 0x4A, 0x4C, 0x4E, 0x50, 0x51, 
	0x53, 0x55, 0x57, 0x59, 0x5A, 0x5C, 0x5E, 0x5F, 0x61, 0x63, 0x64, 0x66, 0x67, 0x69, 0x6A, 0x6C, 
	0x6D, 0x6F, 0x70, 0x72, 0x73, 0x74, 0x76, 0x77, 0x78, 0x7A, 0x7B, 0x7C, 0x7E, 0x7F, 0x80, 0x81, 
	0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 
	0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 
	0xA4, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xAF, 0xB0, 
	0xB1, 0xB2, 0xB3, 0xB3, 0xB4, 0xB5, 0xB6, 0xB6, 0xB7, 0xB8, 0xB9, 0xB9, 0xBA, 0xBB, 0xBC, 0xBC, 
	0xBD, 0xBE, 0xBE, 0xBF, 0xC0, 0xC1, 0xC1, 0xC2, 0xC3, 0xC3, 0xC4, 0xC5, 0xC5, 0xC6, 0xC7, 0xC7, 
	0xC8, 0xC9, 0xC9, 0xCA, 0xCA, 0xCB, 0xCC, 0xCC, 0xCD, 0xCE, 0xCE, 0xCF, 0xCF, 0xD0, 0xD1, 0xD1, 
	0xD2, 0xD2, 0xD3, 0xD3, 0xD4, 0xD5, 0xD5, 0xD6, 0xD6, 0xD7, 0xD7, 0xD8, 0xD9, 0xD9, 0xDA, 0xDA, 
	0xDB, 0xDB, 0xDC, 0xDC, 0xDD, 0xDD, 0xDE, 0xDF, 0xDF, 0xE0, 0xE0, 0xE1, 0xE1, 0xE2, 0xE2, 0xE3, 
	0xE3, 0xE4, 0xE4, 0xE5, 0xE5, 0xE6, 0xE6, 0xE7, 0xE7, 0xE8, 0xE8, 0xE9, 0xE9, 0xEA, 0xEA, 0xEA, 
	0xEB, 0xEB, 0xEC, 0xEC, 0xED, 0xED, 0xEE, 0xEE, 0xEF, 0xEF, 0xF0, 0xF0, 0xF0, 0xF1, 0xF1, 0xF2, 
	0xF2, 0xF3, 0xF3, 0xF4, 0xF4, 0xF4, 0xF5, 0xF5, 0xF6, 0xF6, 0xF7, 0xF7, 0xF7, 0xF8, 0xF8, 0xF9, 
	0xF9, 0xF9, 0xFA, 0xFA, 0xFB, 0xFB, 0xFC, 0xFC, 0xFC, 0xFD, 0xFD, 0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 
};

static void punchium_decoder_type(int src, uint8 *dst);

//инициализация музыкальных переменных и буферов
static void music_var_init (){
	punchium_s.music_track = 0;
	punchium_s.music_tick = 0;
	punchium_s.music_pos = 0;
	punchium_track.sync_tick = 0;
	punchium_track.track_last = 0;
	punchium_track.channels = 2;
	punchium_track.file_type = 0;
	punchium_track.sample_rate = 0;
	punchium_track.total_samples = 0;
	punchium_track.buffer = NULL;
	mp3dec_init(&punchium_track.mp3);
	memset(&punchium_track.mp3_info, 0, sizeof(mp3dec_file_info_t));
	memset(&punchium_track.wav, 0, sizeof(drwav));
	punchium_track.vorbis = NULL;
	memset(&punchium_track.vorbis_info, 0, sizeof(stb_vorbis_info));
	punchium_track.samples_read = 0;
}

// проверка наличия файла
static inline int punchium_file_exists(const char *path) {
#ifdef __vita__
    SceIoStat stat;
    return (sceIoGetstat(path, &stat) >= 0);
#else
	struct stat buffer;
	return (stat(path, &buffer) == 0);
#endif
}

// Загружает и декодирует трек (из папки `punchium/`), настраивает частоту дискретизации.
static void punchium_load_music_file(int track, int reload) {
	punchium_s.music_track = track;
	
	if (track == punchium_track.track_last) {
		if (reload) {
			punchium_s.music_pos = 0;
			punchium_s.music_tick = 0;
		}
		
		return;
	}
	
	punchium_track.track_last = track;
	
	if (!punchium_audio_track_format) {
		punchium_s.music_track = 0;
		return;
	}
	
	// Очистка памяти от предыдущего трека
	if (punchium_track.buffer) {
		free(punchium_track.buffer);
		punchium_track.buffer = NULL;
	}
	
	memset(&punchium_track.mp3, 0, sizeof(mp3dec_t));
	memset(&punchium_track.mp3_info, 0, sizeof(mp3dec_file_info_t));
	drwav_uninit(&punchium_track.wav);
	memset(&punchium_track.wav, 0, sizeof(drwav));
	
	if (punchium_track.vorbis) {
		stb_vorbis_close(punchium_track.vorbis);
		punchium_track.vorbis = NULL;
	}
	
	static const char* track_names[] = {
		[0x01] = "02 90's Acid Dub Character Select", [0x02] = "08 90's Dance", [0x03] = "42 1988 Commercial",
		[0x04] = "05 Asian Chill", [0x05] = "31 Bad Dudes vs Paprium", [0x06] = "43 Blade FM",
		[0x07] = "03 Bone Crusher", [0x0B] = "26 Club Shuffle", [0x0C] = "23 Continue",
		[0x0E] = "07 Cool Groove", [0x0F] = "36 Cyberpunk Ninja", [0x10] = "35 Cyberpunk Funk",
		[0x11] = "30 Cyber Interlude", [0x12] = "21 Cyborg Invasion", [0x13] = "44 Dark Alley",
		[0x14] = "29 Dark & Power Mad", [0x15] = "24 Intro", [0x16] = "27 Dark Rock",
		[0x17] = "04 Drumbass Boss", [0x18] = "45 Dubstep Groove", [0x19] = "15 Electro Acid Funk",
		[0x1B] = "28 Evolve", [0x1C] = "33 Funk Enhanced Mix", [0x1D] = "41 Game Over",
		[0x1E] = "46 Gothic", [0x20] = "13 Hard Rock", [0x21] = "22 Hardcore BP1",
		[0x22] = "11 Hardcore BP2", [0x23] = "38 Hardcore BP3", [0x24] = "40 Score",
		[0x25] = "47 House", [0x26] = "17 Indie Shuffle", [0x27] = "25 Indie Break Beat",
		[0x28] = "16 Jazzy Shuffle", [0x2A] = "19 Neo Metal", [0x2B] = "14 Neon Rider",
		[0x2E] = "09 Retro Beat", [0x2F] = "20 Sadness", [0x31] = "18 Slow Asian Beat",
		[0x32] = "48 Slow Mood", [0x33] = "49 Smooth Coords", [0x34] = "10 Spiral",
		[0x35] = "12 Stage Clear", [0x36] = "32 Summer Breeze", [0x37] = "06 Techno Beats",
		[0x38] = "50 Tension", [0x39] = "01 Theme of Paprium", [0x3A] = "39 Ending",
		[0x3B] = "34 Transe", [0x3C] = "37 Urban", [0x3D] = "51 Water",
		[0x3E] = "52 Waterfront Beat"};

	if (track < 0x01 || track > 0x3E || !track_names[track]) {
		punchium_s.music_track = 0;
		return;
	}

	char temp_path[512];
#ifdef _WIN32
	snprintf(temp_path, sizeof(temp_path), "%s\\paprium\\%s", g_rom_dir, track_names[track]);
#else
	snprintf(temp_path, sizeof(temp_path), "%s/paprium/%s", g_rom_dir, track_names[track]);
#endif
	
	bool track_loaded = false;
	uint8_t formats_to_try[] = {0, 1, 2, 3};
	formats_to_try[0] = punchium_audio_track_format;
	
	for (int i = 0; i < 5 && !track_loaded; i++) {
		uint8_t fmt = formats_to_try[i];
		if (fmt == 0) return;
		
		char full_path[512];
		strcpy(full_path, temp_path);
		
		switch (fmt) {
		// WAV
			case 1:
				strcat(full_path, ".wav");
				if (!punchium_file_exists(full_path)) {
					continue;
				}
				
				if (!drwav_init_file(&punchium_track.wav, full_path, NULL)) {
					continue;
				}
				
				if (punchium_track.wav.totalPCMFrameCount == 0) {
					drwav_uninit(&punchium_track.wav);
					continue;
				}
				
				punchium_track.channels = punchium_track.wav.channels;
				punchium_track.total_samples = punchium_track.wav.totalPCMFrameCount;
				punchium_track.sample_rate = punchium_track.wav.fmt.sampleRate;
				punchium_track.buffer = (int16_t*)malloc(punchium_track.total_samples * punchium_track.channels * sizeof(int16_t));
				
				if (!punchium_track.buffer) {
					drwav_uninit(&punchium_track.wav);
					continue;
				}
				
				drwav_read_pcm_frames_s16(&punchium_track.wav, punchium_track.wav.totalPCMFrameCount, punchium_track.buffer);
				track_loaded = true;
				break;
		
		// MP3		
			case 2:
				strcat(full_path, ".mp3");
				if (!punchium_file_exists(full_path)) {
					continue;
				}
				
				if (mp3dec_load(&punchium_track.mp3, full_path, &punchium_track.mp3_info, NULL, NULL)) {
					punchium_track.buffer = NULL;
					continue;
				}
				
				if (punchium_track.mp3_info.buffer == NULL || punchium_track.mp3_info.samples == 0) {
					continue;
				}
				
				punchium_track.buffer = punchium_track.mp3_info.buffer;
				punchium_track.total_samples = punchium_track.mp3_info.samples / punchium_track.mp3_info.channels;
				punchium_track.channels = punchium_track.mp3_info.channels;
				punchium_track.sample_rate = punchium_track.mp3_info.hz;
				track_loaded = true;
				break;
		
		// OGG
			case 3:
				strcat(full_path, ".ogg");
				if (!punchium_file_exists(full_path)) {
					continue;
				}
							
				int error = 0;
				punchium_track.vorbis = stb_vorbis_open_filename(full_path, &error, NULL);
				if (!punchium_track.vorbis) {
					continue;
				}
				
				punchium_track.vorbis_info = stb_vorbis_get_info(punchium_track.vorbis);
				punchium_track.sample_rate = punchium_track.vorbis_info.sample_rate;
				punchium_track.channels = punchium_track.vorbis_info.channels;
				punchium_track.total_samples = stb_vorbis_stream_length_in_samples(punchium_track.vorbis);
				size_t total_samples = punchium_track.total_samples * punchium_track.channels;
				punchium_track.buffer = (short*)malloc(total_samples * sizeof(short));
				
				if (!punchium_track.buffer) {
					stb_vorbis_close(punchium_track.vorbis);
					punchium_track.vorbis = NULL;
					continue;
				}
				punchium_track.samples_read = stb_vorbis_get_samples_short_interleaved(
					punchium_track.vorbis, 
					punchium_track.channels, 
					punchium_track.buffer, 
					total_samples);
				
				stb_vorbis_close(punchium_track.vorbis);
				punchium_track.vorbis = NULL;
				
				if (punchium_track.samples_read <= 0) {
					free(punchium_track.buffer);
					punchium_track.buffer = NULL;
					punchium_track.vorbis = NULL;
					continue;
				}
				
				track_loaded = true;
				break;
		}
	}
	
	if (!track_loaded) {
		punchium_s.music_track = 0;
		return;
	}
	
	switch (punchium_track.sample_rate) {
		case 48000: punchium_track.sync_tick = 0x10000; break;
		case 44100: punchium_track.sync_tick = 0xE666;  break;
		case 32000: punchium_track.sync_tick = 0xAAAA;  break;
		case 24000: punchium_track.sync_tick = 0x8000;  break;
		case 22050: punchium_track.sync_tick = 0x75C3;  break;
		case 16000: punchium_track.sync_tick = 0x5555;  break;
		case 12000: punchium_track.sync_tick = 0x4000;  break;
		case 11025: punchium_track.sync_tick = 0x3AE1;  break;
		case 8000:  punchium_track.sync_tick = 0x2AAA;  break;
		default: 
			if (punchium_track.sample_rate >= 0) {
				punchium_track.sync_tick = ((punchium_track.sample_rate * 0x10000) + 24000) / 48000;
			} else {
				punchium_track.sync_tick = 0x10000;
			}
			break;
	}
	
	punchium_s.music_tick = 0;
	punchium_s.music_pos = 0;
}

// Обрабатывает "нотный лист" (MIDI-подобные команды) для управления инструментами.
static void punchium_music_sheet()
{
	int ch;
	int l = 0, r = 0;

	/* 00-04 = WMMM */
	/* 05, 06, 07 */
	int sections = punchium_s.music_ram[0x09];
	/* 09 */
	int bars = (punchium_s.music_ram[0x0B] ? punchium_s.music_ram[0x0B] : 0x100) + 8;
	/* 0B */
	int cmds = punchium_s.music_ram[0x0D];
	/* 10-29 */
	/* 2A-43 */
	/* 44-5D */
	/* 5E-77 */
	/* 78-97 = title ^ 0xA5 */
	/* 98-B7 = author ^ 0xA5 */
	/* B8-D7 = comment ^ 0xA5 */


	if( punchium_s.music_segment == -1 ) {
		punchium_s.music_track = 0;
		return;
	}

	if( punchium_s.audio_tick < 4 )
		return;
	punchium_s.audio_tick = 0;

	for( ch = 0; ch < 26; ch++ ) {
		int ptr = *(uint16*)(punchium_s.music_ram + 0xD8 + (ch*sections*2) + (punchium_s.music_segment*2));
		int index = punchium_s.music_ram[(ptr + punchium_s.music_section)^1];
		int code = 0, arg = 0;
		int keyon = 0, duration = 0, freq = -1, volume = 0;
		punchium_voice_t *voice = punchium_s.music + ch;

		if( index == 0 )
			goto next;
		ptr += bars + (index-1) * 8;


		for( int lcv = 0; lcv < cmds; lcv++ ) {
			code = *(uint16*)(punchium_s.music_ram + ptr + lcv*2);
			arg = code & 0xFF;
			code >>= 8;


			if( code == 0x00 )
				{} //break;

			else if( code == 0x01 ) {  /* ?? */
				if( ch >= 11-1 ) keyon = code;
				voice->volume = 255 - punchium_volume_table[arg];  /* z80 table ? */
			}

			else if( code == 0x02 )
				voice->panning = arg;

			else if( code == 0x03 ) {
				voice->volume = 255 - punchium_volume_table[arg];  /* z80 table ? */
			}

			else if( code == 0x05 ) {
				voice->volume = 255 - punchium_volume_table[arg];  /* z80 table ? */
			}

			/* velocity */

			else if( code == 0x08 ) {
#if 0
				snprintf(error_str, "08 - %X %X %X\n", ch, ptr, arg);
				log_cb(RETRO_LOG_ERROR, error_str);
				//MessageBoxA(0, error_str, "---", 0);
#endif

				freq = arg;
			}

			else if( code == 0x0A ) {
#if 0
				snprintf(error_str, "0B - %X %X %X\n", ch, ptr, arg);
				log_cb(RETRO_LOG_ERROR, error_str);
				//MessageBoxA(0, error_str, "---", 0);
#endif			
				freq = 0;  /* faster ? */
			}

			else if( code == 0x0B ) {
#if 0
				snprintf(error_str, "0B - %X %X %X\n", ch, ptr, arg);
				log_cb(RETRO_LOG_ERROR, error_str);
#endif
				//MessageBoxA(0, error_str, "---", 0);
			}

			else if( code == 0x0C ) {
#if 0
				snprintf(error_str, "0C - %X %X %X\n", ch, ptr, arg);
				log_cb(RETRO_LOG_ERROR, error_str);
#endif
				//MessageBoxA(0, error_str, "---", 0);
			}

			else if( code == 0x0E ) {  /* stop ? */
				keyon = 0;
				voice->size = 0;
			}

			else if( code == 0x0F ) {  /* program */
				voice->program = arg;

				if( ch < 6 ) {  /* YM */
				}

				else if( ch < 10 ) {  /* PSG */
				}

				else
				{  /* wave */
				}

#if DEBUG_MODE
				snprintf(error_str, "[%d] Sample %X %X\n", ch, arg, ptr);
				log_cb(RETRO_LOG_ERROR, error_str);
#endif
			}
		}


		if( keyon ) {
#if 0
			uint8* ptr = punchium_wave_ram + voice->program*16;

			voice->ptr = (*(uint16*)(ptr + 0x00)<<16) + *(uint16*)(ptr + 0x02);
			voice->size = (*(uint16*)((punchium_wave_ram + 0x04 + voice->program*16))<<16) + *(uint16*)(punchium_wave_ram + 0x06 + voice->program*16);
			/* *(uint16*)(punchium_wave_ram + 0x08 + voice->program*16)<<16) + *(uint16*)(punchium_wave_ram + 0x0A + voice->program*16); */
			voice->type = *(uint16*)(punchium_wave_ram + 0x0C + voice->program*16) + 1;
			/* *(uint16*)(punchium_wave_ram + 0x0E + voice->program*16); */			

			voice->tick = 0;
			voice->time = 0;

			voice->keyon = keyon;
			voice->duration = 65536;
			voice->release = 256;
#endif
		}

		if( freq >= 0 ) {
			voice->type = freq;
			//if(ch == 16-1) //MessageBoxA(0,"hit","-",0);
		}

next:
		*(uint16*)(punchium_s.ram + ch*4 + 0x1B98) = index ? 0xE0 : 0;  /* L */
		*(uint16*)(punchium_s.ram + ch*4 + 0x1B9A) = index ? 0xE0 : 0;  /* R */
	}


end:
	if( (++punchium_s.music_section) >= bars ) {
		punchium_s.music_section = 0;

		if( (++punchium_s.music_segment) >= sections )
			punchium_s.music_segment = 0;
	}
}

// Генерирует аудио из декодированного файла с учетом громкости/панорамирования.
static void punchium_music_player(int *out_l, int *out_r) {
	if (punchium_s.music_track == 0 || punchium_track.buffer == NULL)
		return;

	if (punchium_s.music_pos >= punchium_track.total_samples)
		punchium_s.music_pos = 0;
	
	int l = 0, r = 0;
	int sample_pos = punchium_s.music_pos * punchium_track.channels;

	if (sample_pos < punchium_track.total_samples * punchium_track.channels) {
		l = punchium_track.buffer[sample_pos];
		r = (punchium_track.channels > 1) ? punchium_track.buffer[sample_pos + 1] : l;
	}
		
	punchium_s.music_tick += punchium_track.sync_tick;
	if (punchium_s.music_tick >= 0x10000) {
		punchium_s.music_tick -= 0x10000;
		punchium_s.music_pos++;
	}

	*out_l += (l * punchium_volume_table[punchium_s.music_volume]) >> 8;
	*out_r += (r * punchium_volume_table[punchium_s.music_volume]) >> 8;
}


static void punchium_sfx_voice(int *out_l, int *out_r) {
    static const int _rates[] = {1 << 16, 2 << 16, 4 << 16, 5 << 16, 8 << 16, 9 << 16};
    int l = 0, r = 0;

    for (int ch = 0; ch < 8; ch++) {
        punchium_voice_t *voice = punchium_s.sfx + ch;
        if (voice->size == 0) continue;

        // Чтение сэмпла (оригинальный способ)
        uint8 sample_byte = *(uint8 *)(cart.rom + punchium_sfx_ptr + (voice->ptr ^ 1));
        int sample;

        // Обработка глубины звука (как в оригинале)
        int depth = voice->type & 0x03;
        if (depth == 1) {
            sample = (sample_byte * 256) - 32768;  // 8-бит -> 16-бит со знаком
        } else if (depth == 2) {
            sample = (voice->count == 0) ? (sample_byte >> 4) : (sample_byte & 0x0F);
            sample = (sample * 4096) - 32768;  // 4-бит -> 16-бит со знаком
        } else {
            sample = 0;  // Неподдерживаемый формат
        }

        // Применение громкости (деление, как в оригинале)
        sample = (sample * voice->volume) / 0x400;

        // Панорамирование (деление, а не сдвиг!)
        int pan = voice->panning;
        int sample_l = (sample * ((pan <= 0x80) ? 0x80 : (0x100 - pan))) / 0x80;
        int sample_r = (sample * ((pan >= 0x80) ? 0x80 : pan)) / 0x80;

        l += sample_l;
        r += sample_r;

        // Эхо-эффект (как в оригинале)
        if (voice->flags & 0x4000) {
            if (voice->echo & 1) {
                punchium_s.echo_l[punchium_s.echo_ptr % (48000/6)] += (sample_l * 33) / 100;
            } else {
                punchium_s.echo_r[punchium_s.echo_ptr % (48000/6)] += (sample_r * 33) / 100;
            }
        }

        // Усиление (как в оригинале)
        if (voice->flags & 0x100) {
            l = (l * 125) / 100;
            r = (r * 125) / 100;
        }
		
		voice->time++;

        // Обновление позиции в сэмпле
        int rate = _rates[voice->type >> 4];
        voice->tick += 0x10000;
        voice->tick -= (voice->flags & 0x8000) ? 0x800 : 0;
        voice->tick -= (voice->flags & 0x2000) ? 0x8000 : 0;

        if (voice->tick >= rate) {
            voice->tick -= rate;
            voice->count++;
            voice->size--;

            if (voice->count >= depth) {
                voice->ptr++;
                voice->count = 0;
            }
        }

        // Проверка на зацикливание
        if (voice->size == 0 && voice->loop) {
            uint8 *sfx = cart.rom + punchium_sfx_ptr;
            voice->ptr = (*(uint16 *)(sfx + voice->num * 8) << 16) | (*(uint16 *)(sfx + voice->num * 8 + 2));
            voice->size = (*(uint8 *)(sfx + voice->num * 8 + 4) << 16) | (*(uint16 *)(sfx + voice->num * 8 + 6));
            voice->count = 0;
        }
    }

    *out_l += l;
    *out_r += r;
}

// Главный аудиомиксер: обрабатывает музыку, SFX, эффекты (эхо), применяет громкость, отправляет данные в blip_buffer.
void punchium_audio(int cycles)
{
	int lcv;
	int samples = blip_clocks_needed(snd.blips[3], cycles);

	punchium_load_music_file(punchium_s.music_track, 0);
	punchium_s.audio_tick++;

	for( lcv = 0; lcv < samples; lcv++ ) {
		int l = 0, r = 0;
		int ch;

		punchium_s.echo_l[punchium_s.echo_ptr] = 0;
		punchium_s.echo_r[punchium_s.echo_ptr] = 0;

		punchium_music_player(&l, &r);
		punchium_sfx_voice(&l, &r);

		punchium_s.echo_ptr = (punchium_s.echo_ptr+1) % (48000/6);

		l += punchium_s.echo_l[punchium_s.echo_ptr];
		r += punchium_s.echo_r[punchium_s.echo_ptr];

		l = (l * punchium_s.sfx_volume) / 0x100;
		r = (r * punchium_s.sfx_volume) / 0x100;

		if( punchium_s.audio_flags & 0x08 ) {  /* gain */
			l = l * 4 / 2;
			r = r * 4 / 2;
		}

		// Клиппинг
		l = (l > 32767) ? 32767 : (l < -32768) ? -32768 : l;
		r = (r > 32767) ? 32767 : (r < -32768) ? -32768 : r;

		blip_add_delta_fast(snd.blips[3], lcv, l-punchium_s.out_l, r-punchium_s.out_r);
		punchium_s.out_l = l;
		punchium_s.out_r = r;
	}

	punchium_music_sheet();
	blip_end_frame(snd.blips[3], samples);
	punchium_track.track_last = punchium_s.music_track;
}

// Декомпрессия LZ-RLE (используется для тайлов/анимаций).
static void punchium_decoder_lz_rle(uint src, uint8 *dst)
{
	int size = 0;
	int len, lz, rle, code;


#if 0
	snprintf(error_str, "LZ-RLE %X\n", src);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	while(1) {
		int type = cart.rom[(src++)^1];

		code = type >> 6;
		len = type & 0x3F;


		if( (code == 0) && (len == 0))
			break;

		else if( code == 1 )
			rle = cart.rom[(src++)^1];

		else if( code == 2 )
			lz = size - cart.rom[(src++)^1];


		while( len-- > 0 ) {
			switch(code) {
			case 0: dst[(size++)^1] = cart.rom[(src++)^1]; break;
			case 1: dst[(size++)^1] = rle; break;
			case 2: dst[(size++)^1] = dst[(lz++)^1]; break;
			case 3: dst[(size++)^1] = 0; break;
			}
		}
	}

#if 0
	snprintf(error_str, "STOP %X - %X\n", src, size);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	punchium_s.decoder_size = size;
}

// Декомпрессия LZO (более сложный алгоритм).
static void punchium_decoder_lzo(uint src, uint8 *dst)
{
	int size = 0;
	int len, lz, raw;
	int state = 0;


#if 0
	snprintf(error_str, "LZO %X\n", src);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	while(1) {
		int code = cart.rom[(src++)^1];

		if( code & 0x80 ) goto code_80;
		if( code & 0x40 ) goto code_40;
		if( code & 0x20 ) goto code_20;
		if( code & 0x10 ) goto code_10;


code_00:
		if( state == 0 ) {
			raw = code & 0x0F;

			if( raw == 0 ) {
				int extra = 0;

				while(1) {
					raw = cart.rom[(src++)^1];
					if( raw ) break;

					extra += 255;
				}

				raw += extra;
				raw += 15;
			}
			raw += 3;

			len = 0;
			state = 4;
			goto copy_loop;
		}

		else if( state < 4 ) {
			raw = code & 0x03;
			lz = (code >> 2) & 0x03;

			lz += cart.rom[(src++)^1] << 2;
			lz += 1;

			len = 2;
			goto copy_loop;
		}

		else {
			raw = code & 0x03;
			lz = (code >> 2) & 0x03;

			lz += cart.rom[(src++)^1] << 2;
			lz += 2049;

			len = 3;
			goto copy_loop;
		}


code_10:
		len = code & 0x07;

		if( len == 0 ) {
			int extra = 0;

			while(1) {
				len = cart.rom[(src++)^1];
				if( len ) break;

				extra += 255;
			}

			len += extra;
			len += 7;
		}
		len += 2;


		lz = ((code >> 3) & 1) << 14;

		code = cart.rom[(src++)^1];
		raw = code & 0x03;
		lz += code >> 2;

		lz += cart.rom[(src++)^1] << 6;
		lz += 16384;

		if( lz == 16384 ) break;
		goto copy_loop;


code_20:
		len = code & 0x1F;

		if( len == 0 ) {
			int extra = 0;

			while(1) {
				len = cart.rom[(src++)^1];
				if( len ) break;

				extra += 255;
			}

			len += extra;
			len += 31;
		}
		len += 2;


		code = cart.rom[(src++)^1];
		raw = code & 0x03;

		lz = code >> 2;
		lz += cart.rom[(src++)^1] << 6;
		lz += 1;

		goto copy_loop;


code_40:
		raw = code & 0x03;
		len = ((code >> 5) & 1) + 3;
		lz = (code >> 2) & 0x07;

		lz += cart.rom[(src++)^1] << 3;
		lz += 1;

		goto copy_loop;


code_80:
		raw = code & 0x03;
		len = ((code >> 5) & 0x03) + 5;
		lz = (code >> 2) & 0x07;

		lz += cart.rom[(src++)^1] << 3;
		lz += 1;


copy_loop:
		if( len > 0 )
			state = raw;
		else
			state = 4;

		
		lz = size - lz;

		while(1) {
			if( len > 0 ) {
				dst[(size++)^1] = dst[(lz++)^1];
				len--;
			}

			else if( raw > 0 ) {
				dst[(size++)^1] = cart.rom[(src++)^1];
				raw--;
			}

			else
				break;
		}		
	}


#if 0
	snprintf(error_str, "END %X - %X\n", src, size);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	punchium_s.decoder_size = size;
}

// Выбор декомпрессора на основе заголовка данных.
static void punchium_decoder_type(int src, uint8 *dst)
{
	int type = cart.rom[(src++)^1];

	if( type == 0x80 )
		punchium_decoder_lz_rle(src, dst);

	else if( type == 0x81 )
		punchium_decoder_lzo(src, dst);

	else {
#if DEBUG_MODE
		snprintf(error_str, "%X - Decoder_Type %X\n", src-1, type);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "punchium_decoder_init", 0);
#endif
	}
}

// Обертка для декомпрессии с управлением через регистры.
static void punchium_decoder(int mode)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Decoder %02X -- %04X %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		mode, *(uint16 *)(punchium_s.ram + 0x1E10), *(uint16 *)(punchium_s.ram + 0x1E12), *(uint16 *)(punchium_s.ram + 0x1E14));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	int offset = *(uint16 *)(punchium_s.ram + 0x1E10);
	int ptr = (*(uint16 *)(punchium_s.ram + 0x1E12) << 16) + *(uint16 *)(punchium_s.ram + 0x1E14);

#if DEBUG_MODE
	if( mode != 2 && mode != 7 ) {
		snprintf(error_str, "Mode %X", mode);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "Decoder_Init", 0);
	}
#endif

	punchium_decoder_type(ptr, punchium_s.decoder_ram + offset);
	punchium_s.decoder_mode = mode;
	punchium_s.decoder_ptr = offset;


#if DEBUG_MODE
	if( punchium_cmd_count != 3 ) {
		snprintf(error_str, "Decoder %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void punchium_decoder_copy(int arg)
{
	/* *(uint16 *)(punchium_s.ram + 0x1E10); */
	int offset = *(uint16 *)(punchium_s.ram + 0x1E12);
	int size = *(uint16 *)(punchium_s.ram + 0x1E14);

#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Decoder_Copy %02X [%X] -- %04X %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg, punchium_s.decoder_ptr,
		*(uint16 *)(punchium_s.ram + 0x1E10), *(uint16 *)(punchium_s.ram + 0x1E12), *(uint16 *)(punchium_s.ram + 0x1E14));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif
#if DEBUG_MODE
	if( *(uint16 *)(punchium_s.ram + 0x1E10) ) {
		snprintf(error_str, "punchium_decoder 1E10 = %X", *(uint16 *)(punchium_s.ram + 0x1E10));
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0,error_str,"decoder",0);
	}
#endif

	punchium_s.decoder_ptr = offset;
	punchium_s.decoder_size = size;

#if DEBUG_MODE
	if( punchium_cmd_count != 3 ) {
		snprintf(error_str, "Decoder_Copy %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}

// Отрисовка спрайтов: декодирует тайлы, формирует DMA-команды для VDP, управляет анимациями.
static void punchium_sprite(int index)
{
	int lcv, spr_x, spr_y, count;
	int spritePtr, gfxPtr;

	int dmaSize = 0;
	int dmaPtr = *(uint16*) (punchium_s.ram + 0x1F16);

	int spriteCount = *(uint16*) (punchium_s.ram + 0x1F18);

	int anim = *(uint16*) (punchium_s.ram + 0xF80 + index*16);
	int nextAnim = *(uint16*) (punchium_s.ram + 0xF82 + index*16);
	int obj = *(uint16*) (punchium_s.ram + 0xF84 + index*16) & 0x7FFF;
	//*(uint16*) (punchium_s.ram + 0xF86 + index*16);
	int objAttr = *(uint16*) (punchium_s.ram + 0xF88 + index*16);
	int reset = *(uint16*) (punchium_s.ram + 0xF8A + index*16);
	int pos_x = *(uint16*) (punchium_s.ram + 0xF8C + index*16);
	int pos_y = *(uint16*) (punchium_s.ram + 0xF8E + index*16);

	int src = punchium_s.draw_src;
	int vram = punchium_s.draw_dst;
	int flip_h = objAttr & 0x800;
	int animFlags;


	int animPtr = *(uint32*) (punchium_obj_ram + (obj+1)*4);
	int framePtr = punchium_s.obj[index];


	if( index != 0x30 ) {
		//return;
	}


#if DEBUG_MODE  /* frozen enemy */
	{
		int color = -1;
		int pri = -1;

		if(0) {}
		else if( (obj > 0x00 && obj < 0x30) && (anim == 5) ) color = 0;
		else if( (obj == 0x0D) && (anim == 3) ) color = 0;

		if( color != -1 ) {
			objAttr &= ~0x6000;

			switch(color) {
			case 0: break;
			case 1: objAttr += 0x2000; break;
			case 2: objAttr += 0x4000; break;
			case 3: objAttr += 0x6000; break;
			}
		}

		if( pri != -1 ) {
			objAttr &= ~0x8000;

			switch(pri) {
			case 0: break;
			case 1: objAttr += 0x8000; break;
			}
		}
	}
#endif


#if DEBUG_SPRITE
	uint8 *ptr = punchium_s.ram + 0xF80 + index*16;

	snprintf(error_str, "[%d] [%04X:%04X] DM Sprite %02X - %04X %04X %04X %04X %04X %04X X=%04X Y=%04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		index,
		*(uint16 *)(ptr+0), *(uint16 *)(ptr+2), *(uint16 *)(ptr+4), *(uint16 *)(ptr+6),
		*(uint16 *)(ptr+8), *(uint16 *)(ptr+10), *(uint16 *)(ptr+12), *(uint16 *)(ptr+14) );
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


reload:
	if( reset == 1 ) {
		framePtr = *(uint32*) (punchium_obj_ram + animPtr + anim*4);

		*(uint16*) (punchium_s.ram + 0xF8A + index*16) = 0;
	}
	if( (framePtr == 0) || (framePtr == -1) ) {
		*(uint16*) (punchium_s.ram + 0xF80 + index*16) = 0;
		return;
	}


	spritePtr  = punchium_obj_ram[framePtr + 0] << 0;
	spritePtr += punchium_obj_ram[framePtr + 1] << 8;
	spritePtr += punchium_obj_ram[framePtr + 2] << 16;
	animFlags = punchium_obj_ram[framePtr + 3];


#if DEBUG_SPRITE
	snprintf(error_str, "%X - %X %X %X - %X - %X %X\n",
		((obj+1) & 0x7FFF)*4,
		animPtr, anim, animPtr + anim*4,
		framePtr,
		spritePtr, animFlags);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	if( spritePtr == 0 ) return;


	framePtr += 4; 

	if( animFlags < 0x80 ) {
		if( nextAnim == 0xFFFF ) {
			int nextFrame;

			nextFrame  = punchium_obj_ram[framePtr + 0] << 0;
			nextFrame += punchium_obj_ram[framePtr + 1] << 8;
			nextFrame += punchium_obj_ram[framePtr + 2] << 16;

			framePtr = nextFrame;
		}
		else if( reset == 0 ) {
			anim = nextAnim;
			reset = 1;

			goto reload;
		}
	}

	punchium_s.obj[index] = framePtr;


	count = punchium_obj_ram[(spritePtr++)^1];
	int flags2 = punchium_obj_ram[(spritePtr++)^1];


#if 0
	snprintf(error_str, "%d - %X %X %X\n", count, src, vram, flags2);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	spr_x = pos_x;
	spr_y = pos_y;

	for( lcv = 0; lcv < count; lcv++ ) {
		int misc = ((punchium_obj_ram[spritePtr + 3] >> 4) & 0x0F);
		int size_x = ((punchium_obj_ram[spritePtr + 3] >> 2) & 0x03) + 1;
		int size_y = ((punchium_obj_ram[spritePtr + 3] >> 0) & 0x03) + 1;
		int tile = *(uint16*) (punchium_obj_ram + spritePtr + 4);
		int tileAttr = *(uint16*) (punchium_obj_ram + spritePtr + 6) & ~0x1FF;
		int ofs = *(uint16*) (punchium_obj_ram + spritePtr + 6) & 0x1FF;

		int tilePtr = punchium_tile_ptr + tile*4;
		int tileSize = size_x * size_y * 0x20;

		int sprAttr = 0;


		if( !flip_h )
			spr_x += (int8) punchium_obj_ram[spritePtr + 1];
		else
			spr_x -= (int8) punchium_obj_ram[spritePtr + 1];

		spr_y += (int8) punchium_obj_ram[spritePtr + 0];
		spritePtr += 8;


		if( tile == 0 ) continue;
		if( spriteCount >= 94 ) continue;


		sprAttr  = (tileAttr & 0x8000) ? 0x8000 : (objAttr & 0x8000);
		sprAttr += (tileAttr & 0x6000) ? (tileAttr & 0x6000) : (objAttr & 0x6000);
		sprAttr += (tileAttr & 0x1800) ^ (objAttr & 0x1800);


		if( (spr_y >= 128) && (spr_y + size_y*8 < 368) ) {
			if( (!flip_h && ((spr_x + size_x*8 >= 128) && spr_x < 448)) ||
				 (flip_h && ((spr_x - size_x*8 < 448) && spr_x >= 128)) ) {
				if( spriteCount < 80 ) {
					*(uint16*) (punchium_s.ram + 0xB00 + spriteCount*8) = spr_y & 0x3FF;
					*(uint16*) (punchium_s.ram + 0xB02 + spriteCount*8) = ((size_x-1) << 10) + ((size_y-1) << 8) + (spriteCount+1);
					*(uint16*) (punchium_s.ram + 0xB04 + spriteCount*8) = sprAttr + ((vram / 0x20) & 0x7FF);
					*(uint16*) (punchium_s.ram + 0xB06 + spriteCount*8) = (spr_x - ((!flip_h) ? 0 : size_x*8)) & 0x1FF;

#if DEBUG_SPRITE
					snprintf(error_str, "%d / %X %X %X %X\n",
						spriteCount,
						*(uint16*) (punchium_s.ram + 0xB00 + (spriteCount-0)*8),
						*(uint16*) (punchium_s.ram + 0xB02 + (spriteCount-0)*8),
						*(uint16*) (punchium_s.ram + 0xB04 + (spriteCount-0)*8),
						*(uint16*) (punchium_s.ram + 0xB06 + (spriteCount-0)*8)
					);
					log_cb(RETRO_LOG_ERROR, error_str);
#endif
				}
				else {
					if( spriteCount == 80 ) {  /* sprite 0 lock */
						punchium_s.ram[0xD7A] = 1;  /* exp.s list */

						memcpy(punchium_s.exps_ram, punchium_s.ram + 0xB00, 8);
						punchium_s.exps_ram[2] = 14;  /* normal list */

						spriteCount++;
					}

					*(uint16*) (punchium_s.exps_ram + 0 + (spriteCount-80)*8) = spr_y & 0x3FF;
					*(uint16*) (punchium_s.exps_ram + 2 + (spriteCount-80)*8) = ((size_x-1) << 10) + ((size_y-1) << 8) + ((spriteCount-80)+1);
					*(uint16*) (punchium_s.exps_ram + 4 + (spriteCount-80)*8) = sprAttr + ((vram / 0x20) & 0x7FF);
					*(uint16*) (punchium_s.exps_ram + 6 + (spriteCount-80)*8) = (spr_x - ((!flip_h) ? 0 : size_x*8)) & 0x1FF;

#if DEBUG_SPRITE
					snprintf(error_str, "%d / %X %X %X %X\n",
						spriteCount,
						*(uint16*) (punchium_s.ram + 0x1F20 + (spriteCount-80)*8),
						*(uint16*) (punchium_s.ram + 0x1F22 + (spriteCount-80)*8),
						*(uint16*) (punchium_s.ram + 0x1F24 + (spriteCount-80)*8),
						*(uint16*) (punchium_s.ram + 0x1F26 + (spriteCount-80)*8)
					);
					log_cb(RETRO_LOG_ERROR, error_str);
#endif
				}

				spriteCount++;

#if DEBUG_SPRITE
				snprintf(error_str, "%d %d / %d %d %X %X / %X %X %X\n", spriteCount, count, spr_x, spr_y, tileAttr, objAttr, src, vram, tile);
				log_cb(RETRO_LOG_ERROR, error_str);
#endif
			}
		}



		int ptr = punchium_tile_ptr + (*(uint16*)(cart.rom + tilePtr) << 16) + *(uint16*)(cart.rom + tilePtr + 2);
		static uint8 tile_ram[0x10000];
		
		punchium_decoder_type(ptr, tile_ram);
		memcpy(punchium_s.ram + src, tile_ram + ofs * 0x20, tileSize);
		//if( !tiledupe && dmaPtr < dmalimit && vram < vramlimit && tile < tilelimit)

		*(uint16*) (punchium_s.ram + 0x1400 + dmaPtr*16) = 0x8F02;
		*(uint16*) (punchium_s.ram + 0x1402 + dmaPtr*16) = 0x9300 + ((tileSize >> 1) & 0xFF);
		*(uint16*) (punchium_s.ram + 0x1404 + dmaPtr*16) = 0x9500 + ((src >> 1) & 0xFF);
		*(uint16*) (punchium_s.ram + 0x1406 + dmaPtr*16) = 0x9400 + ((tileSize >> 9) & 0xFF);
		*(uint16*) (punchium_s.ram + 0x1408 + dmaPtr*16) = 0x9700;
		*(uint16*) (punchium_s.ram + 0x140A + dmaPtr*16) = 0x9600 + ((src >> 9) & 0xFF);
		*(uint16*) (punchium_s.ram + 0x140C + dmaPtr*16) = 0x4000 + (vram & 0x3FFF);
		*(uint16*) (punchium_s.ram + 0x140E + dmaPtr*16) = 0x0080 + (vram >> 14);

		*(uint16*) (punchium_s.ram + 0x1F16) = ++dmaPtr;
		*(uint16*) (punchium_s.ram + 0x1F18) = spriteCount;


		src += tileSize;
		dmaSize += tileSize;
		vram += tileSize;
	}


	punchium_s.draw_src = src;
	punchium_s.draw_dst = vram;
}

// Сброс состояния спрайтов.
static void punchium_sprite_init(int arg)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Sprite_Init %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	memset(punchium_s.ram + 0x1F20, 0, 14*8);

#if DEBUG_MODE
	if( punchium_cmd_count != 0 ) {
		snprintf(error_str, ">>  Sprite_Init %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}

// Подготовка к отрисовке (инициализация DMA-буфера).
static void punchium_sprite_start(int arg)
{
#if DEBUG_SPRITE
	snprintf(error_str, "[%d] [%04X:%04X] DM Sprite_Start %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	int count = *(uint16*)(punchium_s.ram + 0x1F18);

	*(uint16*) (punchium_s.ram + 0x1F16) = 0x0001;  /* dma count */

	*(uint16*) (punchium_s.ram + 0x1400) = 0x8f02;
	*(uint16*) (punchium_s.ram + 0x1402) = 0x9340;
	*(uint16*) (punchium_s.ram + 0x1404) = 0x9580;
	*(uint16*) (punchium_s.ram + 0x1406) = 0x9401;
	*(uint16*) (punchium_s.ram + 0x1408) = 0x9700;
	*(uint16*) (punchium_s.ram + 0x140a) = 0x9605;
	*(uint16*) (punchium_s.ram + 0x140c) = 0x7000;
	*(uint16*) (punchium_s.ram + 0x140e) = 0x0083;


	if( count < 80 )
		memcpy(punchium_s.ram + 0x1F20, punchium_s.ram + 0xB00, 14*8);
	else
		memcpy(punchium_s.ram + 0x1F20, punchium_s.exps_ram, 14*8);


	punchium_s.draw_src = 0x2000;
	punchium_s.draw_dst = 0x200;


#if DEBUG_MODE
	if( punchium_cmd_count != 0 ) {
		snprintf(error_str, ">>  Sprite_Start %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}

// Остановка отрисовки (очистка списка спрайтов).
static void punchium_sprite_stop(int arg)
{
#if DEBUG_SPRITE
	snprintf(error_str, "[%d] [%04X:%04X] DM Sprite_Stop %02X -- %d sprites\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg, *(uint16*)(punchium_s.ram + 0x1f18));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	int count = *(uint16*)(punchium_s.ram + 0x1F18);

	if( count == 0 ) {
		memset(punchium_s.ram + 0xB00, 0, 8);
		memset(punchium_s.exps_ram, 0, 8);
	}
	else if( count <= 80 ) {
		punchium_s.ram[0xB02 + (count-1)*8] = 0;

		if( count <= 14 )
			punchium_s.exps_ram[2 + (count-1)*8] = 0;
	}
	else
		punchium_s.exps_ram[2 + (count-81)*8] = 0;


	if( arg == 2 )
		*(uint16*) (punchium_s.ram + 0x1F1C) = 1;  /* exp.s - force draw */


#if DEBUG_MODE
	if( punchium_cmd_count != 0 ) {
		snprintf(error_str, ">>  Sprite_Stop %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void punchium_sprite_pause(int arg)
{
#if DEBUG_SPRITE
	snprintf(error_str, "[%d] [%04X:%04X] DM Sprite_Pause %02X -- %d sprites\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg, *(uint16*)(punchium_s.ram + 0x1f18));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	int count = *(uint16*)(punchium_s.ram + 0x1F18);

	if( count == 0 )
		memset(punchium_s.ram + 0xB00, 0, 8);

#if DEBUG_MODE
	if( punchium_cmd_count != 0 ) {
		snprintf(error_str, ">>  Sprite_Pause %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}

// Инициализация масштабирования графики (используется для эффектов).
static void punchium_scaler_init(int arg)
{
	int row, col, out;
	static uint8 temp[0x800];

#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Scaler_Init %02X - %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg,
		*(uint16 *)(punchium_s.ram + 0x1E10), *(uint16 *)(punchium_s.ram + 0x1E12));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	int ptr = (*(uint16 *)(punchium_s.ram + 0x1E10) << 16) + *(uint16 *)(punchium_s.ram + 0x1E12);
	punchium_decoder_type(ptr, temp);


	out = 0;

	for( col = 0; col < 64; col++ ) {
		for( row = 0; row < 64; row++ ) {
			if( col & 1 )
				punchium_s.scaler_ram[out] = temp[row*32 + (col/2)^1] & 0x0F;
			else
				punchium_s.scaler_ram[out] = temp[row*32 + (col/2)^1] >> 4;

			out++;
		}
	}

#if DEBUG_MODE
	if( punchium_cmd_count != 2 ) {
		snprintf(error_str, ">>  Scaler_Init %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}

// Применение масштабирования к графике (построчная обработка).
static void punchium_scaler(int arg)
{
	int row, col;

	int left = *(uint16 *)(punchium_s.ram + 0x1E10);
	int right = *(uint16 *)(punchium_s.ram + 0x1E12);
	int scale = *(uint16 *)(punchium_s.ram + 0x1E14);
	int ptr = *(uint16 *)(punchium_s.ram + 0x1E16);
	int step = 64 * 0x10000 / scale;
	int ptr_frac = 0;

#if DEBUG_SPRITE
	snprintf(error_str, "[%d] [%04X:%04X] DM Scaler %02X --  %04X %04X %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg, *(uint16 *)(punchium_s.ram + 0x1E10), *(uint16 *)(punchium_s.ram + 0x1E12), *(uint16 *)(punchium_s.ram + 0x1E14), *(uint16 *)(punchium_s.ram + 0x1E16));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	for( col = 0; col < 128; col++ ) {
		int base = (col & 4) ? 0x600 : 0x200;
		int out = ((col / 8) * 64) + ((col & 2) / 2);

		for( row = 0; row < 64; row += 2 ) {
			if( (col >= left) && (col < right) ) {
				if(col & 1)
					punchium_s.ram[base + (out^1)] += punchium_s.scaler_ram[row*64 + ptr] & 0x0F;
				else
					punchium_s.ram[base + (out^1)] = punchium_s.scaler_ram[row*64 + ptr] << 4;
			}
			else if( (col & 1) == 0 )
				punchium_s.ram[base + (out^1)] = 0;

			out += 2;
		}

		if( (col >= left) && (col < right) && (ptr < 64) ) {
			ptr_frac += 0x10000;

			while( ptr_frac >= step ) {
				ptr++;
				ptr_frac -= step;
			}
		}
	}

#if DEBUG_MODE
	if( punchium_cmd_count != 4 ) {
		snprintf(error_str, ">>  Scaler %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}

// Чтение из SRAM (сохранения) в RAM.
static void punchium_sram_read(int bank)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Sram_Read %02X -- %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		bank, *(uint16 *)(punchium_s.ram + 0x1E10));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	int offset = *(uint16 *)(punchium_s.ram + 0x1E10);

	if( (bank >= 1) && (bank <= 4) )
		memcpy(punchium_s.ram + offset, sram.sram + ((bank-1) * 0x780), 0x780);

#if DEBUG_MODE
	if( punchium_cmd_count != 1 ) {
		snprintf(error_str, ">>  Sram_Read %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}

// Запись из RAM в SRAM.
static void punchium_sram_write(int bank)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Sram_Write %02X -- %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		bank, *(uint16 *)(punchium_s.ram + 0x1E10), *(uint16 *)(punchium_s.ram + 0x1E12));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	/* *(uint16 *)(punchium_s.ram + 0x1E10) */
	int offset = *(uint16 *)(punchium_s.ram + 0x1E12);

	if( (bank >= 1) && (bank <= 4) )
		memcpy(sram.sram + ((bank-1) * 0x780), punchium_s.ram + offset, 0x780);

#if DEBUG_MODE
	if( *(uint16 *)(punchium_s.ram + 0x1E10) != 0xBEEF ) {
		snprintf(error_str, ">>  SRAM write 1E10 %X\n", *(uint16 *)(punchium_s.ram + 0x1E10));
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "sram write", 0);
	}

	if( punchium_cmd_count != 1 ) {
		snprintf(error_str, ">>  Sram_Write %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}

// Управление маппингом ROM (переключение банков памяти).
static void punchium_mapper(int arg)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Mapper %02X -- %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg, *(uint16 *)(punchium_s.ram + 0x1E10), *(uint16 *)(punchium_s.ram + 0x1E12));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	memcpy(punchium_s.ram + 0x8000, cart.rom + 0x8000, 0x8000);  /* troll */

#if DEBUG_MODE
	if( punchium_cmd_count != 2 ) {
		snprintf(error_str, ">>  Mapper %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}

 // Первичная загрузка: инициализирует указатели на ресурсы (музыка, SFX, тайлы).
static void punchium_boot(int arg)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Boot %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	punchium_music_ptr = (*(uint16*)(punchium_s.ram + 0x1E10) << 16) + *(uint16*)(punchium_s.ram + 0x1E12);
	/* (*(uint16*)(punchium_s.ram + 0x1E14) << 16) + *(uint16*)(punchium_s.ram + 0x1E16); */
	punchium_wave_ptr = (*(uint16*)(punchium_s.ram + 0x1E18) << 16) + *(uint16*)(punchium_s.ram + 0x1E1A);
	/* (*(uint16*)(punchium_s.ram + 0x1E1C) << 16) + *(uint16*)(punchium_s.ram + 0x1E1E); */
	punchium_sfx_ptr = (*(uint16*)(punchium_s.ram + 0x1E20) << 16) + *(uint16*)(punchium_s.ram + 0x1E22);
	punchium_sprite_ptr = (*(uint16*)(punchium_s.ram + 0x1E24) << 16) + *(uint16*)(punchium_s.ram + 0x1E26);
	punchium_tile_ptr = (*(uint16*)(punchium_s.ram + 0x1E28) << 16) + *(uint16*)(punchium_s.ram + 0x1E2A);

	/* punchium_wave_unpack(punchium_wave_ptr, punchium_wave_ram); */
	punchium_decoder_type(punchium_sprite_ptr, punchium_obj_ram);
	punchium_s.decoder_size = 0;

#if DEBUG_MODE
	punchium_cmd_count = 0;
#endif
}


static void punchium_EC(int arg)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM EC %02X = %02X %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg, *(uint16 *)(punchium_s.ram + 0x1E10), *(uint16 *)(punchium_s.ram + 0x1E12));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

#if DEBUG_MODE
	if( *(uint16 *)(punchium_s.ram + 0x1E10) ) {
		snprintf(error_str, ">>  EC %02X\n", *(uint16 *)(punchium_s.ram + 0x1E10));
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "ec", 0);
	}
#endif

#if DEBUG_MODE
	if( punchium_cmd_count != 2 ) {
		snprintf(error_str, ">>  EC %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}

// Запускает музыкальный трек: загружает данные нотного листа, сбрасывает состояние каналов.
static void punchium_music(int track)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Music %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		track);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	track &= 0x7F;


	int ptr = (*(uint16*)(cart.rom + punchium_music_ptr + track*4)<<16) + (*(uint16*)(cart.rom + punchium_music_ptr + track*4 + 2));
	punchium_decoder_type(punchium_music_ptr + ptr, punchium_s.music_ram);
	punchium_s.decoder_size = 0;

	punchium_s.music_section = 0;
	punchium_s.music_segment = 0;
	punchium_s.audio_tick = 0;

	memset(punchium_s.music, 0, sizeof(punchium_s.music));
	for( int ch = 0; ch < 26; ch++ ) {
		punchium_voice_t *voice = punchium_s.music + ch;

		voice->panning = 0x80;
		voice->volume = 0x80;
		voice->program = punchium_s.music_ram[0x2A + ch^1];
	}

#if DEBUG_MODE
	if( punchium_cmd_count != 0 ) {
		snprintf(error_str, ">>  Music %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif

	punchium_load_music_file(track, 1);
}

// Устанавливает громкость музыки.
static void punchium_music_volume(int level)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Music_Volume %02X -- %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		level, *(uint16 *)(punchium_s.ram + 0x1E10), *(uint16 *)(punchium_s.ram + 0x1E12));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	punchium_s.music_volume = level;
	/* *(uint16 *)(punchium_s.ram + 0x1E10); */
	/* *(uint16 *)(punchium_s.ram + 0x1E12); */

#if 0
	// stream mp3 / ogg, manage tempo
#endif

#if DEBUG_MODE
	if(*(uint16 *)(punchium_s.ram + 0x1E10) != 0x80) {
		snprintf(error_str, ">>  Music_Volume = %X\n", *(uint16 *)(punchium_s.ram + 0x1E10));
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "ooo", 0);
	}

	if(*(uint16 *)(punchium_s.ram + 0x1E12) != 0x00 && *(uint16 *)(punchium_s.ram + 0x1E12) != 0x08) {
		snprintf(error_str, ">>  Music_Volume-2 = %X\n", *(uint16 *)(punchium_s.ram + 0x1E12));
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "ooo", 0);
	}

	if( punchium_cmd_count != 2 ) {
		snprintf(error_str, ">>  Music_Volume %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void punchium_music_setting(int flag)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Music_Setting %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		flag);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	if( flag == 8 )
		punchium_s.music_segment = -1;

	else if( flag == 0 )
		punchium_s.music_segment = -1;

#if DEBUG_MODE
	else {
		snprintf(error_str, ">>  Music_Setting = %X\n", flag);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "ooo", 0);
	}
#endif

#if DEBUG_MODE
	if( punchium_cmd_count != 0 ) {
		snprintf(error_str, ">>  Music_Setting %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void punchium_music_special(int flag)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Music_Special %02X = %04X %04X %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		flag, *(uint16 *)(punchium_s.ram + 0x1E10), *(uint16 *)(punchium_s.ram + 0x1E12), *(uint16 *)(punchium_s.ram + 0x1E14), *(uint16 *)(punchium_s.ram + 0x1E16));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	if( flag == 1 ) {
		//*(uint16 *)(punchium_s.ram + 0x1E10)
		//*(uint16 *)(punchium_s.ram + 0x1E12)

#if DEBUG_MODE
		if( *(uint16 *)(punchium_s.ram + 0x1E10) != 0x40 ) {
			snprintf(error_str, ">>  Music_Special_01 %X count\n", *(uint16 *)(punchium_s.ram + 0x1E10));
			log_cb(RETRO_LOG_ERROR, error_str);
			//MessageBoxA(0, error_str, "count", 0);
		}

		if( *(uint16 *)(punchium_s.ram + 0x1E12) != 0x08 ) {
			snprintf(error_str, ">>  Music_Special_01-2 %X count\n", *(uint16 *)(punchium_s.ram + 0x1E10));
			log_cb(RETRO_LOG_ERROR, error_str);
			//MessageBoxA(0, error_str, "count", 0);
		}

		if( punchium_cmd_count != 2 ) {
			snprintf(error_str, ">>  Music_Special-01 %d count\n", punchium_cmd_count);
			log_cb(RETRO_LOG_ERROR, error_str);
			//MessageBoxA(0, error_str, "count", 0);
		}
#endif
	}

	else if( flag == 2 ) {
		//*(uint16 *)(punchium_s.ram + 0x1E10)  /* 4 = crisis, 0 = normal ? */

#if DEBUG_MODE
		if( *(uint16 *)(punchium_s.ram + 0x1E10) != 4 && *(uint16 *)(punchium_s.ram + 0x1E10) != 0 ) {
			snprintf(error_str, ">>  Music_Special_02 %X count\n", *(uint16 *)(punchium_s.ram + 0x1E10));
			log_cb(RETRO_LOG_ERROR, error_str);
			//MessageBoxA(0, error_str, "count", 0);
		}

		if( punchium_cmd_count != 1 ) {
			snprintf(error_str, ">>  Music_Special-02 %d count\n", punchium_cmd_count);
			log_cb(RETRO_LOG_ERROR, error_str);
			//MessageBoxA(0, error_str, "count", 0);
		}
#endif
	}

	else if( flag == 4 ) {
		//*(uint16 *)(punchium_s.ram + 0x1E10)  /* 81 = blu pill */

#if DEBUG_MODE
		if( *(uint16 *)(punchium_s.ram + 0x1E10) != 0x80 && *(uint16 *)(punchium_s.ram + 0x1E10) != 0x81 ) {
			snprintf(error_str, ">>  Music_Special_04 %X count\n", *(uint16 *)(punchium_s.ram + 0x1E10));
			log_cb(RETRO_LOG_ERROR, error_str);
			//MessageBoxA(0, error_str, "count", 0);
		}

		if( punchium_cmd_count != 1 ) {
			snprintf(error_str, ">>  Music_Special-04 %d count\n", punchium_cmd_count);
			log_cb(RETRO_LOG_ERROR, error_str);
			//MessageBoxA(0, error_str, "count", 0);
		}
#endif
	}

	else if( flag == 6 ) {  /* sax man */
		//*(uint16 *)(punchium_s.ram + 0x1E10)
		//*(uint16 *)(punchium_s.ram + 0x1E12)
		//*(uint16 *)(punchium_s.ram + 0x1E14)

#if DEBUG_MODE
		if( *(uint16 *)(punchium_s.ram + 0x1E10) != 1 ) {
			snprintf(error_str, ">>  Music_Special_06 %X count\n", *(uint16 *)(punchium_s.ram + 0x1E10));
			log_cb(RETRO_LOG_ERROR, error_str);
			//MessageBoxA(0, error_str, "count", 0);
		}

		if( punchium_cmd_count != 3 ) {
			snprintf(error_str, ">>  Music_Special-06 %d count\n", punchium_cmd_count);
			log_cb(RETRO_LOG_ERROR, error_str);
			//MessageBoxA(0, error_str, "count", 0);
		}
#endif
	}

	else if( flag == 7 ) {  /* blu pill - stage title */
		//*(uint16 *)(punchium_s.ram + 0x1E10)

#if DEBUG_MODE
		if( *(uint16 *)(punchium_s.ram + 0x1E10) != 0x6C && *(uint16 *)(punchium_s.ram + 0x1E10) != 0xA0 ) {
			snprintf(error_str, ">>  Music_Special_07 %X count\n", *(uint16 *)(punchium_s.ram + 0x1E10));
			log_cb(RETRO_LOG_ERROR, error_str);
			//MessageBoxA(0, error_str, "count", 0);
		}

		if( punchium_cmd_count != 1 ) {
			snprintf(error_str, ">>  Music_Special-06 %d count\n", punchium_cmd_count);
			log_cb(RETRO_LOG_ERROR, error_str);
			//MessageBoxA(0, error_str, "count", 0);
		}
#endif
	}

#if DEBUG_MODE
	else {
		snprintf(error_str, ">>  Music special = %X\n", flag);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "ooo", 0);
	}
#endif
}

// Настройки аудио (DAC, NTSC/PAL, усиление).
static void punchium_audio_setting(int flags)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Audio_Settings %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		flags);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	punchium_s.audio_flags = flags;
	punchium_s.ram[0x1801] = flags & 0x01;  /* dac */
	punchium_s.ram[0x1800]  = (flags & 0x01) ? 0x80 : 0x00;  /* dac */
	punchium_s.ram[0x1800] += (flags & 0x02) ? 0x40 : 0x00;  /* ntsc */

#if DEBUG_MODE
	if( punchium_cmd_count != 0 ) {
		snprintf(error_str, ">>  Audio_Settings %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}

// Устанавливает громкость SFX.
static void punchium_sfx_volume(int level)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X] DM Sfx_Volume %02X -- %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		level, *(uint16 *)(punchium_s.ram + 0x1E10));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	punchium_s.sfx_volume = level;
	/* *(uint16 *)(punchium_s.ram + 0x1E10); */
	/* *(uint16 *)(punchium_s.ram + 0x1E12); */


#if DEBUG_MODE
	if(*(uint16 *)(punchium_s.ram + 0x1E10) != 0x80) {
		snprintf(error_str, ">>  Sfx_Volume-1 = %X\n", *(uint16 *)(punchium_s.ram + 0x1E10));
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "ooo", 0);
	}

	if(*(uint16 *)(punchium_s.ram + 0x1E12) != 0x00 && punchium_cmd_count == 2) {
		snprintf(error_str, ">>  Sfx_Volume-2 = %X\n", *(uint16 *)(punchium_s.ram + 0x1E12));
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "ooo", 0);
	}
#endif


#if DEBUG_MODE
	if( punchium_cmd_count != 1 && punchium_cmd_count != 2 ) {
		snprintf(error_str, ">>  Sfx_Volume %d count\n", punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		//MessageBoxA(0, error_str, "count", 0);
	}
#endif
}

// Обработка отладочной печати
void punchium_cmd_print_debug(int cmd, int data, int argscnt, int v_counter, int punchium_cmd_count)
{
#if DEBUG_MODE
	switch (argscnt) {
		case 0:
			snprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X\n",
				v_counter,
				(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
				cmd, data);
			break;
		case 1:
			snprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X -- %04X\n",
				v_counter,
				(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
				cmd, data,
				*(uint16_t *)(punchium_s.ram + 0x1E10));
			break;
		case 2:
			snprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X -- %04X %04X\n",
				v_counter,
				(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
				cmd, data,
				*(uint16_t *)(punchium_s.ram + 0x1E10), *(uint16_t *)(punchium_s.ram + 0x1E12));
			break;
		case 4:
			snprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X -- %04X %04X %04X %04X\n",
				v_counter,
				(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
				cmd, data,
				*(uint16_t *)(punchium_s.ram + 0x1E10), *(uint16_t *)(punchium_s.ram + 0x1E12), 
				*(uint16_t *)(punchium_s.ram + 0x1E14), *(uint16_t *)(punchium_s.ram + 0x1E16));
			break;
		case 5:
			snprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X -- %04X %04X %04X %04X %04X\n",
				v_counter,
				(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
				cmd, data,
				*(uint16_t *)(punchium_s.ram + 0x1E10), *(uint16_t *)(punchium_s.ram + 0x1E12), 
				*(uint16_t *)(punchium_s.ram + 0x1E14), *(uint16_t *)(punchium_s.ram + 0x1E16), 
				*(uint16_t *)(punchium_s.ram + 0x1E18));
			break;
		case 9:
			snprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X -- %04X %04X %04X %04X %04X ..\n",
				v_counter,
				(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
				cmd, data,
				*(uint16_t *)(punchium_s.ram + 0x1E10), *(uint16_t *)(punchium_s.ram + 0x1E12), 
				*(uint16_t *)(punchium_s.ram + 0x1E14), *(uint16_t *)(punchium_s.ram + 0x1E16), 
				*(uint16_t *)(punchium_s.ram + 0x1E18));
			break;
		default:
			if (argscnt == 4) {
				snprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X -- %04X %04X %04X %04X\n",
					v_counter,
					(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
					cmd, data,
					*(uint16_t *)(punchium_s.ram + 0x1E10), *(uint16_t *)(punchium_s.ram + 0x1E12), 
					*(uint16_t *)(punchium_s.ram + 0x1E14), *(uint16_t *)(punchium_s.ram + 0x1E16));
			} else {
				snprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X ## %04X %04X %04X %04X\n",
					v_counter,
					(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
					cmd, data,
					*(uint16_t *)(punchium_s.ram + 0x1E10), *(uint16_t *)(punchium_s.ram + 0x1E12), 
					*(uint16_t *)(punchium_s.ram + 0x1E14), *(uint16_t *)(punchium_s.ram + 0x1E16));
			}
			snprintf(error_str, "DM %02X %02X\n", cmd, data);
			break;
	}
	
	log_cb(RETRO_LOG_ERROR, error_str);
	
	if (argscnt >= 0 && argscnt <= 9 && punchium_cmd_count != argscnt) {
		snprintf(error_str, "%X %d count\n", cmd, punchium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
	}
#endif
}

// Обработка воспроизведения звукового эффекта
void punchium_sfx_play(int data, int v_counter, int retaddr)
{
	int chan, vol, pan, flags;
	int ptr, size, type;
	int lcv;
	int newch = 0, maxtime = 0;
	const int rates[] = {1,2,4,5,8,9};
	uint8_t *sfx = punchium_sfx_ptr + cart.rom;

#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X <== %04X:%04X] DM Sfx_Play %02X -- %04X %04X %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, (retaddr>>16)&0xffff, retaddr&0xffff,
		data, *(uint16_t *)(punchium_s.ram + 0x1E10), *(uint16_t *)(punchium_s.ram + 0x1E12), 
		*(uint16_t *)(punchium_s.ram + 0x1E14), *(uint16_t *)(punchium_s.ram + 0x1E16));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	chan = *(uint16_t *)(punchium_s.ram + 0x1E10);
	vol = *(uint16_t *)(punchium_s.ram + 0x1E12);
	pan = *(uint16_t *)(punchium_s.ram + 0x1E14);
	flags = *(uint16_t *)(punchium_s.ram + 0x1E16);

#if DEBUG_MODE
	if (flags & 0x100) {
		snprintf(error_str, "Sfx flags %X\n", flags);
		log_cb(RETRO_LOG_ERROR, error_str);
	} else if (!(flags & 0x8000) && !(flags & 0x4000) && !(flags & 0x2000) && 
			   !(flags & 0x800) && !(flags & 0x400) && flags != 0) {
		snprintf(error_str, "Sfx flags %X\n", flags);
		log_cb(RETRO_LOG_ERROR, error_str);
	}

	if (chan > 0x80) {
		// Сообщение о предупреждении для канала SFX
	}
#endif

	ptr = (*(uint16_t *)(sfx + data*8) << 16) | (*(uint16_t *)(sfx + data*8 + 2));
	size = (*(uint8_t *)(sfx + data*8 + 4) << 16) | (*(uint16_t *)(sfx + data*8 + 6));
	type = *(uint8_t *)(sfx + data*8 + 5);

#if DEBUG_MODE
	snprintf(error_str, "%X %X | %X %X %X | %X %X %X\n",
		punchium_sfx_ptr + data*8, punchium_sfx_ptr + ptr,
		ptr, size, type,
		rates[type >> 4], type & 0x03, type & 0x0C);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	// Поиск свободного канала
	for (lcv = 0; lcv < 8; lcv++, chan >>= 1) {
		if ((chan & 1) == 0) continue;

		if (punchium_s.sfx[lcv].size) {
			if (maxtime < punchium_s.sfx[lcv].time) {
				maxtime = punchium_s.sfx[lcv].time;
				newch = lcv;
			}
			continue;
		}

		newch = lcv;
		break;
	}

	// Настройка параметров звукового эффекта
	punchium_s.sfx[newch].volume = vol;
	punchium_s.sfx[newch].panning = pan;
	punchium_s.sfx[newch].type = type;
	punchium_s.sfx[newch].num = data;
	punchium_s.sfx[newch].flags = flags;
	punchium_s.sfx[newch].loop = 0;
	punchium_s.sfx[newch].count = 0;
	punchium_s.sfx[newch].time = 0;
	punchium_s.sfx[newch].tick = 0;
	punchium_s.sfx[newch].decay = 0;

	if (flags & 0x4000) {
		punchium_s.sfx[newch].echo = (punchium_s.echo_pan++) & 1;
	}

	punchium_s.sfx[newch].ptr = ptr;
	punchium_s.sfx[newch].start = ptr;
	punchium_s.sfx[newch].size = size;
}

// Обработка зацикливания звукового эффекта
void punchium_sfx_loop(int data, int v_counter, int retaddr)
{
#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X <== %04X:%04X] DM Sfx_Loop %02X -- %04X %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, (retaddr>>16)&0xffff, retaddr&0xffff,
		data, *(uint16_t *)(punchium_s.ram + 0x1E10), *(uint16_t *)(punchium_s.ram + 0x1E12), 
		*(uint16_t *)(punchium_s.ram + 0x1E14));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	for (int lcv = 0; lcv < 8; lcv++, data >>= 1) {
		if ((data & 1) == 0) continue;

		punchium_s.sfx[lcv].volume = *(uint16_t *)(punchium_s.ram + 0x1E10);
		punchium_s.sfx[lcv].panning = *(uint16_t *)(punchium_s.ram + 0x1E12);
		punchium_s.sfx[lcv].decay = *(uint16_t *)(punchium_s.ram + 0x1E14);
		punchium_s.sfx[lcv].loop = 1;
		break;
	}
}

// Обработка остановки звукового эффекта
static void punchium_sfx_off(int data, int v_counter, int retaddr, int lcv)
{
	int flags;

#if DEBUG_MODE
	snprintf(error_str, "[%d] [%04X:%04X <== %04X:%04X] DM Sfx_Off %02X -- %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, (retaddr>>16)&0xffff, retaddr&0xffff,
		data, *(uint16_t *)(punchium_s.ram + 0x1E10));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	flags = *(uint16_t *)(punchium_s.ram + 0x1E10);

	if (flags != 0) {
		// Сообщение о предупреждении для флагов SFX_Off
	}

	for (lcv = 0; lcv < 8; lcv++) {
		if (!(data & (1 << lcv))) continue;

		if (flags == 0) {
			punchium_s.sfx[lcv].size = 0;
		}

		punchium_s.sfx[lcv].decay = flags;
		punchium_s.sfx[lcv].loop = 0;
		break;
	}
}

// Обработчик команд от игры (0x1FEA): вызывает нужную функцию по `cmd`.
void punchium_cmd(int data)
{
	int cmd = data >> 8;
	int lcv;
	int retaddr;
	int argscnt;

	data &= 0xFF;

	if (m68k.prev_pc == 0xb4394) {
		retaddr = m68k_read_immediate_32(m68k.dar[15] + 16);
		argscnt = 4;
	} else {
		retaddr = m68k_read_immediate_32(m68k.dar[15] + 4);
		argscnt = 0;
	}

	switch (cmd) {
#if DEBUG_MODE
		case 0x81:
		case 0x83:
		case 0x95:
		case 0x96:
		case 0xA4:
		case 0xA9:
		case 0xB6:
			punchium_cmd_print_debug(cmd, data, 0, v_counter, punchium_cmd_count);
			break;

		case 0xD0:
			punchium_cmd_print_debug(cmd, data, 2, v_counter, punchium_cmd_count);
			punchium_cmd_print_debug(cmd, data, 5, v_counter, punchium_cmd_count);
			break;

		case 0xE7:  /* T-574120-0 = after demo loop */
			punchium_cmd_print_debug(cmd, data, 9, v_counter, punchium_cmd_count);
			break;

		case 0xD5:  /* secret room */
		case 0xEC:
			punchium_cmd_print_debug(cmd, data, 2, v_counter, punchium_cmd_count);
			break;
#endif
		case 0x84: punchium_mapper(data); break;
		case 0x88: punchium_audio_setting(data); break;
		case 0x8C: punchium_music(data); break;
		case 0x8D: punchium_music_setting(data); break;
		case 0xAD: punchium_sprite(data); break;
		case 0xAE: punchium_sprite_start(data); break;
		case 0xAF: punchium_sprite_stop(data); break;
		case 0xB0: punchium_sprite_init(data); break;
		case 0xB1: punchium_sprite_pause(data); break;
		case 0xC6: punchium_boot(data); break;
		case 0xC9: punchium_music_volume(data); break;
		case 0xCA: punchium_sfx_volume(data); break;
		case 0xD1: punchium_sfx_play(data, v_counter, retaddr); break;
		case 0xD2: punchium_sfx_off(data, v_counter, retaddr, lcv); break;
		case 0xD3: punchium_sfx_loop(data, v_counter, retaddr); break;
		case 0xD6: punchium_music_special(data); break;
		case 0xDA: punchium_decoder(data); break;
		case 0xDB: punchium_decoder_copy(data); break;
		case 0xDF: punchium_sram_read(data); break;
		case 0xE0: punchium_sram_write(data); break;
		case 0xF4: punchium_scaler_init(data); break;
		case 0xF5: punchium_scaler(data); break;

#if DEBUG_MODE
		default:
			handle_print_debug(cmd, data, argscnt, v_counter, punchium_cmd_count);
			break;
#endif
	}

	*(uint16_t*)(punchium_s.ram + 0x1FEA) &= 0x7FFF;
}

static int punchium_addr_test(uint32 address, int mode)
{
#if DEBUG_MODE
	return 0;
#endif


	if( address < 0x100 ) return 0;  /* exception table */

	if( skip_boot1 ) {
		if( address == 0x14CE ) {
			skip_boot1 = 0;
		}
		return 0;
	}


#if DEBUG_MODE
	if( mode == 0 ) {
		if( address >= 0x8000 ) return 0;
	}
#endif


#if DEBUG_MODE
	if( address >= 0x2000 && address < 0x8000 ) return 0;
#endif

#if DEBUG_MODE
	if( address >= 0x200 && address < 0xb00 ) return 0;
#endif

#if DEBUG_MODE
	if( address >= 0xb00 && address < 0xf00 ) return 0;
	if( address >= 0x1f20 && address < 0x1f90 ) return 0;  /* exp.s */
#endif

#if DEBUG_MODE
	if( address >= 0xF80 && address < 0x1290 ) return 0;  /* OBJ list */
#endif

#if 0
	if( address >= 0x1290 && address < 0x1400 ) {
		//MessageBoxA(0, "address", "me", 0);
		return 0;
	}
#endif

#if 0
	if( address >= 0x1B40 && address < 0x1b90 ) {
		//MessageBoxA(0, "address 2", "me", 0);
		return 1;
	}
#endif

#if 0
	if( address >= 0xd80 && address < 0xf80 ) {
		//MessageBoxA(0, "address 3", "me", 0);
		return 1;
	}
#endif

#if DEBUG_MODE
	if( address >= 0x1400 && address < 0x1800 ) return 0;  /* DMA list */	
#endif

#if DEBUG_MODE
	if( address >= 0x1800 && address < 0x1A00 ) return 0;  /* DAC list ?? */
#endif

#if DEBUG_MODE
	if( address == 0x1F10 ) return 0;  /* DMA size = header [10h] + packet [x words] */
	if( address == 0x1F16 ) return 0;  /* DMA transfer count */
	if( address == 0x1F18 ) return 0;  /* OBJ count */
	if( address == 0x1F12 ) return 0;  /* sprite ram available ??? 0B00 or 1200 */
#endif

#if DEBUG_MODE
	if( address >= 0x1b98 && address < 0x1c00 ) return 0;  /* soundshock x26 */
#endif


	if( address == 0x1FEA ) return 0;  /* DM command */


	//if( address == 0x1FE4 ) return 0;  /* DM ?? */
	//if( address == 0x1FE6 ) return 0;  /* DM ?? */

	return 1;
}

// Обработчики чтения из RAM (с эмуляцией регистров).
static inline unsigned int FASTCALL punchium_r8(uint32 address)
{
	int data = punchium_s.ram[address^1];

#if DEBUG_MODE
	if( punchium_addr_test(address, 0) ) {
		snprintf(error_str, "[%04X:%04X] R8 %04X = %04X\n",
			(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, address, data);

		log_cb(RETRO_LOG_ERROR, error_str);
	}
#endif


	if( address == 0x1800 )
		data = 0;

	else if( address >= 0x1880 && address < 0x1b00 ) {
		data = rand() % 256;
		//punchium_s.ram[address^1] = data;  /* dma blowout if no limit */
	}


	return data;
}

// Обработчики чтения из RAM (с эмуляцией регистров).
static inline unsigned int FASTCALL punchium_r16(uint32 address)
{
	int data = 0;

	if( (address == 0xC000) && (punchium_s.decoder_size > 0) ) {
#if DEBUG_MODE
		snprintf(error_str, "[%d] Decoder %X %X @ C000\n", v_counter, punchium_s.decoder_ptr, punchium_s.decoder_size);
		log_cb(RETRO_LOG_ERROR, error_str);
#endif

		int max, size;

		if( punchium_s.decoder_mode == 2 )
			max = 0x4000;

		else if( punchium_s.decoder_mode == 7 )
			max = 0x800;


		size = (punchium_s.decoder_size > max) ? max : punchium_s.decoder_size;

		memcpy(punchium_s.ram + 0xC000, punchium_s.decoder_ram + punchium_s.decoder_ptr, size);

		punchium_s.decoder_ptr += size;
		punchium_s.decoder_size -= size;
	}


	switch( address ) {
	case 0x1FE4:
		data = 0xFFFF;
		data &= ~(1<<2);
		data &= ~(1<<6);
		break;

	case 0x1FE6:
		data = 0xFFFF;
		data &= ~(1<<14);
		data &= ~(1<<8);  /* sram */
		data &= ~(1<<9);  /* sram */
		break;
	
	case 0x1FEA:
		data = 0xFFFF;
		data &= ~(1<<15);
		break;

	default:
		data = *(uint16 *)(punchium_s.ram + address);
		break;
	}


#if DEBUG_MODE
	if( punchium_addr_test(address, 0) ) {
		snprintf(error_str, "[%d] [%04X:%04X] R16 %04X = %04X\n",
			v_counter, (m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, address, data);

		log_cb(RETRO_LOG_ERROR, error_str);
	}
#endif

	return data;
}

// Обработчики записи в RAM (с триггерами команд).
static inline void FASTCALL punchium_w8(uint32 address, uint32 data)
{
#if DEBUG_MODE
	if( punchium_addr_test(address, 1) ) {
		snprintf(error_str, "[%d] [%04X:%04X] W8 %04X = %02X\n",
			v_counter, (m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, address, data);

		log_cb(RETRO_LOG_ERROR, error_str);
	}
#endif

	punchium_s.ram[address^1] = data;
}

// Обработчики записи в RAM (с триггерами команд).
static inline void FASTCALL punchium_w16(uint32 address, uint32 data)
{
#if DEBUG_MODE
	if( punchium_addr_test(address, 1) ) {
		snprintf(error_str, "[%d] [%04X:%04X] W16 %04X = %04X\n",
			v_counter, (m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, address, data);

		log_cb(RETRO_LOG_ERROR, error_str);
	}

	if( address >= 0x1E10 && address <= 0x1E30 ) punchium_cmd_count++;
#endif

	*(uint16 *)(punchium_s.ram + address) = data;
	
	if( address == 0x1FEA ) {
		punchium_cmd(data);
		punchium_cmd_count = 0;
	}
}

// Работа с IO-портами (TMSS, управление банками).
static inline unsigned int FASTCALL punchium_io_r8(uint32 address)
{
	if( address == 0xA14101 )
		return punchium_tmss;

	return ctrl_io_read_byte(address);
}

// Работа с IO-портами (TMSS, управление банками).
static inline void FASTCALL punchium_io_w8(uint32 address, uint32 data)
{
	if( address >= 0xA130F3 && address <= 0xA130FF )  /* no bank mapper */
		return;

	if( address == 0xA14101 ) {
		punchium_tmss = data;
		return;
	}

	ctrl_io_write_byte(address, data);
}

// Настройка memory map для M68k: назначает обработчики чтения/записи для областей памяти (RAM, IO).
static void punchium_map()
{
	m68k.memory_map[0x00].base	= punchium_s.ram;
	m68k.memory_map[0x00].read8   = punchium_r8;
	m68k.memory_map[0x00].read16  = punchium_r16;
	m68k.memory_map[0x00].write8  = punchium_w8;
	m68k.memory_map[0x00].write16 = punchium_w16;

	m68k.memory_map[0xA1].read8  = punchium_io_r8;
	m68k.memory_map[0xA1].write8 = punchium_io_w8;

	zbank_memory_map[0x00].read  = punchium_r8;
	zbank_memory_map[0x00].write = punchium_w8;
}

// Первичная инициализация: настройка памяти, загрузка указателей на ресурсы (музыка, SFX, спрайты), патчинг ROM.
static void punchium_init()
{
	int ptr;

	punchium_map();

	music_var_init ();

	memset(&punchium_s, 0, sizeof(punchium_s));
	memcpy(punchium_s.ram, cart.rom, 0x10000);


	/* fast loadstate */
	ptr = (*(uint16*)(cart.rom + 0xaf77c) << 16) + *(uint16*)(cart.rom + 0xaf77e);

	punchium_music_ptr = (*(uint16*)(cart.rom + 0x10054)<< 16) + *(uint16*)(cart.rom + 0x10056);
	// (*(uint16*)(cart.rom + 0x10058)<< 16) + *(uint16*)(cart.rom + 0x1005a);
	punchium_wave_ptr = (*(uint16*)(cart.rom + ptr + 0x774)<< 16) + *(uint16*)(cart.rom + ptr + 0x776);
	// (*(uint16*)(cart.rom + 0x1005c)<< 16) + *(uint16*)(cart.rom + 0x1005e);
	punchium_sfx_ptr = (*(uint16*)(cart.rom + ptr + 0x778)<< 16) + *(uint16*)(cart.rom + ptr + 0x77a);
	punchium_sprite_ptr = (*(uint16*)(cart.rom + 0x10014)<< 16) + *(uint16*)(cart.rom + 0x10016);
	punchium_tile_ptr = (*(uint16*)(cart.rom + ptr + 0x780)<< 16) + *(uint16*)(cart.rom + ptr + 0x782);

	punchium_decoder_type(punchium_sprite_ptr, punchium_obj_ram);
	punchium_s.decoder_size = 0;

	*(uint16*)(punchium_s.ram + 0x192) = 0x3634;  /* 6-button, multitap */

	punchium_s.ram[0x1D1D^1] = 0x04;  /* rom ok - dynamic patch */
	punchium_s.ram[0x1D2C^1] = 0x67;

#if 1  /* boot hack */
	*(uint16*) (punchium_s.ram + 0x1560) = 0x4EF9;
	*(uint16*) (punchium_s.ram + 0x1562) = 0x0001;
	*(uint16*) (punchium_s.ram + 0x1564) = 0x0100;
#endif

	fast_dma_hack = 1;  /* skip vram management */

#if DEBUG_CHEAT  /* cheat - big hurt */
	*(uint16*) (cart.rom + 0x9FE38 + 6) = 0x007F;	/* Tug */
	*(uint16*) (cart.rom + 0x9FE58 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9FF18 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9FF38 + 6) = 0x007F;

	*(uint16*) (cart.rom + 0x9FB58 + 6) = 0x007F;	/* Alex */
	*(uint16*) (cart.rom + 0x9FB78 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9FBF8 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9FC18 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9FCB8 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9FCD8 + 6) = 0x007F;

	*(uint16*) (cart.rom + 0x9F758 + 6) = 0x007F;	/* Dice */
	*(uint16*) (cart.rom + 0x9F778 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9F798 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9F7B8 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9F7D8 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9F898 + 6) = 0x007F;
#endif

#if 1  /* WM text - pre-irq delay */
	*(uint16*)(cart.rom + 0xb9094) = 0x2079;
	*(uint16*)(cart.rom + 0xb9096) = 0x000a;
	*(uint16*)(cart.rom + 0xb9098) = 0xf85c;

	*(uint16*)(cart.rom + 0xb909a) = 0x20bc;
	*(uint16*)(cart.rom + 0xb909c) = 0x0000;
	*(uint16*)(cart.rom + 0xb909e) = 0x0003;

	*(uint16*)(cart.rom + 0xb90a0) = 0x20bc;
	*(uint16*)(cart.rom + 0xb90a2) = 0x0000;
	*(uint16*)(cart.rom + 0xb90a4) = 0x0003;

	*(uint16*)(cart.rom + 0xb90a6) = 0x20bc;
	*(uint16*)(cart.rom + 0xb90a8) = 0x0000;
	*(uint16*)(cart.rom + 0xb90aa) = 0x0003;

	*(uint16*)(cart.rom + 0xb90ac) = 0x20bc;
	*(uint16*)(cart.rom + 0xb90ae) = 0x0000;
	*(uint16*)(cart.rom + 0xb90b0) = 0x0003;

	*(uint16*)(cart.rom + 0xb90b2) = 0x20bc;
	*(uint16*)(cart.rom + 0xb90b4) = 0x0000;
	*(uint16*)(cart.rom + 0xb90b6) = 0x0003;
#endif


	srand((uintptr_t) &ptr);  /* discard time library */
	punchium_s.echo_pan = rand();


	punchium_s.music_segment = -1;
}

// Полный сброс состояния игры: инициализирует MP3-декодер, сбрасывает флаги загрузки, очищает состояние звука/графики.
static void punchium_reset()
{
	punchium_init();

	//extern int trace_start;
	//trace_start = 1;
	skip_boot1 = 1;
}

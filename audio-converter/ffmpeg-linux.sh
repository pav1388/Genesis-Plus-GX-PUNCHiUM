#!/bin/bash

completed=777

# Проверка наличия FFmpeg
check_ffmpeg() {
    if [ -f "../tools/ffmpeg/linux/ffmpeg" ]; then
        FFMPEG="../tools/ffmpeg/linux/ffmpeg"
        return 0
    fi

    if command -v ffmpeg &> /dev/null; then
        FFMPEG="ffmpeg"
        return 0
    fi

    echo "Error: FFmpeg not found in:"
    echo "- Local path: ../tools/ffmpeg/linux/ffmpeg"
    echo "- System PATH"
    read -p "Press any key to continue..." -n1 -s
    echo
    return 1
}

# Главное меню
main_menu() {
    clear
    echo
    echo "================================"
    echo "      FFMPEG Audio Converter"
    echo "================================"
    echo
    
    if [ "$completed" -eq 0 ]; then
        echo "      Output files in folder: $OUTPUT_DIR/*.$format"
        echo "   [V] COMPLETED :)"
        echo
        completed=777
    elif [ "$completed" -eq 1 ]; then
        echo "   [X] ERROR :("
        echo
        completed=777
    fi
    
    OUTPUT_DIR=""
    format=""
    
    echo "   Formats:"
    echo "  1) WAV  (recommended for lowspec)"
    echo "  2) MP3"
    echo "  3) OGG"
    echo
    echo "  0) Exit"
    echo
    echo "Place original MP3 files in 'music-original' folder"
    echo
    
    read -p "Select format [1-3,0]: " choice
    
    case $choice in
        1) format="wav"; codec_menu_wav ;;
        2) format="mp3"; codec_mp3 ;;
        3) format="ogg"; codec_menu_ogg ;;
        0) exit 0 ;;
        *) main_menu ;;
    esac
}

# Меню кодеков для WAV
codec_menu_wav() {
    clear
    echo
    echo "================================"
    echo "      FFMPEG Audio Converter"
    echo "================================"
    echo
    echo "   Codec:"
    echo "  1) ADPCM IMA    4 bit  (recommended for lowspec)"
    echo "  2) ADPCM MS     4 bit"  
    echo "  3) PCM unsigned 8 bit  (recommended for lowspec)"
    echo "  4) PCM signed  16 bit"
    echo "  5) PCM signed  24 bit"
    echo
    echo "  0) Back"
    echo
    echo "        Format: $format"
    echo
    
    read -p "Select codec [1-5,0]: " choice
    
    command_line="-c:a"
    case $choice in
        1) codec="adpcm_ima_wav"; command_line="$command_line adpcm_ima_wav"; sample_rate_menu ;;
        2) codec="adpcm_ms"; command_line="$command_line adpcm_ms"; sample_rate_menu ;;
        3) codec="pcm_u8"; command_line="$command_line pcm_u8"; sample_rate_menu ;;
        4) codec="pcm_s16le"; command_line="$command_line pcm_s16le"; sample_rate_menu ;;
        5) codec="pcm_s24le"; command_line="$command_line pcm_s24le"; sample_rate_menu ;;
        0) main_menu ;;
        *) codec_menu_wav ;;
    esac
}

# Кодеки для MP3
codec_mp3() {
    codec="libmp3lame"
    command_line="-c:a libmp3lame"
    bitrate_menu
}

# Меню кодеков для OGG
codec_menu_ogg() {
    clear
    echo
    echo "================================"
    echo "      FFMPEG Audio Converter"
    echo "================================"
    echo
    echo "   Codec:"
    echo "  1) Vorbis  (recommended for old/legacy)"
    echo "  2) Opus"
    echo
    echo "  0) Back"
    echo
    echo "        Format: $format"
    echo
    
    read -p "Select codec [1-2,0]: " choice
    
    command_line="-c:a"
    case $choice in
        1) codec="libvorbis"; command_line="$command_line libvorbis"; bitrate_menu ;;
        2) codec="libopus"; command_line="$command_line libopus"; bitrate_menu ;;
        0) main_menu ;;
        *) codec_menu_ogg ;;
    esac
}

# Меню битрейта
bitrate_menu() {
    clear
    echo
    echo "================================"
    echo "      FFMPEG Audio Converter"
    echo "================================"
    echo
    echo "   Bitrate, Kb/s:"
    echo "  1)  32  (recommended for lowspec)"
    echo "  2)  64  (recommended for lowspec)"
    echo "  3)  80  (recommended for lowspec)"
    echo "  4)  96  (recommended for lowspec)"
    echo "  5) 112"
    echo "  6) 128"
    echo "  7) 192"
    echo "  8) 224"
    echo "  9) 320"
    echo
    echo "  0) Back"
    echo
    echo "        Format: $format"
    echo "         Codec: $codec"
    echo
    
    read -p "Select bitrate [1-9,0]: " choice
    
    command_line="$command_line -b:a"
    case $choice in
        1) bitrate="32"; command_line="$command_line 32k"; sample_rate_menu ;;
        2) bitrate="64"; command_line="$command_line 64k"; sample_rate_menu ;;
        3) bitrate="80"; command_line="$command_line 80k"; sample_rate_menu ;;
        4) bitrate="96"; command_line="$command_line 96k"; sample_rate_menu ;;
        5) bitrate="112"; command_line="$command_line 112k"; sample_rate_menu ;;
        6) bitrate="128"; command_line="$command_line 118k"; sample_rate_menu ;;
        7) bitrate="192"; command_line="$command_line 192k"; sample_rate_menu ;;
        8) bitrate="224"; command_line="$command_line 224k"; sample_rate_menu ;;
        9) bitrate="320"; command_line="$command_line 320k"; sample_rate_menu ;;
        0) main_menu ;;
        *) bitrate_menu ;;
    esac
}

# Меню частоты дискретизации
sample_rate_menu() {
    clear
    echo
    echo "================================"
    echo "      FFMPEG Audio Converter"
    echo "================================"
    echo
    echo "   Sample rate, Hz:"
    echo "  1) 8000   (recommended for lowspec)"
    echo "  2) 12000  (recommended and for lowspec)"
    echo "  3) 16000"
    echo "  4) 24000  (recommended)"
    echo "  5) 32000"
    echo "  6) 48000  (recommended)"
    echo
    echo "  0) Back"
    echo
    echo "        Format: $format"
    echo "         Codec: $codec"
    if [ "$format" != "wav" ]; then echo "       Bitrate: $bitrate Kb/s"; fi
    echo
    
    read -p "Select rate [1-6,0]: " choice
    
    command_line="$command_line -ar"
    case $choice in
        1) sample_rate="8000"; command_line="$command_line 8000"; channels_menu ;;
        2) sample_rate="12000"; command_line="$command_line 12000"; channels_menu ;;
        3) sample_rate="16000"; command_line="$command_line 16000"; channels_menu ;;
        4) sample_rate="24000"; command_line="$command_line 24000"; channels_menu ;;
        5) sample_rate="32000"; command_line="$command_line 32000"; channels_menu ;;
        6) sample_rate="48000"; command_line="$command_line 48000"; channels_menu ;;
        0) main_menu ;;
        *) sample_rate_menu ;;
    esac
}

# Меню каналов
channels_menu() {
    clear
    echo
    echo "================================"
    echo "      FFMPEG Audio Converter"
    echo "================================"
    echo
    echo "   Channels:"
    echo "  1) Mono (recommended for lowspec)"
    echo "  2) Stereo"
    echo
    echo "  0) Back"
    echo
    echo "        Format: $format"
    echo "         Codec: $codec"
    if [ "$format" != "wav" ]; then echo "       Bitrate: $bitrate Kb/s"; fi
    echo "   Sample rate: $sample_rate Hz"
    echo
    
    read -p "Select channels [1-2,0]: " choice
    
    command_line="$command_line -ac"
    case $choice in
        1) channels="1"; command_line="$command_line 1"; confirmation ;;
        2) channels="2"; command_line="$command_line 2"; confirmation ;;
        0) main_menu ;;
        *) channels_menu ;;
    esac
}

# Подтверждение и запуск
confirmation() {
    if [ "$channels" -eq 1 ]; then channels_txt="mono"; fi
    if [ "$channels" -eq 2 ]; then channels_txt="stereo"; fi
    
    clear
    echo
    echo "================================"
    echo "      FFMPEG Audio Converter"
    echo "================================"
    echo
    echo "        Format: $format"
    echo "         Codec: $codec"
    if [ "$format" != "wav" ]; then echo "       Bitrate: $bitrate Kb/s"; fi
    echo "   Sample rate: $sample_rate Hz"
    echo "      Channels: $channels_txt"
    echo
    
    read -p "Start FFmpeg [Y/N]? " -n1 choice
    echo
    
    case $choice in
        [Yy]) run_ffmpeg ;;
        [Nn]) main_menu ;;
        *) confirmation ;;
    esac
}

# Запуск FFmpeg
run_ffmpeg() {
    if [ "$codec" = "libopus" ]; then command_line="$command_line -vbr off"; fi
    if [ "$codec" = "libvorbis" ]; then command_line="$command_line -maxrate ${bitrate}k -minrate ${bitrate}k"; fi
    
    if [ "$format" != "wav" ]; then
        OUTPUT_DIR="${format}-${codec}-${bitrate}-${sample_rate}-${channels_txt}"
    else
        OUTPUT_DIR="${format}-${codec}-${sample_rate}-${channels_txt}"
    fi
    
    mkdir -p "$OUTPUT_DIR"

    for file in music-original/*.mp3 music-original/*.wav music-original/*.ogg; do
        if [ -f "$file" ]; then
            filename=$(basename -- "$file")
            filename="${filename%.*}"
            $FFMPEG -y -hide_banner -i "$file" $command_line "$OUTPUT_DIR/$filename.$format"
        fi
    done

    if [ $? -eq 0 ]; then
        completed=0
    else
        completed=1
    fi
    
    main_menu
}

# Основной цикл
if check_ffmpeg; then
    main_menu
fi
@echo off

set "completed=777"

if exist "..\tools\ffmpeg\win\ffmpeg.exe" (
    set FFMPEG="..\tools\ffmpeg\win\ffmpeg.exe"
    goto START_POINT
)

where ffmpeg >nul 2>&1
if %errorlevel% equ 0 (
    set FFMPEG=ffmpeg
    goto START_POINT
)

echo Error: FFmpeg not found in:
echo - Local path: ..\tools\ffmpeg\win\ffmpeg.exe
echo - System PATH
pause
exit /b 1


:START_POINT
set "command_line="
set "bitrate="
set "codec="
set "sample_rate="
set "channels="
if %completed% neq 1 cls
echo.
echo ================================
echo      FFMPEG Audio Converter
echo ================================
echo.
if %completed% equ 0 (
	echo      Output files in folder: %OUTPUT_DIR%\*.%format%
	echo   [V] COMPLETED ^:^)
	echo.
	set "completed=777"
)
if %completed% equ 1 (
	echo   [X] ERROR ^:^(
	echo.
	set "completed=777"
)
set "OUTPUT_DIR="
set "format="
echo   Formats:
echo  1) WAV  (recommended for lowspec)
echo  2) MP3
echo  3) OGG
echo.
echo  0) Exit
echo.
echo Place original MP3 files in 'music-original' folder
echo.
choice /C 1230 /M "Select format "
set "command_line=-c:a"
if %errorlevel% equ 1 set "format=wav"&goto CODEC_WAV
if %errorlevel% equ 2 set "format=mp3"&goto CODEC_MP3
if %errorlevel% equ 3 set "format=ogg"&goto CODEC_OGG
if %errorlevel% equ 4 exit /b
goto START_POINT

:CODEC_WAV
cls
echo.
echo ================================
echo      FFMPEG Audio Converter
echo ================================
echo.
echo   Codec:
echo  1) ADPCM IMA    4 bit  (recommended for lowspec)
echo  2) ADPCM MS     4 bit  
echo  3) PCM unsigned 8 bit  (recommended for lowspec)
echo  4) PCM signed  16 bit
echo  5) PCM signed  24 bit
echo.
echo  0) Back
echo.
echo Format: %format%
echo.
choice /C 123450 /M "Select codec "
if %errorlevel% equ 1 set "codec=adpcm_ima_wav"&set "command_line=%command_line% adpcm_ima_wav"&goto SAMPLE_RATE
if %errorlevel% equ 2 set "codec=adpcm_ms"&set "command_line=%command_line% adpcm_ms"&goto SAMPLE_RATE
if %errorlevel% equ 3 set "codec=pcm_u8"&set "command_line=%command_line% pcm_u8"&goto SAMPLE_RATE
if %errorlevel% equ 4 set "codec=pcm_s16le"&set "command_line=%command_line% pcm_s16le"&goto SAMPLE_RATE
if %errorlevel% equ 5 set "codec=pcm_s24le"&set "command_line=%command_line% pcm_s24le"&goto SAMPLE_RATE
if %errorlevel% equ 6 goto START_POINT
goto CODEC_WAV

:CODEC_MP3
set "codec=libmp3lame"&set "command_line=%command_line% libmp3lame"
goto BITRATE

:CODEC_OGG
cls
echo.
echo ================================
echo      FFMPEG Audio Converter
echo ================================
echo.
echo   Codec:
echo  1) Vorbis  (recommended for old/legacy)
echo  2) Opus
echo.
echo  0) Back
echo.
echo Format: %format%
echo.
choice /C 1230 /M "Select codec "
if %errorlevel% equ 1 set "codec=libvorbis"&set "command_line=%command_line% libvorbis"&goto BITRATE
if %errorlevel% equ 2 set "codec=libopus"&set "command_line=%command_line% libopus"&goto BITRATE
if %errorlevel% equ 3 goto START_POINT
goto CODEC_OGG

:BITRATE
cls
echo.
echo ================================
echo      FFMPEG Audio Converter
echo ================================
echo.
echo   Bitrate, Kb/s:
echo  1)  32  (recommended for lowspec)
echo  2)  64  (recommended for lowspec)
echo  3)  80  (recommended for lowspec)
echo  4)  96  (recommended for lowspec)
echo  5) 112
echo  6) 128
echo  7) 192
echo  8) 224
echo  9) 320
echo.
echo  0) Back
echo.
echo        Format: %format%
echo         Codec: %codec%
echo.
choice /C 1234567890 /M "Select bitrate "
set "command_line=%command_line% -b:a"
if %errorlevel% equ 1 set "bitrate=32"&set "command_line=%command_line% 32k"&goto SAMPLE_RATE
if %errorlevel% equ 2 set "bitrate=64"&set "command_line=%command_line% 64k"&goto SAMPLE_RATE
if %errorlevel% equ 3 set "bitrate=80"&set "command_line=%command_line% 80k"&goto SAMPLE_RATE
if %errorlevel% equ 4 set "bitrate=96"&set "command_line=%command_line% 96k"&goto SAMPLE_RATE
if %errorlevel% equ 5 set "bitrate=112"&set "command_line=%command_line% 112k"&goto SAMPLE_RATE
if %errorlevel% equ 6 set "bitrate=128"&set "command_line=%command_line% 118k"&goto SAMPLE_RATE
if %errorlevel% equ 7 set "bitrate=192"&set "command_line=%command_line% 192k"&goto SAMPLE_RATE
if %errorlevel% equ 8 set "bitrate=224"&set "command_line=%command_line% 224k"&goto SAMPLE_RATE
if %errorlevel% equ 9 set "bitrate=320"&set "command_line=%command_line% 320k"&goto SAMPLE_RATE
if %errorlevel% equ 10 goto START_POINT
goto BITRATE

:SAMPLE_RATE
cls
echo.
echo ================================
echo      FFMPEG Audio Converter
echo ================================
echo.
echo   Sample rate, Hz:
echo  1) 8000   (recommended for lowspec)
echo  2) 12000  (recommended and for lowspec)
echo  3) 16000
echo  4) 24000  (recommended)
echo  5) 32000
echo  6) 48000  (recommended)
echo.
echo  0) Back
echo.
echo        Format: %format%
echo         Codec: %codec%
if %format% neq wav echo       Bitrate: %bitrate% Kb/s
echo.
choice /C 1234560 /M "Select rate "
set "command_line=%command_line% -ar"
if %errorlevel% equ 1 set "sample_rate=8000"&set "command_line=%command_line% 8000"&goto CHANNELS
if %errorlevel% equ 2 set "sample_rate=12000"&set "command_line=%command_line% 12000"&goto CHANNELS
if %errorlevel% equ 3 set "sample_rate=16000"&set "command_line=%command_line% 16000"&goto CHANNELS
if %errorlevel% equ 4 set "sample_rate=24000"&set "command_line=%command_line% 24000"&goto CHANNELS
if %errorlevel% equ 5 set "sample_rate=32000"&set "command_line=%command_line% 32000"&goto CHANNELS
if %errorlevel% equ 6 set "sample_rate=48000"&set "command_line=%command_line% 48000"&goto CHANNELS
if %errorlevel% equ 7 goto START_POINT
goto SAMPLE_RATE

:CHANNELS
cls
echo.
echo ================================
echo      FFMPEG Audio Converter
echo ================================
echo.
echo   Channels:
echo  1) Mono (recommended for lowspec)
echo  2) Stereo
echo.
echo  0) Back
echo.
echo        Format: %format%
echo         Codec: %codec%
if %format% neq wav echo       Bitrate: %bitrate% Kb/s
echo   Sample rate: %sample_rate% Hz
echo.
choice /C 120 /M "Select channels "
set "command_line=%command_line% -ac"
if %errorlevel% equ 1 set "channels=1"&set "command_line=%command_line% 1"&goto CONFIRMATION
if %errorlevel% equ 2 set "channels=2"&set "command_line=%command_line% 2"&goto CONFIRMATION
if %errorlevel% equ 3 goto START_POINT
goto CHANNELS

:CONFIRMATION
if %channels% equ 1 set "channels_txt=mono"
if %channels% equ 2 set "channels_txt=stereo"
cls
echo.
echo ================================
echo      FFMPEG Audio Converter
echo ================================
echo.
echo        Format: %format%
echo         Codec: %codec%
if %format% neq wav echo       Bitrate: %bitrate% Kb/s
echo   Sample rate: %sample_rate% Hz
echo      Channels: %channels_txt%
echo.
choice /C YN /M "Start FFmpeg "
if %errorlevel% equ 1 goto RUN_FFMPEG
if %errorlevel% equ 2 goto START_POINT
goto CONFIRMATION

:RUN_FFMPEG
if %codec% equ libopus set "command_line=%command_line% -vbr off"
if %codec% equ libvorbis set "command_line=%command_line% -maxrate %bitrate%k -minrate %bitrate%k "
if "%format%" neq "wav" (
    set "OUTPUT_DIR=%format%-%codec%-%bitrate%-%sample_rate%-%channels_txt%"
) else (
    set "OUTPUT_DIR=%format%-%codec%-%sample_rate%-%channels_txt%"
)
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

for %%F in ("music-original\*.mp3" "music-original\*.wav" "music-original\*.ogg") do (
	%FFMPEG% -y -hide_banner -i "%%F" %command_line% "%OUTPUT_DIR%\%%~nF.%format%"
)

if %errorlevel% equ 0 set "completed=0"&goto START_POINT
set "completed=1"&goto START_POINT

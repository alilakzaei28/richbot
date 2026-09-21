@echo off
echo Compiling C Trading Bot with GCC...
gcc -Wall -O3 -fopenmp -I./include src/main.c src/twelvedata.c src/strategy.c src/risk.c src/cJSON.c -o twelvedata_bot.exe -lcurl -lm
if %errorlevel% neq 0 (
    echo [ERROR] Build failed. Verify that gcc and libcurl are properly installed in MinGW.
) else (
    echo [SUCCESS] Build complete: twelvedata_bot.exe
)
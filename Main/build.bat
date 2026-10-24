@echo off
echo Attempting to compile your code...

:: Pointing directly to the w64devkit compiler's include and lib folders
g++ tamagotchi.cpp -o game.exe -O2 -Wall -Wno-missing-braces -I C:\raylib\w64devkit\x86_64-w64-mingw32\include -L C:\raylib\w64devkit\x86_64-w64-mingw32\lib -lraylib -lopengl32 -lgdi32 -lwinmm 2> error_details.txt

if %errorlevel% neq 0 (
    echo COMPILE_ERROR > activity.log
    type error_details.txt >> activity.log
    
    echo ----------------------------------------
    echo BUILD FAILED! The pet noticed your error:
    echo ----------------------------------------
    type error_details.txt 
    echo ----------------------------------------
) else (
    echo COMPILE_SUCCESS > activity.log
    echo ----------------------------------------
    echo BUILD SUCCESS! The pet gains EXP.
    echo ----------------------------------------
    game.exe
)
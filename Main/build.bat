@echo off
cd /d "%~dp0"
:: game.exe can't be replaced while it's open, and that failure shouldn't count as your mistake
tasklist /fi "imagename eq game.exe" | find /i "game.exe" >nul && (
    echo The pet is already running. Close it first if you want to rebuild it.
    exit /b 0
)
echo Attempting to compile your code...

:: Use whichever raylib setup this PC has (see Notes.txt); plain g++ may be an old 32-bit compiler
set "CXX=g++"
if exist "C:\raylib\w64devkit\bin\g++.exe" (set "CXX=C:\raylib\w64devkit\bin\g++.exe") else if exist "C:\mingw64\bin\g++.exe" set "CXX=C:\mingw64\bin\g++.exe"

:: w64devkit folders are for the raylib installer, C:\raylib\include + lib for the raylib zip
"%CXX%" tamagotchi.cpp -o game.exe -O2 -Wall -Wno-missing-braces -I C:\raylib\w64devkit\x86_64-w64-mingw32\include -L C:\raylib\w64devkit\x86_64-w64-mingw32\lib -I C:\raylib\include -L C:\raylib\lib -lraylib -lopengl32 -lgdi32 -lwinmm -static 2> error_details.txt

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
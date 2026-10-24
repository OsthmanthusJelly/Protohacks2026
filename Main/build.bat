@echo off
echo Attempting to compile your code...

:: 1. Compile, but send any error text (2>) into a temporary file called error_details.txt
g++ main.cpp -o game.exe -O2 -Wall -Wno-missing-braces -I C:/raylib/raylib/src -L C:/raylib/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm 2> error_details.txt

if %errorlevel% neq 0 (
    :: If it failed, write COMPILE_ERROR to the log...
    echo COMPILE_ERROR > activity.log
    :: ...and copy all the exact error text into activity.log right below it!
    type error_details.txt >> activity.log
    
    echo BUILD FAILED! The pet is reading the error.
) else (
    :: If it succeeded, just write success.
    echo COMPILE_SUCCESS > activity.log
    echo BUILD SUCCESS! 
    game.exe
)
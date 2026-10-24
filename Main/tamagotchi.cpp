#include "raylib.h"
#include <fstream>
#include <string>
#include <cmath>
#include <cstdlib>
#include <algorithm>





enum Mood {
    HAPPY,
    SAD,
    ANGRY,
    READING
};
// Struct to hold the state of the pet
Class Pet {
Public:
    Mood myMOOD = HAPPY;
    int maxExp = 3;
    int maxHealth = 10;
    int health = 10;
    int tokens = 0;
    int exp = 0;
    std::string currentAdvice = "Ready to code!";
    void addExp(){
        exp+=1
        if (exp>=maxExp){
            LevelUp();
        }
    }
    void LevelUp(){
        exp = 0
        level+=1
        if (level<= 1){
            maxHealth += 10;
            maxExp += level+3;
        }
        elif (level>= 5){
            maxHealth += 5;
            maxExp += level+5;
        }
        elif (level>25)
        {
            maxHealth += 1;
            maxExp += level+6; 
        }
        health=maxHealth;
        currentAdvice="Level Up! You are now level " + std::to_string(level) + "!";
        };
    void Update(){
        if (0>health){
            health = 0
    }
};
main(){
    void ReadActivityLog(Pet& pet) {
    std::ifstream file("activity.log");
    std::string line;
    
    bool foundError = false;
    std::string newAdvice = "";

    // Read the file one line at a time
    while (std::getline(file, line)) {
        
        if (line.find("COMPILE_SUCCESS") != std::string::npos) {
            pet.AddExp();
            pet.currentAdvice = "Compiled perfectly! Good job.";
            break; // Stop reading, we don't need to look for errors
        }
        else if (line.find("COMPILE_ERROR") != std::string::npos) {
            foundError = true;
            pet.health -= 10;
            pet.currentMood = READING;
            newAdvice = "I don't recognize this error, check the terminal!"; // Default advice
        }
        
        // If we know an error happened, start searching the compiler's text for clues
        if (foundError) {
            if (line.find("expected ';'") != std::string::npos) {
                newAdvice = "Advice: You forgot a semicolon ';' at the end of a line!";
            }
            else if (line.find("was not declared") != std::string::npos || line.find("undeclared") != std::string::npos) {
                newAdvice = "Advice: You typed a variable name that doesn't exist. Typo?";
            }
            else if (line.find("expected '}'") != std::string::npos) {
                newAdvice = "Advice: You are missing a closing curly bracket '}'!";
            }
            else if (line.find("No such file or directory") != std::string::npos) {
                newAdvice = "Advice: You are trying to #include a file that is missing.";
            }
        }
    }
    file.close();

    // If we found an error while reading, update the pet's advice
    if (foundError) {
        pet.currentAdvice = newAdvice;
    }
    
    // Empty the file so we don't read the same error on the next frame
    std::ofstream clearFile("activity.log", std::ofstream::trunc);
}}
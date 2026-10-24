#include "raylib.h"
#include <fstream>
#include <string>
#include <cmath>
#include <cstdlib>
#include <algorithm>


// Struct to hold the state of the pet



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
    void ReadActivityLog(Pet& pet){
        std::ifstream file("activity.log");
        std::string line;
    while (std::getline(file, line)) {
        if (line.find("COMPILE_ERROR") != std::string::npos) {
            pet.health -= 10; // 10 damage per error
            pet.currentMood = READING;
            pet.currentAdvice = "Ouch! Compile error.";
        } 
        else if (line.find("COMPILE_SUCCESS") != std::string::npos) {
            pet.AddExp();
            pet.currentAdvice = "Compiled perfectly!";
        }
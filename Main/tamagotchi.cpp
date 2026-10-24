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
class Pet { // C++ is case-sensitive: 'class' must be lowercase
public:     // C++ is case-sensitive: 'public' must be lowercase
    Mood myMOOD = HAPPY;
    int maxExp = 3;
    int maxHealth = 10;
    int health = 10;
    int tokens = 0;
    int exp = 0;
    int level = 1;
    
    float uptimeTimer = 0.0f; // NEW: Timer for 5-minute tokens
    
    std::string currentAdvice = "Ready to code!";
    
    void addExp() {
        exp += 1;
        if (exp >= maxExp) {
            LevelUp();
        }
    }
    
    void LevelUp() {
        exp = 0;
        level += 1;
        
        // FIXED: Reordered conditions using else-if so it catches every level properly
        if (level < 5) {
            maxHealth += 10;
            maxExp += level + 3;
        }
        else if (level < 25) { // Handles levels 5 through 24
            maxHealth += 5;
            maxExp += level + 5;
        }
        else { // Handles level 25+
            maxHealth += 1;
            maxExp += level + 6; 
        }
        
        health = maxHealth;
        currentAdvice = "Level Up! You are now level " + std::to_string(level) + "!";
    }
    
    // NEW: Passed deltaTime in so we can track the 5 minutes
    void Update(float deltaTime) {
        // Track time and award token every 300 seconds (5 minutes)
        uptimeTimer += deltaTime;
        if (uptimeTimer >= 300.0f) {
            tokens += 1;
            uptimeTimer = 0.0f; 
            currentAdvice = "Here is 1 Token for hanging out!";
        }

        if (health < 0) {
            health = 0;
        }

        // Re-calculate mood based on health percentage so it scales with level-ups
        float healthPercent = (float)health / maxHealth;
        if (healthPercent > 0.5f) myMOOD = HAPPY;
        else if (healthPercent > 0.2f) myMOOD = SAD;
        else myMOOD = ANGRY;
    }
}; // FIXED: Added missing closing bracket and semicolon to close the Pet class

void ReadActivityLog(Pet& pet) {
    std::ifstream file("activity.log");
    std::string line;
    
    bool foundError = false;
    std::string newAdvice = "";
    std::string fullErrorText = "";

    while (std::getline(file, line)) {
        if (line.find("COMPILE_SUCCESS") != std::string::npos) {
            pet.addExp();
            pet.currentAdvice = "Compiled perfectly! Good job.";
            break; 
        }
        else if (line.find("COMPILE_ERROR") != std::string::npos) {
            foundError = true;
            pet.health -= 10;
            pet.myMOOD = READING;
        }
        
        // If we hit an actual compiler error line
        if (foundError && line.find("error:") != std::string::npos) {
            size_t errorPos = line.find("error:");
            fullErrorText = line.substr(errorPos + 7); // Grab everything after "error: "
            
            // Extract the specific variable/symbol causing the issue (usually inside ' ')
            std::string problemItem = "something";
            size_t firstQuote = fullErrorText.find("'");
            size_t secondQuote = fullErrorText.find("'", firstQuote + 1);
            if (firstQuote != std::string::npos && secondQuote != std::string::npos) {
                problemItem = fullErrorText.substr(firstQuote, secondQuote - firstQuote + 1);
            }

            // TRANSLATE TO PLAIN ENGLISH
            if (fullErrorText.find("expected ';'") != std::string::npos) {
                newAdvice = "You forgot a semicolon ';' near " + problemItem + "!";
            }
            else if (fullErrorText.find("was not declared in this scope") != std::string::npos) {
                newAdvice = "You tried to use " + problemItem + ", but it hasn't been created yet!";
            }
            else if (fullErrorText.find("expected '}'") != std::string::npos) {
                newAdvice = "You missed a closing curly brace '}' somewhere above this.";
            }
            else if (fullErrorText.find("expected unqualified-id") != std::string::npos) {
                newAdvice = "You put code in the wrong place, or added an extra symbol near " + problemItem + ".";
            }
            else if (fullErrorText.find("redefinition of") != std::string::npos) {
                newAdvice = "You already created " + problemItem + " earlier. You can't make it twice!";
            }
            else if (fullErrorText.find("too few arguments to function") != std::string::npos) {
                newAdvice = "You didn't give " + problemItem + " enough information inside its parentheses ().";
            }
            else if (fullErrorText.find("invalid conversion") != std::string::npos) {
                newAdvice = "You are trying to shove the wrong type of data into a variable.";
            }
            else if (fullErrorText.find("No such file") != std::string::npos) {
                newAdvice = "You tried to include " + problemItem + ", but that file doesn't exist.";
            }
            else {
                // Catch-all: Just show the cleaned-up error without the file paths
                newAdvice = "Error: " + fullErrorText;
            }
            
            // Once we find the first actual error and translate it, stop reading
            break; 
        }
    }
    file.close();

    if (foundError && newAdvice != "") {
        pet.currentAdvice = newAdvice;
    } else if (foundError) {
        pet.currentAdvice = "Uh oh, the code crashed. Check the terminal!";
    }
    
    std::ofstream clearFile("activity.log", std::ofstream::trunc);
}
    int main() {
    // 1. Create the Raylib window
    InitWindow(800, 600, "Tamagothu: The Dev Pet");
    SetTargetFPS(60);

    // 2. CREATE THE PET HERE (This fixes the undefined error!)
    Pet myPet; 
    
    float logCheckTimer = 0.0f;

    // 3. Start the infinite game loop
    while (!WindowShouldClose()) {
        
        // Track how much time passed this frame
        float dt = GetFrameTime(); 
        
        // Update the pet's timers and mood
        myPet.Update(dt); 

        // Check for new activity logs every 2 seconds
        logCheckTimer += dt;
        if (logCheckTimer > 2.0f) {
            ReadActivityLog(myPet);
            logCheckTimer = 0.0f;
        }

        // 4. DRAWING PHASE
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Draw Stats
        DrawText(TextFormat("Level: %i", myPet.level), 10, 10, 20, DARKGRAY);
        DrawText(TextFormat("EXP: %i / %i", myPet.exp, myPet.maxExp), 10, 35, 20, BLUE);
        DrawText(TextFormat("Health: %i / %i", myPet.health, myPet.maxHealth), 10, 60, 20, RED);
        DrawText(TextFormat("Tokens: %i", myPet.tokens), 10, 85, 20, GOLD);

        // Draw Advice
        if (myPet.currentAdvice != "") {
            DrawText(myPet.currentAdvice.c_str(), 10, 120, 20, DARKBLUE);
        }

        EndDrawing();
    }

    // Clean up when the user closes the window
    CloseWindow();
    return 0;
}
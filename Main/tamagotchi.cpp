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

    // Read the file one line at a time
    while (std::getline(file, line)) {
        
        if (line.find("COMPILE_SUCCESS") != std::string::npos) {
            pet.addExp(); // FIXED: Capitalization matched to class method (addExp)
            pet.currentAdvice = "Compiled perfectly! Good job.";
            break; // Stop reading, we don't need to look for errors
        }
        else if (line.find("COMPILE_ERROR") != std::string::npos) {
            foundError = true;
            pet.health -= 10;
            pet.myMOOD = READING; // FIXED: Variable name matched to class property (myMOOD)
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
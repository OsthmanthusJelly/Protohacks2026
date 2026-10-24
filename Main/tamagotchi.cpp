#include "raylib.h"
#include <fstream>
#include <string>
#include <cmath>
#include <cstdlib>
#include <algorithm>


// Struct to hold the state of the pet
struct PetState {
    int health = 10;
    int exp = 0;
    int level = 1;
    int streak = 0;
    std::string name = "Tama";
    std::string mood = "Neutral";

};
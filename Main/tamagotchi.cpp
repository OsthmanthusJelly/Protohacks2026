#include "raylib.h"
#include "rlgl.h"
#include <fstream>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include <vector>
#include <sstream>
#include <regex>
#include <random>

// Two Windows functions for the one-pet-at-a-time check (windows.h itself clashes with raylib's names)
extern "C" __declspec(dllimport) void* __stdcall CreateMutexA(void* attributes, int initialOwner, const char* name);
extern "C" __declspec(dllimport) unsigned long __stdcall GetLastError();

enum Mood {
    HAPPY,
    SAD,
    ANGRY,
    READING
};

// Timings in seconds.
// shortcut: hunger drops every minute for testing, set HUNGER_EVERY to 3600 (1 hour) for real play.
const float TOKEN_EVERY = 60.0f;  // 1 token for every minute the pet is open
const float HUNGER_EVERY = 60.0f;
const int HUNGER_DROP = 10;       // hunger lost each time
const float IDLE_EVERY = 20.0f;   // quiet time before the pet says something on its own
const int CLOTHES_COUNT = 40;
const int WISH_COST = 10, WISH10_COST = 90; // the prices printed on the wish buttons in UI.jpg

// ---------- PERSONALITIES ----------
struct Personality { const char* name; const char* hello; const char* idle[5]; const char* poke[3]; };

const Personality PERSONALITIES[] = {
    { "Normal", "Hi! Let's write some code together!",
      { "Write some code, I'm bored!", "Semicolons are my favourite snack.", "*hums in binary*",
        "Remember to drink some water!", "I believe in you!" },
      { "Hehe, that tickles!", "Boop!", "Hi there!" } },
    { "Tsundere", "I-it's not like I wanted you to pick me or anything!",
      { "I-it's not like I want you to compile or anything... b-baka!",
        "Don't get the wrong idea, I'm only watching your code because I'm bored!",
        "Hmph. Your indentation is... acceptable. For now.",
        "I wasn't waiting for you! I just happened to be here!",
        "If you get an error I'll be a LITTLE disappointed, okay?!" },
      { "D-don't touch me without permission!", "Hmph! ...do it again. N-no wait!", "B-baka! What are you doing?!" } },
    { "Dramatic", "At last... a personality WORTHY of me!",
      { "Alas! Another minute without a compile... I wither away...",
        "The silence of your keyboard is DEAFENING.",
        "Shakespeare wrote 37 plays. You haven't written 37 lines.",
        "I stared into the void. The void had a segfault.",
        "Is this... the end? No. It's just a pause. HOW DRAMATIC." },
      { "You WOUND me!", "Unhand me, villain!", "Ah, the touch of a coder... how poetic." } },
    { "Gamer", "gg, let's go. queue up the compiler",
      { "bro just compile it, zero risk", "skill issue tbh", "gg ez... wait, you haven't even started",
        "touch grass? nah, touch keyboard", "my K/D beats your error/compile ratio" },
      { "stop spamming bro", "that's a reportable offense", "+1 poke combo" } },
    { "Pirate", "Arr! Welcome aboard, matey!",
      { "Arr, these waters be calm... too calm. Write some code, matey!",
        "Yo ho ho and a bottle of semicolons!", "Shiver me timbers, is that a missing brace?",
        "X marks the spot... and the spot be line 1. Start there!",
        "A pirate's life fer me, and a coder's life fer ye!" },
      { "Avast! Keep yer hands to yerself!", "Arr, that be mutiny!", "Walk the plank, ye poker!" } },
    { "Grandma", "Oh, hello sweetie! Have you eaten?",
      { "Have you eaten today, sweetie? Your code looks thin.",
        "Back in my day we wrote code on punch cards. Uphill. Both ways.",
        "Wear a jacket, it's cold in the server room.",
        "You should call your mother. After this compile.",
        "Such a smart little programmer! I'll tell all my friends." },
      { "Oh! You startled me, dear.", "Ohoho, cheeky!", "Come here and give Grandma a hug." } },
};
const int PERSONALITY_COUNT = sizeof(PERSONALITIES) / sizeof(PERSONALITIES[0]);

// Struct to hold the state of the pet
class Pet { // C++ is case-sensitive: 'class' must be lowercase
public:     // C++ is case-sensitive: 'public' must be lowercase
    std::string name = "Tamagothu";
    int personality = 0;
    Mood myMOOD = HAPPY;
    int maxExp = 3;
    int maxHealth = 10;
    int health = 10;
    int maxHunger = 100;
    int hunger = 100;
    int tokens = 0;
    int exp = 0;
    int level = 1;

    // Clothes gacha
    int owned[CLOTHES_COUNT] = {};
    int equipped = -1;
    int pity5 = 0; // pulls since the last 5-star
    int pity4 = 0; // pulls since the last 4-star or better

    float uptimeTimer = 0.0f;  // counts up to the next free token
    float hungerTimer = 0.0f;  // counts up to the next hunger drop
    float quietTimer = 0.0f;   // time since the pet last said something
    float readingTimer = 0.0f; // how long to keep the READING mood after a compile error

    std::string currentAdvice = "Ready to code!";
    bool adviceIsError = false; // errors get the red speech bubble

    bool IsDead() const { return health <= 0; }

    void Say(const std::string& text, bool isError = false) {
        currentAdvice = text;
        adviceIsError = isError;
        quietTimer = 0.0f;
    }

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
            maxHunger += 10;
            maxExp += level + 3;
        }
        else if (level < 25) { // Handles levels 5 through 24
            maxHealth += 5;
            maxHunger += 5;
            maxExp += level + 5;
        }
        else { // Level 25+: diminishing returns, everything grows by just 1
            maxHealth += 1;
            maxHunger += 1;
            maxExp += 1;
        }

        health = maxHealth;
        hunger = maxHunger;
        Say("Level Up! You are now level " + std::to_string(level) + "!");
    }

    int OneHeart() const { return std::max(1, maxHealth / 10); } // the hearts bar always shows 10 hearts

    void Hurt() { health = std::max(0, health - OneHeart()); }

    void Poke() {
        if (IsDead()) return;
        Say(PERSONALITIES[personality].poke[std::rand() % 3]);
    }

    void Revive() {
        health = maxHealth;
        hunger = maxHunger;
        tokens = 0;
        myMOOD = HAPPY;
        Say("I'm back! ...the doctor took all your tokens though.");
    }

    void Update(float deltaTime) {
        if (IsDead()) return; // nothing happens until it's revived

        // 1 token for every minute the pet is open
        uptimeTimer += deltaTime;
        while (uptimeTimer >= TOKEN_EVERY) {
            tokens += 1;
            uptimeTimer -= TOKEN_EVERY;
            if (!adviceIsError) Say("Here is 1 Token for hanging out!"); // don't hide an error the user hasn't fixed
        }

        // Hunger goes down over time; at 0 the pet starts losing hearts
        hungerTimer += deltaTime;
        while (hungerTimer >= HUNGER_EVERY) {
            hungerTimer -= HUNGER_EVERY;
            hunger = std::max(0, hunger - HUNGER_DROP);
            if (hunger == 0) {
                Hurt();
                Say("I'm starving! Buy me food in the shop.", true);
            }
            else if (hunger <= maxHunger * 0.3f && !adviceIsError) Say("I'm getting hungry... the shop has food!");
        }

        // Idle chatter in the pet's personality when nothing has happened for a while
        quietTimer += deltaTime;
        if (quietTimer >= IDLE_EVERY && !adviceIsError) Say(PERSONALITIES[personality].idle[std::rand() % 5]);

        if (health < 0) {
            health = 0;
        }

        // Keep READING for a few seconds after an error, otherwise the line below overwrites it instantly
        if (readingTimer > 0.0f) {
            readingTimer -= deltaTime;
            return;
        }

        // Re-calculate mood based on health percentage so it scales with level-ups
        float healthPercent = (float)health / maxHealth;
        if (healthPercent > 0.5f) myMOOD = HAPPY;
        else if (healthPercent > 0.2f) myMOOD = SAD;
        else myMOOD = ANGRY;
    }

    // ---------- SAVING (save.txt next to game.exe) ----------
    std::vector<std::pair<const char*, int*>> Numbers() {
        return { {"personality", &personality}, {"level", &level}, {"exp", &exp}, {"maxExp", &maxExp},
                 {"health", &health}, {"maxHealth", &maxHealth}, {"hunger", &hunger}, {"maxHunger", &maxHunger},
                 {"tokens", &tokens}, {"pity5", &pity5}, {"pity4", &pity4}, {"equipped", &equipped} };
    }

    void Save(const std::string& path) {
        std::ofstream f(path);
        f << "name " << name << "\n";
        for (auto& field : Numbers()) f << field.first << " " << *field.second << "\n";
        f << "owned";
        for (int n : owned) f << " " << n;
        f << "\n";
    }

    bool Load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        std::string key;
        while (f >> key) {
            if (key == "name") std::getline(f >> std::ws, name);
            else if (key == "owned") for (int& n : owned) f >> n;
            else for (auto& field : Numbers()) if (key == field.first) f >> *field.second;
        }
        personality = std::clamp(personality, 0, PERSONALITY_COUNT - 1);
        equipped = std::clamp(equipped, -1, CLOTHES_COUNT - 1);
        return true;
    }
}; // FIXED: Added missing closing bracket and semicolon to close the Pet class

// ---------- READING YOUR COMPILES ----------
// petlog.ps1, build.bat and the VS Code tasks write activity.log after every compile/run:
// first line COMPILE_SUCCESS or COMPILE_ERROR, then the error output.

// TRANSLATE TO PLAIN ENGLISH (gcc/clang errors, then Python errors)
std::string PlainEnglish(const std::string& msg) {
    // The thing the error is about is usually the last 'quoted' bit, e.g. expected ';' before 'return'
    std::string item = "something";
    size_t q2 = msg.rfind('\'');
    size_t q1 = (q2 == std::string::npos || q2 == 0) ? std::string::npos : msg.rfind('\'', q2 - 1);
    if (q1 != std::string::npos) item = msg.substr(q1, q2 - q1 + 1);
    auto has = [&](const char* text) { return msg.find(text) != std::string::npos; };

    if (has("expected ';'") || has("expected ',' or ';'")) return "You forgot a semicolon ';' near " + item + "!";
    if (has("was not declared in this scope")) return "You tried to use " + item + ", but it hasn't been created yet!";
    if (has("expected '}'")) return "You missed a closing curly brace '}' somewhere above this.";
    if (has("expected unqualified-id")) return "You put code in the wrong place, or added an extra symbol near " + item + ".";
    if (has("redefinition of")) return "You already created " + item + " earlier. You can't make it twice!";
    if (has("too few arguments to function")) return "You didn't give " + item + " enough information inside its parentheses ().";
    if (has("invalid conversion") || has("cannot convert")) return "You are trying to shove the wrong type of data into a variable.";
    if (has("No such file")) return "You tried to include " + item + ", but that file doesn't exist.";
    if (has("ld returned")) return "Something you used was declared but never written. Check your function names.";
    if (has("never closed")) return "You opened a bracket " + item + " but never closed it.";
    if (has("NameError")) return "You used " + item + ", but it was never created. Typo?";
    if (has("IndentationError")) return "Your indentation (the spaces at the start of the line) is off.";
    if (has("SyntaxError")) return "Python can't read this line. Check brackets, colons and quotes.";
    if (has("ZeroDivisionError")) return "You divided by zero!";
    if (has("ModuleNotFoundError")) return "You imported " + item + ", but it isn't installed.";
    if (has("IndexError")) return "You asked for an item past the end of a list.";
    if (has("TypeError")) return "You mixed up types of data, like adding text to a number.";
    return "Error: " + msg; // Catch-all: the error without the file path
}

void ReadActivityLog(Pet& pet, const std::string& path) {
    std::ifstream file(path);
    std::string line;
    bool success = false, foundError = false;
    std::string gccMessage, pythonMessage;
    int gccLine = 0, pythonLine = 0;
    static const std::regex gccWhere(R"(:(\d+):\d+:\s*(?:fatal\s+)?error:)");  // main.cpp:5:12: error:
    static const std::regex pythonWhere(R"(File ".*", line (\d+))");           // File "a.py", line 3
    static const std::regex pythonError(R"(^\w*(Error|Exception)\b)");         // NameError: ...
    std::smatch m;

    while (std::getline(file, line)) {
        if (line.find("COMPILE_SUCCESS") != std::string::npos) { success = true; break; }
        if (line.find("COMPILE_ERROR") != std::string::npos) { foundError = true; continue; }
        if (!foundError) continue;

        size_t errorPos = line.find("error:");
        if (gccMessage.empty() && errorPos != std::string::npos) { // gcc: the first error is the real one
            gccMessage = line.substr(errorPos + 6);
            gccMessage.erase(0, gccMessage.find_first_not_of(' '));
            if (std::regex_search(line, m, gccWhere)) gccLine = std::stoi(m[1]);
        }
        if (std::regex_search(line, m, pythonWhere)) pythonLine = std::stoi(m[1]);
        if (std::regex_search(line, pythonError)) pythonMessage = line; // Python: the last line is the real one
    }
    file.close();

    if (pet.IsDead()) { /* a crashed pet can't watch your code */ }
    else if (success) {
        pet.tokens += 1;
        pet.Say("Compiled perfectly! Good job. +1 token");
        pet.addExp(); // a level up replaces the message above
    }
    else if (foundError) {
        pet.tokens += 1; // every compile earns a token, errors also cost a heart
        pet.Hurt();
        pet.myMOOD = READING;
        pet.readingTimer = 5.0f;
        bool isGcc = !gccMessage.empty();
        std::string message = isGcc ? gccMessage : pythonMessage;
        int where = isGcc ? gccLine : pythonLine;
        std::string advice = message.empty() ? "Uh oh, the code crashed. Check the terminal!" : PlainEnglish(message);
        if (where > 0) advice = "Line " + std::to_string(where) + ": " + advice;
        pet.Say(advice, true);
    }

    std::ofstream clearFile(path, std::ofstream::trunc); // so the same compile isn't counted twice
}

// ---------- SHOP ----------
struct Food { const char* name; int price; int hunger; int hearts; int exp; };

// Same order as the food sprites on food&shop.png: top row, then bottom row
const Food FOODS[8] = {
    { "Pizza", 3, 30, 0, 0 }, { "Burger", 4, 40, 0, 0 }, { "Ramen", 5, 50, 1, 1 }, { "Donut", 2, 15, 0, 0 },
    { "Sushi", 3, 25, 1, 0 }, { "Apple", 1, 10, 1, 0 },  { "Milk", 1, 10, 1, 0 },  { "Bone", 1, 5, 0, 0 },
};

bool Buy(Pet& pet, const Food& food) {
    if (pet.tokens < food.price) {
        pet.Say("You need " + std::to_string(food.price) + " tokens for " + food.name + ".");
        return false;
    }
    pet.tokens -= food.price;
    pet.hunger = std::min(pet.maxHunger, pet.hunger + food.hunger);
    pet.health = std::min(pet.maxHealth, pet.health + food.hearts * pet.OneHeart());
    pet.Say(std::string("Yum, ") + food.name + "! Thank you!");
    for (int i = 0; i < food.exp; i++) pet.addExp(); // a level up replaces the message above
    return true;
}

// ---------- CLOTHES GACHA (Genshin-style) ----------
// How an outfit is shown on the pet: most pictures already include the pet wearing it; the rest go with the pet
enum Wear { WORN, HELD, AURA, HAT };
struct Outfit { const char* name; int stars; Rectangle src; Wear wear = WORN; }; // src: its picture on clothes.png (960x717)

const Outfit CLOTHES[CLOTHES_COUNT] = {
    { "Blue Jeans", 1, { 28, 30, 85, 85 } },          { "Denim Jacket", 1, { 222, 30, 98, 85 } },
    { "Plain T-Shirt", 1, { 420, 30, 92, 85 } },      { "Apron", 1, { 605, 30, 95, 85 } },
    { "Pencil", 1, { 790, 30, 100, 85 } },
    { "Flannel Shirt", 2, { 28, 140, 85, 85 } },      { "Life Vest", 2, { 228, 140, 90, 85 } },
    { "Lanyard", 2, { 428, 140, 85, 85 } },           { "Biker Helmet", 2, { 612, 140, 88, 85 } },
    { "Crew Cap", 2, { 800, 140, 80, 85 } },
    { "Smartwatch", 3, { 28, 250, 92, 85 } },         { "Backpack", 3, { 232, 250, 84, 85 } },
    { "Flashcards", 3, { 430, 250, 82, 85 } },        { "Green Glasses", 3, { 612, 250, 86, 85 } },
    { "Hacker Hoodie", 3, { 792, 250, 88, 85 } },
    { "Syndicate Suit", 4, { 30, 355, 95, 85 } },     { "Oscilloscope Visor", 4, { 138, 355, 85, 85 } },
    { "Cyber-Armor", 4, { 238, 355, 80, 85 } },       { "Sparking Staff", 4, { 328, 352, 92, 88 } },
    { "Wizard Robes", 4, { 426, 355, 92, 85 } },      { "Logic Shield", 4, { 522, 355, 72, 85 }, HELD },
    { "Web Tunic", 4, { 598, 355, 75, 85 } },         { "Hologram Aura", 4, { 680, 355, 88, 85 }, AURA },
    { "Drone", 4, { 765, 355, 90, 85 } },             { "Trench Coat", 4, { 858, 352, 82, 88 } },
    { "Celestial Robes", 5, { 12, 492, 125, 78 } },   { "Solar Raiment", 5, { 158, 492, 125, 78 } },
    { "Overlord Plate", 5, { 292, 492, 118, 78 } },   { "Exosuit", 5, { 425, 488, 105, 82 } },
    { "Galaxy Singularity", 5, { 562, 492, 92, 78 }, AURA }, { "Cyber-Matrix", 5, { 692, 492, 106, 78 }, AURA },
    { "Grand Crown", 5, { 812, 492, 125, 78 }, HAT },
    { "Fraser Shield", 5, { 36, 598, 80, 82 }, HELD },{ "Quantum Blade", 5, { 158, 598, 90, 82 }, HELD },
    { "Star Cloak", 5, { 282, 598, 82, 82 } },        { "Crimson Coat", 5, { 392, 598, 86, 82 } },
    { "Dragon Mount", 5, { 498, 598, 110, 82 } },     { "Mountain Pauldrons", 5, { 618, 598, 95, 82 } },
    { "Lightning Wings", 5, { 722, 598, 120, 82 } },  { "Frost Core", 5, { 852, 598, 80, 82 }, HELD },
};

// Rates like Genshin: 5-star 0.6% with soft pity from pull 74 and a guarantee at 90, 4-star 5.1% with a guarantee every 10
int RollStars(Pet& pet, std::mt19937& rng) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    pet.pity5++;
    pet.pity4++;
    float rate5 = 0.006f + (pet.pity5 >= 74 ? (pet.pity5 - 73) * 0.06f : 0.0f);
    if (pet.pity5 >= 90 || roll(rng) < rate5) { pet.pity5 = 0; pet.pity4 = 0; return 5; }
    if (pet.pity4 >= 10 || roll(rng) < 0.051f) { pet.pity4 = 0; return 4; }
    float r = roll(rng);
    return r < 0.15f ? 3 : r < 0.45f ? 2 : 1;
}

int Wish(Pet& pet, std::mt19937& rng) {
    int stars = RollStars(pet, rng);
    std::vector<int> pool;
    for (int i = 0; i < CLOTHES_COUNT; i++) if (CLOTHES[i].stars == stars) pool.push_back(i);
    int item = pool[rng() % pool.size()];
    pet.owned[item]++;
    return item;
}

// Spend tokens on `count` wishes; returns the outfits won (empty if you can't afford it)
std::vector<int> Wishes(Pet& pet, int count, std::mt19937& rng) {
    int cost = count == 10 ? WISH10_COST : WISH_COST * count;
    if (pet.tokens < cost) {
        pet.Say("Wishing costs " + std::to_string(cost) + " tokens. Go write some code!");
        return {};
    }
    pet.tokens -= cost;
    std::vector<int> won;
    for (int i = 0; i < count; i++) won.push_back(Wish(pet, rng));
    return won;
}

// ---------- SPRITES ----------
// The sheets in Sprites/ are AI-generated: the "transparent" checkerboard is painted in, and raylib
// can't load .jpg (pet_sheet/ui_sheet/clothes/wish_bg.png are PNG copies of the .jpg sheets). So at
// startup we erase the background ourselves and find each sprite by the empty gaps around it.

struct Sprites {
    std::vector<Texture2D> mood[4]; // one animation per Mood: HAPPY, SAD, ANGRY, READING
    std::vector<Texture2D> hearts;  // full, 3/4, 1/4, empty
    std::vector<Texture2D> coin;    // spinning token
    std::vector<Texture2D> food;    // same order as FOODS
    std::vector<Texture2D> shop;    // the shop stall
    std::vector<Texture2D> bubble;  // white speech bubble
    std::vector<Texture2D> alert;   // red spiky bubble for errors
    std::vector<Texture2D> clothes; // same order as CLOTHES
    std::vector<Texture2D> tombstone, ghost;
    Texture2D wishBg = {}, devKey = {};
};

bool Near(Color c, int r, int g, int b) { return abs(c.r - r) + abs(c.g - g) + abs(c.b - b) < 30; }

Texture2D SmoothTexture(Image& img) {
    Texture2D tex = LoadTextureFromImage(img);
    GenTextureMipmaps(&tex); // smooth when drawn much smaller than the sheet
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    return tex;
}

template <class IsBackground>
void KeyOut(Image& img, IsBackground isBackground) {
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color* px = (Color*)img.data;
    for (int i = 0; i < img.width * img.height; i++)
        if (isBackground(px[i])) px[i].a = 0;
}

// Cut out every sprite inside `area`, left to right, as its own texture
std::vector<Texture2D> Cut(const Image& sheet, Rectangle area) {
    const Color* px = (const Color*)sheet.data;
    int x0 = (int)area.x, y0 = (int)area.y, x1 = x0 + (int)area.width, y1 = y0 + (int)area.height;
    auto solid = [&](int x, int y) { return px[y * sheet.width + x].a > 0; };
    auto used = [&](int x) { // a column belongs to a sprite if it has a few solid pixels (ignores JPEG specks)
        int n = 0;
        for (int y = y0; y < y1; y++) n += solid(x, y);
        return n >= 6;
    };
    std::vector<Texture2D> frames;
    for (int x = x0; x < x1; x++) {
        if (!used(x)) continue;
        int start = x, end = x + 1;
        for (int gap = 0; x < x1 && gap < 8; x++) { // a sprite ends after 8 empty columns
            if (used(x)) { end = x + 1; gap = 0; } else gap++;
        }
        if (end - start < 30) continue; // stray marks, not a sprite
        int top = y1, bottom = y0;
        for (int y = y0; y < y1; y++)
            for (int cx = start; cx < end; cx++)
                if (solid(cx, y)) { top = std::min(top, y); bottom = y + 1; break; }
        Image part = ImageFromImage(sheet, { (float)start, (float)top, (float)(end - start), (float)(bottom - top) });
        frames.push_back(SmoothTexture(part));
        UnloadImage(part);
    }
    return frames;
}

// For sheets whose checkerboard is as black as the sprites' outlines: flood-fill the background in from
// the edges of `area` (so dark pixels inside the sprite survive), then redraw a black outline `outline`
// pixels thick. Anything inside `erase` (relative to `area`) is removed too.
template <class IsBackground>
Texture2D CutOne(const Image& sheet, Rectangle area, IsBackground isBackground, int outline, Rectangle erase = { 0, 0, 0, 0 }) {
    Image img = ImageFromImage(sheet, area);
    Color* px = (Color*)img.data;
    int w = img.width, h = img.height;
    std::vector<char> bg(w * h, 0);
    std::vector<int> todo;
    auto visit = [&](int x, int y) {
        int i = y * w + x;
        if (!bg[i] && isBackground(px[i])) { bg[i] = 1; todo.push_back(i); }
    };
    for (int x = 0; x < w; x++) { visit(x, 0); visit(x, h - 1); }
    for (int y = 0; y < h; y++) { visit(0, y); visit(w - 1, y); }
    while (!todo.empty()) {
        int i = todo.back(), x = i % w, y = i / w;
        todo.pop_back();
        if (x > 0) visit(x - 1, y);
        if (x < w - 1) visit(x + 1, y);
        if (y > 0) visit(x, y - 1);
        if (y < h - 1) visit(x, y + 1);
    }
    for (int y = (int)erase.y; y < (int)(erase.y + erase.height) && y < h; y++)
        for (int x = (int)erase.x; x < (int)(erase.x + erase.width) && x < w; x++) bg[y * w + x] = 1;

    // Drop small separate bits (the sheet's number labels and name text) but keep big parts like flames
    std::vector<int> part(w * h, -1), partSize;
    for (int start = 0; start < w * h; start++) {
        if (bg[start] || part[start] >= 0) continue;
        int id = (int)partSize.size(), size = 0;
        part[start] = id;
        todo.push_back(start);
        while (!todo.empty()) {
            int i = todo.back(), x = i % w, y = i / w;
            todo.pop_back();
            size++;
            for (int j : { x > 0 ? i - 1 : -1, x < w - 1 ? i + 1 : -1, y > 0 ? i - w : -1, y < h - 1 ? i + w : -1 })
                if (j >= 0 && !bg[j] && part[j] < 0) { part[j] = id; todo.push_back(j); }
        }
        partSize.push_back(size);
    }
    int biggest = partSize.empty() ? 0 : *std::max_element(partSize.begin(), partSize.end());
    for (int i = 0; i < w * h; i++)
        if (!bg[i] && partSize[part[i]] < std::max(120, biggest / 8)) bg[i] = 1;

    int left = w, top = h, right = 0, bottom = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            bool sprite = !bg[i];
            if (!sprite) {
                for (int dy = -outline; dy <= outline && !sprite; dy++)
                    for (int dx = -outline; dx <= outline && !sprite; dx++) {
                        int nx = x + dx, ny = y + dy;
                        sprite = nx >= 0 && ny >= 0 && nx < w && ny < h && !bg[ny * w + nx];
                    }
                px[i] = sprite ? BLACK : BLANK;
            }
            if (sprite) { left = std::min(left, x); top = std::min(top, y); right = std::max(right, x + 1); bottom = std::max(bottom, y + 1); }
        }
    }
    if (right > left) ImageCrop(&img, { (float)left, (float)top, (float)(right - left), (float)(bottom - top) });
    Texture2D tex = SmoothTexture(img);
    UnloadImage(img);
    return tex;
}

Sprites LoadSprites() {
    Sprites s;
    std::string dir = std::string(GetApplicationDirectory()) + "Sprites/";
    // Areas below are pixel positions on each sheet; update them if a sheet changes

    Image pet = LoadImage((dir + "pet_sheet.png").c_str()); // 2752x1536, light grey checkerboard
    if (pet.data) {
        KeyOut(pet, [](Color c) {
            int hi = std::max({ c.r, c.g, c.b }), lo = std::min({ c.r, c.g, c.b });
            return lo > 140 && hi - lo < 25;
        });
        const float rows[4][2] = { {14, 248}, {255, 498}, {506, 740}, {743, 970} }; // happy, sad, angry, reading
        for (int m = 0; m < 4; m++) s.mood[m] = Cut(pet, { 0, rows[m][0], (float)pet.width, rows[m][1] - rows[m][0] });
        UnloadImage(pet);
    }

    Image ui = LoadImage((dir + "ui_sheet.png").c_str()); // 2752x1536, dark blue-grey background
    if (ui.data) {
        KeyOut(ui, [](Color c) { return Near(c, 44, 47, 57) || Near(c, 58, 63, 74); });
        s.hearts = Cut(ui, { 1431, 1186, 579, 128 });
        s.coin = Cut(ui, { 1431, 1342, 667, 124 });
        UnloadImage(ui);
    }

    Image food = LoadImage((dir + "food&shop.png").c_str()); // 1024x765, dark checkerboard
    if (food.data) {
        KeyOut(food, [](Color c) { return Near(c, 63, 66, 81) || Near(c, 38, 40, 52); });
        s.shop = Cut(food, { 370, 40, 290, 310 });
        s.food = Cut(food, { 100, 395, 830, 160 });
        std::vector<Texture2D> row2 = Cut(food, { 100, 590, 830, 140 });
        s.food.insert(s.food.end(), row2.begin(), row2.end());
        UnloadImage(food);
    }

    Image talk = LoadImage((dir + "txtcloud.png").c_str()); // 1024x572, grey checkerboard
    if (talk.data) {
        KeyOut(talk, [](Color c) { // any plain grey between the two checker shades, including the blurry edges
            int hi = std::max({ c.r, c.g, c.b }), lo = std::min({ c.r, c.g, c.b });
            return hi - lo < 12 && lo > 115 && hi < 205;
        });
        s.bubble = Cut(talk, { 20, 150, 260, 75 });
        s.alert = Cut(talk, { 278, 425, 232, 75 });
        UnloadImage(talk);
    }

    Image clothes = LoadImage((dir + "clothes.png").c_str()); // 960x717, black and dark grey checkerboard
    if (clothes.data) {
        ImageFormat(&clothes, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        auto checker = [](Color c) {
            int hi = std::max({ c.r, c.g, c.b }), lo = std::min({ c.r, c.g, c.b });
            return hi < 50 && hi - lo < 32; // both checker shades, the blurry seams between them, and dark backdrops
        };
        for (const Outfit& o : CLOTHES) s.clothes.push_back(CutOne(clothes, o.src, checker, 3));
        UnloadImage(clothes);
    }

    Image dead = LoadImage((dir + "dead.png").c_str()); // 1024x765, black and grey checkerboard
    if (dead.data) {
        ImageFormat(&dead, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        auto checker = [](Color c) {
            int hi = std::max({ c.r, c.g, c.b }), lo = std::min({ c.r, c.g, c.b });
            return hi - lo < 16 && hi < 75; // both checker shades and the blurry seams between them
        };
        s.ghost.push_back(CutOne(dead, { 560, 190, 170, 185 }, checker, 4));
        s.tombstone.push_back(CutOne(dead, { 320, 280, 400, 320 }, checker, 4, { 235, 0, 165, 95 })); // erase the ghost's tail
        UnloadImage(dead);
    }

    s.wishBg = LoadTexture((dir + "wish_bg.png").c_str()); // 960x717 wish screen from UI.jpg
    SetTextureFilter(s.wishBg, TEXTURE_FILTER_BILINEAR);
    s.devKey = LoadTexture((dir + "devkey.png").c_str());

    TraceLog(LOG_INFO, "SPRITES: moods %i/%i/%i/%i hearts %i coin %i food %i shop %i bubbles %i/%i clothes %i grave %i/%i",
             (int)s.mood[0].size(), (int)s.mood[1].size(), (int)s.mood[2].size(), (int)s.mood[3].size(),
             (int)s.hearts.size(), (int)s.coin.size(), (int)s.food.size(), (int)s.shop.size(),
             (int)s.bubble.size(), (int)s.alert.size(), (int)s.clothes.size(), (int)s.tombstone.size(), (int)s.ghost.size());
    return s;
}

// ---------- DRAWING HELPERS ----------
// Draw frame i of an animation centred on `center`, scaled to `height` pixels tall.
// squash > 1 makes it wider and shorter (for pokes); flip mirrors it left-right.
void DrawSprite(const std::vector<Texture2D>& frames, int i, Vector2 center, float height,
                float rotation = 0.0f, Color tint = WHITE, float squash = 1.0f, bool flip = false) {
    if (frames.empty() || height <= 0) return; // sheet missing: the text still shows
    const Texture2D& tex = frames[i % frames.size()];
    float width = height * tex.width / tex.height * squash;
    height /= squash;
    DrawTexturePro(tex, { 0, 0, flip ? -(float)tex.width : (float)tex.width, (float)tex.height },
                   { center.x, center.y, width, height }, { width / 2, height / 2 }, rotation, tint);
}

// Text with a dark shadow so it stays readable on the see-through window
void Text(const char* text, float x, float y, int size, Color color) {
    DrawText(text, (int)x + 2, (int)y + 2, size, Fade(BLACK, color.a / 255.0f * 0.8f));
    DrawText(text, (int)x, (int)y, size, color);
}
void TextCentered(const char* text, float cx, float y, int size, Color color) { Text(text, cx - MeasureText(text, size) / 2.0f, y, size, color); }

void DrawStar(Vector2 c, float r, Color color) {
    Vector2 p[10];
    for (int k = 0; k < 10; k++) {
        float a = (-90.0f + k * 36.0f) * DEG2RAD, rr = k % 2 ? r * 0.45f : r;
        p[k] = { c.x + cosf(a) * rr, c.y + sinf(a) * rr };
    }
    for (int k = 0; k < 10; k++) DrawTriangle(c, p[(k + 1) % 10], p[k], color);
}

void DrawStars(int count, float cx, float y, float r, Color color) {
    for (int i = 0; i < count; i++) DrawStar({ cx + (i - (count - 1) / 2.0f) * r * 2.3f, y }, r, color);
}

const Color RARITY_COLORS[6] = { {}, { 160, 160, 160, 255 }, { 90, 200, 90, 255 }, { 80, 150, 255, 255 },
                                 { 185, 100, 255, 255 }, { 255, 200, 60, 255 } };

std::vector<std::string> WrapText(const std::string& text, int fontSize, int maxWidth) {
    std::vector<std::string> lines;
    std::istringstream words(text);
    std::string word, line;
    while (words >> word) {
        std::string longer = line.empty() ? word : line + " " + word;
        if (!line.empty() && MeasureText(longer.c_str(), fontSize) > maxWidth) {
            lines.push_back(line);
            line = word;
        }
        else line = longer;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

// Speech bubble whose bottom edge sits at `bottom`, centred on `centerX`
void DrawSpeechBubble(const Sprites& art, const Pet& pet, float centerX, float bottom,
                      int fontSize = 20, int maxWidth = 460, size_t maxLines = 99) {
    std::vector<std::string> lines = WrapText(pet.currentAdvice, fontSize, maxWidth);
    if (lines.empty()) return;
    if (lines.size() > maxLines) {
        lines.resize(maxLines);
        lines.back() += "...";
    }
    int textWidth = 0, lineHeight = fontSize + 4;
    for (auto& l : lines) textWidth = std::max(textWidth, MeasureText(l.c_str(), fontSize));

    // 9-slice so the bubble's corners (and the white bubble's tail) keep their shape at any size
    bool isError = pet.adviceIsError;
    const std::vector<Texture2D>& frames = isError ? art.alert : art.bubble;
    float padX = isError ? 26.0f : 22.0f, padTop = isError ? 20.0f : 14.0f, padBottom = isError ? 20.0f : 30.0f;
    Rectangle box = { 0, 0, textWidth + 2 * padX, lines.size() * (float)lineHeight + padTop + padBottom };
    box.x = centerX - box.width / 2;
    box.y = bottom - box.height;
    if (!frames.empty()) {
        const Texture2D& tex = frames[0];
        NPatchInfo patch = isError ? NPatchInfo{ { 0, 0, (float)tex.width, (float)tex.height }, 22, 20, 22, 20, NPATCH_NINE_PATCH }
                                   : NPatchInfo{ { 0, 0, (float)tex.width, (float)tex.height }, 16, 14, 46, 26, NPATCH_NINE_PATCH };
        DrawTextureNPatch(tex, patch, box, { 0, 0 }, 0, WHITE);
    }
    else DrawRectangleRounded(box, 0.3f, 8, RAYWHITE);

    for (size_t i = 0; i < lines.size(); i++)
        DrawText(lines[i].c_str(), (int)(box.x + padX), (int)(box.y + padTop + i * lineHeight), fontSize, isError ? MAROON : DARKGRAY);
}

// The pet in its outfit (or its mood animation) at `at`, `k` times its normal size
void DrawPet(const Sprites& art, const Pet& pet, Vector2 at, float k, float t, float squash, bool flip) {
    int fit = pet.equipped >= 0 && pet.myMOOD != READING && !art.clothes.empty() ? pet.equipped : -1;
    Wear wear = fit >= 0 ? CLOTHES[fit].wear : WORN;
    float side = flip ? -1.0f : 1.0f;
    if (fit >= 0 && wear == AURA) DrawSprite(art.clothes, fit, at, 280 * k, sinf(t) * 5, Fade(WHITE, 0.85f), 1, flip);
    if (fit >= 0 && wear == WORN)
        DrawSprite(art.clothes, fit, at, std::min(260.0f, art.clothes[fit].height * 2.6f) * k, 0, WHITE, squash, flip);
    else DrawSprite(art.mood[pet.myMOOD], (int)(t * 6), at, 200 * k, 0, WHITE, squash, flip);
    if (fit >= 0 && wear == HELD) DrawSprite(art.clothes, fit, { at.x + 125 * k * side, at.y + (25 + sinf(t * 2) * 6) * k }, 110 * k, 0, WHITE, 1, flip);
    if (fit >= 0 && wear == HAT) DrawSprite(art.clothes, fit, { at.x, at.y - 68 * k }, 64 * k);
}

// Immediate-mode buttons: a click is used by the first button under the mouse
bool g_click = false;

bool Clicked(Rectangle r) {
    if (!g_click || !CheckCollisionPointRec(GetMousePosition(), r)) return false;
    g_click = false;
    return true;
}

bool Button(Rectangle r, const char* label, Color color, int fontSize = 20) {
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRounded(r, 0.3f, 6, hover ? ColorBrightness(color, 0.25f) : color);
    DrawRectangleRoundedLinesEx(r, 0.3f, 6, 2, Fade(BLACK, 0.6f));
    TextCentered(label, r.x + r.width / 2, r.y + (r.height - fontSize) / 2, fontSize, RAYWHITE);
    return Clicked(r);
}

void Panel(Rectangle r, Color border) {
    DrawRectangle(0, 0, 800, 600, Fade(BLACK, 0.55f));
    DrawRectangleRounded(r, 0.05f, 8, Color{ 30, 33, 40, 245 });
    DrawRectangleRoundedLinesEx(r, 0.05f, 8, 3, border);
}

// Typing into a text box: letters, Backspace, and Enter (returns true on Enter)
bool TypeInto(std::string& text, size_t maxLength) {
    for (int c = GetCharPressed(); c > 0; c = GetCharPressed())
        if (c >= 32 && c < 127 && text.size() < maxLength) text += (char)c;
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !text.empty()) text.pop_back();
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
}

// ---------- TESTS ----------
// game.exe --selftest checks the game rules without opening a window
int SelfTest() {
    int failures = 0;
    auto check = [&](bool ok, const std::string& what) {
        printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
        failures += !ok;
    };
    const std::string log = "selftest_activity.log", save = "selftest_save.txt";
    auto writeLog = [&](const std::string& text) { std::ofstream(log) << text; };

    { Pet p; p.addExp(); p.addExp();
      check(p.level == 1 && p.exp == 2, "2 EXP is still level 1");
      p.hunger = 50; p.addExp();
      check(p.level == 2 && p.exp == 0 && p.maxHealth == 20 && p.health == 20 && p.maxHunger == 110 && p.hunger == 110 && p.maxExp == 8,
            "3rd EXP: level 2, max health 20 and max hunger 110 (both refilled), next level at 8 EXP"); }

    { Pet p; p.level = 10; p.maxHealth = 80; p.maxHunger = 180; p.maxExp = 60; p.LevelUp();
      check(p.level == 11 && p.maxHealth == 85 && p.maxHunger == 185 && p.maxExp == 76, "levels 5-24: +5 health, +5 hunger, EXP grows by level+5");
      p.level = 25; p.LevelUp();
      check(p.level == 26 && p.maxHealth == 86 && p.maxHunger == 186 && p.maxExp == 77, "past level 25: health, hunger and EXP needed grow by just 1"); }

    { Pet p; writeLog("COMPILE_SUCCESS\n"); ReadActivityLog(p, log);
      check(p.tokens == 1 && p.exp == 1 && p.health == 10 && !p.adviceIsError, "successful compile: +1 token, +1 EXP");
      ReadActivityLog(p, log);
      check(p.tokens == 1, "the same compile isn't counted twice"); }

    { Pet p; writeLog("COMPILE_ERROR\nC:\\x\\main.cpp: In function 'int main()':\n"
                      "C:\\x\\main.cpp:5:5: error: expected ';' before 'return'\n");
      ReadActivityLog(p, log);
      check(p.tokens == 1 && p.health == 9 && p.myMOOD == READING && p.adviceIsError,
            "compile error: +1 token, -1 heart, READING mood, red bubble");
      check(p.currentAdvice == "Line 5: You forgot a semicolon ';' near 'return'!", "gcc error -> " + p.currentAdvice); }

    { Pet p; writeLog("COMPILE_ERROR\nTraceback (most recent call last):\n  File \"C:\\x\\a.py\", line 3, in <module>\n"
                      "    print(y)\nNameError: name 'y' is not defined\n");
      ReadActivityLog(p, log);
      check(p.currentAdvice == "Line 3: You used 'y', but it was never created. Typo?", "python error -> " + p.currentAdvice); }

    { Pet p;
      p.health = 6; p.Update(0); bool happy = p.myMOOD == HAPPY;
      p.health = 5; p.Update(0); bool sad = p.myMOOD == SAD;
      p.health = 2; p.Update(0); bool angry = p.myMOOD == ANGRY;
      check(happy && sad && angry, "mood: >50% health happy, >20% sad, otherwise angry"); }

    { Pet p; p.Update(59.0f);
      check(p.tokens == 0, "no free token before 1 minute");
      p.Update(1.0f);
      check(p.tokens == 1, "1 free token after 1 minute"); }

    { Pet p; p.Update(HUNGER_EVERY);
      check(p.hunger == 90, "hunger drops by 10 each tick");
      p.hunger = 10; p.Update(HUNGER_EVERY);
      check(p.hunger == 0 && p.health == 9, "starving (hunger 0) costs a heart"); }

    { Pet p; p.hunger = 50; p.health = 8;
      check(!Buy(p, FOODS[5]) && p.hunger == 50, "can't buy food without enough tokens");
      p.tokens = 5;
      check(Buy(p, FOODS[2]) && p.tokens == 0 && p.hunger == 100 && p.health == 9 && p.exp == 1, "ramen: -5 tokens, +50 food, +1 heart, +1 EXP"); }

    { Pet p; p.personality = 1; p.Update(IDLE_EVERY);
      bool tsundere = false;
      for (auto line : PERSONALITIES[1].idle) tsundere |= p.currentAdvice == line;
      check(tsundere, "idle for 20 s: a tsundere line -> " + p.currentAdvice);
      p.Poke();
      bool poked = false;
      for (auto line : PERSONALITIES[1].poke) poked |= p.currentAdvice == line;
      check(poked, "poke: a tsundere poke line -> " + p.currentAdvice); }

    { Pet p; p.health = 1; p.tokens = 7; p.Hurt(); p.Update(100.0f);
      writeLog("COMPILE_SUCCESS\n"); ReadActivityLog(p, log);
      check(p.IsDead() && p.tokens == 7 && p.hunger == 100, "a dead pet doesn't eat, earn or count compiles");
      p.Revive();
      check(!p.IsDead() && p.health == 10 && p.tokens == 0, "revive: full health, all tokens lost"); }

    { Pet p; std::mt19937 rng(1);
      int longest5 = 0, longest4 = 0, since5 = 0, since4 = 0, fives = 0, pulls = 200000;
      for (int i = 0; i < pulls; i++) {
          int stars = CLOTHES[Wish(p, rng)].stars;
          since5++; since4++;
          if (stars == 5) { fives++; longest5 = std::max(longest5, since5); since5 = 0; }
          if (stars >= 4) { longest4 = std::max(longest4, since4); since4 = 0; }
      }
      check(longest5 <= 90 && longest4 <= 10, "pity: a 5-star at least every 90 wishes, 4-star every 10");
      float rate = 100.0f * fives / pulls;
      check(rate > 1.3f && rate < 1.9f, "overall 5-star rate about 1.6% like Genshin -> " + std::to_string(rate) + "%"); }

    { Pet p; std::mt19937 rng(2); p.tokens = 95;
      check(Wishes(p, 10, rng).size() == 10 && p.tokens == 5, "10 wishes cost 90 tokens");
      check(Wishes(p, 1, rng).empty() && p.tokens == 5, "can't wish without 10 tokens"); }

    { Pet p; p.name = "Mr Byte"; p.personality = 4; p.level = 7; p.tokens = 33; p.owned[12] = 2; p.equipped = 12; p.pity5 = 61;
      p.Save(save);
      Pet q; q.Load(save);
      check(q.name == "Mr Byte" && q.personality == 4 && q.level == 7 && q.tokens == 33 && q.owned[12] == 2 && q.equipped == 12 && q.pity5 == 61,
            "save and load keep name, personality, level, tokens, clothes and pity"); }

    std::remove(log.c_str());
    std::remove(save.c_str());
    printf(failures ? "%d FAILED\n" : "ALL PASSED\n", failures);
    return failures ? 1 : 0;
}

// ---------- THE GAME ----------
enum Screen { MAIN, SHOP, GACHA, WARDROBE, PERSONALITIES_MENU, DEV_LOGIN, DEV };

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--selftest") return SelfTest();

    // Only one pet at a time: two would fight over activity.log, and VS Code may try to open a second one
    CreateMutexA(nullptr, 0, "Tamagothu_one_pet");
    if (GetLastError() == 183) return 0; // ERROR_ALREADY_EXISTS

    // 1. Create the Raylib window: small, see-through, resizable, always on top of other windows.
    // The game is drawn at 800x600 and scaled to fit.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_TOPMOST);
    InitWindow(400, 300, "Tamagothu: The Dev Pet");
    SetWindowMinSize(200, 150);
    SetExitKey(KEY_NULL); // Esc shouldn't close the game while you're typing a name or password
    SetTargetFPS(60);
    RenderTexture2D screen = LoadRenderTexture(800, 600);
    SetTextureFilter(screen.texture, TEXTURE_FILTER_BILINEAR);
    Sprites art = LoadSprites(); // needs the window to exist first
    std::string dir = GetApplicationDirectory();
    std::string logPath = dir + "activity.log"; // next to game.exe and petlog.ps1
    std::string savePath = dir + "save.txt";
    std::srand((unsigned)std::time(nullptr));
    std::mt19937 rng((unsigned)std::time(nullptr));

    // 2. CREATE THE PET HERE (This fixes the undefined error!)
    Pet myPet;
    myPet.Load(savePath);

    Screen screenNow = MAIN;
    bool editingName = false, devUnlocked = false;
    std::string password, devMessage;
    std::vector<int> lastWish;
    float timeScale = 1.0f; // dev mode can speed up time
    float logCheckTimer = 0.0f, saveTimer = 0.0f;
    int lastHealth = myPet.health, lastTokens = myPet.tokens;
    float hurtShake = 0.0f, coinPop = 0.0f, eatTimer = 0.0f, pokeTimer = 0.0f, wishFlash = 0.0f, deathTimer = 0.0f;
    int eatenFood = 0;
    const Rectangle shopButton = { 20, 470, 120, 120 };
    const Rectangle shopPanel = { 90, 90, 620, 420 };
    auto foodCell = [&](int i) { return Rectangle{ shopPanel.x + 15 + (i % 4) * 150.0f, shopPanel.y + 85 + (i / 4) * 160.0f, 140, 150 }; };

    // Wander mode: the window shrinks to just the pet, loses its frame, and walks along the bottom of the screen
    const int WANDER_W = 280, WANDER_H = 210;
    bool wandering = false, facingLeft = false;
    Vector2 homePos = {};
    int homeW = 0, homeH = 0;
    float walkX = 0, walkTarget = 0, walkPause = 0, bubbleTimer = 0;
    std::string lastAdvice = myPet.currentAdvice;
    auto startWandering = [&]() {
        homePos = GetWindowPosition();
        homeW = GetScreenWidth();
        homeH = GetScreenHeight();
        SetWindowState(FLAG_WINDOW_UNDECORATED);
        SetWindowSize(WANDER_W, WANDER_H);
        walkX = walkTarget = homePos.x;
        walkPause = 1.0f;
        wandering = true;
        screenNow = MAIN;
        myPet.Say("I'm going for a walk! Click me to bring me back.");
    };
    auto stopWandering = [&]() {
        ClearWindowState(FLAG_WINDOW_UNDECORATED); // stays always-on-top
        SetWindowSize(homeW, homeH);
        SetWindowPosition((int)homePos.x, (int)homePos.y);
        wandering = false;
    };

    // 3. Start the infinite game loop
    while (!WindowShouldClose()) {

        // Track how much time passed this frame
        float realDt = GetFrameTime();
        float dt = realDt * timeScale;

        // Fit the 800x600 game into the window, centred, and map the mouse back to game coordinates
        float scale = std::min(GetScreenWidth() / 800.0f, GetScreenHeight() / 600.0f);
        Vector2 offset = { (GetScreenWidth() - 800 * scale) / 2, (GetScreenHeight() - 600 * scale) / 2 };
        SetMouseOffset((int)-offset.x, (int)-offset.y);
        SetMouseScale(1 / scale, 1 / scale);

        // Update the pet's timers and mood
        myPet.Update(dt);

        // Check for new activity logs every 2 seconds
        logCheckTimer += realDt;
        if (logCheckTimer > 2.0f) {
            ReadActivityLog(myPet, logPath);
            logCheckTimer = 0.0f;
        }

        saveTimer += realDt;
        if (saveTimer > 10.0f) {
            myPet.Save(savePath);
            saveTimer = 0.0f;
        }

        // Start effects when something changes
        if (myPet.health < lastHealth) hurtShake = 0.5f;
        if (myPet.tokens > lastTokens) coinPop = 0.4f;
        lastHealth = myPet.health;
        lastTokens = myPet.tokens;
        hurtShake = std::max(0.0f, hurtShake - realDt);
        coinPop = std::max(0.0f, coinPop - realDt);
        eatTimer = std::max(0.0f, eatTimer - realDt);
        pokeTimer = std::max(0.0f, pokeTimer - realDt);
        wishFlash = std::max(0.0f, wishFlash - realDt);
        deathTimer = myPet.IsDead() ? deathTimer + realDt : 0.0f;
        float t = (float)GetTime();

        // While wandering, the speech bubble only pops up for a few seconds after the pet says something new
        if (myPet.currentAdvice != lastAdvice) { lastAdvice = myPet.currentAdvice; bubbleTimer = 6.0f; }
        bubbleTimer = std::max(0.0f, bubbleTimer - realDt);
        if (wandering && myPet.IsDead()) stopWandering(); // come home so the death screen shows
        if (wandering) {
            int monitor = GetCurrentMonitor();
            Vector2 mon = GetMonitorPosition(monitor);
            if (walkPause > 0) walkPause -= realDt;
            else {
                float step = 70 * realDt;
                facingLeft = walkTarget < walkX;
                walkX += std::clamp(walkTarget - walkX, -step, step);
                if (fabsf(walkTarget - walkX) < 1) { // arrived: rest a bit, then pick a new spot
                    walkPause = (float)GetRandomValue(2, 6);
                    walkTarget = mon.x + GetRandomValue(0, GetMonitorWidth(monitor) - WANDER_W);
                }
            }
            SetWindowPosition((int)walkX, (int)(mon.y + GetMonitorHeight(monitor) - WANDER_H - 48)); // 48: above the taskbar
        }

        // A click on the main screen only counts when no window is open on top of it.
        // Windows on top only get clicks if they were already open, so the click that opens one can't also close it.
        bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        Screen screenBefore = screenNow;
        g_click = click && screenNow == MAIN;

        // 4. DRAWING PHASE
        BeginTextureMode(screen);
        ClearBackground(BLANK);
        // Keep the alpha channel correct for the see-through window
        rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        if (wandering) {
            // Just the pet, big and hopping while it walks, with a short-lived speech bubble; click it to bring it home
            bool walking = walkPause <= 0;
            float hop = walking ? -fabsf(sinf(t * 9.0f)) * 30.0f : sinf(t * 3.0f) * 10.0f;
            float squash = 1.0f + 0.25f * sinf(pokeTimer / 0.4f * PI);
            DrawPet(art, myPet, { 400, 390 + hop }, 1.9f, t, squash, facingLeft);
            if (bubbleTimer > 0) DrawSpeechBubble(art, myPet, 400, 200, 44, 740, 2);
            if (Clicked({ 200, 200, 400, 400 })) stopWandering();
        }
        else {
            DrawRectangleRounded({ 4, 4, 792, 592 }, 0.06f, 8, Color{ 43, 47, 56, 150 }); // tinted glass behind the pet

            // Hearts: 10 hearts, each worth a tenth of max health. They shake when hurt and beat when low.
            float perHeart = myPet.maxHealth / 10.0f;
            bool lowHealth = myPet.health <= myPet.maxHealth * 0.3f;
            for (int i = 0; i < 10; i++) {
                float fill = (myPet.health - i * perHeart) / perHeart;
                int frame = fill >= 0.875f ? 0 : fill >= 0.5f ? 1 : fill > 0.0f ? 2 : 3;
                float beat = lowHealth ? 1.0f + 0.15f * std::max(0.0f, sinf(t * 8.0f)) : 1.0f;
                float shake = hurtShake > 0 ? (float)GetRandomValue(-4, 4) : 0.0f;
                DrawSprite(art.hearts, frame, { 28 + i * 36.0f + shake, 30 }, 30 * beat);
            }
            Text(TextFormat("%i / %i", myPet.health, myPet.maxHealth), 380, 20, 20, RED);

            // Token: spinning coin that bounces when you earn one
            float pop = 1.0f + 0.5f * sinf(coinPop / 0.4f * PI);
            DrawSprite(art.coin, (int)(t * 10), { 690, 30 }, 36 * pop);
            Text(TextFormat("x %i", myPet.tokens), 715, 18, 24, GOLD);

            // Name (click to rename) and level
            Rectangle nameBox = { 10, 56, 300, 28 };
            if (editingName) {
                DrawRectangleRounded(nameBox, 0.3f, 6, Fade(BLACK, 0.6f));
                Text(TextFormat("%s%s", myPet.name.c_str(), fmodf(t, 1.0f) < 0.5f ? "_" : ""), nameBox.x + 6, nameBox.y + 4, 20, GOLD);
                Text("Enter to save", nameBox.x + nameBox.width + 8, nameBox.y + 6, 16, GRAY);
                if (TypeInto(myPet.name, 16) || (click && !CheckCollisionPointRec(GetMousePosition(), nameBox))) {
                    if (myPet.name.empty()) myPet.name = "Tamagothu";
                    editingName = false;
                    myPet.Say("Call me " + myPet.name + "!");
                }
            }
            else {
                const char* label = TextFormat("%s  Lv %i", myPet.name.c_str(), myPet.level);
                bool hover = CheckCollisionPointRec(GetMousePosition(), nameBox) && screenNow == MAIN;
                Text(label, nameBox.x + 6, nameBox.y + 4, 20, hover ? GOLD : RAYWHITE);
                if (hover) Text("(click to rename)", nameBox.x + 12 + MeasureText(label, 20), nameBox.y + 7, 14, GRAY);
                if (Clicked(nameBox)) editingName = true;
            }
            Text(TextFormat("EXP: %i / %i", myPet.exp, myPet.maxExp), 16, 88, 20, SKYBLUE);

            // Hunger bar with the amount written on it
            Text("Food", 540, 62, 20, RAYWHITE);
            Rectangle hungerBar = { 600, 60, 185, 24 };
            float full = (float)myPet.hunger / myPet.maxHunger;
            Color hungerColor = full > 0.5f ? GREEN : full > 0.25f ? ORANGE : RED;
            DrawRectangleRec(hungerBar, Color{ 20, 22, 28, 220 });
            DrawRectangle((int)hungerBar.x, (int)hungerBar.y, (int)(hungerBar.width * full), (int)hungerBar.height, hungerColor);
            DrawRectangleLinesEx(hungerBar, 2, BLACK);
            TextCentered(TextFormat("%i / %i", myPet.hunger, myPet.maxHunger), hungerBar.x + hungerBar.width / 2, hungerBar.y + 4, 18, RAYWHITE);

            const Vector2 petCenter = { 400, 330 };
            if (myPet.IsDead()) {
                // Death: the pet keels over and fades, the tombstone rises, then its ghost floats up
                float d = deathTimer;
                if (d < 1.5f) {
                    float k = d / 1.5f;
                    DrawSprite(art.mood[SAD], 0, { petCenter.x, petCenter.y + k * 40 }, 200 * (1 - 0.4f * k), 90 * k, Fade(GRAY, 1 - k));
                }
                float rise = std::clamp((d - 1.0f) / 1.0f, 0.0f, 1.0f);
                rise = 1 - (1 - rise) * (1 - rise); // ease out
                DrawSprite(art.tombstone, 0, { petCenter.x - 20, 640 - rise * 260 }, 220);
                if (d > 1.8f) {
                    float g = std::min(1.0f, (d - 1.8f) / 1.0f);
                    DrawSprite(art.ghost, 0, { petCenter.x + 90, 260 - g * 40 + sinf(t * 2) * 8 }, 110, sinf(t * 1.5f) * 8, Fade(WHITE, g * 0.9f));
                }
                if (d > 2.5f) {
                    TextCentered(TextFormat("%s crashed for good...", myPet.name.c_str()), 400, 120, 28, RAYWHITE);
                    if (Button({ 285, 160, 230, 46 }, "Revive (lose all tokens)", MAROON, 18)) myPet.Revive();
                }
            }
            else {
                // Character: its outfit (or the mood animation), bobbing gently; angry trembles, pokes squish, eating bounces
                float bob = myPet.myMOOD == ANGRY ? (float)GetRandomValue(-3, 3) : sinf(t * 3.0f) * 8.0f;
                if (eatTimer > 0) bob = -fabsf(sinf(eatTimer * 12.0f)) * 12.0f;
                float squash = 1.0f + 0.25f * sinf(pokeTimer / 0.4f * PI);
                DrawPet(art, myPet, { petCenter.x, petCenter.y + bob }, 1.0f, t, squash, false);

                // Food being eaten: shrinks into the pet's mouth
                if (eatTimer > 0) DrawSprite(art.food, eatenFood, { 400, 370 }, 70 * eatTimer);

                // Poke the pet
                if (Clicked({ petCenter.x - 100, petCenter.y - 100, 200, 200 })) {
                    myPet.Poke();
                    pokeTimer = 0.4f;
                }

                // Mood and personality
                const char* moodNames[] = { "Happy", "Sad", "Angry", "Reading your error..." };
                const Color moodColors[] = { GREEN, SKYBLUE, RED, YELLOW };
                TextCentered(TextFormat("Mood: %s", moodNames[myPet.myMOOD]), 400, 438, 22, moodColors[myPet.myMOOD]);
                TextCentered(TextFormat("(%s)", PERSONALITIES[myPet.personality].name), 400, 462, 16, GRAY);

                // Speech bubble with the pet's advice (red for errors)
                DrawSpeechBubble(art, myPet, 400, 215);

                // Bottom buttons
                bool overShop = CheckCollisionPointRec(GetMousePosition(), shopButton) && screenNow == MAIN;
                DrawSprite(art.shop, 0, { shopButton.x + 60, shopButton.y + 55 }, overShop ? 100.0f : 90.0f);
                if (Clicked(shopButton)) screenNow = SHOP;
                if (Button({ 160, 530, 120, 48 }, "Wish", Color{ 170, 120, 20, 255 })) screenNow = GACHA;
                if (Button({ 290, 530, 150, 48 }, "Wardrobe", Color{ 70, 90, 160, 255 })) screenNow = WARDROBE;
                if (Button({ 450, 530, 160, 48 }, "Personality", Color{ 140, 60, 120, 255 })) screenNow = PERSONALITIES_MENU;
                if (Button({ 615, 530, 80, 48 }, "Wander", Color{ 40, 120, 110, 255 }, 16)) startWandering();
            }

            // Dev key
            Rectangle devButton = { 700, 528, 87, 50 };
            if (art.devKey.id) DrawTextureEx(art.devKey, { devButton.x, devButton.y }, 0, 1, WHITE);
            else Button(devButton, "DEV", DARKGRAY);
            if (Clicked(devButton)) { screenNow = devUnlocked ? DEV : DEV_LOGIN; password.clear(); devMessage.clear(); }

            // ---------- WINDOWS ON TOP ----------
            g_click = click && screenBefore != MAIN && screenNow != MAIN;

            if (screenNow == SHOP) {
                Panel(shopPanel, GOLD);
                Text("SHOP", shopPanel.x + 20, shopPanel.y + 15, 32, GOLD);
                Text(TextFormat("You have %i tokens. Click food to feed me!", myPet.tokens), shopPanel.x + 20, shopPanel.y + 55, 18, RAYWHITE);
                if (Button({ shopPanel.x + shopPanel.width - 110, shopPanel.y + 14, 95, 34 }, "Close", DARKGRAY, 18)) screenNow = MAIN;
                for (int i = 0; i < 8; i++) {
                    Rectangle cell = foodCell(i);
                    bool canAfford = myPet.tokens >= FOODS[i].price;
                    if (CheckCollisionPointRec(GetMousePosition(), cell)) DrawRectangleRounded(cell, 0.1f, 6, Fade(GOLD, 0.15f));
                    float cx = cell.x + cell.width / 2;
                    DrawSprite(art.food, i, { cx, cell.y + 40 }, 64);
                    TextCentered(FOODS[i].name, cx, cell.y + 80, 20, RAYWHITE);
                    TextCentered(TextFormat("%i token%s", FOODS[i].price, FOODS[i].price == 1 ? "" : "s"), cx, cell.y + 104, 18, canAfford ? GOLD : GRAY);
                    std::string fills = TextFormat("+%i food", FOODS[i].hunger);
                    if (FOODS[i].hearts) fills += " +1 heart";
                    if (FOODS[i].exp) fills += " +1 EXP";
                    TextCentered(fills.c_str(), cx, cell.y + 126, FOODS[i].exp ? 14 : 16, GREEN);
                    if (Clicked(cell) && Buy(myPet, FOODS[i])) {
                        eatenFood = i;
                        eatTimer = 1.0f;
                        screenNow = MAIN;
                    }
                }
                if (Clicked({ 0, 0, 800, 600 }) && !CheckCollisionPointRec(GetMousePosition(), shopPanel)) screenNow = MAIN;
            }

            if (screenNow == GACHA) {
                // UI.jpg's wish screen, scaled from 960 to 800 wide; its buttons and Close/Skip are part of the picture
                DrawRectangle(0, 0, 800, 600, Color{ 15, 17, 22, 250 });
                if (art.wishBg.id) DrawTexturePro(art.wishBg, { 0, 0, 960, 600 }, { 0, 0, 800, 500 }, { 0, 0 }, 0, WHITE);
                int best = -1;
                for (int item : lastWish) if (best < 0 || CLOTHES[item].stars > CLOTHES[best].stars) best = item;
                Color glow = best >= 0 ? RARITY_COLORS[CLOTHES[best].stars] : GOLD;
                Rectangle stage = { 150, 120, 500, 292 }; // covers the example result painted on UI.jpg
                DrawRectangleRounded(stage, 0.08f, 8, Color{ 20, 18, 30, 255 });
                DrawRectangleRoundedLinesEx(stage, 0.08f, 8, 3, glow);
                if (best >= 0) {
                    DrawCircleGradient(400, 230, 120, Fade(glow, 0.45f), Fade(glow, 0.0f));
                    DrawSprite(art.clothes, best, { 400, 228 }, 150);
                    TextCentered(CLOTHES[best].name, 400, 318, 28, glow);
                    DrawStars(CLOTHES[best].stars, 400, 362, 11, glow);
                }
                else {
                    TextCentered("Wish for outfits!", 400, 160, 30, GOLD);
                    TextCentered("5-star: 0.6%, guaranteed by 90 wishes", 400, 215, 18, RARITY_COLORS[5]);
                    TextCentered("4-star: 5.1%, guaranteed every 10 wishes", 400, 240, 18, RARITY_COLORS[4]);
                    TextCentered("3-star 15%   2-star 30%   1-star 55%", 400, 265, 18, RAYWHITE);
                    TextCentered("Wear them from the Wardrobe", 400, 310, 18, GRAY);
                }
                TextCentered(TextFormat("5-star pity %i/90     4-star pity %i/10", myPet.pity5, myPet.pity4), 400, 388, 16, GRAY);
                DrawRectangle(30, 428, 140, 60, Color{ 15, 17, 22, 255 }); // covers the example tokens/level on UI.jpg
                Text(TextFormat("TOKENS: %i", myPet.tokens), 36, 434, 18, GREEN);
                Text(TextFormat("LVL: %i", myPet.level), 36, 460, 18, GREEN);

                Rectangle wish1 = { 240, 417, 155, 62 }, wish10 = { 406, 417, 155, 62 }, close = { 620, 448, 150, 40 };
                if (!art.wishBg.id) { Button(wish1, "Wish x1 (10)", DARKGREEN, 18); Button(wish10, "Wish x10 (90)", DARKGREEN, 18); Button(close, "Close", DARKGRAY, 18); }
                for (Rectangle r : { wish1, wish10, close })
                    if (CheckCollisionPointRec(GetMousePosition(), r)) DrawRectangleRounded(r, 0.2f, 6, Fade(WHITE, 0.15f));
                int count = Clicked(wish1) ? 1 : Clicked(wish10) ? 10 : 0;
                if (count) {
                    std::vector<int> won = Wishes(myPet, count, rng);
                    if (!won.empty()) {
                        lastWish = won;
                        wishFlash = 0.8f;
                        myPet.Say("Ooh, new clothes! Check the Wardrobe.");
                    }
                    else devMessage = myPet.currentAdvice;
                }
                if (Clicked(close)) { screenNow = MAIN; devMessage.clear(); }
                if (!devMessage.empty()) TextCentered(devMessage.c_str(), 400, 490, 18, RED);

                // This wish's results, one card per outfit, framed in its rarity colour
                for (size_t i = 0; i < lastWish.size(); i++) {
                    const Outfit& o = CLOTHES[lastWish[i]];
                    Rectangle card = { 400 - lastWish.size() * 38.0f + i * 76.0f, 512, 70, 80 };
                    DrawRectangleRounded(card, 0.15f, 6, Color{ 30, 30, 40, 255 });
                    DrawRectangleRoundedLinesEx(card, 0.15f, 6, 3, RARITY_COLORS[o.stars]);
                    DrawSprite(art.clothes, lastWish[i], { card.x + 35, card.y + 34 }, 52);
                    TextCentered(TextFormat("%i*", o.stars), card.x + 35, card.y + 62, 14, RARITY_COLORS[o.stars]);
                }
                if (wishFlash > 0) DrawRectangle(0, 0, 800, 600, Fade(glow, wishFlash * 0.7f));
            }

            if (screenNow == WARDROBE) {
                Rectangle panel = { 40, 40, 720, 520 };
                Panel(panel, Color{ 120, 150, 255, 255 });
                int ownedKinds = 0;
                for (int n : myPet.owned) ownedKinds += n > 0;
                Text("WARDROBE", panel.x + 20, panel.y + 14, 30, Color{ 150, 180, 255, 255 });
                Text(TextFormat("%i / %i outfits. Click one to wear it, click again to take it off.", ownedKinds, CLOTHES_COUNT),
                     panel.x + 20, panel.y + 50, 16, RAYWHITE);
                if (Button({ panel.x + panel.width - 110, panel.y + 14, 95, 34 }, "Close", DARKGRAY, 18)) screenNow = MAIN;
                for (int i = 0; i < CLOTHES_COUNT; i++) {
                    Rectangle cell = { panel.x + 20 + (i % 8) * 85.0f, panel.y + 80 + (i / 8) * 86.0f, 80, 80 };
                    const Outfit& o = CLOTHES[i];
                    bool have = myPet.owned[i] > 0, on = myPet.equipped == i;
                    DrawRectangleRounded(cell, 0.15f, 6, on ? Fade(RARITY_COLORS[o.stars], 0.35f) : Color{ 22, 24, 30, 255 });
                    DrawRectangleRoundedLinesEx(cell, 0.15f, 6, on ? 4.0f : 2.0f, have ? RARITY_COLORS[o.stars] : Fade(RARITY_COLORS[o.stars], 0.3f));
                    if (have) {
                        DrawSprite(art.clothes, i, { cell.x + 40, cell.y + 36 }, 54);
                        if (myPet.owned[i] > 1) Text(TextFormat("x%i", myPet.owned[i]), cell.x + 52, cell.y + 60, 14, RAYWHITE);
                        if (CheckCollisionPointRec(GetMousePosition(), cell)) {
                            TextCentered(TextFormat("%s (%i-star)", o.name, o.stars), 400, panel.y + panel.height - 30, 20, RARITY_COLORS[o.stars]);
                        }
                        if (Clicked(cell)) {
                            myPet.equipped = on ? -1 : i;
                            myPet.Say(on ? "Back to my natural look!" : std::string("How do I look in my ") + o.name + "?");
                        }
                    }
                    else TextCentered("?", cell.x + 40, cell.y + 26, 28, Fade(RARITY_COLORS[o.stars], 0.4f));
                    TextCentered(TextFormat("%i*", o.stars), cell.x + 14, cell.y + 4, 12, Fade(RARITY_COLORS[o.stars], have ? 1.0f : 0.4f));
                }
            }

            if (screenNow == PERSONALITIES_MENU) {
                Rectangle panel = { 110, 50, 580, 500 };
                Panel(panel, Color{ 220, 120, 200, 255 });
                Text("PERSONALITY", panel.x + 20, panel.y + 14, 30, Color{ 240, 150, 220, 255 });
                Text("Pick how I talk when I'm bored or poked.", panel.x + 20, panel.y + 52, 16, RAYWHITE);
                if (Button({ panel.x + panel.width - 110, panel.y + 14, 95, 34 }, "Close", DARKGRAY, 18)) screenNow = MAIN;
                for (int i = 0; i < PERSONALITY_COUNT; i++) {
                    Rectangle row = { panel.x + 20, panel.y + 85 + i * 67.0f, panel.width - 40, 60 };
                    bool current = myPet.personality == i;
                    bool hover = CheckCollisionPointRec(GetMousePosition(), row);
                    DrawRectangleRounded(row, 0.2f, 6, current ? Color{ 110, 50, 100, 255 } : hover ? Color{ 60, 50, 70, 255 } : Color{ 40, 40, 50, 255 });
                    Text(PERSONALITIES[i].name, row.x + 14, row.y + 8, 22, current ? GOLD : RAYWHITE);
                    Text(TextFormat("\"%s\"", PERSONALITIES[i].idle[0]), row.x + 14, row.y + 36, 14, GRAY);
                    if (Clicked(row)) {
                        myPet.personality = i;
                        myPet.Say(PERSONALITIES[i].hello);
                        screenNow = MAIN;
                    }
                }
            }

            if (screenNow == DEV_LOGIN) {
                Rectangle panel = { 200, 180, 400, 220 };
                Panel(panel, GREEN);
                Text("DEV MODE", panel.x + 20, panel.y + 16, 28, GREEN);
                Text("Password:", panel.x + 20, panel.y + 66, 20, RAYWHITE);
                Rectangle box = { panel.x + 20, panel.y + 94, 360, 36 };
                DrawRectangleRounded(box, 0.2f, 6, BLACK);
                Text(TextFormat("%s%s", std::string(password.size(), '*').c_str(), fmodf(t, 1.0f) < 0.5f ? "_" : ""), box.x + 10, box.y + 8, 22, GREEN);
                bool enter = TypeInto(password, 32);
                if (Button({ panel.x + 20, panel.y + 150, 170, 44 }, "Unlock", DARKGREEN) || enter) {
                    if (password == "robloxslop") { devUnlocked = true; screenNow = DEV; devMessage = "Welcome, developer."; }
                    else { devMessage = "Wrong password"; password.clear(); }
                }
                if (Button({ panel.x + 210, panel.y + 150, 170, 44 }, "Cancel", DARKGRAY)) screenNow = MAIN;
                if (!devMessage.empty()) Text(devMessage.c_str(), panel.x + 160, panel.y + 66, 18, RED);
            }

            if (screenNow == DEV) {
                Rectangle panel = { 30, 30, 740, 540 };
                Panel(panel, GREEN);
                Text("DEV MODE", panel.x + 20, panel.y + 14, 30, GREEN);
                if (Button({ panel.x + panel.width - 110, panel.y + 14, 95, 34 }, "Close", DARKGRAY, 18)) screenNow = MAIN;
                Text(TextFormat("HP %i/%i   Food %i/%i   Lv %i   EXP %i/%i   Tokens %i", myPet.health, myPet.maxHealth, myPet.hunger,
                                myPet.maxHunger, myPet.level, myPet.exp, myPet.maxExp, myPet.tokens), panel.x + 20, panel.y + 56, 18, RAYWHITE);
                Text(TextFormat("Pity 5* %i/90  4* %i/10   Next token %.0fs   Next hunger drop %.0fs   Time x%.0f", myPet.pity5, myPet.pity4,
                                TOKEN_EVERY - myPet.uptimeTimer, HUNGER_EVERY - myPet.hungerTimer, timeScale), panel.x + 20, panel.y + 80, 16, GRAY);

                const char* labels[] = { "+10 tokens", "+100 tokens", "+1000 tokens", "Level up",
                                         "Hurt -1 heart", "Heal full", "Starve (food 0)", "Feed full",
                                         "Kill pet", "Revive", "Fake compile OK", "Fake compile error",
                                         "Idle line now", "5* pity -> 89", "Reset pity", "Unlock all clothes",
                                         "Time x1", "Time x10", "Time x60", "Reset pet" };
                int pressed = -1;
                for (int i = 0; i < 20; i++) {
                    Rectangle b = { panel.x + 20 + (i % 4) * 177.0f, panel.y + 115 + (i / 4) * 62.0f, 167, 50 };
                    Color c = i >= 16 ? Color{ 40, 90, 140, 255 } : i >= 8 && i < 10 ? MAROON : Color{ 30, 100, 50, 255 };
                    if (i == 19) c = Color{ 120, 40, 40, 255 };
                    if (Button(b, labels[i], c, 18)) pressed = i;
                }
                switch (pressed) {
                    case 0: myPet.tokens += 10; break;
                    case 1: myPet.tokens += 100; break;
                    case 2: myPet.tokens += 1000; break;
                    case 3: myPet.LevelUp(); break;
                    case 4: myPet.Hurt(); break;
                    case 5: myPet.health = myPet.maxHealth; break;
                    case 6: myPet.hunger = 0; break;
                    case 7: myPet.hunger = myPet.maxHunger; break;
                    case 8: myPet.health = 0; screenNow = MAIN; break;
                    case 9: if (myPet.IsDead()) myPet.Revive(); break;
                    case 10: std::ofstream(logPath) << "COMPILE_SUCCESS\n"; break;
                    case 11: std::ofstream(logPath) << "COMPILE_ERROR\ndev.cpp:7:3: error: expected ';' before 'return'\n"; break;
                    case 12: myPet.quietTimer = IDLE_EVERY; myPet.adviceIsError = false; break;
                    case 13: myPet.pity5 = 89; break;
                    case 14: myPet.pity5 = myPet.pity4 = 0; break;
                    case 15: for (int& n : myPet.owned) n = std::max(n, 1); break;
                    case 16: timeScale = 1; break;
                    case 17: timeScale = 10; break;
                    case 18: timeScale = 60; break;
                    case 19: { std::string name = myPet.name; myPet = Pet(); myPet.name = name; lastWish.clear(); break; }
                }
                if (pressed >= 0) devMessage = std::string("Done: ") + labels[pressed];
                Text(devMessage.c_str(), panel.x + 20, panel.y + panel.height - 36, 18, GREEN);
            }

        }
        EndBlendMode();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLANK);
        // Copy the game into the window with premultiplied colour, which the see-through window expects
        rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ZERO, RL_ONE, RL_ZERO, RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        // render textures are stored upside down, hence the negative height
        DrawTexturePro(screen.texture, { 0, 0, 800, -600 }, { offset.x, offset.y, 800 * scale, 600 * scale }, { 0, 0 }, 0, WHITE);
        EndBlendMode();
        EndDrawing();
    }
    myPet.Save(savePath);
    UnloadRenderTexture(screen);

    // Clean up when the user closes the window
    auto unload = [](std::vector<Texture2D>& frames) { for (auto& tex : frames) UnloadTexture(tex); };
    for (auto& frames : art.mood) unload(frames);
    unload(art.hearts); unload(art.coin); unload(art.food); unload(art.shop); unload(art.bubble); unload(art.alert);
    unload(art.clothes); unload(art.tombstone); unload(art.ghost);
    UnloadTexture(art.wishBg);
    UnloadTexture(art.devKey);
    CloseWindow();
    return 0;
}

#include "raylib.h"
#include <fstream>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <vector>
#include <sstream>
#include <regex>

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
const int HUNGER_DROP = 10;       // hunger lost each time (100 = full, 0 = starving)

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
    int hunger = 100;

    float uptimeTimer = 0.0f;  // counts up to the next free token
    float hungerTimer = 0.0f;  // counts up to the next hunger drop
    float readingTimer = 0.0f; // how long to keep the READING mood after a compile error

    std::string currentAdvice = "Ready to code!";
    bool adviceIsError = false; // errors get the red speech bubble

    void Say(const std::string& text, bool isError = false) {
        currentAdvice = text;
        adviceIsError = isError;
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
        Say("Level Up! You are now level " + std::to_string(level) + "!");
    }

    int OneHeart() const { return std::max(1, maxHealth / 10); } // the hearts bar always shows 10 hearts

    void Hurt() { health = std::max(0, health - OneHeart()); }

    void Update(float deltaTime) {
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
            else if (hunger <= 30 && !adviceIsError) Say("I'm getting hungry... the shop has food!");
        }

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

    if (success) {
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
struct Food { const char* name; int price; int hunger; int hearts; };

// Same order as the food sprites on food&shop.png: top row, then bottom row
const Food FOODS[8] = {
    { "Pizza", 3, 30, 0 }, { "Burger", 4, 40, 0 }, { "Ramen", 5, 50, 1 }, { "Donut", 2, 15, 0 },
    { "Sushi", 3, 25, 1 }, { "Apple", 1, 10, 1 },  { "Milk", 1, 10, 1 },  { "Bone", 1, 5, 0 },
};

bool Buy(Pet& pet, const Food& food) {
    if (pet.tokens < food.price) {
        pet.Say("You need " + std::to_string(food.price) + " tokens for " + food.name + ".");
        return false;
    }
    pet.tokens -= food.price;
    pet.hunger = std::min(100, pet.hunger + food.hunger);
    pet.health = std::min(pet.maxHealth, pet.health + food.hearts * pet.OneHeart());
    pet.Say(std::string("Yum, ") + food.name + "! Thank you!");
    return true;
}

// ---------- SPRITES ----------
// The sheets in Sprites/ are AI-generated: the "transparent" checkerboard is painted in, and raylib
// can't load .jpg (pet_sheet.png / ui_sheet.png are PNG copies of the two .jpg sheets). So at startup
// we erase the background ourselves and find each sprite by the empty gaps around it.

struct Sprites {
    std::vector<Texture2D> mood[4]; // one animation per Mood: HAPPY, SAD, ANGRY, READING
    std::vector<Texture2D> hearts;  // full, 3/4, 1/4, empty
    std::vector<Texture2D> coin;    // spinning token
    std::vector<Texture2D> food;    // same order as FOODS
    std::vector<Texture2D> shop;    // the shop stall
    std::vector<Texture2D> bubble;  // white speech bubble
    std::vector<Texture2D> alert;   // red spiky bubble for errors
};

bool Near(Color c, int r, int g, int b) { return abs(c.r - r) + abs(c.g - g) + abs(c.b - b) < 30; }

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
        Texture2D tex = LoadTextureFromImage(part);
        GenTextureMipmaps(&tex); // smooth when drawn much smaller than the sheet
        SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
        frames.push_back(tex);
        UnloadImage(part);
    }
    return frames;
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

    TraceLog(LOG_INFO, "SPRITES: happy %i, sad %i, angry %i, reading %i, hearts %i, coin %i, food %i, shop %i, bubble %i, alert %i",
             (int)s.mood[0].size(), (int)s.mood[1].size(), (int)s.mood[2].size(), (int)s.mood[3].size(),
             (int)s.hearts.size(), (int)s.coin.size(), (int)s.food.size(), (int)s.shop.size(),
             (int)s.bubble.size(), (int)s.alert.size());
    return s;
}

// Draw frame i of an animation centred on `center`, scaled to `height` pixels tall
void DrawSprite(const std::vector<Texture2D>& frames, int i, Vector2 center, float height) {
    if (frames.empty() || height <= 0) return; // sheet missing: the text still shows
    const Texture2D& tex = frames[i % frames.size()];
    float width = height * tex.width / tex.height;
    DrawTexturePro(tex, { 0, 0, (float)tex.width, (float)tex.height },
                   { center.x - width / 2, center.y - height / 2, width, height }, { 0, 0 }, 0, WHITE);
}

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
void DrawSpeechBubble(const Sprites& art, const Pet& pet, float centerX, float bottom) {
    std::vector<std::string> lines = WrapText(pet.currentAdvice, 20, 460);
    if (lines.empty()) return;
    int textWidth = 0;
    for (auto& l : lines) textWidth = std::max(textWidth, MeasureText(l.c_str(), 20));

    // 9-slice so the bubble's corners (and the white bubble's tail) keep their shape at any size
    bool isError = pet.adviceIsError;
    const std::vector<Texture2D>& frames = isError ? art.alert : art.bubble;
    float padX = isError ? 26.0f : 22.0f, padTop = isError ? 20.0f : 14.0f, padBottom = isError ? 20.0f : 30.0f;
    Rectangle box = { 0, 0, textWidth + 2 * padX, lines.size() * 24.0f + padTop + padBottom };
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
        DrawText(lines[i].c_str(), (int)(box.x + padX), (int)(box.y + padTop + i * 24), 20, isError ? MAROON : DARKGRAY);
}

// ---------- TESTS ----------
// game.exe --selftest checks the game rules without opening a window
int SelfTest() {
    int failures = 0;
    auto check = [&](bool ok, const std::string& what) {
        printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
        failures += !ok;
    };
    const std::string log = "selftest_activity.log";
    auto writeLog = [&](const std::string& text) { std::ofstream(log) << text; };

    { Pet p; p.addExp(); p.addExp();
      check(p.level == 1 && p.exp == 2, "2 EXP is still level 1");
      p.addExp();
      check(p.level == 2 && p.exp == 0 && p.maxHealth == 20 && p.health == 20 && p.maxExp == 8,
            "3rd EXP: level 2, max health 20 (refilled), next level at 8 EXP"); }

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

    { Pet p; writeLog("COMPILE_ERROR\nmain.cpp:3:5: error: 'y' was not declared in this scope\n"); ReadActivityLog(p, log);
      check(p.currentAdvice == "Line 3: You tried to use 'y', but it hasn't been created yet!", "gcc error -> " + p.currentAdvice); }

    { Pet p; writeLog("COMPILE_ERROR\nTraceback (most recent call last):\n  File \"C:\\x\\a.py\", line 3, in <module>\n"
                      "    print(y)\nNameError: name 'y' is not defined\n");
      ReadActivityLog(p, log);
      check(p.currentAdvice == "Line 3: You used 'y', but it was never created. Typo?", "python error -> " + p.currentAdvice); }

    { Pet p; p.myMOOD = READING; p.readingTimer = 5.0f; p.Update(1.0f);
      check(p.myMOOD == READING, "READING mood stays for a few seconds");
      p.Update(5.0f); p.Update(0.1f);
      check(p.myMOOD == HAPPY, "then mood goes back to following health"); }

    { Pet p;
      p.health = 6; p.Update(0); bool happy = p.myMOOD == HAPPY;
      p.health = 5; p.Update(0); bool sad = p.myMOOD == SAD;
      p.health = 3; p.Update(0); bool stillSad = p.myMOOD == SAD;
      p.health = 2; p.Update(0); bool angry = p.myMOOD == ANGRY;
      check(happy && sad && stillSad && angry, "mood: >50% health happy, >20% sad, otherwise angry"); }

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
      check(Buy(p, FOODS[2]) && p.tokens == 0 && p.hunger == 100 && p.health == 9, "ramen: -5 tokens, +50 food, +1 heart"); }

    std::remove(log.c_str());
    printf(failures ? "%d FAILED\n" : "ALL PASSED\n", failures);
    return failures ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--selftest") return SelfTest();

    // 1. Create the Raylib window
    // The game is drawn at 800x600 and scaled to fit, so the window can be any size
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(480, 360, "Tamagothu: The Dev Pet");
    SetWindowMinSize(240, 180);
    SetTargetFPS(60);
    RenderTexture2D screen = LoadRenderTexture(800, 600);
    SetTextureFilter(screen.texture, TEXTURE_FILTER_BILINEAR);
    Sprites art = LoadSprites(); // needs the window to exist first
    std::string logPath = std::string(GetApplicationDirectory()) + "activity.log"; // next to game.exe and petlog.ps1

    // 2. CREATE THE PET HERE (This fixes the undefined error!)
    Pet myPet;

    float logCheckTimer = 0.0f;
    int lastHealth = myPet.health, lastTokens = myPet.tokens;
    float hurtShake = 0.0f, coinPop = 0.0f, eatTimer = 0.0f; // seconds left on each effect
    int eatenFood = 0;
    bool shopOpen = false;
    const Rectangle shopButton = { 20, 470, 120, 120 };
    const Rectangle shopPanel = { 90, 90, 620, 420 };
    auto foodCell = [&](int i) { return Rectangle{ shopPanel.x + 15 + (i % 4) * 150.0f, shopPanel.y + 85 + (i / 4) * 160.0f, 140, 150 }; };

    // 3. Start the infinite game loop
    while (!WindowShouldClose()) {

        // Track how much time passed this frame
        float dt = GetFrameTime();

        // Fit the 800x600 game into the window, centred, and map the mouse back to game coordinates
        float scale = std::min(GetScreenWidth() / 800.0f, GetScreenHeight() / 600.0f);
        Vector2 offset = { (GetScreenWidth() - 800 * scale) / 2, (GetScreenHeight() - 600 * scale) / 2 };
        SetMouseOffset((int)-offset.x, (int)-offset.y);
        SetMouseScale(1 / scale, 1 / scale);

        // Update the pet's timers and mood
        myPet.Update(dt);

        // Check for new activity logs every 2 seconds
        logCheckTimer += dt;
        if (logCheckTimer > 2.0f) {
            ReadActivityLog(myPet, logPath);
            logCheckTimer = 0.0f;
        }

        // Clicks: open the shop, buy food, or click outside the shop to close it
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            if (!shopOpen) shopOpen = CheckCollisionPointRec(mouse, shopButton);
            else if (!CheckCollisionPointRec(mouse, shopPanel)) shopOpen = false;
            else for (int i = 0; i < 8; i++) {
                if (CheckCollisionPointRec(mouse, foodCell(i)) && Buy(myPet, FOODS[i])) {
                    eatenFood = i;
                    eatTimer = 1.0f;
                    shopOpen = false;
                }
            }
        }

        // Start effects when something changes
        if (myPet.health < lastHealth) hurtShake = 0.5f;
        if (myPet.tokens > lastTokens) coinPop = 0.4f;
        lastHealth = myPet.health;
        lastTokens = myPet.tokens;
        hurtShake = std::max(0.0f, hurtShake - dt);
        coinPop = std::max(0.0f, coinPop - dt);
        eatTimer = std::max(0.0f, eatTimer - dt);
        float t = (float)GetTime();

        // 4. DRAWING PHASE
        BeginTextureMode(screen);
        ClearBackground(Color{ 43, 47, 56, 255 }); // same dark grey as the UI sprite sheet

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
        DrawText(TextFormat("%i / %i", myPet.health, myPet.maxHealth), 380, 20, 20, RED);

        // Token: spinning coin that bounces when you earn one
        float pop = 1.0f + 0.5f * sinf(coinPop / 0.4f * PI);
        DrawSprite(art.coin, (int)(t * 10), { 690, 30 }, 36 * pop);
        DrawText(TextFormat("x %i", myPet.tokens), 715, 18, 24, GOLD);

        // Draw Stats
        DrawText(TextFormat("Level: %i", myPet.level), 14, 60, 20, RAYWHITE);
        DrawText(TextFormat("EXP: %i / %i", myPet.exp, myPet.maxExp), 14, 85, 20, SKYBLUE);

        // Hunger bar
        DrawText("Hunger", 560, 60, 20, RAYWHITE);
        Rectangle hungerBar = { 645, 62, 140, 18 };
        Color hungerColor = myPet.hunger > 50 ? GREEN : myPet.hunger > 25 ? ORANGE : RED;
        DrawRectangleRec(hungerBar, Color{ 20, 22, 28, 255 });
        DrawRectangle((int)hungerBar.x, (int)hungerBar.y, (int)(hungerBar.width * myPet.hunger / 100), (int)hungerBar.height, hungerColor);
        DrawRectangleLinesEx(hungerBar, 2, BLACK);

        // Character: the animation for its mood, bobbing gently (angry trembles, eating bounces)
        float bob = myPet.myMOOD == ANGRY ? (float)GetRandomValue(-3, 3) : sinf(t * 3.0f) * 8.0f;
        if (eatTimer > 0) bob = -fabsf(sinf(eatTimer * 12.0f)) * 12.0f;
        DrawSprite(art.mood[myPet.myMOOD], (int)(t * 6), { 400, 330 + bob }, 200);

        // Food being eaten: shrinks into the pet's mouth
        if (eatTimer > 0) DrawSprite(art.food, eatenFood, { 400, 370 }, 70 * eatTimer);

        // Mood
        const char* moodNames[] = { "Happy", "Sad", "Angry", "Reading your error..." };
        const Color moodColors[] = { GREEN, SKYBLUE, RED, YELLOW };
        const char* moodText = TextFormat("Mood: %s", moodNames[myPet.myMOOD]);
        DrawText(moodText, 400 - MeasureText(moodText, 24) / 2, 445, 24, moodColors[myPet.myMOOD]);

        // Speech bubble with the pet's advice (red for errors)
        DrawSpeechBubble(art, myPet, 400, 215);

        // Shop button
        bool overShop = CheckCollisionPointRec(GetMousePosition(), shopButton);
        DrawSprite(art.shop, 0, { shopButton.x + 60, shopButton.y + 50 }, overShop ? 100.0f : 90.0f);
        DrawText("Click the shop", 160, 560, 18, overShop ? GOLD : GRAY);

        // Shop window
        if (shopOpen) {
            DrawRectangle(0, 0, 800, 600, Fade(BLACK, 0.6f));
            DrawRectangleRounded(shopPanel, 0.05f, 8, Color{ 30, 33, 40, 255 });
            DrawRectangleRoundedLinesEx(shopPanel, 0.05f, 8, 3, GOLD);
            DrawText("SHOP", (int)shopPanel.x + 20, (int)shopPanel.y + 15, 32, GOLD);
            DrawText(TextFormat("You have %i tokens. Click food to feed me!", myPet.tokens), (int)shopPanel.x + 20, (int)shopPanel.y + 55, 18, RAYWHITE);
            DrawText("click outside to close", (int)(shopPanel.x + shopPanel.width) - 200, (int)shopPanel.y + 22, 16, GRAY);
            for (int i = 0; i < 8; i++) {
                Rectangle cell = foodCell(i);
                bool canAfford = myPet.tokens >= FOODS[i].price;
                if (CheckCollisionPointRec(GetMousePosition(), cell)) DrawRectangleRounded(cell, 0.1f, 6, Fade(GOLD, 0.15f));
                float cx = cell.x + cell.width / 2;
                DrawSprite(art.food, i, { cx, cell.y + 40 }, 64);
                DrawText(FOODS[i].name, (int)cx - MeasureText(FOODS[i].name, 20) / 2, (int)cell.y + 80, 20, RAYWHITE);
                const char* price = TextFormat("%i token%s", FOODS[i].price, FOODS[i].price == 1 ? "" : "s");
                DrawText(price, (int)cx - MeasureText(price, 18) / 2, (int)cell.y + 104, 18, canAfford ? GOLD : GRAY);
                const char* fills = FOODS[i].hearts ? TextFormat("+%i food +1 heart", FOODS[i].hunger) : TextFormat("+%i food", FOODS[i].hunger);
                DrawText(fills, (int)cx - MeasureText(fills, 16) / 2, (int)cell.y + 126, 16, GREEN);
            }
        }

        EndTextureMode();
        BeginDrawing();
        ClearBackground(Color{ 43, 47, 56, 255 });
        // render textures are stored upside down, hence the negative height
        DrawTexturePro(screen.texture, { 0, 0, 800, -600 }, { offset.x, offset.y, 800 * scale, 600 * scale }, { 0, 0 }, 0, WHITE);
        EndDrawing();
    }
    UnloadRenderTexture(screen);

    // Clean up when the user closes the window
    auto unload = [](std::vector<Texture2D>& frames) { for (auto& tex : frames) UnloadTexture(tex); };
    for (auto& frames : art.mood) unload(frames);
    unload(art.hearts); unload(art.coin); unload(art.food); unload(art.shop); unload(art.bubble); unload(art.alert);
    CloseWindow();
    return 0;
}

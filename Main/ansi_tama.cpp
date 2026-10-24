// Codegotchi: a Tamagotchi that lives off your compiler output.
//   tamagotchi                          play (feed, games, talk, check code)
//   tamagotchi check main.cpp           compile a file with the matching compiler
//   tamagotchi check g++ -Wall x.cpp    run any build command
//   g++ x.cpp 2>&1 | tamagotchi check   feed it a log
//   tamagotchi designs                  show every sprite
//   tamagotchi selftest                 run the parser checks
// Build: g++ -std=gnu++14 -O2 -o tamagotchi tamagotchi.cpp
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <io.h>
#define popen _popen
#define pclose _pclose
#define isatty _isatty
#define CLEAR "cls"
#else
#include <unistd.h>
#define CLEAR "clear"
#endif
using namespace std;

const char* USAGE =
    "  tamagotchi                          play (feed, games, talk, check code)\n"
    "  tamagotchi check main.cpp           compile a file with the matching compiler\n"
    "  tamagotchi check g++ -Wall x.cpp    run any build command\n"
    "  g++ x.cpp 2>&1 | tamagotchi check   feed it a log\n"
    "  tamagotchi designs                  show every sprite\n";
const int W = 24, H = 9;  // screen size inside the device
mt19937 rng((unsigned)time(nullptr));

template <class T> string str(T v) { ostringstream o; o << v; return o.str(); }  // MinGW.org lacks std::to_string

string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == string::npos ? "" : s.substr(a, b - a + 1);
}

string replaceAll(string s, const string& from, const string& to) {
    for (size_t i = 0; (i = s.find(from, i)) != string::npos; i += to.size()) s.replace(i, from.size(), to);
    return s;
}

string center(const string& s, size_t w) {
    if (s.size() >= w) return s;
    size_t l = (w - s.size()) / 2;
    return string(l, ' ') + s + string(w - s.size() - l, ' ');
}

template <class T> const T& pick(const vector<T>& v) { return v[rng() % v.size()]; }

// ---------- designs: {e} = eyes (3 chars), {m} = mouth (1 char) ----------
const map<string, vector<string>> SPRITES = {
    {"egg", {
        R"(     ___)",
        R"(   .'   '.)",
        R"(  /  . .  \)",
        R"( |  .   .  |)",
        R"(  \   .   /)",
        R"(   '.___.')"}},
    {"baby", {
        R"(   .-"""-.)",
        R"(  /       \)",
        R"( |   {e}   |)",
        R"(  \   {m}   /)",
        R"(   '-...-')"}},
    {"teen", {
        R"(  o       o)",
        R"(   \.---./)",
        R"(   /     \)",
        R"(  |  {e}  |)",
        R"(  |   {m}   |)",
        R"(   \_____/)",
        R"(    d   b)"}},
    {"adult", {
        R"(    _.---._)",
        R"(  .'       '.)",
        R"( /    {e}    \)",
        R"( |     {m}     |)",
        R"(/|   .---.   |\)",
        R"( |   '---'   |)",
        R"(  '.__   __.')",
        R"(    (_) (_))"}},
    {"ghost", {
        R"(   .-----.)",
        R"(  /       \)",
        R"( |   {e}   |)",
        R"( |    {m}    |)",
        R"( |         |)",
        R"( |/\/\/\/\/|)"}},
};
const vector<string> STAGES = {"egg", "baby", "teen", "adult", "ghost"};
const map<string, pair<string, string>> FACES = {
    {"happy", {"^ ^", "w"}}, {"ok", {"o o", "-"}}, {"sad", {"T T", "n"}},
    {"sick", {"@ @", "~"}}, {"dead", {"x x", "o"}}};

vector<string> art(const string& stage, const string& mood) {
    vector<string> a = SPRITES.at(stage);
    for (auto& l : a) l = replaceAll(replaceAll(l, "{e}", FACES.at(mood).first), "{m}", FACES.at(mood).second);
    return a;
}

string device(vector<string> lines, const string& caption) {
    size_t w = 0;
    for (auto& l : lines) w = max(w, l.size());
    for (auto& l : lines) l.resize(w, ' ');
    size_t top = (H - lines.size()) / 2;
    lines.insert(lines.begin(), top, "");
    lines.resize(H, "");
    ostringstream o;
    o << "   " << string(W + 4, '_') << "\n"
      << " .'" << center(caption, W + 4) << "'.\n"
      << " |  ." << string(W, '-') << ".  |\n";
    for (auto& l : lines) o << " |  |" << center(l, W) << "|  |\n";
    o << " |  '" << string(W, '-') << "'  |\n"
      << " |" << center("(A)    (B)    (C)", W + 6) << "|\n"
      << "  '." << string(W + 2, '_') << ".'\n";
    return o.str();
}

// ---------- pet state ----------
struct Pet {
    string name = "Byte";
    double hp = 100, food = 80, joy = 80;
    int xp = 0;
    bool failing = false;
    long long seen = time(nullptr);
    vector<string> hints;
};

string savePath() {
    const char* h = getenv("USERPROFILE");
    if (!h) h = getenv("HOME");
    return string(h ? h : ".") + "/.tamagotchi";
}

bool load(Pet& p) {
    ifstream f(savePath());
    if (!f) return false;
    string key, val;
    while (f >> key && getline(f >> ws, val)) {
        if (key == "name") p.name = val;
        else if (key == "hp") p.hp = atof(val.c_str());
        else if (key == "food") p.food = atof(val.c_str());
        else if (key == "joy") p.joy = atof(val.c_str());
        else if (key == "xp") p.xp = atoi(val.c_str());
        else if (key == "failing") p.failing = val == "1";
        else if (key == "seen") p.seen = atoll(val.c_str());
        else if (key == "hint") p.hints.push_back(val);
    }
    return true;
}

void fix(Pet& p) {
    for (double* v : {&p.hp, &p.food, &p.joy}) *v = max(0.0, min(100.0, *v));
}

void save(Pet& p) {
    fix(p);
    ofstream f(savePath());
    f << "name " << p.name << "\nhp " << p.hp << "\nfood " << p.food << "\njoy " << p.joy
      << "\nxp " << p.xp << "\nfailing " << p.failing << "\nseen " << p.seen << "\n";
    for (auto& h : p.hints) f << "hint " << h << "\n";
}

void tick(Pet& p) {
    double hours = difftime(time(nullptr), (time_t)p.seen) / 3600;
    double starving = max(0.0, hours - p.food / 4);  // hours spent at 0 food
    p.food -= 4 * hours;
    p.joy -= 3 * hours;
    p.hp -= 2 * starving;
    p.seen = time(nullptr);
    fix(p);
}

int level(const Pet& p) { return 1 + p.xp / 100; }

string stage(const Pet& p) {
    if (p.hp <= 0) return "ghost";
    if (p.xp < 20) return "egg";
    return level(p) < 3 ? "baby" : level(p) < 6 ? "teen" : "adult";
}

string mood(const Pet& p) {
    if (p.hp <= 0) return "dead";
    if (p.hp < 30) return "sick";
    if (min(p.food, p.joy) < 30) return "sad";
    return p.joy >= 70 ? "happy" : "ok";
}

string bar(double v) {
    int n = (int)(v / 10 + 0.5);
    string num = str((int)(v + 0.5));
    return "[" + string(n, '#') + string(10 - n, '-') + "] " + string(3 - min<size_t>(3, num.size()), ' ') + num;
}

string chatter(const Pet& p) {
    if (p.hp <= 0) return "...";
    if (!p.hints.empty()) return p.hints[0];
    if (p.food < 30) return "I'm hungry...";
    if (p.joy < 30) return "I'm bored. Play with me?";
    if (stage(p) == "egg") return "*wobble*";
    return pick(vector<string>{"Ready to compile!", "*hums in binary*", "Let's write some code.",
                               "I believe in you.", "Compile me something nice."});
}

void show(const Pet& p) {
    cout << device(art(stage(p), mood(p)), p.name + "  Lv " + str(level(p)))
         << "   HP   " << bar(p.hp) << "\n   Food " << bar(p.food) << "\n   Joy  " << bar(p.joy)
         << "\n   XP   " << p.xp << " (" << stage(p) << ")\n\n   " << p.name << ": \"" << chatter(p) << "\"\n";
}

// ---------- reading compiler output ----------
struct Diag { string kind, file; int line; string msg; };

vector<Diag> parse(const string& log) {
    // gcc/clang/javac: file:line:col: error: msg   MSVC: file(line): error C123: msg   tsc: file(l,c): error TS1: msg
    static const regex diag(R"(^(.+?)[:(](\d+)(?:[:,]\d+)*\)?:\s*(?:fatal\s+)?(error|warning)\b[^:]*:\s*(.+))",
                            regex::icase);
    vector<Diag> out;
    istringstream in(log);
    string raw;
    smatch m;
    while (getline(in, raw)) {
        // gcc prints curly quotes on UTF-8 terminals
        string s = replaceAll(replaceAll(trim(raw), "\xE2\x80\x98", "'"), "\xE2\x80\x99", "'");
        if (!regex_search(s, m, diag)) continue;
        string kind = m[3];
        transform(kind.begin(), kind.end(), kind.begin(), ::tolower);
        out.push_back({kind, m[1], atoi(m[2].str().c_str()), trim(m[4])});
    }
    return out;
}

const vector<pair<string, string>> HINTS = {
    // gcc reports a missing ; on the NEXT line ("expected ',' or ';' before 'return'")
    {"expected .*';'|';' expected|missing ';'|semicolon", "check {at} (or the end of the line above) for a missing semicolon ;"},
    {"unterminated|missing terminating|newline in constant", "{at}: a string is never closed, check your quotes"},
    {R"(never closed|unmatched|expected '[)\]}]'|'[)\]}]' expected|missing '[)\]}]'|end of (input|file))",
     "{at}: a bracket ( [ { isn't closed"},
    {"no matching function|too many arguments|too few arguments", "{at}: wrong arguments for that function call"},
    {"undeclared|not declared|not defined|cannot find symbol|undefined|unresolved|has no member",
     "{at}: you're using a name that doesn't exist. Typo or missing #include?"},
    {"type|cannot convert|invalid conversion|incompatible|mismatch|no match for", "{at}: the types don't match"},
    {"unused", "{at}: something is declared but never used"},
    {"syntax|unexpected|expected", "{at}: there's a typo here, check this line and the one before"},
};

string hint(const Diag& d) {
    string at = "line " + str(d.line) + " of " + d.file.substr(d.file.find_last_of("/\\") + 1);
    for (auto& h : HINTS)
        if (regex_search(d.msg, regex(h.first, regex::icase))) return replaceAll(h.second, "{at}", at);
    return at + ": " + d.msg.substr(0, 60);
}

const map<string, string> COMPILERS = {
    {".cpp", "g++ -fsyntax-only -Wall"}, {".cc", "g++ -fsyntax-only -Wall"},
    {".c", "gcc -fsyntax-only -Wall"}, {".java", "javac"}};

// Run a source file (picks its compiler) or any build command; returns its output.
string runBuild(string cmd, int& code) {
    string f = trim(cmd);
    f.erase(remove(f.begin(), f.end(), '"'), f.end());
    size_t dot = f.rfind('.');
    if (dot != string::npos && COMPILERS.count(f.substr(dot)) && ifstream(f).good())
        cmd = COMPILERS.at(f.substr(dot)) + " \"" + f + "\"";
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) { code = -1; return "could not start the build"; }
    string out;
    char buf[512];
    while (fgets(buf, sizeof buf, pipe)) out += buf;
    code = pclose(pipe);
    return out;
}

// Apply damage/XP for one build and return what the pet says about it.
vector<string> judge(Pet& p, vector<Diag> found, int code, const string& log = "") {
    stable_partition(found.begin(), found.end(), [](const Diag& d) { return d.kind == "error"; });
    int errors = count_if(found.begin(), found.end(), [](const Diag& d) { return d.kind == "error"; });
    int warns = (int)found.size() - errors;
    vector<string> hints;
    for (auto& d : found) {
        string h = hint(d);
        if (find(hints.begin(), hints.end(), h) == hints.end()) hints.push_back(h);
    }
    if (code && !errors) {  // failed but nothing we could read (linker, missing compiler...)
        errors = 1;
        istringstream in(log);
        string first = "no output", l;
        while (getline(in, l)) if (!trim(l).empty()) { first = trim(l); break; }
        hints.insert(hints.begin(), "the build failed: " + first.substr(0, 70));
    }
    int oldLevel = level(p);
    string oldStage = stage(p);
    vector<string> said;
    if (errors) {
        int dmg = min(40, 8 * errors);
        p.hp -= dmg;
        p.joy -= 5;
        said.push_back("OUCH! " + str(errors) + " error(s), -" + str(dmg) + " HP");
    } else {
        int gain = p.failing ? 35 : 20;
        p.xp += gain;
        p.hp += 5;
        p.joy += 5;
        said.push_back(string(p.failing ? "You fixed it!" : "Clean build!") + " +" + str(gain) + " XP");
    }
    if (warns) {
        said.push_back(str(warns) + " warning(s), -" + str(min(10, 2 * warns)) + " HP");
        p.hp -= min(10, 2 * warns);
    }
    p.failing = errors > 0;
    if (hints.size() > 5) hints.resize(5);
    p.hints = hints;
    fix(p);
    if (p.hp <= 0) said.push_back(p.name + " crashed for good...");
    else if (stage(p) != oldStage)
        said.push_back(oldStage == "egg" ? "The egg hatched!" : p.name + " evolved into a " + stage(p) + "!");
    else if (level(p) > oldLevel) said.push_back("Level up! Lv " + str(level(p)));
    for (auto& h : p.hints) said.push_back(p.name + ": \"" + h + "\"");
    return said;
}

// ---------- things to do with it ----------
const vector<string> TIPS = {
    "Read the first error first. Later ones are often caused by it.",
    "Compile often. Small changes are easier to fix.",
    "If it's weird, print it.",
    "Commit when it works.",
    "Explain the bug out loud to a rubber duck (or me).",
    "Off by one? Check < vs <=.",
    "Name variables for what they hold.",
    "Delete code you don't need. Less code, fewer bugs."};

struct Bug { vector<string> code; int line; string why; };
const vector<Bug> BUGS = {
    {{"int main() {", "  int x = 5", "  return x;", "}"}, 2, "missing ; after int x = 5"},
    {{"if (x = 5) {", "  go();", "}"}, 1, "= assigns, == compares"},
    {{"int a[3] = {1, 2, 3};", "for (int i = 0; i <= 3; i++)", "  sum += a[i];"}, 2, "<= 3 reads past the end, use < 3"},
    {{"int* p = new int(4);", "delete p;", "cout << *p;"}, 3, "p was already deleted"},
    {{"vector<int> v = {1, 2};", "int x = v[2];", "cout << x;"}, 2, "v only has indexes 0 and 1"},
    {{"int sum(int a, int b) {", "  return a + b;", "}", "sum(2);"}, 4, "sum needs two arguments"},
};

string ask(const string& q) {
    cout << q << flush;
    string s;
    if (!getline(cin, s)) return "q";
    return trim(s);
}

void feed(Pet& p) {
    string c = ask("   [1] Meal (rice)  [2] Snack (cookie) > ");
    if (c != "1" && c != "2") return;
    if (p.food >= 95) { p.joy -= 5; cout << "   " << p.name << ": \"I'm full!\" -5 Joy\n"; }
    else if (c == "1") { p.food += 35; cout << "   *munch munch* +35 Food\n"; }
    else { p.food += 10; p.joy += 10; cout << "   *crunch* +10 Food +10 Joy\n"; }
}

void leftRight(Pet& p) {
    int wins = 0;
    for (int r = 1; r <= 5; r++) {
        string g = ask("   Round " + str(r) + "/5: will " + p.name + " look left or right? [l/r] ");
        char turn = rng() % 2 ? 'l' : 'r';
        bool hit = !g.empty() && tolower(g[0]) == turn;
        wins += hit;
        cout << "   " << p.name << (turn == 'l' ? " looks <-- left" : " looks right -->") << (hit ? "  nice!" : "") << "\n";
    }
    p.food -= 5;
    if (wins >= 3) { p.joy += 20; p.xp += 10; cout << "   You won " << wins << "/5! +20 Joy +10 XP\n"; }
    else { p.joy += 5; cout << "   " << wins << "/5. " << p.name << " still had fun. +5 Joy\n"; }
}

void bugHunt(Pet& p) {
    const Bug& b = pick(BUGS);
    cout << "\n";
    for (size_t i = 0; i < b.code.size(); i++) cout << "   " << i + 1 << " | " << b.code[i] << "\n";
    p.food -= 5;
    if (ask("\n   Which line has the bug? ") == str(b.line)) {
        p.xp += 15; p.joy += 10;
        cout << "   Got it! " << b.why << ". +15 XP +10 Joy\n";
    } else {
        p.joy += 3;
        cout << "   Nope, line " << b.line << ": " << b.why << ". +3 Joy\n";
    }
}

void talk(Pet& p) {
    if (!p.hints.empty()) {
        cout << "   About your last build:\n";
        for (auto& h : p.hints) cout << "    - " << h << "\n";
    }
    if (p.food < 30) cout << "   I'm starving... feed me?\n";
    if (p.joy < 30) cout << "   Play with me?\n";
    if (p.hp < 30) cout << "   I don't feel good. Fix those errors please.\n";
    cout << "   Tip: " << pick(TIPS) << "\n";
}

void checkMenu(Pet& p) {
    string cmd = ask("   File or build command (e.g. main.cpp or g++ main.cpp): ");
    if (cmd.empty() || cmd == "q") return;
    int code = 0;
    string log = runBuild(cmd, code);
    for (auto& s : judge(p, parse(log), code, log)) cout << "   " << s << "\n";
}

void play(Pet& p, bool firstRun) {
    if (firstRun) { string n = ask("   An egg appeared! Name it: "); if (!n.empty() && n != "q") p.name = n; }
    for (;;) {
        system(CLEAR);
        show(p);
        if (p.hp <= 0) {
            string a = ask("\n   " + p.name + " crashed for good. Hatch a new egg? [y/n] ");
            if (a.empty() || tolower(a[0]) != 'y') return;
            p = Pet();
            string n = ask("   Name it: ");
            if (!n.empty() && n != "q") p.name = n;
            continue;
        }
        string c = ask("\n   [1] Feed  [2] Left/Right  [3] Bug Hunt  [4] Talk  [5] Check code  [q] Quit\n   > ");
        if (c == "q" || c == "Q") return;
        if (c == "1") feed(p);
        else if (c == "2") leftRight(p);
        else if (c == "3") bugHunt(p);
        else if (c == "4") talk(p);
        else if (c == "5") checkMenu(p);
        else continue;
        save(p);
        ask("\n   (enter)");
    }
}

int selftest() {
    string log =
        "main.cpp: In function 'int main()':\n"
        "main.cpp:5:12: error: expected \xE2\x80\x98;\xE2\x80\x99 before \xE2\x80\x98return\xE2\x80\x99\n"
        "C:\\x\\a.cpp:3:5: warning: unused variable 'y' [-Wunused-variable]\n"
        "main.c(7): error C2065: 'z': undeclared identifier\n"
        "Main.java:9: error: ';' expected\n"
        "app.ts(2,4): error TS1005: ')' expected.\n"
        "main.cpp:5:3: note: something\n";
    auto d = parse(log);
    assert(d.size() == 5);
    assert(d[0].kind == "error" && d[0].file == "main.cpp" && d[0].line == 5 && d[0].msg == "expected ';' before 'return'");
    assert(d[1].kind == "warning" && d[1].file == "C:\\x\\a.cpp" && d[1].line == 3);
    assert(hint(d[0]) == "check line 5 of main.cpp (or the end of the line above) for a missing semicolon ;");
    assert(hint({"error", "b.cpp", 3, "expected ',' or ';' before 'return'"}).find("semicolon") != string::npos);
    assert(hint(d[2]).find("doesn't exist") != string::npos);
    assert(hint(d[3]).find("semicolon") != string::npos);
    assert(hint(d[4]).find("bracket") != string::npos);
    Pet p;
    judge(p, d, 1);
    assert(p.hp == 100 - 32 - 2 && p.failing);  // 4 errors, 1 warning
    judge(p, {}, 0);
    assert(p.xp == 35 && !p.failing);  // fix bonus
    judge(p, {}, 1, "'g++' is not recognized\n");
    assert(p.failing && p.hints[0].find("the build failed") == 0);
    for (auto& st : STAGES) {  // every design fits the screen
        istringstream in(device(art(st, st == "ghost" ? "dead" : "ok"), st));
        string l;
        for (int i = 0; getline(in, l); i++)
            if (i >= 3 && i < 3 + H) assert(l.size() == (size_t)W + 9);
    }
    cout << "selftest ok\n";
    return 0;
}

int main(int argc, char** argv) {
    vector<string> args(argv + 1, argv + argc);
    string sub = args.empty() ? "" : args[0];
    if (sub == "selftest") return selftest();
    if (sub == "designs") {
        for (auto& st : STAGES) cout << device(art(st, st == "ghost" ? "dead" : "happy"), st) << "\n";
        cout << "   moods:";
        for (auto& f : FACES) cout << "  " << f.first << " " << f.second.first << " " << f.second.second;
        cout << "\n";
        return 0;
    }
    Pet p;
    bool firstRun = !load(p);
    tick(p);
    if (sub == "check") {
        if (p.hp <= 0) {
            show(p);
            cout << "\n   Run tamagotchi with no arguments to hatch a new egg.\n";
            return 1;
        }
        string cmd, log;
        for (size_t i = 1; i < args.size(); i++)
            cmd += (i > 1 ? " " : "") + (args[i].find(' ') != string::npos ? "\"" + args[i] + "\"" : args[i]);
        int code = 0;
        if (!cmd.empty()) log = runBuild(cmd, code);
        else if (!isatty(fileno(stdin))) { ostringstream o; o << cin.rdbuf(); log = o.str(); }
        else { cout << USAGE; return 1; }
        auto said = judge(p, parse(log), code, log);
        show(p);
        cout << "\n";
        for (auto& s : said) cout << "   " << s << "\n";
        save(p);
        return p.failing ? 1 : 0;  // keep the build's pass/fail for scripts and VS Code
    }
    play(p, firstRun);
    save(p);
    return 0;
}

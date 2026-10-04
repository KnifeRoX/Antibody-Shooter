#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
#include <vector>
#include <GL/glut.h>

using namespace std;

const int WIDTH  = 800;
const int HEIGHT = 600;

//Game States & Menus
enum GameState { STATE_MENU, STATE_PLAYING, STATE_CUSTOMIZE, STATE_INSTRUCTIONS, STATE_SCORECARD, STATE_GAME_OVER };
GameState currentState = STATE_MENU;

int selectedMenuOption = 0;
const int TOTAL_MENU_OPTIONS = 5;
const char* menuOptions[TOTAL_MENU_OPTIONS] = {
    "1. New Game",
    "2. Antibody Customization",
    "3. Instructions",
    "4. Previous Scorecard",
    "5. Quit"
    "6. Antibody Shooter"
};

bool isFullScreen = true;
int viewX = 0;
int viewY = 0;
float viewScale = 1.0f;

//Customization options
struct ColorTheme {
    const char* name;
    float r, g, b;
};

ColorTheme themes[] = {
    {"Cyan Sentinel", 0.25f, 0.65f, 1.00f},
    {"Crimson Vindicator", 0.95f, 0.25f, 0.35f},
    {"Emerald Bio-Guard", 0.20f, 0.85f, 0.45f},
    {"Golden Core", 0.95f, 0.80f, 0.20f},
    {"Neon Purple", 0.70f, 0.30f, 0.95f}
};
int selectedTheme = 0;

// Scorecard Log
vector<int> scoreHistory;

//Game Types
enum VirusType { NORMAL, FAST, TANK, MITOSIS, SHOOTER };

struct Virus {
    float x, y;
    float speed;
    float radius;
    int   hp;
    int   type;
    float pulse;
    bool  alive;
    int   shootTimer;
};

struct Bullet {
    float x, y;
    float vx, vy;
    bool  isEnemy;
    bool  active;
};

struct PowerUp {
    float x, y;
    int   type;
    bool  alive;
};

struct Particle {
    float x, y, vx, vy;
    float r, g, b, alpha;
    int   life;
    int   maxLife;
    float size;
};

struct FloatingText {
    float x, y;
    char  text[32];
    float r, g, b, alpha;
    int   life;
};

struct CellBackground {
    float x, y, radius, speed, alpha;
};

struct Shockwave {
    float x, y, radius, maxRadius;
    bool active;
};

//Global State
vector<Virus>        viruses;
vector<Bullet>       bullets;
vector<PowerUp>      powerUps;
vector<Particle>     particles;
vector<FloatingText> floatTexts;
vector<CellBackground> bgCells;
Shockwave            shockwave = {0, 0, 0, 0, false};

float antibodyX = WIDTH / 2.0f;
float antibodyY = 80.0f;
const float antibodySpeed = 9.0f;
const float antibodyHalfWidth = 25.0f;
const float minAntibodyY = 40.0f;
const float maxAntibodyY = 260.0f;

int score = 0;
int level = 1;
int lives = 5;
bool gameOver = false;
bool paused = false;
bool hasShield = false;
int multiShotTimer = 0;
int fireCooldown = 0;
int shockwaveCharge = 100;
int spawnCounter = 0;
int spawnInterval = 80;
int powerUpCounter = 0;
const int MAX_VIRUSES_ON_SCREEN = 6;
float screenShake = 0.0f;

bool leftPressed = false, rightPressed = false, upPressed = false, downPressed = false, spacePressed = false;
int frameCount = 0;

//Helper Drawing Functions
float randf(float lo, float hi) { return lo + (hi - lo) * (rand() / (float)RAND_MAX); }

void drawCircle(float cx, float cy, float r, int segments = 24) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * M_PI * i / segments;
        glVertex2f(cx + r * cosf(angle), cy + r * sinf(angle));
    }
    glEnd();
}

void drawRing(float cx, float cy, float r, int segments = 28) {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; i++) {
        float angle = 2.0f * M_PI * i / segments;
        glVertex2f(cx + r * cosf(angle), cy + r * sinf(angle));
    }
    glEnd();
}

void drawText(float x, float y, const char* text, void* font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; c++) glutBitmapCharacter(font, *c);
}


void drawHeart(float cx, float cy, float size, float r, float g, float b) {
    glColor3f(r, g, b);
    float rad = size * 0.5f;
    drawCircle(cx - rad * 0.5f, cy + rad * 0.3f, rad * 0.62f, 16);
    drawCircle(cx + rad * 0.5f, cy + rad * 0.3f, rad * 0.62f, 16);
    glBegin(GL_TRIANGLES);
        glVertex2f(cx - rad * 1.15f, cy + rad * 0.28f);
        glVertex2f(cx + rad * 1.15f, cy + rad * 0.28f);
        glVertex2f(cx, cy - rad * 1.35f);
    glEnd();
}

#ifdef _WIN32

vector<unsigned char> musicBuffer;

void wavPush32(vector<unsigned char>& b, unsigned int v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF); b.push_back((v >> 24) & 0xFF);
}
void wavPush16(vector<unsigned char>& b, unsigned short v) {
    b.push_back(v & 0xFF); b.push_back((v >> 8) & 0xFF);
}
void wavPushStr(vector<unsigned char>& b, const char* s) {
    for (int i = 0; i < 4; i++) b.push_back(s[i]);
}


vector<unsigned char> buildBackgroundMusic() {
    const int sampleRate = 22050;
    float notes[] = { 261.63f, 329.63f, 392.00f, 493.88f, 392.00f, 329.63f };
    int   numNotes = 6;
    float noteDur = 0.32f;
    int samplesPerNote = (int)(sampleRate * noteDur);
    int totalSamples = samplesPerNote * numNotes;

    vector<short> pcm(totalSamples);
    int idx = 0;
    for (int n = 0; n < numNotes; n++) {
        float freq = notes[n];
        for (int i = 0; i < samplesPerNote; i++) {
            float t = i / (float)sampleRate;
            float env = 1.0f;
            int fade = sampleRate / 40;
            if (i < fade) env = i / (float)fade;
            else if (i > samplesPerNote - fade) env = (samplesPerNote - i) / (float)fade;
            float sample = sinf(2.0f * (float)M_PI * freq * t) * env * 5000.0f;
            pcm[idx++] = (short)sample;
        }
    }

    int dataSize = totalSamples * (int)sizeof(short);
    vector<unsigned char> wav;
    wavPushStr(wav, "RIFF");
    wavPush32(wav, 36 + dataSize);
    wavPushStr(wav, "WAVE");
    wavPushStr(wav, "fmt ");
    wavPush32(wav, 16);
    wavPush16(wav, 1);
    wavPush16(wav, 1);
    wavPush32(wav, sampleRate);
    wavPush32(wav, sampleRate * 2);
    wavPush16(wav, 2);
    wavPush16(wav, 16);
    wavPushStr(wav, "data");
    wavPush32(wav, dataSize);

    const unsigned char* raw = reinterpret_cast<const unsigned char*>(&pcm[0]);
    wav.insert(wav.end(), raw, raw + dataSize);
    return wav;
}

void initAudio() {
    musicBuffer = buildBackgroundMusic();
}

void startMusic() {
    if (musicBuffer.empty()) return;
    PlaySoundA(reinterpret_cast<LPCSTR>(&musicBuffer[0]), NULL, SND_MEMORY | SND_ASYNC | SND_LOOP);
}

void stopMusic() {
    PlaySoundA(NULL, NULL, 0);
}

struct BeepArgs { int freq; int duration; };

DWORD WINAPI beepThreadProc(LPVOID param) {
    BeepArgs* a = (BeepArgs*)param;
    Beep((DWORD)a->freq, (DWORD)a->duration);
    delete a;
    return 0;
}

void asyncBeep(int freq, int durationMs) {
    BeepArgs* a = new BeepArgs;
    a->freq = freq;
    a->duration = durationMs;
    HANDLE h = CreateThread(NULL, 0, beepThreadProc, a, 0, NULL);
    if (h) CloseHandle(h);
}

#else
void initAudio() {}
void startMusic() {}
void stopMusic() {}
void asyncBeep(int freq, int durationMs) {}
#endif

void sfxShoot()  { asyncBeep(1300, 35); }
void sfxImpact() { asyncBeep(550, 55); }

void spawnFloatingText(float x, float y, const char* txt, float r, float g, float b) {
    FloatingText ft;
    ft.x = x; ft.y = y;
    strncpy(ft.text, txt, sizeof(ft.text) - 1);
    ft.r = r; ft.g = g; ft.b = b; ft.alpha = 1.0f;
    ft.life = 40;
    floatTexts.push_back(ft);
}

void initBgCells() {
    bgCells.clear();
    for (int i = 0; i < 25; i++) {
        CellBackground c;
        c.x = randf(0, WIDTH);
        c.y = randf(0, HEIGHT);
        c.radius = randf(15.0f, 40.0f);
        c.speed = randf(0.3f, 1.0f);
        c.alpha = randf(0.03f, 0.08f);
        bgCells.push_back(c);
    }
}

void updateBgCells() {
    for (size_t i = 0; i < bgCells.size(); i++) {
        bgCells[i].y -= bgCells[i].speed;
        if (bgCells[i].y < -bgCells[i].radius) {
            bgCells[i].y = HEIGHT + bgCells[i].radius;
            bgCells[i].x = randf(0, WIDTH);
        }
    }
}

void drawBackground() {
    glBegin(GL_QUADS);
        glColor3f(0.02f, 0.01f, 0.05f);
        glVertex2f(0, 0);
        glVertex2f(WIDTH, 0);
        glColor3f(0.08f, 0.03f, 0.12f);
        glVertex2f(WIDTH, HEIGHT);
        glVertex2f(0, HEIGHT);
    glEnd();

    for (size_t i = 0; i < bgCells.size(); i++) {
        glColor4f(0.8f, 0.2f, 0.3f, bgCells[i].alpha);
        drawCircle(bgCells[i].x, bgCells[i].y, bgCells[i].radius, 16);
    }
}

void drawBar(float x1, float y1, float x2, float y2, float halfW) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len == 0.0f) return;

    float nx = -dy / len * halfW;
    float ny =  dx / len * halfW;

    glBegin(GL_QUADS);
        glVertex2f(x1 + nx, y1 + ny);
        glVertex2f(x2 + nx, y2 + ny);
        glVertex2f(x2 - nx, y2 - ny);
        glVertex2f(x1 - nx, y1 - ny);
    glEnd();
}

void drawAntibodyShape(float px, float py, float grow) {
    float hingeY = py + 6;

    drawBar(px, py - 14, px, hingeY, 7.0f + grow);
    drawCircle(px, hingeY, 7.5f + grow, 16);

    drawBar(px, hingeY, px - 17, py + 34, 5.0f + grow);
    drawBar(px, hingeY, px + 17, py + 34, 5.0f + grow);

    drawBar(px - 14.75f, py + 14.3f, px - 24.1f, py + 29.7f, 2.8f + grow);
    drawBar(px + 14.75f, py + 14.3f, px + 24.1f, py + 29.7f, 2.8f + grow);

    drawCircle(px - 17, py + 35, 5.0f + grow, 14);
    drawCircle(px + 17, py + 35, 5.0f + grow, 14);
    drawCircle(px - 24.1f, py + 30.0f, 3.2f + grow, 10);
    drawCircle(px + 24.1f, py + 30.0f, 3.2f + grow, 10);
}

void drawAntibodyAt(float px, float py) {
    float bob = sinf(frameCount * 0.15f) * 2.0f;
    ColorTheme current = themes[selectedTheme];
    py = py + bob;

    float lightR = current.r * 0.5f + 0.5f;
    float lightG = current.g * 0.5f + 0.5f;
    float lightB = current.b * 0.5f + 0.5f;

    glColor3f(1.0f, 0.5f, 0.1f);
    glBegin(GL_TRIANGLES);
        glVertex2f(px - 6, py - 14);
        glVertex2f(px + 6, py - 14);
        glVertex2f(px, py - 30 + randf(0, 6));
    glEnd();

    glColor3f(0.02f, 0.02f, 0.06f);
    drawAntibodyShape(px, py, 1.8f);

    glColor3f(current.r, current.g, current.b);
    drawBar(px, py - 14, px, py + 6, 7.0f);
    drawBar(px, py + 6, px - 17, py + 34, 5.0f);
    drawBar(px, py + 6, px + 17, py + 34, 5.0f);

    glColor3f(current.r * 0.55f, current.g * 0.55f, current.b * 0.55f);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
        glVertex2f(px, py - 13);
        glVertex2f(px, py + 3);
    glEnd();
    glLineWidth(1.0f);

    glColor3f(lightR, lightG, lightB);
    drawBar(px - 14.75f, py + 14.3f, px - 24.1f, py + 29.7f, 2.8f);
    drawBar(px + 14.75f, py + 14.3f, px + 24.1f, py + 29.7f, 2.8f);

    glColor3f(0.95f, 0.95f, 1.0f);
    drawCircle(px, py + 6, 4.0f, 12);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
        glVertex2f(px - 13.6f, py + 20.7f); glVertex2f(px - 15.8f, py + 19.4f);
        glVertex2f(px + 13.6f, py + 20.7f); glVertex2f(px + 15.8f, py + 19.4f);
    glEnd();
    glLineWidth(1.0f);

    float glow = 0.35f + 0.25f * sinf(frameCount * 0.2f);
    glColor4f(1.0f, 0.95f, 0.4f, glow);
    drawCircle(px - 17, py + 35, 8.5f, 14);
    drawCircle(px + 17, py + 35, 8.5f, 14);

    glColor3f(1.0f, 0.95f, 0.55f);
    drawCircle(px - 17, py + 35, 5.0f, 14);
    drawCircle(px + 17, py + 35, 5.0f, 14);
    glColor3f(lightR, lightG, lightB);
    drawCircle(px - 24.1f, py + 30.0f, 3.2f, 10);
    drawCircle(px + 24.1f, py + 30.0f, 3.2f, 10);

    py = py - bob;

    if (hasShield && currentState == STATE_PLAYING) {
        glColor4f(0.3f, 1.0f, 0.5f, 0.3f);
        drawCircle(px, py + 10 + bob, 42);
        glColor3f(0.3f, 1.0f, 0.5f);
        glLineWidth(2.5f);
        drawRing(px, py + 10 + bob, 42);
        glLineWidth(1.0f);
    }
}

void virusColor(int type) {
    switch (type) {
        case FAST:    glColor3f(1.0f, 0.55f, 0.1f); break;
        case TANK:    glColor3f(0.6f, 0.2f, 0.8f); break;
        case MITOSIS: glColor3f(0.2f, 0.9f, 0.3f); break;
        case SHOOTER: glColor3f(0.9f, 0.8f, 0.1f); break;
        default:      glColor3f(0.9f, 0.2f, 0.2f); break;
    }
}

void drawVirus(const Virus& v) {
    float pulse = 1.0f + 0.08f * sinf(v.pulse);
    float r = v.radius * pulse;

    virusColor(v.type);
    glColor4f(0.9f, 0.2f, 0.2f, 0.15f);
    drawCircle(v.x, v.y, r * 1.4f);

    virusColor(v.type);
    drawCircle(v.x, v.y, r);

    glColor3f(0.08f, 0.02f, 0.02f);
    drawCircle(v.x, v.y, r * 0.4f, 14);

    virusColor(v.type);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    int spikes = (v.type == TANK) ? 12 : 8;
    for (int i = 0; i < spikes; i++) {
        float angle = 2.0f * M_PI * i / spikes + v.pulse * 0.05f;
        float x1 = v.x + r * cosf(angle);
        float y1 = v.y + r * sinf(angle);
        float x2 = v.x + (r + 7) * cosf(angle);
        float y2 = v.y + (r + 7) * sinf(angle);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
    }
    glEnd();
    glLineWidth(1.0f);

    if (v.type == TANK) {
        glColor3f(1, 1, 1);
        for (int i = 0; i < v.hp; i++) {
            drawCircle(v.x - r + 6 + i * 8, v.y + r + 12, 2.5f, 8);
        }
    }
}

void drawBullet(const Bullet& b) {
    if (b.isEnemy) {
        glColor4f(0.9f, 0.8f, 0.1f, 0.3f);
        drawCircle(b.x, b.y, 8.0f, 10);
        glColor3f(1.0f, 0.3f, 0.1f);
        drawCircle(b.x, b.y, 4.0f, 10);
    } else {
        glColor4f(0.3f, 0.8f, 1.0f, 0.3f);
        drawCircle(b.x, b.y, 9.0f, 12);
        glColor3f(1.0f, 0.95f, 0.4f);
        drawCircle(b.x, b.y, 4.5f, 12);
    }
}

void drawPowerUp(const PowerUp& p) {
    float pulse = 1.0f + 0.15f * sinf(frameCount * 0.2f);
    if (p.type == 0) {
        glColor3f(0.3f, 1.0f, 0.5f);
        drawCircle(p.x, p.y, 12 * pulse, 16);
        glColor3f(1, 1, 1);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
            glVertex2f(p.x - 5, p.y); glVertex2f(p.x + 5, p.y);
            glVertex2f(p.x, p.y - 5); glVertex2f(p.x, p.y + 5);
        glEnd();
        glLineWidth(1.0f);
    } else {
        glColor3f(0.4f, 0.8f, 1.0f);
        drawCircle(p.x, p.y, 12 * pulse, 16);
        glColor3f(1, 1, 1);
        drawText(p.x - 4, p.y - 5, "3", GLUT_BITMAP_HELVETICA_12);
    }
}

void drawParticles() {
    for (size_t i = 0; i < particles.size(); i++) {
        float a = (particles[i].life / (float)particles[i].maxLife) * particles[i].alpha;
        glColor4f(particles[i].r, particles[i].g, particles[i].b, a);
        drawCircle(particles[i].x, particles[i].y, particles[i].size, 8);
    }
}

void drawShockwave() {
    if (!shockwave.active) return;
    float alpha = 1.0f - (shockwave.radius / shockwave.maxRadius);
    glColor4f(0.2f, 0.8f, 1.0f, alpha * 0.5f);
    drawCircle(shockwave.x, shockwave.y, shockwave.radius, 36);
    glColor4f(0.8f, 1.0f, 1.0f, alpha);
    glLineWidth(4.0f);
    drawRing(shockwave.x, shockwave.y, shockwave.radius, 36);
    glLineWidth(1.0f);
}

//Gameplay Logic
void resetGame() {
    viruses.clear();
    bullets.clear();
    powerUps.clear();
    particles.clear();
    floatTexts.clear();
    antibodyX = WIDTH / 2.0f;
    antibodyY = 80.0f;
    score = 0;
    level = 1;
    lives = 5;
    gameOver = false;
    hasShield = false;
    multiShotTimer = 0;
    fireCooldown = 0;
    spawnCounter = 0;
    spawnInterval = 80;
    powerUpCounter = 0;
    shockwaveCharge = 100;
    shockwave.active = false;
    paused = false;
    currentState = STATE_PLAYING;
    startMusic();
}

void spawnExplosion(float x, float y, int type) {
    float r = 0.9f, g = 0.2f, b = 0.2f;
    if (type == FAST) { r = 1.0f; g = 0.55f; b = 0.1f; }
    else if (type == TANK) { r = 0.55f; g = 0.15f; b = 0.75f; }
    else if (type == MITOSIS) { r = 0.2f; g = 0.9f; b = 0.3f; }
    else if (type == SHOOTER) { r = 0.9f; g = 0.8f; b = 0.1f; }

    for (int i = 0; i < 18; i++) {
        Particle p;
        p.x = x; p.y = y;
        float angle = randf(0, 2 * M_PI);
        float speed = randf(2.0f, 5.5f);
        p.vx = cosf(angle) * speed;
        p.vy = sinf(angle) * speed;
        p.r = r; p.g = g; p.b = b; p.alpha = 1.0f;
        p.life = p.maxLife = rand() % 15 + 15;
        p.size = randf(2.0f, 5.0f);
        particles.push_back(p);
    }
}

void spawnThrusterParticle(float x, float y) {
    Particle p;
    p.x = x + randf(-4.0f, 4.0f);
    p.y = y;
    p.vx = randf(-0.5f, 0.5f);
    p.vy = randf(-3.0f, -1.5f);
    p.r = themes[selectedTheme].r;
    p.g = themes[selectedTheme].g;
    p.b = themes[selectedTheme].b;
    p.alpha = 0.8f;
    p.life = p.maxLife = 12;
    p.size = randf(2.0f, 4.0f);
    particles.push_back(p);
}

void triggerShockwave() {
    if (shockwaveCharge >= 100 && !shockwave.active) {
        shockwave.x = antibodyX;
        shockwave.y = antibodyY;
        shockwave.radius = 10.0f;
        shockwave.maxRadius = 450.0f;
        shockwave.active = true;
        shockwaveCharge = 0;
        screenShake = 12.0f;
    }
}

void fireBullet() {
    if (fireCooldown > 0) return;
    fireCooldown = 9;
    sfxShoot();

    if (multiShotTimer > 0) {
        float spreads[3] = { -2.5f, 0.0f, 2.5f };
        for (int i = 0; i < 3; i++) {
            Bullet b;
            b.x = antibodyX; b.y = antibodyY + 35; b.vx = spreads[i]; b.vy = 12.0f;
            b.isEnemy = false; b.active = true;
            bullets.push_back(b);
        }
    } else {
        Bullet b;
        b.x = antibodyX; b.y = antibodyY + 35; b.vx = 0.0f; b.vy = 12.0f;
        b.isEnemy = false; b.active = true;
        bullets.push_back(b);
    }
}

void spawnVirus(int overrideType = -1, float spawnX = -1, float spawnY = -1) {
    Virus v;
    v.x = (spawnX < 0) ? (40 + rand() % (WIDTH - 80)) : spawnX;
    v.y = (spawnY < 0) ? (HEIGHT + 20) : spawnY;
    v.pulse = randf(0, 6.28f);
    v.shootTimer = rand() % 60 + 40;

    int type = overrideType;
    if (type == -1) {
        int roll = rand() % 100;
        if (roll < 15 && level >= 2) type = TANK;
        else if (roll < 35 && level >= 2) type = MITOSIS;
        else if (roll < 55) type = SHOOTER;
        else if (roll < 75) type = FAST;
        else type = NORMAL;
    }

    v.type = type;
    v.alive = true;

    switch (type) {
        case TANK:    v.radius = 28; v.hp = 4; v.speed = 0.5f + level * 0.06f; break;
        case FAST:    v.radius = 11; v.hp = 1; v.speed = 1.7f + level * 0.18f; break;
        case MITOSIS: v.radius = 20; v.hp = 2; v.speed = 0.8f + level * 0.09f; break;
        case SHOOTER: v.radius = 16; v.hp = 1; v.speed = 0.7f + level * 0.09f; break;
        default:      v.radius = 16; v.hp = 1; v.speed = 1.0f + level * 0.12f; break;
    }
    viruses.push_back(v);
}

void checkCollisions() {
    if (shockwave.active) {
        for (size_t i = 0; i < viruses.size(); i++) {
            if (!viruses[i].alive) continue;
            float dx = viruses[i].x - shockwave.x;
            float dy = viruses[i].y - shockwave.y;
            if (sqrtf(dx * dx + dy * dy) <= shockwave.radius + viruses[i].radius) {
                viruses[i].alive = false;
                spawnExplosion(viruses[i].x, viruses[i].y, viruses[i].type);
                score += 10;
            }
        }
        for (size_t i = 0; i < bullets.size(); i++) {
            if (bullets[i].isEnemy) {
                float dx = bullets[i].x - shockwave.x;
                float dy = bullets[i].y - shockwave.y;
                if (sqrtf(dx * dx + dy * dy) <= shockwave.radius) bullets[i].active = false;
            }
        }
    }

    for (size_t i = 0; i < bullets.size(); i++) {
        if (!bullets[i].active || bullets[i].isEnemy) continue;
        for (size_t j = 0; j < viruses.size(); j++) {
            if (!viruses[j].alive) continue;
            float dx = bullets[i].x - viruses[j].x;
            float dy = bullets[i].y - viruses[j].y;
            if (sqrtf(dx * dx + dy * dy) < viruses[j].radius + 5.0f) {
                bullets[i].active = false;
                viruses[j].hp--;
                sfxImpact();

                if (viruses[j].hp <= 0) {
                    viruses[j].alive = false;
                    spawnExplosion(viruses[j].x, viruses[j].y, viruses[j].type);

                    int pts = (viruses[j].type == TANK) ? 30 : 15;
                    score += pts;
                    if (shockwaveCharge < 100) shockwaveCharge = min(100, shockwaveCharge + 8);

                    char buf[16];
                    sprintf(buf, "+%d", pts);
                    spawnFloatingText(viruses[j].x, viruses[j].y, buf, 0.3f, 1.0f, 0.4f);

                    if (viruses[j].type == MITOSIS) {
                        spawnVirus(FAST, viruses[j].x - 12, viruses[j].y);
                        spawnVirus(FAST, viruses[j].x + 12, viruses[j].y);
                    }

                    int newLevel = 1 + score / 120;
                    if (newLevel != level) {
                        level = newLevel;
                        if (spawnInterval > 35) spawnInterval -= 3;
                        spawnFloatingText(WIDTH / 2 - 40, HEIGHT / 2, "LEVEL UP!", 1.0f, 0.8f, 0.2f);
                    }
                }
                break;
            }
        }
    }

    for (size_t i = 0; i < bullets.size(); i++) {
        if (!bullets[i].active || !bullets[i].isEnemy) continue;
        float dx = bullets[i].x - antibodyX;
        float dy = bullets[i].y - (antibodyY + 10);
        if (sqrtf(dx * dx + dy * dy) < 22.0f) {
            bullets[i].active = false;
            screenShake = 8.0f;
            if (hasShield) {
                hasShield = false;
            } else {
                lives--;
                if (lives <= 0) {
                    gameOver = true;
                    stopMusic();
                    scoreHistory.push_back(score);
                }
            }
        }
    }

    for (size_t i = 0; i < powerUps.size(); i++) {
        if (!powerUps[i].alive) continue;
        float dx = powerUps[i].x - antibodyX;
        float dy = powerUps[i].y - antibodyY;
        if (sqrtf(dx * dx + dy * dy) < 30.0f) {
            powerUps[i].alive = false;
            if (powerUps[i].type == 0) {
                hasShield = true;
                spawnFloatingText(antibodyX, antibodyY + 30, "SHIELD!", 0.3f, 1.0f, 0.5f);
            } else {
                multiShotTimer = 350;
                spawnFloatingText(antibodyX, antibodyY + 30, "MULTI-SHOT!", 0.4f, 0.8f, 1.0f);
            }
        }
    }
}

void updateGame() {
    frameCount++;
    updateBgCells();

    if (screenShake > 0.0f) screenShake -= 0.6f;
    if (screenShake < 0.0f) screenShake = 0.0f;

    if (currentState != STATE_PLAYING || gameOver) return;
    if (paused) return;

    bool moving = false;
    if (leftPressed)  { antibodyX -= antibodySpeed; moving = true; }
    if (rightPressed) { antibodyX += antibodySpeed; moving = true; }
    if (upPressed)    { antibodyY += antibodySpeed; moving = true; }
    if (downPressed)  { antibodyY -= antibodySpeed; moving = true; }

    if (moving || frameCount % 2 == 0) spawnThrusterParticle(antibodyX, antibodyY - 15);
    if (spacePressed) fireBullet();

    if (antibodyX < antibodyHalfWidth) antibodyX = antibodyHalfWidth;
    if (antibodyX > WIDTH - antibodyHalfWidth) antibodyX = WIDTH - antibodyHalfWidth;
    if (antibodyY < minAntibodyY) antibodyY = minAntibodyY;
    if (antibodyY > maxAntibodyY) antibodyY = maxAntibodyY;

    if (fireCooldown > 0) fireCooldown--;
    if (multiShotTimer > 0) multiShotTimer--;

    if (shockwave.active) {
        shockwave.radius += 18.0f;
        if (shockwave.radius > shockwave.maxRadius) shockwave.active = false;
    }

    spawnCounter++;
    if (spawnCounter >= spawnInterval && (int)viruses.size() < MAX_VIRUSES_ON_SCREEN) {
        spawnCounter = 0;
        spawnVirus();
    }

    powerUpCounter++;
    if (powerUpCounter >= 420) {
        powerUpCounter = 0;
        if (rand() % 100 < 65) {
            PowerUp p;
            p.x = 40 + rand() % (WIDTH - 80);
            p.y = HEIGHT + 20;
            p.type = rand() % 2;
            p.alive = true;
            powerUps.push_back(p);
        }
    }

    for (size_t i = 0; i < bullets.size(); i++) {
        if (!bullets[i].active) continue;
        bullets[i].x += bullets[i].vx;
        bullets[i].y += bullets[i].vy;
        if (bullets[i].y > HEIGHT + 20 || bullets[i].y < -20) bullets[i].active = false;
    }

    for (size_t i = 0; i < viruses.size(); i++) {
        if (!viruses[i].alive) continue;
        viruses[i].y -= viruses[i].speed;
        viruses[i].pulse += 0.1f;

        if (viruses[i].type == SHOOTER) {
            viruses[i].shootTimer--;
            if (viruses[i].shootTimer <= 0) {
                viruses[i].shootTimer = rand() % 70 + 50;
                Bullet b;
                b.x = viruses[i].x; b.y = viruses[i].y - 10;
                b.vx = 0.0f; b.vy = -5.0f;
                b.isEnemy = true; b.active = true;
                bullets.push_back(b);
            }
        }

        if (viruses[i].y - viruses[i].radius <= antibodyY - 20) {
            viruses[i].alive = false;
            spawnExplosion(viruses[i].x, viruses[i].y, viruses[i].type);
            screenShake = 10.0f;
            if (hasShield) {
                hasShield = false;
            } else {
                lives--;
                if (lives <= 0) {
                    gameOver = true;
                    stopMusic();
                    scoreHistory.push_back(score);
                }
            }
        }
    }

    for (size_t i = 0; i < powerUps.size(); i++) {
        if (!powerUps[i].alive) continue;
        powerUps[i].y -= 2.0f;
        if (powerUps[i].y < -20) powerUps[i].alive = false;
    }

    for (size_t i = 0; i < particles.size(); i++) {
        particles[i].x += particles[i].vx;
        particles[i].y += particles[i].vy;
        particles[i].life--;
    }

    for (size_t i = 0; i < floatTexts.size(); i++) {
        floatTexts[i].y += 0.8f;
        floatTexts[i].life--;
        floatTexts[i].alpha = floatTexts[i].life / 40.0f;
    }

    checkCollisions();

    for (size_t i = 0; i < bullets.size(); )   { if (!bullets[i].active) bullets.erase(bullets.begin() + i); else i++; }
    for (size_t i = 0; i < viruses.size(); )   { if (!viruses[i].alive)  viruses.erase(viruses.begin() + i); else i++; }
    for (size_t i = 0; i < powerUps.size(); )  { if (!powerUps[i].alive) powerUps.erase(powerUps.begin() + i); else i++; }
    for (size_t i = 0; i < particles.size(); ) { if (particles[i].life <= 0) particles.erase(particles.begin() + i); else i++; }
    for (size_t i = 0; i < floatTexts.size(); ){ if (floatTexts[i].life <= 0) floatTexts.erase(floatTexts.begin() + i); else i++; }
}

//Render Screens
float strokeWidthSpaced(const char* text, float spacing) {
    float total = 0.0f;
    int count = 0;
    for (const char* c = text; *c != '\0'; c++) {
        if (*c == ' ') total += 55.0f;
        else total += glutStrokeWidth(GLUT_STROKE_ROMAN, *c);
        count++;
    }
    if (count > 1) total += spacing * (count - 1);
    return total;
}

void drawStrokeText(float x, float y, float scale, const char* text, float spacing) {
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(scale, scale, 1);
    for (const char* c = text; *c != '\0'; c++) {
        if (*c == ' ') glTranslatef(55.0f, 0, 0);
        else glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
        glTranslatef(spacing, 0, 0);
    }
    glPopMatrix();
}

void drawStrokeTextBold(float x, float y, float scale, const char* text, float spacing, float thick, float lineW) {
    glLineWidth(lineW * viewScale);
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            drawStrokeText(x + i * thick, y + j * thick, scale, text, spacing);
        }
    }
    glLineWidth(1.0f);
}

void drawLabel(float x, float y, float size, const char* text) {
    glLineWidth(1.6f * viewScale);
    drawStrokeText(x, y, size / 119.0f, text, 4.0f);
    glLineWidth(1.0f);
}

float labelWidth(float size, const char* text) {
    return strokeWidthSpaced(text, 4.0f) * size / 119.0f;
}

float headingWidth(float size, const char* text) {
    return strokeWidthSpaced(text, 16.0f) * size / 119.0f;
}

void drawLabelCentered(float y, float size, const char* text) {
    drawLabel(WIDTH / 2 - labelWidth(size, text) / 2, y, size, text);
}

void drawHeading(float x, float y, float size, const char* text) {
    drawStrokeTextBold(x, y, size / 119.0f, text, 16.0f, 0.3f, 2.0f);
}

void setScissorLogical(float x, float y, float w, float h) {
    glScissor(viewX + (int)(x * viewScale), viewY + (int)(y * viewScale),
              (int)(w * viewScale), (int)(h * viewScale));
}

void drawBitmapSpaced(float x, float y, const char* text, void* font, float spacing) {
    float cx = x;
    for (const char* c = text; *c != '\0'; c++) {
        glRasterPos2f(cx, y);
        glutBitmapCharacter(font, *c);
        cx += glutBitmapWidth(font, *c) + spacing;
    }
}

float bitmapWidthSpaced(const char* text, void* font, float spacing) {
    float total = 0.0f;
    for (const char* c = text; *c != '\0'; c++) {
        total += glutBitmapWidth(font, *c) + spacing;
    }
    return total - spacing;
}

void drawTitleLogo() {
    const char* word1 = "ANTIBODY";
    float space1 = 12.0f;
    float width1 = strokeWidthSpaced(word1, space1);
    float scale1 = 520.0f / width1;
    float x1 = WIDTH / 2 - width1 * scale1 / 2;
    float y1 = 468.0f;
    float capH = 119.0f * scale1;

    float pulse = 0.5f + 0.5f * sinf(frameCount * 0.05f);

    glColor4f(0.8f, 0.0f, 0.0f, 0.10f + 0.08f * pulse);
    drawStrokeTextBold(x1, y1, scale1, word1, space1, 9.0f, 12.0f);

    glColor4f(0.0f, 0.0f, 0.0f, 0.85f);
    drawStrokeTextBold(x1 + 6, y1 - 6, scale1, word1, space1, 5.0f, 7.0f);

    glColor3f(0.0f, 0.0f, 0.0f);
    drawStrokeTextBold(x1, y1, scale1, word1, space1, 5.0f, 7.0f);

    glColor3f(0.55f, 0.0f, 0.0f);
    drawStrokeTextBold(x1, y1, scale1, word1, space1, 3.0f, 6.0f);

    glEnable(GL_SCISSOR_TEST);
    setScissorLogical(0, y1 + capH * 0.45f, WIDTH, capH);
    glColor3f(0.85f, 0.06f, 0.04f);
    drawStrokeTextBold(x1, y1, scale1, word1, space1, 3.0f, 6.0f);

    setScissorLogical(0, y1 + capH * 0.80f, WIDTH, capH);
    glColor3f(1.0f, 0.35f, 0.25f);
    drawStrokeTextBold(x1, y1, scale1, word1, space1, 3.0f, 6.0f);
    glDisable(GL_SCISSOR_TEST);

    const char* word2 = "SHOOTER";
    float space2 = 50.0f;
    float width2 = strokeWidthSpaced(word2, space2);
    float scale2 = 280.0f / width2;
    float x2 = WIDTH / 2 - width2 * scale2 / 2;
    float y2 = 402.0f;
    float capH2 = 119.0f * scale2;

    glColor3f(0.0f, 0.0f, 0.0f);
    drawStrokeTextBold(x2, y2, scale2, word2, space2, 3.2f, 5.0f);
    glColor3f(0.93f, 0.93f, 0.93f);
    drawStrokeTextBold(x2, y2, scale2, word2, space2, 1.4f, 3.5f);

    float barY = y2 + capH2 / 2;
    glColor3f(0.0f, 0.0f, 0.0f);
    glRectf(x2 - 120, barY - 3, x2 - 18, barY + 3);
    glRectf(x2 + width2 * scale2 + 18, barY - 3, x2 + width2 * scale2 + 120, barY + 3);
    glColor3f(0.75f, 0.03f, 0.03f);
    glRectf(x2 - 118, barY - 1.5f, x2 - 20, barY + 1.5f);
    glRectf(x2 + width2 * scale2 + 20, barY - 1.5f, x2 + width2 * scale2 + 118, barY + 1.5f);

    const char* tagline = "SURVIVE.   DEFEND.   DESTROY.";
    float tagW = bitmapWidthSpaced(tagline, GLUT_BITMAP_HELVETICA_12, 3.0f);
    float tagX = WIDTH / 2 - tagW / 2;
    glColor3f(0.0f, 0.0f, 0.0f);
    drawBitmapSpaced(tagX + 1, 377, tagline, GLUT_BITMAP_HELVETICA_12, 3.0f);
    glColor3f(0.85f, 0.78f, 0.78f);
    drawBitmapSpaced(tagX, 378, tagline, GLUT_BITMAP_HELVETICA_12, 3.0f);
}

void drawMenuScreen() {
    drawBackground();

    drawTitleLogo();

    float boxTop = 362.0f;
    float boxBottom = 116.0f;

    glColor4f(0.05f, 0.08f, 0.15f, 0.85f);
    glRectf(WIDTH / 2 - 180, boxBottom, WIDTH / 2 + 180, boxTop);
    glColor3f(0.2f, 0.6f, 0.9f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(WIDTH / 2 - 180, boxBottom);
        glVertex2f(WIDTH / 2 + 180, boxBottom);
        glVertex2f(WIDTH / 2 + 180, boxTop);
        glVertex2f(WIDTH / 2 - 180, boxTop);
    glEnd();
    glLineWidth(1.0f);

    for (int i = 0; i < TOTAL_MENU_OPTIONS; i++) {
        float itemY = 322 - i * 43;
        if (i == selectedMenuOption) {
            glColor3f(1.0f, 0.85f, 0.2f);
            drawText(WIDTH / 2 - 140, itemY, ">", GLUT_BITMAP_HELVETICA_18);
            drawText(WIDTH / 2 - 120, itemY, menuOptions[i], GLUT_BITMAP_HELVETICA_18);
        } else {
            glColor3f(0.7f, 0.7f, 0.8f);
            drawText(WIDTH / 2 - 120, itemY, menuOptions[i], GLUT_BITMAP_HELVETICA_18);
        }
    }

    glColor3f(0.6f, 0.7f, 0.8f);
    drawLabelCentered(60, 11, "Use UP / DOWN keys & ENTER to select");
}

void drawCustomizationScreen() {
    drawBackground();

    glColor3f(0.3f, 0.8f, 1.0f);
    drawText(WIDTH / 2 - 140, HEIGHT - 100, "ANTIBODY CUSTOMIZATION", GLUT_BITMAP_TIMES_ROMAN_24);

    drawAntibodyAt(WIDTH / 2, HEIGHT / 2 + 30);

    char buf[64];
    sprintf(buf, "Theme: < %s >", themes[selectedTheme].name);
    glColor3f(1.0f, 0.85f, 0.2f);
    drawText(WIDTH / 2 - 100, HEIGHT / 2 - 80, buf, GLUT_BITMAP_HELVETICA_18);

    glColor3f(0.7f, 0.7f, 0.8f);
    drawText(WIDTH / 2 - 130, HEIGHT / 2 - 120, "Use LEFT / RIGHT arrows to switch colors", GLUT_BITMAP_HELVETICA_12);
    drawText(WIDTH / 2 - 80, HEIGHT / 2 - 150, "Press ESC to return to Menu", GLUT_BITMAP_HELVETICA_12);
}

void drawInstructionLine(float x, float y, const char* text) {
    glColor3f(0.85f, 0.87f, 0.93f);
    drawLabel(x, y, 12, text);
}

void drawControlRow(float y, const char* key, const char* action) {
    glColor3f(0.3f, 0.8f, 1.0f);
    drawLabel(75, y, 12, key);
    glColor3f(0.85f, 0.87f, 0.93f);
    drawLabel(185, y, 12, action);
}

void drawInstructionsScreen() {
    drawBackground();

    float titleW = strokeWidthSpaced("INSTRUCTIONS", 22.0f) * 26 / 119.0f;
    glColor3f(0.0f, 0.0f, 0.0f);
    drawStrokeTextBold(WIDTH / 2 - titleW / 2, 537, 26 / 119.0f, "INSTRUCTIONS", 22.0f, 1.8f, 3.0f);
    glColor3f(0.3f, 0.8f, 1.0f);
    drawStrokeTextBold(WIDTH / 2 - titleW / 2, 537, 26 / 119.0f, "INSTRUCTIONS", 22.0f, 0.5f, 2.5f);

    glColor4f(0.05f, 0.08f, 0.15f, 0.88f);
    glRectf(40, 95, WIDTH - 40, HEIGHT - 80);
    glColor3f(0.2f, 0.6f, 0.9f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(40, 95);
        glVertex2f(WIDTH - 40, 95);
        glVertex2f(WIDTH - 40, HEIGHT - 80);
        glVertex2f(40, HEIGHT - 80);
    glEnd();
    glBegin(GL_LINES);
        glVertex2f(WIDTH / 2 + 5, 150);
        glVertex2f(WIDTH / 2 + 5, HEIGHT - 100);
    glEnd();
    glLineWidth(1.0f);

    glColor3f(1.0f, 0.85f, 0.2f);
    drawHeading(60, 488, 16, "CONTROLS");
    drawControlRow(460, "ARROWS", "Move the antibody");
    drawControlRow(438, "SPACE", "Shoot (hold to rapid fire)");
    drawControlRow(416, "E / SHIFT", "Shockwave (bar full)");
    drawControlRow(394, "P", "Pause / resume");
    drawControlRow(372, "R", "Restart the run");
    drawControlRow(350, "ESC", "Back to main menu");
    drawControlRow(328, "F11", "Toggle full screen");

    glColor3f(1.0f, 0.85f, 0.2f);
    drawHeading(60, 294, 16, "OBJECTIVE");
    drawInstructionLine(60, 268, "Viruses drift down toward you.");
    drawInstructionLine(60, 249, "Shoot them before they reach you.");
    drawInstructionLine(60, 230, "+15 per kill, +30 for a Tank virus.");
    drawInstructionLine(60, 211, "Every 120 points = LEVEL UP.");
    drawInstructionLine(60, 192, "Green Mitosis viruses split in two.");
    drawInstructionLine(60, 173, "Yellow Shooters fire back - dodge!");
    drawInstructionLine(60, 154, "Purple Tanks take 4 hits.");

    glColor3f(1.0f, 0.85f, 0.2f);
    drawHeading(425, 488, 16, "POWER-UPS");

    PowerUp shieldIcon;
    shieldIcon.x = 440; shieldIcon.y = 460; shieldIcon.type = 0; shieldIcon.alive = true;
    drawPowerUp(shieldIcon);
    glColor3f(0.3f, 1.0f, 0.5f);
    drawHeading(465, 462, 12, "SHIELD");
    drawInstructionLine(465, 443, "Blocks the next hit.");

    PowerUp multiIcon;
    multiIcon.x = 440; multiIcon.y = 417; multiIcon.type = 1; multiIcon.alive = true;
    drawPowerUp(multiIcon);
    glColor3f(0.4f, 0.8f, 1.0f);
    drawHeading(465, 419, 12, "MULTI-SHOT");
    drawInstructionLine(465, 400, "3-way spread for ~6 sec.");

    glColor3f(0.2f, 0.8f, 1.0f);
    glRectf(428, 370, 452, 382);
    drawHeading(465, 376, 12, "SHOCKWAVE");
    drawInstructionLine(465, 357, "Clears viruses + enemy shots.");
    drawInstructionLine(465, 338, "+8% charge per kill.");

    glColor3f(1.0f, 0.85f, 0.2f);
    drawHeading(425, 294, 16, "HEALTH (HEARTS)");
    for (int i = 0; i < 5; i++) {
        drawHeart(440 + i * 22, 272, 15.0f, 0.95f, 0.2f, 0.4f);
    }
    drawInstructionLine(425, 244, "You start with 5 hearts.");
    drawInstructionLine(425, 225, "Lose 1 if an enemy shot hits you");
    drawInstructionLine(425, 206, "or a virus reaches your line.");
    drawInstructionLine(425, 187, "A Shield takes the hit instead.");
    drawInstructionLine(425, 168, "0 hearts = GAME OVER.");
    drawInstructionLine(425, 149, "Score saved to the Scorecard.");

    float pulse = 0.75f + 0.25f * sinf(frameCount * 0.12f);
    glColor4f(0.15f, 0.12f, 0.02f, 0.9f);
    glRectf(WIDTH / 2 - 60, 106, WIDTH / 2 + 60, 138);
    glColor3f(1.0f * pulse, 0.85f * pulse, 0.2f * pulse);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(WIDTH / 2 - 60, 106);
        glVertex2f(WIDTH / 2 + 60, 106);
        glVertex2f(WIDTH / 2 + 60, 138);
        glVertex2f(WIDTH / 2 - 60, 138);
    glEnd();
    glLineWidth(1.0f);
    glColor3f(1.0f, 0.85f, 0.2f);
    float backW = headingWidth(15, "> BACK");
    drawHeading(WIDTH / 2 - backW / 2, 115, 15, "> BACK");

    glColor3f(0.6f, 0.7f, 0.8f);
    drawLabelCentered(60, 11, "Press ENTER or ESC to go back");
}

void drawScorecardScreen() {
    drawBackground();

    glColor3f(0.3f, 0.8f, 1.0f);
    drawText(WIDTH / 2 - 120, HEIGHT - 100, "PREVIOUS SCORECARD", GLUT_BITMAP_TIMES_ROMAN_24);

    glColor4f(0.05f, 0.08f, 0.15f, 0.85f);
    glRectf(WIDTH / 2 - 180, 120, WIDTH / 2 + 180, HEIGHT - 140);
    glColor3f(0.2f, 0.6f, 0.9f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(WIDTH / 2 - 180, 120);
        glVertex2f(WIDTH / 2 + 180, 120);
        glVertex2f(WIDTH / 2 + 180, HEIGHT - 140);
        glVertex2f(WIDTH / 2 - 180, HEIGHT - 140);
    glEnd();
    glLineWidth(1.0f);

    if (scoreHistory.empty()) {
        glColor3f(0.7f, 0.7f, 0.7f);
        drawText(WIDTH / 2 - 80, HEIGHT / 2, "No scores recorded yet!", GLUT_BITMAP_HELVETICA_18);
    } else {
        int startIdx = max(0, (int)scoreHistory.size() - 8);
        int line = 0;
        for (int i = scoreHistory.size() - 1; i >= startIdx; i--) {
            char buf[64];
            sprintf(buf, "Run #%d : %d PTS", i + 1, scoreHistory[i]);
            glColor3f(0.9f, 0.9f, 0.9f);
            drawText(WIDTH / 2 - 100, HEIGHT - 180 - line * 30, buf, GLUT_BITMAP_HELVETICA_18);
            line++;
        }
    }

    glColor3f(0.5f, 0.6f, 0.7f);
    drawText(WIDTH / 2 - 80, 70, "Press ESC to return to Menu", GLUT_BITMAP_HELVETICA_12);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if (screenShake > 0.0f) {
        float rx = randf(-screenShake, screenShake);
        float ry = randf(-screenShake, screenShake);
        glTranslatef(rx, ry, 0);
    }

    if (currentState == STATE_MENU) {
        drawMenuScreen();
    } else if (currentState == STATE_CUSTOMIZE) {
        drawCustomizationScreen();
    } else if (currentState == STATE_INSTRUCTIONS) {
        drawInstructionsScreen();
    } else if (currentState == STATE_SCORECARD) {
        drawScorecardScreen();
    } else {
        drawBackground();
        drawShockwave();
        drawAntibodyAt(antibodyX, antibodyY);

        for (size_t i = 0; i < viruses.size(); i++)  drawVirus(viruses[i]);
        for (size_t i = 0; i < bullets.size(); i++)  drawBullet(bullets[i]);
        for (size_t i = 0; i < powerUps.size(); i++) drawPowerUp(powerUps[i]);
        drawParticles();

        for (size_t i = 0; i < floatTexts.size(); i++) {
            glColor4f(floatTexts[i].r, floatTexts[i].g, floatTexts[i].b, floatTexts[i].alpha);
            drawText(floatTexts[i].x, floatTexts[i].y, floatTexts[i].text, GLUT_BITMAP_HELVETICA_18);
        }

        // HUD
        char buf[64];
        glColor3f(1, 1, 1);
        sprintf(buf, "SCORE: %d", score);
        drawText(20, HEIGHT - 30, buf);

        sprintf(buf, "LEVEL: %d", level);
        drawText(20, HEIGHT - 55, buf);

        drawText(WIDTH - 195, HEIGHT - 30, "LIVES:");
        for (int i = 0; i < lives; i++) {
            drawHeart(WIDTH - 115 + i * 20, HEIGHT - 24, 15.0f, 0.95f, 0.2f, 0.4f);
        }

        glColor3f(0.8f, 0.8f, 0.8f);
        drawText(20, 25, "SHOCKWAVE [E / SHIFT]:", GLUT_BITMAP_HELVETICA_12);
        glColor3f(0.2f, 0.2f, 0.3f);
        glRectf(180, 15, 330, 28);
        if (shockwaveCharge >= 100) glColor3f(0.2f, 0.8f, 1.0f);
        else glColor3f(0.8f, 0.5f, 0.1f);
        glRectf(180, 15, 180 + (shockwaveCharge / 100.0f) * 150, 28);

        if (paused && !gameOver) {
            glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
            glRectf(WIDTH / 2 - 150, HEIGHT / 2 - 50, WIDTH / 2 + 150, HEIGHT / 2 + 50);
            glColor3f(1.0f, 0.85f, 0.2f);
            drawText(WIDTH / 2 - 50, HEIGHT / 2 + 10, "PAUSED", GLUT_BITMAP_TIMES_ROMAN_24);
            glColor3f(1, 1, 1);
            drawText(WIDTH / 2 - 80, HEIGHT / 2 - 20, "Press P to resume");
        }

        if (gameOver) {
            glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
            glRectf(WIDTH / 2 - 150, HEIGHT / 2 - 60, WIDTH / 2 + 150, HEIGHT / 2 + 60);
            glColor3f(1.0f, 0.2f, 0.2f);
            drawText(WIDTH / 2 - 70, HEIGHT / 2 + 20, "GAME OVER", GLUT_BITMAP_TIMES_ROMAN_24);
            glColor3f(1, 1, 1);
            drawText(WIDTH / 2 - 90, HEIGHT / 2 - 10, "Press R to Restart");
            drawText(WIDTH / 2 - 105, HEIGHT / 2 - 35, "Press ESC for Main Menu");
        }
    }

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;

    float scaleX = w / (float)WIDTH;
    float scaleY = h / (float)HEIGHT;
    viewScale = (scaleX < scaleY) ? scaleX : scaleY;

    int viewW = (int)(WIDTH * viewScale);
    int viewH = (int)(HEIGHT * viewScale);
    viewX = (w - viewW) / 2;
    viewY = (h - viewH) / 2;

    glViewport(viewX, viewY, viewW, viewH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WIDTH, 0, HEIGHT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

//Input Callbacks
void togglePause() {
    paused = !paused;
    leftPressed = false;
    rightPressed = false;
    upPressed = false;
    downPressed = false;
    spacePressed = false;
    if (paused) stopMusic();
    else startMusic();
}

void toggleFullScreen() {
    if (isFullScreen) {
        glutReshapeWindow(WIDTH, HEIGHT);
        glutPositionWindow(100, 100);
        glutSetCursor(GLUT_CURSOR_INHERIT);
        isFullScreen = false;
    } else {
        glutFullScreen();
        glutSetCursor(GLUT_CURSOR_NONE);
        isFullScreen = true;
    }
}

void keyboardDown(unsigned char key, int x, int y) {
    if (currentState == STATE_MENU) {
        if (key == 13) { // ENTER Key
            if (selectedMenuOption == 0) resetGame();
            else if (selectedMenuOption == 1) currentState = STATE_CUSTOMIZE;
            else if (selectedMenuOption == 2) currentState = STATE_INSTRUCTIONS;
            else if (selectedMenuOption == 3) currentState = STATE_SCORECARD;
            else if (selectedMenuOption == 4) exit(0);
        }
    } else if (currentState == STATE_INSTRUCTIONS) {
        if (key == 27 || key == 13 || key == 8) currentState = STATE_MENU;
    } else if (currentState == STATE_CUSTOMIZE || currentState == STATE_SCORECARD) {
        if (key == 27) currentState = STATE_MENU; // ESC
    } else if (currentState == STATE_PLAYING) {
        if ((key == 'p' || key == 'P') && !gameOver) togglePause();
        if (key == 'r' || key == 'R') resetGame();
        if (key == 27) { stopMusic(); paused = false; currentState = STATE_MENU; }
        if (paused) return;
        if (key == ' ') spacePressed = true;
        if (key == 'e' || key == 'E') triggerShockwave();
    }
}

void keyboardUp(unsigned char key, int x, int y) {
    if (key == ' ') spacePressed = false;
}

void specialKeysDown(int key, int x, int y) {
    if (key == GLUT_KEY_F11) {
        toggleFullScreen();
        return;
    }

    if (currentState == STATE_MENU) {
        if (key == GLUT_KEY_UP) {
            selectedMenuOption = (selectedMenuOption - 1 + TOTAL_MENU_OPTIONS) % TOTAL_MENU_OPTIONS;
        } else if (key == GLUT_KEY_DOWN) {
            selectedMenuOption = (selectedMenuOption + 1) % TOTAL_MENU_OPTIONS;
        }
    } else if (currentState == STATE_CUSTOMIZE) {
        int themeCount = sizeof(themes) / sizeof(themes[0]);
        if (key == GLUT_KEY_LEFT) {
            selectedTheme = (selectedTheme - 1 + themeCount) % themeCount;
        } else if (key == GLUT_KEY_RIGHT) {
            selectedTheme = (selectedTheme + 1) % themeCount;
        }
    } else if (currentState == STATE_PLAYING && !paused) {
        if (key == GLUT_KEY_LEFT)  leftPressed = true;
        if (key == GLUT_KEY_RIGHT) rightPressed = true;
        if (key == GLUT_KEY_UP)    upPressed = true;
        if (key == GLUT_KEY_DOWN)  downPressed = true;

        if (glutGetModifiers() & GLUT_ACTIVE_SHIFT) triggerShockwave();
    }
}

void specialKeysUp(int key, int x, int y) {
    if (key == GLUT_KEY_LEFT)  leftPressed = false;
    if (key == GLUT_KEY_RIGHT) rightPressed = false;
    if (key == GLUT_KEY_UP)    upPressed = false;
    if (key == GLUT_KEY_DOWN)  downPressed = false;
}

void timer(int value) {
    updateGame();
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

//Main
int main(int argc, char** argv) {
    srand((unsigned)time(0));
    initBgCells();
    initAudio();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_ALPHA);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Antibody Shooter - Bio Warfare");
    glutFullScreen();
    glutSetCursor(GLUT_CURSOR_NONE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0, 0, 0, 1);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialKeysDown);
    glutSpecialUpFunc(specialKeysUp);
    glutTimerFunc(0, timer, 0);

    glutMainLoop();
    return 0;
}

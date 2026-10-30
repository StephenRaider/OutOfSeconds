#include "raylib.h"
#include <math.h>
#include <stdio.h>

#define VW 320
#define VH 180
#define GY 125
#define PX 70
#define CW 48
#define CH 44
#define COLS 10
#define STEP (1.f/120)

#define TIME_LIMIT 30.f
#define RUN_V 110.f
#define DASH_V 300.f
#define DASH_T .35f
#define DASH_CD 1.2f
#define SLIDE_V .7f
#define SLIDE_T 1.f
#define JUMP_V 330.f
#define GRAV 900.f

typedef struct { int first, size; float fps; int loop; } Anim;
enum { A_RUN, A_DASH_IN, A_DASH, A_DASH_OUT, A_TAKEOFF, A_RISE, A_APEX, A_FALL, A_LAND, A_SLIDE_IN, A_SLIDE, A_SLIDE_OUT };
static const Anim ANIMS[] = {
    {0,19,20,1}, {19,2,20,0}, {21,8,14,1}, {29,2,16,0}, {31,2,20,0}, {33,2,10,1},
    {35,1,1,0},  {36,2,10,1}, {38,2,14,0}, {40,2,16,0}, {42,2,10,1}, {44,2,14,0} };

enum { MENU, UPG, PLAY, OVER };
static int scene = MENU, quit, sel;
static Texture2D sheet;
static float sc = 1, ox, oy;
static Vector2 ptr[11]; static int nptr, tapNow, prevN; static Vector2 tapPos;

static float px, py, vy, tleft, dash, cd, slide, animT, deadT, best, lean;
static int anim, dead, pj, pd, ps;

static unsigned hsh(unsigned size) { size = (size ^ 61) ^ (size >> 16); size *= 9; size ^= size >> 4; size *= 0x27d4eb2d; size ^= size >> 15; return size; }
static Vector2 toV(Vector2 p) { return (Vector2){ (p.x - ox)/sc, (p.y - oy)/sc }; }
static int inR(Vector2 p, Rectangle r) { return CheckCollisionPointRec(p, r); }
static int held(Rectangle r) { for (int idx = 0; idx < nptr; idx++) if (inR(ptr[idx], r)) return 1; return 0; }
static int tapped(Rectangle r) { return tapNow && inR(tapPos, r); }

static void play(int a) { if (anim != a) { anim = a; animT = 0; } }
static int animDone(void) { return !ANIMS[anim].loop && animT*ANIMS[anim].fps >= ANIMS[anim].size; }
static int frameOf(void) {
    int f = (int)(animT*ANIMS[anim].fps), size = ANIMS[anim].size;
    return ANIMS[anim].first + (ANIMS[anim].loop ? f % size : (f >= size ? size - 1 : f));
}

static void startRun(void) {
    px = PX; py = GY; vy = 0; tleft = TIME_LIMIT; dash = cd = lean = 0; slide = -1; dead = 0; deadT = 0; pj = pd = ps = 0;
    anim = -1; play(A_RUN); scene = PLAY;
}

static void kill(void) { if (!dead) { dead = 1; deadT = 0; best = fmaxf(best, px); } }

static void stepPlay(float dt) {
    if (dead) { deadT += dt; return; }
    int ground = py >= GY;
    if (pj && ground) { vy = -JUMP_V; py -= 1; slide = -1; play(A_TAKEOFF); }
    if (pd && cd <= 0 && slide < 0) { dash = DASH_T; cd = DASH_CD; if (ground) play(A_DASH); }
    if (ps && ground && dash <= 0 && slide < 0) { slide = 0; play(A_SLIDE_IN); }
    pj = pd = ps = 0;

    px += (dash > 0 ? DASH_V : slide >= 0 ? RUN_V*SLIDE_V : RUN_V)*dt;
    if (cd > 0) cd -= dt;
    if (dash > 0) dash -= dt;
    if (slide >= 0 && (slide += dt) >= SLIDE_T) { slide = -1; play(A_SLIDE_OUT); }
    if (py < GY || vy < 0) {
        vy += GRAV*dt; py += vy*dt;
        if (py >= GY) { py = GY; vy = 0; if (dash <= 0 && slide < 0) play(A_LAND); }
    }
    if ((tleft -= dt) <= 0) { tleft = 0; kill(); }

    animT += dt;
    lean += ((dash > 0 && slide < 0) - lean)*fminf(1.f, 14*dt);
    if (py < GY) {
        if (dash > 0) play(A_DASH);
        else if (anim == A_TAKEOFF) { if (animDone()) play(vy < 0 ? A_RISE : A_FALL); }
        else play(vy < -80 ? A_RISE : vy < 80 ? A_APEX : A_FALL);
    } else if (dash > 0) play(A_DASH);
    else if (slide >= 0) { if (anim == A_SLIDE_IN && animDone()) play(A_SLIDE); }
    else if (anim == A_LAND || anim == A_DASH_OUT || anim == A_SLIDE_OUT) { if (animDone()) play(A_RUN); }
    else play(A_RUN);
}

#define C_BG   (Color){ 24, 24, 24, 255 }
#define C_LINE (Color){ 200, 200, 200, 255 }
#define C_BTN  (Color){ 52, 52, 52, 255 }
#define C_SEL  (Color){ 96, 96, 96, 255 }
#define C_DIM  (Color){ 130, 130, 130, 255 }

static void drawBG(float cam) {
    (void)cam;
    ClearBackground(C_BG);
    DrawRectangle(0, GY, VW, 1, C_LINE);
}

static void drawHud(Rectangle *b) {
    char t[32]; snprintf(t, sizeof t, "%.1f", tleft);
    Color tc = tleft < 5 && ((int)(tleft*4) & 1) ? (Color){ 190, 190, 190, 255 } : WHITE;
    DrawText(t, VW/2 - MeasureText(t, 20)/2, 4, 20, tc);
    DrawRectangle(60, 26, 200, 4, C_BTN);
    DrawRectangle(60, 26, (int)(200*tleft/TIME_LIMIT), 4, tc);
    snprintf(t, sizeof t, "%dm", (int)((px - PX)/10)); DrawText(t, 6, 6, 10, C_DIM);
    const char *lab[3] = { "SLIDE", "DASH", "JUMP" };
    for (int idx = 0; idx < 3; idx++) {
        int on = held(b[idx]), off = idx == 1 && cd > 0;
        DrawRectangleRec(b[idx], off ? (Color){ 34, 34, 34, 255 } : on ? C_SEL : C_BTN);
        if (idx == 1 && cd > 0) DrawRectangle(b[idx].x, b[idx].y, b[idx].width*(1 - cd/DASH_CD), b[idx].height, C_BTN);
        DrawRectangleLinesEx(b[idx], 1, C_DIM);
        DrawText(lab[idx], b[idx].x + b[idx].width/2 - MeasureText(lab[idx], 10)/2, b[idx].y + 12, 10, WHITE);
    }
}

static int button(Rectangle r, const char *s, int focus) {
    DrawRectangleRec(r, focus ? C_SEL : C_BTN);
    DrawRectangleLinesEx(r, 1, C_DIM);
    DrawText(s, r.x + r.width/2 - MeasureText(s, 10)/2, r.y + r.height/2 - 5, 10, WHITE);
    return tapped(r);
}

static void centered(const char *s, int output, int size, Color c) { DrawText(s, VW/2 - MeasureText(s, size)/2, output, size, c); }

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Out of Seconds");
    SetExitKey(KEY_NULL);
    SetTargetFPS(144);
#if defined(PLATFORM_ANDROID)
    sheet = LoadTexture("runner.png");
#else
    ChangeDirectory(GetApplicationDirectory());
    sheet = LoadTexture(FileExists("assets/runner.png") ? "assets/runner.png" : "../assets/runner.png");
#endif
    SetTextureFilter(sheet, TEXTURE_FILTER_POINT);
    SetTextureFilter(GetFontDefault().texture, TEXTURE_FILTER_POINT);
    RenderTexture2D rt = LoadRenderTexture(VW, VH); SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);
    Rectangle pb[3] = { { 6, 142, 92, 34 }, { 114, 142, 92, 34 }, { 222, 142, 92, 34 } };
    Rectangle mb[3] = { { 110, 78, 100, 22 }, { 110, 106, 100, 22 }, { 110, 134, 100, 22 } };
    Rectangle ob[2] = { { 55, 128, 100, 24 }, { 165, 128, 100, 24 } };
    int prevH[3] = { 0, 0, 0 };
    float acc = 0;

    while (!quit && !WindowShouldClose()) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float s = fminf((float)sw/VW, (float)sh/VH); sc = s >= 1 ? floorf(s) : s;
        ox = (sw - VW*sc)/2; oy = (sh - VH*sc)/2;
        nptr = 0;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ptr[nptr++] = toV(GetMousePosition());
        for (int idx = 0; idx < GetTouchPointCount() && nptr < 10; idx++) ptr[nptr++] = toV(GetTouchPosition(idx));
        tapNow = nptr > 0 && prevN == 0; if (tapNow) tapPos = ptr[0]; prevN = nptr;
        float dt = fminf(GetFrameTime(), 1.f/30);

        if (scene == PLAY) {
            int h[3] = { held(pb[0]) || IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_LEFT_CONTROL),
                         held(pb[1]) || IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT),
                         held(pb[2]) || IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) };
            if (h[0] && !prevH[0]) ps = 1;
            if (h[1] && !prevH[1]) pd = 1;
            if (h[2] && !prevH[2]) pj = 1;
            for (int idx = 0; idx < 3; idx++) prevH[idx] = h[idx];
            for (acc += dt; acc >= STEP; acc -= STEP) stepPlay(STEP);
            if (IsKeyPressed(KEY_ESCAPE)) scene = MENU;
            if (dead && deadT > .6f) scene = OVER;
        } else {
            for (int idx = 0; idx < 3; idx++) prevH[idx] = 1;
            if (scene == MENU) {
                if (IsKeyPressed(KEY_DOWN)) sel = (sel + 1) % 3;
                if (IsKeyPressed(KEY_UP)) sel = (sel + 2) % 3;
                if (IsKeyPressed(KEY_ESCAPE)) quit = 1;
            }
            if (scene == UPG && IsKeyPressed(KEY_ESCAPE)) scene = MENU;
            if (scene == OVER && IsKeyPressed(KEY_ESCAPE)) scene = MENU;
        }

        float cam = px - PX;
        BeginTextureMode(rt);
        drawBG(cam);
        if (scene == PLAY || scene == OVER) {
            int f = frameOf(); Color tint = dead ? (Color){ 150, 150, 150, 255 } : WHITE;
            if (dash > 0) for (int idx = 0; idx < 5; idx++) DrawRectangle(PX - 30 - (int)(hsh(idx*13 + (int)(GetTime()*30))%30), GY - 36 + idx*7, 22, 1, (Color){ 160, 160, 160, 160 });
            if (slide >= 0) for (int idx = 0; idx < 4; idx++) { int key = (int)(GetTime()*20) + idx*5; DrawRectangle(PX - 18 - (int)(hsh(key)%14), GY - 2 - (int)(hsh(key + 99)%7), 2, 2, (Color){ 120, 120, 120, 200 }); }

            for (int r = 0; r < CH; r++)
                DrawTextureRec(sheet, (Rectangle){ (f%COLS)*CW, (f/COLS)*CH + r, CW, 1 }, (Vector2){ PX - 20 + (int)((CH - r)*.45f*lean), py - CH + r }, tint);
            if (scene == PLAY) drawHud(pb);
        }
        if (scene == MENU) {
            centered("OUT OF SECONDS", 22, 30, WHITE);
            centered("30 seconds. Run. Don't stop.", 56, 10, C_DIM);
            const char *lab[3] = { "RUN", "UPGRADES", "EXIT" };
            for (int idx = 0; idx < 3; idx++) if (button(mb[idx], lab[idx], sel == idx) || (sel == idx && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)))) {
                if (idx == 0) startRun(); else if (idx == 1) scene = UPG; else quit = 1;
            }
            centered("Jump: Space/W   Dash: Shift/D   Slide: S/Ctrl", 164, 10, (Color){ 100, 100, 100, 255 });
        } else if (scene == UPG) {
            centered("UPGRADES", 30, 20, WHITE);
            centered("Nothing here yet.", 80, 10, C_DIM);
            if (button((Rectangle){ 110, 134, 100, 22 }, "BACK", 1) || IsKeyPressed(KEY_ENTER)) scene = MENU;
        } else if (scene == OVER) {
            DrawRectangle(0, 0, VW, VH, (Color){ 10, 10, 10, 200 });
            centered("You're Out of Seconds", 24, 20, WHITE);
            centered("Time ran out.", 52, 10, C_DIM);
            char t[48]; snprintf(t, sizeof t, "Distance %dm   Best %dm", (int)((px - PX)/10), (int)((best - PX)/10));
            centered(t, 78, 10, WHITE);
            if (button(ob[0], "RETRY", 1) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) startRun();
            else if (button(ob[1], "MENU", 0)) scene = MENU;
        }
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(rt.texture, (Rectangle){ 0, 0, VW, -VH }, (Rectangle){ ox, oy, VW*sc, VH*sc }, (Vector2){ 0, 0 }, 0, WHITE);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
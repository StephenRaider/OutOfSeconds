#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define VWM     1024
#define VW      vw
#define VHM     512
#define VH      vh
#define GY      gy
#define SW      (VW * 2)
#define SWM     (VWM * 2)
#define SHM     (VHM * 2)
#define SH      (VH * 2)
#define SGY     (GY * 2)
#define FL      64
#define PX      150
#define STEP    (1.f / 120)
#define LEADMAX 70.f
static int vw = 480, vh = 270, gy = 236;

#define RUN_V        250.f
#define DASH_V       650.f
#define DASH_T       .35f
#define DASH_CD      1.2f
#define SLIDE_V      .7f
#define SLIDE_T      1.f
#define JUMP_V       365.f
#define GRAV         900.f
#define ROLL_H       50.f
#define STUMBLE_LOSS 1.5f
#define FSTEP        2

#define HAZE     .80f
#define BLOOM    .30f
#define NEON     .30f
#define AMB      .10f
#define AMB_M    .88f
#define TINT     .80f
#define FOG_FAR  .50f
#define FOG_MID  .30f
#define DARK_FAR .82f
#define DARK_MID .52f
#define BLUR_FAR 1.5f
#define BLUR_MID .5f
#define SAT      1.60f
#define CON      1.25f
#define BRI      .85f
static const float ACCENT[3] = {230, 60, 130};

#define CW  128
#define CH  120
#define CGY 118
#define DPX 61.7f
enum { C_SPRINT, C_JSTART, C_JAIR, C_JLAND, C_ROLL, C_SSTART, C_SLOOP, C_SEXIT, C_DEATH, C_DASH, C_STUMBLE };
typedef struct {
    int size, loop, then, thenFrame;
    float rate;
} Clip;
static const Clip CLIPS[] = {{16, 1, -1, 0, 1},       {18, 0, C_JAIR, 0, 1.5f},  {48, 1, -1, 0, 1},       {15, 0, C_SPRINT, 2, 1},
                             {18, 0, C_SPRINT, 2, 1}, {21, 0, C_SLOOP, 0, 1.5f}, {48, 1, -1, 0, 1},       {16, 0, C_SPRINT, 3, 1},
                             {47, 0, -1, 0, 1},       {9, 0, C_SPRINT, 0, 1},    {17, 0, C_SPRINT, 11, 1}};

enum { MENU, UPG, PLAY, OVER, PAUSE, OPT, WIN, IDX };
static int scene = MENU, quit, sel;
static float sc = 1, ox, oy;
static Vector2 ptr[11];
static int nptr, tapNow, prevN;
static Vector2 tapPos;

static float camX, px, py, vy, tleft, dash, cd, slide, animT, deadT, best, apex, stumbleT, invul, lost;
static int dbgBoxes;
static int clip, dead, pj, pd, ps, inAir, lane, devTime = 0;

static Vector2 toV(Vector2 p) {
    return (Vector2){(p.x - ox) / sc, (p.y - oy) / sc};
}
static int inR(Vector2 p, Rectangle r) { return CheckCollisionPointRec(p, r); }

static Image mAl, mNm, mDp, fAl, fNd, sky, oAl, oNm,
    oDp;
static Color *MA, *MN, *MD, *OA, *ON, *OD;
static const char *artPath(const char *size) {
#if defined(PLATFORM_ANDROID)
    return size;
#else
    return FileExists(TextFormat("assets/%s", size)) ? TextFormat("assets/%s", size) : TextFormat("../assets/%s", size);
#endif
}
static Image loadArt(const char *size) {
    Image bound = LoadImage(artPath(size));
    ImageFormat(&bound, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return bound;
}

static const unsigned char PAL[24][3] = {{137, 158, 147}, {123, 140, 130}, {114, 130, 119}, {106, 122, 112}, {100, 113, 103}, {95, 106, 95},
                                         {88, 100, 91},   {83, 93, 82},    {78, 89, 81},    {75, 83, 73},    {69, 79, 71},    {67, 75, 67},
                                         {63, 71, 63},    {59, 67, 59},    {57, 62, 55},    {52, 59, 53},    {48, 53, 47},    {44, 50, 45},
                                         {40, 46, 41},    {36, 40, 36},    {32, 37, 33},    {27, 31, 28},    {20, 22, 20},    {5, 6, 5}};
static const unsigned char *P(int counter) { return PAL[counter < 23 ? counter : 23]; }
static const unsigned char SIGN[3] = {8, 12, 12};

typedef struct {
    int y0, w, floors, ncol;
    int cols[6];
} Block;
static const Block BLK[3] = {
    {0, 612, 6, 6, {0, 132, 251, 395, 502, 612}}, {384, 511, 5, 6, {0, 75, 203, 326, 450, 511}}, {704, 403, 4, 5, {0, 96, 188, 283, 375}}};

typedef struct {
    int value, temp, w, h;
} SprRect;
static const SprRect MIDR[] = {{0, 0, 100, 210},    {101, 0, 76, 210},  {178, 0, 89, 186},   {268, 0, 68, 186},   {337, 0, 156, 185},
                               {494, 0, 81, 185},   {576, 0, 74, 120},  {651, 0, 52, 120},   {704, 0, 73, 115},   {778, 0, 55, 115},
                               {834, 0, 162, 112},  {0, 211, 204, 112}, {205, 211, 94, 111}, {300, 211, 52, 111}, {523, 211, 71, 92},
                               {595, 211, 70, 92},  {786, 211, 79, 91}, {866, 211, 71, 91},  {325, 324, 149, 53}, {475, 324, 129, 53},
                               {605, 324, 233, 50}, {0, 382, 205, 49},  {206, 382, 161, 47}, {368, 382, 233, 46}};
static const SprRect FARR[] = {{353, 211, 50, 105}, {404, 211, 38, 105}, {443, 211, 44, 93},  {488, 211, 34, 93}, {666, 211, 78, 92},
                               {745, 211, 40, 92},  {938, 211, 37, 60},  {976, 211, 26, 60},  {0, 324, 36, 57},   {37, 324, 27, 57},
                               {65, 324, 81, 56},   {147, 324, 102, 56}, {250, 324, 47, 55},  {298, 324, 26, 55}, {602, 382, 35, 46},
                               {638, 382, 35, 46},  {674, 382, 39, 45},  {714, 382, 35, 45},  {750, 382, 74, 26}, {825, 382, 64, 26},
                               {890, 382, 116, 25}, {0, 432, 102, 24},   {103, 432, 116, 23}, {220, 432, 80, 23}};
#define NSPR 24

static unsigned seed = 1312;
static unsigned hashu(int a, int b) {
    unsigned h = (unsigned)a * 374761393u + (unsigned)b * 668265263u + seed * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
static float hash(int a, int b) { return (hashu(a, b) >> 8) / 16777216.f; }
typedef struct {
    unsigned s;
} Rng;
static Rng rngOf(int a, int b) { return (Rng){hashu(a, b)}; }
static float rnd(Rng *r) {
    unsigned t = r->s += 0x6D2B79F5u;
    t = (t ^ (t >> 15)) * (1 | t);
    t = (t + (t ^ (t >> 7)) * (61 | t)) ^ t;
    return ((t ^ (t >> 14)) >> 8) / 16777216.f;
}
static float vnoise(float value, float temp) {
    int xi = (int)floorf(value), yi = (int)floorf(temp);
    float fx = value - xi, fy = temp - yi, a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1);
    float u = fx * fx * (3 - 2 * fx), v = fy * fy * (3 - 2 * fy);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}

static const float EMC[6][3] = {{0, 0, 0}, {40, 225, 180}, {0, 0, 0}, {150, 215, 180}, {205, 175, 120}, {120, 165, 140}};
static const float *emCol(int t) { return t  ==2 ? ACCENT : EMC[t]; }

#define BEACON 0x80000000u
static int fclass(unsigned g) {
    if (!g) return 0;
    if (g & BEACON) return 3;
    float h = hash((int)g, 11);
    return h < .03f ? 2 : h < .08f ? 1 : 0;
}
static float flick(unsigned g, float t) {
    int c = fclass(g);
    if (!c) return 1;
    if (c  ==3) {
        float P = 2.4f, ph = fmodf(t + hash((int)g, 5) * P, P);
        return ph < .18f ? 1 : .05f;
    }
    float base = c  ==1 ? 1 : .04f, P = 3.5f + hash((int)g, 3) * 5;
    int tertiary = (int)floorf(t / P);
    float lt = t - tertiary * P;
    if (hash((int)g * 7 + 1, tertiary) > .6f) return base;
    int size = 2 + (int)(hash((int)g * 13 + 2, tertiary) * 2);
    float bs = hash((int)g * 5 + 3, tertiary) * (P - .5f), u = lt - bs;
    if (u < 0 || u >= size * .13f) return base;
    return fmodf(u, .13f) < .06f ? (c  ==1 ? .08f : 1) : base;
}

#define BAL_H  64.f
#define ROOF_H 128.f
#define PW     6.f
#define STEPUP 8.f
enum { PK_BAL, PK_ROOF, PK_BLOCK, PK_RAMP };
typedef struct {
    float x0, x1, top, bot, top1, hint;
    int kind;
} Plat;
static Plat pf[160];
static int npf;

#define GOAL_PX 150000.f
#ifndef TOK_GAP0
#define TOK_GAP0 140.f
#endif
#ifndef TOK_GAP1
#define TOK_GAP1 225.f
#endif
#ifndef TOK_ARC
#define TOK_ARC .45f
#endif
#ifndef TOK_T2
#define TOK_T2 .5f
#endif
static float difAt(float value) {
    float d = (value - PX) / 100000.f;
    d = d < 0 ? 0 : d > 1 ? 1 : d;
    return powf(d, .75f);
}
#define SUN_PX 20000.f
static float sunAt(float value) {
    float d = ((value - PX) - (GOAL_PX - SUN_PX)) / SUN_PX;
    d = d < 0 ? 0 : d > 1 ? 1 : d;
    return d * d * (3 - 2 * d);
}
enum { P_T1, P_T2, P_DRINK, P_SHIELD };
typedef struct {
    int type;
    float value, temp;
} Pick;
static Pick pk[200];
static int npk;
static float boostT, shieldT, phaseT, flashT;
static char flashTxt[48];
static int won, endless, banked, lastGain;
static struct {
    unsigned magic;
    int startLv, capLv, durLv, credits, endless;
    float best;
} sv = {0x4F4F5331u};
static float tcap(void) { return 30.f + 10.f * sv.capLv; }
static float tstart(void) { return fminf(tcap(), 25.f + 5.f * sv.startLv); }
static float pdur(void) { return 6.f + 2.f * sv.durLv; }
static void saveGame(void) { SaveFileData("oos.sav", &sv, sizeof sv); }
static void loadGame(void) {
    int size = 0;
    unsigned char *d = LoadFileData("oos.sav", &size);
    if (d && size  ==(int)sizeof sv && *(unsigned *)d  ==sv.magic) memcpy(&sv, d, sizeof sv);
    if (d) UnloadFileData(d);
}
enum { S_TRASH, S_HURDLE, S_BARRIER, S_RAIL, S_AC1, S_AC2, S_STAIRS, S_FUEL, S_TRUCK, S_BIN1, S_BIN2, S_BIN3, NSP };
static const struct {
    int value, temp, w, h;
    float dpx;
    int hx0, hx1;
} OSP[NSP] = {
    {0, 0, 37, 61, 26.1f, 1, 35},      {38, 0, 103, 96, 112.7f, 2, 101},   {142, 0, 96, 64, 37.3f, 0, 96},
    {239, 0, 88, 54, 6.0f, 7, 88},     {328, 0, 35, 35, 28.0f, 0, 34},     {364, 0, 71, 42, 41.5f, 6, 71},
    {436, 0, 115, 128, 80.3f, 2, 113}, {0, 129, 583, 127, 105.8f, 1, 581}, {584, 129, 425, 132, 94.4f, 1, 419},
    {0, 262, 75, 72, 62.4f, 4, 71},    {76, 262, 103, 83, 88.4f, 3, 97},   {180, 262, 135, 75, 75.8f, 54, 125}};
enum { O_TRASH, O_HURDLE, O_BARRIER, O_AC, O_STAIRS, O_BOX, O_FUEL };
typedef struct {
    int type, spr, st, hit, lane;
    float value, temp, ty, vy, t;
} Obj;
static Obj obj[160];
static int nobj;
static void addPlat(float x0, float x1, float top, float bot, float top1, int kind) {
    if (npf < 160) pf[npf++] = (Plat){x0, x1, top, bot, top1, 0, kind};
}
static Obj *addObj(int type, int spr, float value, float temp, int ln) {
    if (nobj >= 160) return 0;
    Obj *o = &obj[nobj++];
    *o = (Obj){type, spr, 0, 0, ln, value, temp, temp, 0, 0};
    return o;
}
static float lastAC = -1e9f;
static void addAC(float xc, float ty, int ln, Rng *r) {
    if (xc - lastAC < 560) return;
    int s = rnd(r) < .5f ? S_AC1 : S_AC2;
    Obj *o = addObj(O_AC, s, xc - OSP[s].w / 4.f, GY - BAL_H - 48, ln);
    if (o) {
        o->ty = ty;
        lastAC = xc;
    }
}
static void addTruck(int fuel, float value) {
    addObj(fuel ? O_FUEL : O_BOX, fuel ? S_FUEL : S_TRUCK, value, GY, 3);
    if (fuel) {
        addPlat(value, value + 40, GY - 61, GY, 0, PK_BLOCK);
        addPlat(value + 50, value + 250, GY - 62, GY, 0, PK_BLOCK);
        addPlat(value + 250, value + 291, GY - 52, GY, 0, PK_BLOCK);
    } else {
        addPlat(value, value + 170, GY - 66, GY, 0, PK_BLOCK);
        addPlat(value + 170, value + 212, GY - 54, GY, 0, PK_BLOCK);
    }
}
static void prune(float cam) {
    int tertiary = 0;
    for (int counter = 0; counter < npf; counter++)
        if (pf[counter].x1 > 1e8f || pf[counter].x1 > cam - 900) pf[tertiary++] = pf[counter];
    npf = tertiary;
    tertiary = 0;
    for (int counter = 0; counter < nobj; counter++)
        if (obj[counter].value + 300 > cam - 900) obj[tertiary++] = obj[counter];
    nobj = tertiary;
    tertiary = 0;
    for (int counter = 0; counter < npk; counter++)
        if (pk[counter].value + 300 > cam - 900) pk[tertiary++] = pk[counter];
    npk = tertiary;
}

static float supportY(float value, float temp, int *kind) {
    float f = GY;
    int tertiary = -1;
    for (int counter = 0; counter < npf; counter++) {
        const Plat *p = &pf[counter];
        if (value + 3 <= p->x0 || value - 3 >= p->x1) continue;
        float s = p->kind  ==PK_RAMP ? p->top + (p->top1 - p->top) * fminf(1.f, fmaxf(0.f, (value - p->x0) / (p->x1 - p->x0))) : p->top;
        if (s >= temp - STEPUP && s < f) {
            f = s;
            tertiary = p->kind;
        }
    }
    if (kind) *kind = tertiary;
    return f;
}

typedef struct {
    float value, temp, extra, R, s;
    int t;
    unsigned g;
} Light;
#define MAXL 64
typedef struct {
    int gap, glow, x0, w, id, roof;
    unsigned char *al, *a, *et, *ei;
    unsigned *eg;
    float *size;
    float *lit;
    Light L[MAXL];
    int nl;
} Bld;
static Bld *blds;
static int nb, cap, bEnd, bId;

static void addLight(Bld *b, Light l) {
    if (b->nl < MAXL) b->L[b->nl++] = l;
}
static void bpx(Bld *b, int value, int temp, const unsigned char *c, float nx, float ny, float extra) {
    if (value < 0 || value >= b->w || temp < 0 || temp >= GY) return;
    int counter = temp * b->w + value;
    memcpy(b->al + counter * 3, c, 3);
    b->a[counter] = 255;
    float *size = b->size + counter * 4;
    size[0] = nx;
    size[1] = ny;
    size[2] = sqrtf(fmaxf(0, 1 - nx * nx - ny * ny));
    size[3] = extra;
    b->et[counter] = 0;
}
static void brect(Bld *b, int value, int temp, int w, int h, const unsigned char *c, float nx, float ny, float extra) {
    for (int jdx = temp; jdx < temp + h; jdx++)
        for (int counter = value; counter < value + w; counter++) bpx(b, counter, jdx, c, nx, ny, extra);
}
static void bem(Bld *b, int value, int temp, int w, int h, int t, int s, unsigned g) {
    if (!fclass(g)) g = 0;
    for (int jdx = temp; jdx < temp + h; jdx++)
        for (int counter = value; counter < value + w; counter++) {
            if (counter < 0 || counter >= b->w || jdx < 0 || jdx >= GY) continue;
            int tertiary = jdx * b->w + counter;
            b->et[tertiary] = t;
            b->ei[tertiary] = s;
            b->eg[tertiary] = g;
            b->a[tertiary] = 255;
        }
}
static void glyph(Bld *b, int value, int temp, int t, Rng *r, unsigned g) {
    int bound = (int)(rnd(r) * 512);
    for (int jdx = 0; jdx < 3; jdx++)
        for (int counter = 0; counter < 3; counter++)
            if ((bound >> (jdx * 3 + counter)) & 1 || (counter  ==1 && jdx  ==1)) bem(b, value + counter * 2, temp + jdx * 2, 2, 2, t, 220, g);
}

static void freeBld(Bld *b) {
    free(b->al);
    free(b->a);
    free(b->et);
    free(b->ei);
    free(b->eg);
    free(b->size);
    free(b->lit);
    b->al = b->a = b->et = b->ei = 0;
    b->eg = 0;
    b->size = b->lit = 0;
}

static void genBuilding(Bld *b, int x0, Rng *r, int mode, float slabX) {
    const Block *B = &BLK[(int)(rnd(r) * 3)];
    int nbay = B->ncol - 1;
    int size = 1 + (int)(rnd(r) * (nbay < 3 ? nbay : 3)), b0 = (int)(rnd(r) * (nbay - size + 1)), w = 0, bsrc[3], bw[3];
    for (int jdx = 0; jdx < size; jdx++) {
        bsrc[jdx] = B->cols[b0 + jdx];
        bw[jdx] = B->cols[b0 + jdx + 1] - B->cols[b0 + jdx];
        w += bw[jdx];
    }
    int floors = mode  ==2 ? 1 : mode  ==1 ? 2 + (int)(rnd(r) * 3) : rnd(r) < .45f ? 1 + (int)(rnd(r) * 2) : 3 + (int)(rnd(r) * 3), rows[5];
    for (int counter = 0; counter < floors; counter++) rows[counter] = (int)(rnd(r) * B->floors);
    int id = ++bId, N = w * GY;
    memset(b, 0, sizeof *b);
    b->x0 = x0;
    b->w = w;
    b->id = id;
    b->roof = floors  ==1;
    b->al = calloc(N * 3, 1);
    b->a = calloc(N, 1);
    b->et = calloc(N, 1);
    b->ei = calloc(N, 1);
    b->eg = calloc(N, sizeof(unsigned));
    b->size = malloc(N * 4 * sizeof(float));
    for (int counter = 0; counter < N; counter++) {
        float *q = b->size + counter * 4;
        q[0] = q[1] = 0;
        q[2] = 1;
        q[3] = -28;
    }
    Color *FA = fAl.data, *FN = fNd.data;
    int FW = fAl.width;
    int top = GY - FL - floors * FL;

    for (int counter = 0; counter < floors; counter++) {
        int y0 = GY - FL - (counter + 1) * FL;
        if (y0 + FL < 0) continue;
        int xo = 0;
        for (int bi = 0; bi < size; bi++) {
            for (int yy = 0; yy < FL; yy++) {
                int Y = y0 + yy;
                if (Y < 0) continue;
                int sy = B->y0 + rows[counter] * FL + yy;
                for (int xx = 0; xx < bw[bi]; xx++) {
                    Color s = FA[sy * FW + bsrc[bi] + xx];
                    if (s.a < 128) continue;
                    Color nd = FN[sy * FW + bsrc[bi] + xx];
                    int tertiary = Y * w + xo + xx;
                    b->al[tertiary * 3] = s.r;
                    b->al[tertiary * 3 + 1] = s.g;
                    b->al[tertiary * 3 + 2] = s.b;
                    b->a[tertiary] = s.a  ==254 ? 254 : 255;
                    float *q = b->size + tertiary * 4;
                    q[0] = nd.r / 127.5f - 1;
                    q[1] = -(nd.g / 127.5f - 1);
                    q[2] = nd.b / 127.5f - 1;
                    q[3] = -28 + (nd.a / 255.f - .25f) * 30;
                }
            }

            Rng lr = rngOf(id * 31 + counter, xo);
            if (rnd(&lr) < .28f) {
                int t = rnd(&lr) < .5f ? 4 : 5, s = 45 + (int)(rnd(&lr) * 50);
                unsigned g = id * 1000 + counter * 16 + xo, eg = fclass(g) ? g : 0;
                float cx = 0, cy = 0;
                int cn = 0;
                for (int yy = 0; yy < FL; yy++) {
                    int Y = y0 + yy;
                    if (Y < 0) continue;
                    for (int xx = 0; xx < bw[bi]; xx++) {
                        int tertiary = Y * w + xo + xx;
                        if (b->a[tertiary]  ==254) {
                            b->et[tertiary] = t;
                            b->ei[tertiary] = s;
                            b->eg[tertiary] = eg;
                            cx += xo + xx;
                            cy += Y;
                            cn++;
                        }
                    }
                }
                if (cn > 20 && counter < 2) addLight(b, (Light){x0 + cx / cn, cy / cn, -20, 80, .45f, t, 0});
            }
            xo += bw[bi];
        }
    }

    if (top > 4) {
        brect(b, 0, top - 6, w, 6, P(5), 0, -.6f, -28);
        brect(b, 0, top - 6, w, 1, P(2), 0, -1, -28);
        if (rnd(r) < .7f) {
            int tx = 6 + (int)(rnd(r) * (w - 30));
            brect(b, tx, top - 26, 18, 20, P(9), .3f, 0, -30);
            brect(b, tx - 1, top - 28, 20, 2, P(6), 0, -.8f, -28);
            for (int jdx = top - 6; jdx < top; jdx++) {
                bpx(b, tx + 2, jdx, P(12), 0, 0, -28);
                bpx(b, tx + 15, jdx, P(12), 0, 0, -28);
            }
        }
        for (int tertiary = 0; tertiary < 2; tertiary++)
            if (rnd(r) < .6f) {
                int ax = (int)(rnd(r) * (w - 4)), ah = 10 + (int)(rnd(r) * 22);
                for (int jdx = top - 6 - ah; jdx < top - 6; jdx++) bpx(b, ax, jdx, P(14), 0, 0, -28);
                if (rnd(r) < .5f) bem(b, ax, top - 7 - ah, 1, 1, 2, 200, BEACON | (unsigned)(id * 7 + tertiary));
            }
        for (int tertiary = 0; tertiary < 2; tertiary++)
            if (rnd(r) < .5f) {
                int ax = 4 + (int)(rnd(r) * (w - 20));
                brect(b, ax, top - 14, 12, 8, P(7), 0, -.3f, -28);
            }
    }

    int gx = 0;
    while (gx < w) {
        int sw = 40 + (int)(rnd(r) * 50);
        if (sw > w - gx) sw = w - gx;
        float kind = rnd(r);
        int y0 = GY - FL;
        brect(b, gx, y0, sw, FL, P(10), 0, 0, -28);
        brect(b, gx, y0, sw, 4, P(6), 0, -.7f, -28);
        if (kind < .45f) {
            for (int jdx = y0 + 12; jdx < GY; jdx++)
                for (int counter = gx + 3; counter < gx + sw - 3; counter++) {
                    int s = (jdx - y0) % 3;
                    bpx(b, counter, jdx, s  ==0 ? P(13) : s  ==1 ? P(8) : P(11), 0, s  ==0 ? -.5f : s  ==2 ? .5f : 0, -29);
                }
            brect(b, gx + 3, y0 + 8, sw - 6, 4, P(15), 0, -.6f, -26);
            for (int counter = gx + 5; counter < gx + sw - 5; counter += 7)
                if (rnd(r) < .5f) {
                    int gy = y0 + 20 + (int)(rnd(r) * 30), gw = 3 + (int)(rnd(r) * 4);
                    brect(b, counter, gy, gw, 1, P(4), 0, 0, -29);
                }
        } else if (kind < .8f) {
            brect(b, gx + 3, y0 + 10, sw - 6, FL - 12, P(18), 0, 0, -34);
            int t = rnd(r) < .5f ? 3 : 4;
            unsigned g = id * 1000 + 900 + gx;
            bem(b, gx + 4, y0 + 14, sw - 8, FL - 20, t, 30 + (int)(rnd(r) * 30), g);
            for (int counter = gx + 10; counter < gx + sw - 6; counter += 12) brect(b, counter, y0 + 14, 1, FL - 20, P(12), 0, 0, -31);
            brect(b, gx + 3, y0 + 10, sw - 6, 3, P(4), 0, -.8f, -24);
            addLight(b, (Light){x0 + gx + sw / 2.f, y0 + 34, -26, 95, .6f, t, g});
        } else {
            brect(b, gx + sw / 2 - 10, y0 + 16, 20, FL - 16, P(19), 0, 0, -40);
            brect(b, gx + sw / 2 - 12, y0 + 14, 24, 2, P(6), 0, -.8f, -28);
            for (int jdx = GY - 8; jdx < GY; jdx += 3) brect(b, gx + sw / 2 - 10, jdx, 20, 1, P(9), 0, -.6f, -34);
        }
        if (rnd(r) < .75f) {
            int tx = gx + sw / 2 - 7;
            unsigned g = id * 1000 + 800 + gx;
            brect(b, tx - 1, y0 + 5, 16, 2, P(12), 0, .6f, -22);
            bem(b, tx, y0 + 7, 14, 1, 3, 230, g);
            addLight(b, (Light){x0 + tx + 7, y0 + 9, -12, 120, .9f, 3, g});
        }
        gx += sw;
    }

    if (mode) {
        int sx = (int)(slabX - x0);
        if (sx < 0) sx = 0;
        if (sx < w) {
            int y0 = GY - FL;
            brect(b, sx, y0, w - sx, 6, P(2), 0, -.7f, -34);
            brect(b, sx, y0 + 5, w - sx, 1, P(20), 0, .6f, -34);
            bem(b, sx, y0, w - sx, 2, 1, 170, 0);
            for (int lx = sx + 30; lx < w; lx += 90) addLight(b, (Light){x0 + lx, y0 - 6, -30, 85, .55f, 1, 0});
        }
    }
    if (floors  ==1) {
        brect(b, 0, top, w, 5, P(2), 0, -.7f, -34);
        brect(b, 0, top + 4, w, 1, P(20), 0, .6f, -34);
        bem(b, 0, top, w, 2, 1, 170, 0);
        for (int lx = 30; lx < w; lx += 90) addLight(b, (Light){x0 + lx, top - 6, -30, 85, .55f, 1, 0});
    }

    if (rnd(r) < NEON) {
        int sw = 14, sh = 36 + (int)(rnd(r) * 30), sx = 4 + (int)(rnd(r) * (w - 24)), sy = GY - FL - sh - 6, t = rnd(r) < .6f ? 1 : 2;
        unsigned g = id * 1000 + 700;
        brect(b, sx, sy, sw, sh, SIGN, 0, 0, -18);
        for (int counter = sx; counter < sx + sw; counter++) {
            bem(b, counter, sy, 1, 1, t, 255, g);
            bem(b, counter, sy + sh - 1, 1, 1, t, 255, g);
        }
        for (int jdx = sy; jdx < sy + sh; jdx++) {
            bem(b, sx, jdx, 1, 1, t, 255, g);
            bem(b, sx + sw - 1, jdx, 1, 1, t, 255, g);
        }
        for (int jdx = sy + 4; jdx < sy + sh - 8; jdx += 8) glyph(b, sx + 4, jdx, t, r, g);
        addLight(b, (Light){x0 + sx + 7, sy + sh / 2.f, -10, 120, .8f, t, g});
    }
}

enum { Z_G, Z_BAL, Z_ROOF, Z_HOP };
static int zk = -1, zn, zNext;
static float zEnd, slabX;
static void addGap(int w) {
    if (nb  ==cap) {
        cap = cap ? cap * 2 : 64;
        blds = realloc(blds, cap * sizeof *blds);
    }
    Rng r = rngOf(778, nb + 1);
    Bld *b = &blds[nb++];
    memset(b, 0, sizeof *b);
    b->gap = 1;
    b->x0 = bEnd;
    b->w = w;
    b->glow = rnd(&r) < .5f;
    bEnd += w;
}
static int laneNJ;
static void addPick(int tp, float value, float temp) {
    if (npk < 195) pk[npk++] = (Pick){tp, value, temp};
}
static float placeObs(int tertiary, float value, float yb, int ln,
                      Rng *r) {
    if (laneNJ && (tertiary  ==0 || tertiary  ==2)) tertiary = 1;
    if (tertiary  ==0) {
        static const int bins[4] = {S_TRASH, S_BIN1, S_BIN2, S_BIN3};
        int v = bins[(int)(rnd(r) * 4)];
        addObj(O_TRASH, v, value, yb, ln);
        float w = OSP[v].w / 2.f;
        if (rnd(r) < TOK_ARC) addPick(rnd(r) < .3f ? P_T2 : P_T1, value + w / 2, yb - 62);
        return w;
    }
    if (tertiary  ==1) {
        addObj(O_HURDLE, S_HURDLE, value, yb, ln);
        return OSP[S_HURDLE].w / 2.f;
    }
    if (tertiary  ==2) {
        addObj(O_BARRIER, S_BARRIER, value, yb, ln);
        float w = OSP[S_BARRIER].w / 2.f;
        if (rnd(r) < TOK_ARC) addPick(rnd(r) < .3f ? P_T2 : P_T1, value + w / 2, yb - 62);
        return w;
    }
    if (tertiary  ==3) {
        addAC(value + 10, yb, ln, r);
        return 20;
    }
    if (tertiary  ==4) {
        addTruck(0, value);
        if (rnd(r) < TOK_ARC * 1.5f)
            for (int counter = 0; counter < 2; counter++) addPick(P_T1, value + 60 + 60 * counter, yb - 88);
        return 212;
    }
    addTruck(1, value);
    if (rnd(r) < TOK_ARC * 1.5f)
        for (int counter = 0; counter < 3; counter++) addPick(counter  ==2 ? P_T2 : P_T1, value + 70 + 70 * counter, yb - 88);
    return 291;
}

static float chainGap(int prev, int next, Rng *r) {
    static const float g[4][4][2] = {
        {{215, 60}, {190, 60}, {240, 80}, {270, 50}},
        {{110, 40}, {230, 50}, {260, 80}, {175, 35}},
        {{210, 50}, {210, 50}, {560, 40}, {260, 50}},
        {{320, 80}, {320, 80}, {400, 80}, {340, 80}}};
    return g[prev][next][0] + rnd(r) * g[prev][next][1];
}

typedef struct {
    signed char t[7];
    int tier;
} Motif;
static const Motif MOT[] = {{{0, -1}, 0},
                            {{1, -1}, 0},
                            {{2, -1}, 0},
                            {{3, -1}, 0},
                            {{0, 0, -1}, 1},
                            {{1, 0, -1}, 1},
                            {{0, 1, -1}, 1},
                            {{1, 1, -1}, 1},
                            {{2, 0, -1}, 1},
                            {{3, 1, -1}, 1},
                            {{0, 0, 0, -1}, 2},
                            {{1, 0, 1, -1}, 2},
                            {{0, 1, 0, -1}, 2},
                            {{1, 2, -1}, 2},
                            {{3, 0, 1, -1}, 2},
                            {{0, 1, 0, 1, -1}, 3},
                            {{1, 0, 1, 0, 1, -1}, 3},
                            {{0, 0, 1, 2, 0, -1}, 3},
                            {{2, 0, 1, 0, 0, -1}, 3}};

static void fillLane(float xs, float xe, float yb, int ln, int ac, int truck, Rng *r) {
    float ph = rnd(r) * 6.28f;
    for (float value = xs; value < xe;) {
        float dif = difAt(value), wave = .5f + .5f * sinf(ph + value / 2200.f * 6.28f);
        int tier = (int)(dif * 3.99f + .4f * wave);
        if (tier > 3) tier = 3;
        int pool[24], np = 0;
        for (int counter = 0; counter < (int)(sizeof MOT / sizeof *MOT); counter++)
            if (MOT[counter].tier <= tier && (MOT[counter].tier  ==tier || tier < 2 || (counter & 1))) pool[np++] = counter;
        const Motif *bound = &MOT[pool[(int)(rnd(r) * np)]];
        int prev = -1;
        float loose = 1.f + .6f * (1.f - dif);
        for (int counter = 0; bound->t[counter] >= 0 && value < xe; counter++) {
            int t = bound->t[counter];
            if (t  ==2 && !ac) t = 0;
            if (t  ==3 && !truck) t = 1;
            if (laneNJ && t  ==0) t = 1;
            int kind = t  ==0 ? (rnd(r) < .6f ? 0 : 2) : t  ==1 ? 1 : t  ==2 ? 3 : 4 + (rnd(r) < .4f);
            if (prev >= 0) {
                float g = chainGap(prev, t, r) * loose;
                if (value + g > xe) break;
                value += g;
            }
            value += placeObs(kind, value, yb, ln, r);
            prev = t;
        }
        value += (660.f - 330.f * dif) * (1.35f - .7f * wave) +
             rnd(r) * (300.f - 120.f * dif);
    }
}
static int pickClear(float x0, float x1, int ln) {
    for (int counter = 0; counter < nobj; counter++) {
        const Obj *o = &obj[counter];
        int l = o->lane  ==3 ? 0 : o->lane;
        if (l != ln) continue;
        float w = OSP[o->spr].w / 2.f;
        if (o->value < x1 && o->value + w > x0) return 0;
    }
    return 1;
}
static void fillPicks(float xs, float xe, float yb, int ln, float rate,
                      Rng *r) {
    for (float value = xs; value < xe;) {
        value += (TOK_GAP0 + (TOK_GAP1 - TOK_GAP0) * difAt(value)) * (.6f + .8f * rnd(r)) / rate;
        if (value >= xe || npk >= 200) break;
        if (!pickClear(value - 70, value + 90, ln)) continue;
        float u = rnd(r), hi = rnd(r), cy = yb - (hi < (ln ? .55f : .3f) && !laneNJ ? 62.f : 22.f);
        int tp;
        if (u < .08f)
            tp = rnd(r) < .55f ? P_DRINK : P_SHIELD;
        else
            tp = rnd(r) < (ln ? TOK_T2 : TOK_T2 * .4f) ? P_T2 : P_T1;
        pk[npk++] = (Pick){tp, value, cy};
    }
}
static int pickAfter(int tertiary, Rng *r) {
    float u = rnd(r), d = difAt(bEnd), kp = 1.f - .75f * sunAt(bEnd),
          hp = (d > .1f ? .08f + .17f * d : 0.f) * kp;
    return tertiary  ==Z_G     ? (u < .55f * kp        ? Z_BAL
                           : u < .55f * kp + hp ? Z_HOP
                                                : Z_G)
           : tertiary  ==Z_BAL ? (u < .38f              ? Z_ROOF
                           : u < .38f + hp * .6f ? Z_HOP
                                                 : Z_G)
                        : Z_G;
}
static void genBlds(float upto,
                    float slab) {
    while (bEnd < upto) {
        if (nb  ==cap) {
            cap = cap ? cap * 2 : 64;
            blds = realloc(blds, cap * sizeof *blds);
        }
        Rng r = rngOf(777, nb + 1);
        Bld *b = &blds[nb];
        genBuilding(b, bEnd, &r, Z_BAL, slab);
        nb++;
        bEnd += b->w;
    }
}
static void nextZone(void) {
    Rng r = rngOf(31, ++zn);
    int old = zk, nk = zn  ==1 ? Z_G : zNext;
    zNext = pickAfter(nk, &r);
    if ((old  ==Z_BAL || old  ==Z_ROOF || old  ==Z_HOP) && nk != Z_ROOF) {
        for (int counter = 0; counter < npf; counter++)
            if (pf[counter].x1 > 1e8f) pf[counter].x1 = bEnd;
        addGap(old  ==Z_ROOF ? 100 + (int)(rnd(&r) * 40) : 50 + (int)(rnd(&r) * 50));
    }
    float zs = bEnd;
    zk = nk;
    if (nk  ==Z_G) {
        float zd = difAt(zs);
        zEnd = (zs + (zn  ==1 ? 1800 : 800 + rnd(&r) * 500 + 1800 * zd));
        fillLane(fmaxf(zs + 300, 560), zEnd - 450, GY, 0, 1, 1, &r);
        fillPicks(fmaxf(zs + 60, 420), zEnd - 120, GY, 0, 1.f,
                  &r);
    } else if (nk  ==Z_BAL) {
        int e = (int)(rnd(&r) * 10);
        e = e < 4 ? 0 : e < 8 ? 1 : 2;
        zEnd = (zs + 700 + rnd(&r) * 500 + 1000 * difAt(zs));
        slabX = e  ==0 ? zs + 112 : zs;
        if (e  ==0) {
            addPlat(zs + 3, zs + 107, GY, GY, GY - 60, PK_RAMP);
            addObj(O_STAIRS, S_STAIRS, zs, GY, 0);
        }
        if (e  ==2) addTruck(1, zs - 250);
        addPlat(slabX, 1e9f, GY - BAL_H, GY - BAL_H + 6, 0, PK_BAL);
        if (e  ==1) pf[npf - 1].hint = zs - 105;
        fillLane(slabX + 300, zEnd - 250, GY - BAL_H, 1, 1, 0, &r);
        fillPicks(slabX + 60, zEnd - 80, GY - BAL_H, 1, 1.f, &r);
    } else if (nk  ==Z_HOP) {
        float dif = difAt(zs), yb = GY - BAL_H;
        int e = (int)(rnd(&r) * 10);
        e = e < 4 ? 0 : e < 8 ? 1 : 2;
        slabX = e  ==0 ? zs + 112 : zs;
        if (e  ==0) {
            addPlat(zs + 3, zs + 107, GY, GY, GY - 60, PK_RAMP);
            addObj(O_STAIRS, S_STAIRS, zs, GY, 0);
        }
        if (e  ==2) addTruck(1, zs - 250);
        int nh = 2 + (int)(rnd(&r) * (2.f + 3.f * dif));
        float segS = slabX;
        for (int h = 0; h <= nh; h++) {
            genBlds(segS + (h  ==0 ? 440 + rnd(&r) * 160 : 240 + rnd(&r) * 170 - 60 * dif), segS);
            addPlat(segS, bEnd, yb, yb + 6, 0, PK_BAL);
            if (h  ==0 && e  ==1) pf[npf - 1].hint = zs - 105;
            float a = segS + (h  ==0 ? 160 : 90), b = bEnd - 60;
            if (h > 0) addPick(rnd(&r) < .5f ? P_T2 : P_T1, segS + 50, yb - 22);
            if (h > 0 && rnd(&r) < dif * .7f && b - a > 150) placeObs(1, (a + b) / 2 - 50, yb, 1, &r);
            if (b > a) fillPicks(a, b, yb, 1, 1.5f, &r);
            if (h < nh) {
                float g = 80.f + rnd(&r) * 60.f + 45.f * dif, gs = bEnd;
                addGap((int)g);
                for (int tertiary = 0; tertiary < 3; tertiary++)
                    if (tertiary  ==1 || rnd(&r) < .6f)
                        addPick(tertiary  ==1 ? P_T2 : P_T1, gs + g * (.2f + .3f * tertiary),
                                yb - (tertiary  ==1 ? 66.f : 46.f));
                segS = bEnd;
            }
        }
        zEnd = bEnd;
    } else {
        zEnd = (zs + 650 + rnd(&r) * 400 + 800 * difAt(zs));
        slabX = zs;
        addPlat(zs, 1e9f, GY - ROOF_H, GY - ROOF_H + 6, 0, PK_ROOF);
        pf[npf - 1].hint = zs - 95;
        laneNJ = 1;
        fillLane(zs + 200, zEnd - 250, GY - BAL_H, 1, 1, 0, &r);
        fillPicks(zs + 100, zEnd - 100, GY - BAL_H, 1, .8f, &r);
        laneNJ = 0;
        fillPicks(zs + 60, zEnd - 80, GY - ROOF_H, 2, 1.f,
                  &r);
    }
}
static void ensure(int upto) {
    while (bEnd < upto) {
        while (bEnd >= zEnd) nextZone();
        if (nb  ==cap) {
            cap = cap ? cap * 2 : 64;
            blds = realloc(blds, cap * sizeof *blds);
        }
        Rng r = rngOf(777, nb + 1);
        Bld *b = &blds[nb];
        {
            float sn = sunAt(bEnd);
            if (zk  ==Z_G && rnd(&r) < .3f + .6f * sn) {
                addGap(20 + (int)(rnd(&r) * 40) + (int)(rnd(&r) * 300 * sn));
                continue;
            }
        }
        genBuilding(b, bEnd, &r, zk, slabX);
        nb++;
        if (b->roof && zk != Z_ROOF) addPlat(bEnd, bEnd + b->w, GY - ROOF_H, GY - ROOF_H + 6, 0, PK_ROOF);
        bEnd += b->w;
    }
}
static int bAt(int wx) {
    int lo = 0, hi = nb - 1;
    while (lo <= hi) {
        int bound = (lo + hi) >> 1;
        Bld *b = &blds[bound];
        if (wx < b->x0)
            hi = bound - 1;
        else if (wx >= b->x0 + b->w)
            lo = bound + 1;
        else
            return bound;
    }
    return -1;
}

static void bake(int counter) {
    Bld *b = &blds[counter];
    b->lit = calloc(b->w * GY * 3, sizeof(float));
    for (int tertiary = counter - 2; tertiary <= counter + 2; tertiary++) {
        if (tertiary < 0 || tertiary >= nb || blds[tertiary].gap) continue;
        for (int li = 0; li < blds[tertiary].nl; li++) {
            Light *L = &blds[tertiary].L[li];
            if (fclass(L->g)) continue;
            const float *c = emCol(L->t);
            int xa = (int)(L->value - L->R) - b->x0, xb = (int)(L->value + L->R) - b->x0 + 1;
            if (xa < 0) xa = 0;
            if (xb > b->w) xb = b->w;
            for (int jdx = 0; jdx < GY; jdx++)
                for (int value = xa; value < xb; value++) {
                    int q = jdx * b->w + value;
                    if (!b->a[q]) continue;
                    float *size = b->size + q * 4;
                    float dx = L->value - (b->x0 + value), dy = L->temp - jdx, dz = L->extra - size[3] + 8, d = sqrtf(dx * dx + dy * dy + dz * dz);
                    if (d > L->R || d <= 0) continue;
                    float f = (1 - d / L->R) * (1 - d / L->R) * fmaxf(0, (size[0] * dx + size[1] * dy + size[2] * dz) / d) * 2.2f * L->s / 255;
                    b->lit[q * 3] += f * c[0];
                    b->lit[q * 3 + 1] += f * c[1];
                    b->lit[q * 3 + 2] += f * c[2];
                }
        }
    }
}

static unsigned char *blurred[2][NSPR];
typedef struct {
    int spr, x0, id;
    unsigned char *px;
} Far;
typedef struct {
    Far *list;
    int size, cap, end;
} FarStore;
static FarStore mids, fars;

static void blurOne(int lay, int si, float rad) {
    const SprRect *R = lay ? &FARR[si] : &MIDR[si];
    int w = R->w, h = R->h, size = w * h, r = (int)roundf(rad);
    if (r < 1) r = 1;
    float *A = malloc(size * 4 * sizeof(float)), *B = malloc(size * 4 * sizeof(float));
    Color *S = sky.data;
    for (int temp = 0; temp < h; temp++)
        for (int value = 0; value < w; value++) {
            Color c = S[(R->temp + temp) * sky.width + R->value + value];
            float a = c.a ? 1 : 0, *o = A + (temp * w + value) * 4;
            o[0] = c.r * a;
            o[1] = c.g * a;
            o[2] = c.b * a;
            o[3] = a;
        }
    for (int pass = 0; pass < 2; pass++) {
        for (int temp = 0; temp < h; temp++)
            for (int value = 0; value < w; value++) {
                float s[4] = {0};
                int c = 0;
                for (int tertiary = -r; tertiary <= r; tertiary++) {
                    int xx = value + tertiary;
                    if (xx < 0 || xx >= w) continue;
                    float *q = A + (temp * w + xx) * 4;
                    for (int bound = 0; bound < 4; bound++) s[bound] += q[bound];
                    c++;
                }
                for (int bound = 0; bound < 4; bound++) B[(temp * w + value) * 4 + bound] = s[bound] / c;
            }
        for (int temp = 0; temp < h; temp++)
            for (int value = 0; value < w; value++) {
                float s[4] = {0};
                int c = 0;
                for (int tertiary = -r; tertiary <= r; tertiary++) {
                    int yy = temp + tertiary;
                    if (yy < 0 || yy >= h) continue;
                    float *q = B + (yy * w + value) * 4;
                    for (int bound = 0; bound < 4; bound++) s[bound] += q[bound];
                    c++;
                }
                for (int bound = 0; bound < 4; bound++) A[(temp * w + value) * 4 + bound] = s[bound] / c;
            }
    }
    unsigned char *d = calloc(size * 4, 1);
    for (int counter = 0; counter < size; counter++) {
        float a = A[counter * 4 + 3];
        if (a > .02f) {
            d[counter * 4] = (unsigned char)fminf(255, A[counter * 4] / a);
            d[counter * 4 + 1] = (unsigned char)fminf(255, A[counter * 4 + 1] / a);
            d[counter * 4 + 2] = (unsigned char)fminf(255, A[counter * 4 + 2] / a);
            d[counter * 4 + 3] = (unsigned char)(a * 255);
        }
    }
    free(A);
    free(B);
    blurred[lay][si] = d;
}

static void bakeFar(Far *o, int lay) {
    const SprRect *R = lay ? &FARR[o->spr] : &MIDR[o->spr];
    int w = R->w, h = R->h, yo = lay ? -6 : 0, top = GY + yo - h;
    float fog = lay ? FOG_FAR : FOG_MID, dk = 1 - (lay ? DARK_FAR : DARK_MID), wl = lay ? .06f : .09f;
    float FD[3] = {24 + (8 - 24) * TINT, 28 + (30 - 28) * TINT, 27 + (22 - 27) * TINT};
    unsigned char *bd = blurred[lay][o->spr];
    Color *S = sky.data;
    o->px = calloc(w * h * 7, 1);
    for (int row = 0; row < h; row++)
        for (int sx = 0; sx < w; sx++) {
            int s = row * w + sx;
            if (!bd[s * 4 + 3]) continue;
            int temp = top + row;
            unsigned char *q = o->px + s * 7;
            Color src = S[(R->temp + row) * sky.width + R->value + sx];
            float r = bd[s * 4] * dk, g = bd[s * 4 + 1] * dk, b = bd[s * 4 + 2] * dk, f = fminf(1, fog + (1 - fog) * .2f * ((float)temp / GY)),
                  e[3] = {0, 0, 0};
            r = r * (1 - f) + FD[0] * f;
            g = g * (1 - f) + FD[1] * f;
            b = b * (1 - f) + FD[2] * f;
            int cov = bd[s * 4 + 3];
            float h2 = hash(o->id * 613 + sx, row);
            if (src.a && h2 < (src.a  ==254 ? wl * .15f : .0012f)) {
                const float *c = emCol(hash(sx, temp) < .55f ? 4 : 3);
                float tertiary = .85f * (1 - fog * .3f);
                r = c[0] * tertiary;
                g = c[1] * tertiary;
                b = c[2] * tertiary;
                e[0] = r * 1.4f;
                e[1] = g * 1.4f;
                e[2] = b * 1.4f;
                cov = 255;
            } else if (src.a  ==254) {
                float hh = hash(o->id * 977 + (sx >> 1), row >> 2);
                if (hh < wl) {
                    const float *c = emCol(hh < wl * .5f ? 4 : 5);
                    float tertiary = .35f * (1 - fog * .6f);
                    r = c[0] * tertiary;
                    g = c[1] * tertiary;
                    b = c[2] * tertiary;
                    e[0] = r * .6f;
                    e[1] = g * .6f;
                    e[2] = b * .6f;
                }
            }
            q[0] = (unsigned char)fminf(255, r);
            q[1] = (unsigned char)fminf(255, g);
            q[2] = (unsigned char)fminf(255, b);
            q[3] = cov;
            q[4] = (unsigned char)fminf(255, e[0]);
            q[5] = (unsigned char)fminf(255, e[1]);
            q[6] = (unsigned char)fminf(255, e[2]);
        }
}
static void farSeq(FarStore *st, int lay, float upto) {
    while (st->end < upto) {
        Rng r = rngOf(lay ? 52 : 51, st->size + 1);
        int si = (int)(rnd(&r) * NSPR), gap = lay ? (int)(rnd(&r) * 16) - 6 : (int)(rnd(&r) * 30) - 8;
        if (st->size  ==st->cap) {
            st->cap = st->cap ? st->cap * 2 : 64;
            st->list = realloc(st->list, st->cap * sizeof(Far));
        }
        Far *o = &st->list[st->size];
        o->spr = si;
        o->x0 = st->end + gap;
        o->id = st->size;
        o->px = 0;
        bakeFar(o, lay);
        st->size++;
        st->end += gap + (lay ? FARR[si].w : MIDR[si].w);
    }
}
static Far *farAt(FarStore *st, int lay, int wx) {
    for (int counter = st->size - 1; counter >= 0; counter--) {
        Far *o = &st->list[counter];
        int w = lay ? FARR[o->spr].w : MIDR[o->spr].w;
        if (wx >= o->x0 && wx < o->x0 + w) return o;
        if (o->x0 + w < wx - 300) break;
    }
    return 0;
}

static void resetCity(void) {
    for (int counter = 0; counter < nb; counter++) freeBld(&blds[counter]);
    for (int counter = 0; counter < mids.size; counter++) free(mids.list[counter].px);
    for (int counter = 0; counter < fars.size; counter++) free(fars.list[counter].px);
    nb = 0;
    bEnd = -300;
    bId = 0;
    mids.size = fars.size = 0;
    mids.end = fars.end = -200;
    npf = nobj = npk = 0;
    lastAC = -1e9f;
    zk = -1;
    zn = 0;
    zNext = Z_G;
    zEnd = -1e9f;
}

#define RT 128
typedef struct {
    float v, tertiary, a;
    int size;
    float ox, oy, b;
    float pts[1024];
    int np;
} Rain;
static Rain rainL[2] = {{300, .5f, .09f, 45, 0, 0, -1, {0}, 0}, {560, 1, .17f, 22, 0, 0, -1, {0}, 0}};
static void buildRain(float scroll) {
    for (int l = 0; l < 2; l++) {
        Rain *R = &rainL[l];
        if (fabsf(R->b - scroll) < 8) continue;
        R->b = scroll;
        float sl = (R->v * .05f + scroll * R->tertiary) / R->v;
        Rng r = rngOf(99, (int)R->v);
        R->np = 0;
        for (int counter = 0; counter < R->size; counter++) {
            float value = rnd(&r) * RT, temp = rnd(&r) * RT;
            int total = 4 + (int)(rnd(&r) * 6);
            for (int jdx = 0; jdx < total && R->np < 1022; jdx++) {
                R->pts[R->np++] = value + jdx * sl;
                R->pts[R->np++] = temp - jdx;
            }
        }
    }
}
#define MW  (VW / 8)
#define MWM (VWM / 8)
#define MH  (VH / 8)
#define MHM (VHM / 8)
#define BW  (VW / 4)
#define BWM (VWM / 4)
#define BH  (VH / 4)
#define BHM (VHM / 4)
static float out[VWM * VHM * 3], emb[VWM * VHM * 3], big[SWM * SHM * 3], mist[(MWM + 2) * (MHM + 2)], blm[BWM * BHM * 3],
    buffer[BWM * BHM * 3];
static Color pix[SWM * SHM], bpix[BWM * BHM];
static const float FOG[3] = {34, 44, 41};

typedef struct {
    float value, temp, extra, R, s, f;
    const float *c;
} SLight;
#define MAXS 256
static SLight sl[MAXS];
static int nsl;
typedef struct {
    float value, temp, extra, R, s, f;
    const float *c;
} FLight;
static FLight fl[MAXS];
static int nfl;

static int mishuFrame(void);

static unsigned char mm[SWM * SHM];
static int mm0, mm1, behind;
static void drawSpr(int id, int X0, int Y0, int cx0, int cx1, float rot, const float *amb) {
    const int sx = OSP[id].value, sy = OSP[id].temp, w = OSP[id].w, h = OSP[id].h;
    const float dp = OSP[id].dpx, cs = cosf(rot), sn = sinf(rot);
    int pad = rot != 0 ? (w + h) / 3 : 0, xa = X0 - pad, xb = X0 + w + pad, ya = Y0 - pad, yb = Y0 + h + pad;
    SLight *ls[48];
    int nl = 0;
    float cxm = X0 + w / 2.f, cym = Y0 + h / 2.f, rad = (w > h ? w : h) * .75f;
    for (int jdx = 0; jdx < nsl && nl < 48; jdx++) {
        float dx = sl[jdx].value * 2 - cxm, dy = sl[jdx].temp * 2 - cym, R = sl[jdx].R * 2.6f + rad;
        if (dx * dx + dy * dy < R * R) ls[nl++] = &sl[jdx];
    }
    if (xa < cx0) xa = cx0;
    if (xb > cx1) xb = cx1;
    if (xb > SW) xb = SW;
    if (ya < 0) ya = 0;
    if (yb > SH) yb = SH;
    for (int Y = ya; Y < yb; Y++)
        for (int X = xa; X < xb; X++) {
            int u = X - X0, v = Y - Y0;
            if (rot != 0) {
                float rx = X - X0 - w * .5f + .5f, ry = Y - Y0 - h * .5f + .5f;
                u = (int)floorf(rx * cs + ry * sn + w * .5f);
                v = (int)floorf(-rx * sn + ry * cs + h * .5f);
            }
            if (u < 0 || v < 0 || u >= w || v >= h) continue;
            if (u < behind && mm[Y * SW + X]) continue;
            int s = (sy + v) * oAl.width + sx + u;
            Color A = OA[s];
            if (!A.a) continue;
            float *o = big + (Y * SW + X) * 3;
            if (A.a  ==254) {
                o[0] = A.r * .85f;
                o[1] = A.g * .85f;
                o[2] = A.b * .85f;
                float *e = emb + ((Y >> 1) * VW + (X >> 1)) * 3;
                e[0] += A.r * .05f;
                e[1] += A.g * .05f;
                e[2] += A.b * .05f;
                continue;
            }
            Color N = ON[s];
            float nx = N.r / 127.5f - 1, ny = -(N.g / 127.5f - 1), nz = N.b / 127.5f - 1, pz = (OD[s].r / 255.f - .5f) * dp, lr = amb[0],
                  lg = amb[1], lb = amb[2];
            if (rot != 0) {
                float a = nx * cs + ny * sn;
                ny = -nx * sn + ny * cs;
                nx = a;
            }
            for (int jdx = 0; jdx < nl; jdx++) {
                SLight *L = ls[jdx];
                float dx = L->value * 2 - X, dy = L->temp * 2 - Y, dz = L->extra * 2 - pz, d = sqrtf(dx * dx + dy * dy + dz * dz), R = L->R * 2.6f;
                if (d > R || d <= 0) continue;
                float nd = (nx * dx + ny * dy + nz * dz) / d;
                if (nd <= 0) continue;
                float n2 = nd * nd, n4 = n2 * n2, n8 = n4 * n4, n16 = n8 * n8;
                float s2 = (1 - d / R) * (1 - d / R) * L->f * L->s * (nd * 2.6f + n16 * 1.2f) / 255;
                lr += L->c[0] * s2;
                lg += L->c[1] * s2;
                lb += L->c[2] * s2;
            }
            o[0] = A.r * lr;
            o[1] = A.g * lg;
            o[2] = A.b * lb;
        }
}
static float acTilt(const Obj *o, float t) {
    if (o->st  ==1) return -.45f - o->vy * .002f;
    float d = o->value + OSP[o->spr].w / 4.f - px;
    if (d > 260 || d < -20) return 0;
    float tertiary = fminf(1.f, (260 - d) / 190);
    return -.4f * tertiary * tertiary + .05f * tertiary * sinf(t * 38);
}
static void shadow(int cx, int yb, int hw) {
    for (int temp = yb - 2; temp < yb + 5; temp++)
        for (int value = cx - hw; value < cx + hw; value++) {
            if (value < 0 || value >= SW || temp < 0 || temp >= SH) continue;
            float u = (value - cx) / (float)hw, v = (temp - yb - 1.5f) / 3.5f, d = u * u + v * v;
            if (d >= 1) continue;
            float *o = big + (temp * SW + value) * 3, tertiary = 1 - .6f * (1 - d);
            o[0] *= tertiary;
            o[1] *= tertiary;
            o[2] *= tertiary;
        }
}
#define HURDLE_SPLIT 34
static void drawObjs(float cam, float t, const float *amb0, int pass, int showMishu) {
    const float amb[3] = {amb0[0] * 1.7f + .08f, amb0[1] * 1.7f + .1f,
                          amb0[2] * 1.7f + .1f};
    for (int counter = 0; counter < nobj; counter++) {
        const Obj *o = &obj[counter];
        int id = o->spr, w = OSP[id].w, h = OSP[id].h, X = (int)floorf((o->value - cam) * 2), Y = (int)(o->temp * 2) - h;
        if (X >= SW || X + w < -40 || (o->type  ==O_AC && o->st >= 2)) continue;
        if (o->type  ==O_HURDLE) {
            if (pass  ==0)
                shadow(X + w / 2, (int)(o->temp * 2), w * 5 / 12);
            else {
                behind = HURDLE_SPLIT;
                drawSpr(id, X, Y, 0, SW, 0, amb);
                behind = 0;
            }
            continue;
        }
        if (pass != 0) continue;
        if (o->type  ==O_STAIRS) {
            drawSpr(id, X, Y, 0, SW, 0, amb);
            drawSpr(id, X + 100, Y - 59, 0, SW, 0, amb);
            continue;
        }
        if (o->type  ==O_TRASH || o->type  ==O_BARRIER || o->type  ==O_BOX || o->type  ==O_FUEL)
            shadow(X + (OSP[id].hx0 + OSP[id].hx1) / 2, (int)(o->temp * 2), (OSP[id].hx1 - OSP[id].hx0) * 5 / 12);
        drawSpr(id, X, Y, 0, SW, o->type  ==O_AC ? acTilt(o, t) : 0, amb);
    }
    int after = showMishu && lane  ==1;
    if (pass  ==after)
        for (int counter = 0; counter < npf; counter++) {
            const Plat *p = &pf[counter];
            if (p->kind != PK_BAL) continue;
            int cx0 = (int)floorf((p->x0 - cam) * 2), cx1 = p->x1 > 1e8f ? SW : (int)floorf((p->x1 - cam) * 2), tw = OSP[S_RAIL].w,
                Y = (int)((p->top) * 2) - OSP[S_RAIL].h;
            if (cx1 <= 0 || cx0 >= SW) continue;
            if (cx0 < 0) cx0 = 0;
            if (cx1 > SW) cx1 = SW;
            int k0 = (int)floorf((cam * 2 - p->x0 * 2) / tw);
            if (k0 < 0) k0 = 0;
            for (int tertiary = k0;; tertiary++) {
                int X = (int)floorf((p->x0 - cam) * 2) + tertiary * tw;
                if (X >= cx1) break;
                drawSpr(S_RAIL, X, Y, cx0, cx1, 0, amb);
            }
        }
}

static void render(float cam, float t, int showMishu) {
    memset(emb, 0, VW * VH * 3 * sizeof(float));
    float sunK = sunAt(cam + 240),
          fg[3] = {FOG[0] + (205 - FOG[0]) * sunK * .9f, FOG[1] + (150 - FOG[1]) * sunK * .9f, FOG[2] + (120 - FOG[2]) * sunK * .9f},
          hzK = 1.f - .45f * sunK;
    prune(cam);
    ensure((int)cam + VW + 300);
    farSeq(&mids, 0, cam * .4f + VW + 200);
    farSeq(&fars, 1, cam * .15f + VW + 200);
    for (int temp = 0; temp < GY; temp++) {
        float u = (float)temp / GY, r = 5 + 16 * u * u, g = 7 + 22 * u * u, b = 7 + 19 * u * u;
        {
            float pw = powf(u, 1.8f);
            r += (82 + 173 * pw - r) * sunK;
            g += (116 + 64 * pw - g) * sunK;
            b += (172 - 52 * pw - b) * sunK;
        }
        for (int value = 0; value < VW; value++) {
            float *o = out + (temp * VW + value) * 3;
            o[0] = r;
            o[1] = g;
            o[2] = b;
        }
    }
    if (sunK > .01f) {
        float cx = VW * .68f, cy = GY - 10 - 80 * sunK, rad = 15;
        for (int temp = (int)(cy - rad * 5); temp < (int)(cy + rad * 5); temp++)
            for (int value = (int)(cx - rad * 5); value < (int)(cx + rad * 5); value++) {
                if (value < 0 || value >= VW || temp < 0 || temp >= GY) continue;
                float dx = value - cx, dy = temp - cy, d = sqrtf(dx * dx + dy * dy), *o = out + (temp * VW + value) * 3;
                if (d < rad) {
                    float e = fminf(1.f, (rad - d) * .7f + .3f);
                    o[0] += (255 - o[0]) * e;
                    o[1] += (238 - o[1]) * e;
                    o[2] += (195 - o[2]) * e;
                    float *bound = emb + (temp * VW + value) * 3;
                    bound[0] += 120 * sunK;
                    bound[1] += 90 * sunK;
                    bound[2] += 50 * sunK;
                } else if (d < rad * 5) {
                    float tertiary = (1 - (d - rad) / (rad * 4));
                    tertiary = tertiary * tertiary * .55f * sunK;
                    o[0] += (255 - o[0]) * tertiary;
                    o[1] += (190 - o[1]) * tertiary;
                    o[2] += (130 - o[2]) * tertiary;
                }
            }
    }

    for (int lay = 1; lay >= 0; lay--) {
        FarStore *st = lay ? &fars : &mids;
        float p = lay ? .15f : .4f;
        int yo = lay ? -6 : 0;
        for (int value = 0; value < VW; value++) {
            int wx = (int)floorf(value + cam * p);
            Far *o = farAt(st, lay, wx);
            if (!o) continue;
            if (sunK > 0 && hash(o->id * 31 + lay, 5) < .92f * sunAt(o->x0 / p)) continue;
            const SprRect *R = lay ? &FARR[o->spr] : &MIDR[o->spr];
            int sx = wx - o->x0, top = GY + yo - R->h;
            for (int temp = top > 0 ? top : 0; temp < GY && temp < top + R->h; temp++) {
                unsigned char *q = o->px + ((temp - top) * R->w + sx) * 7;
                if (!q[3]) continue;
                float cov = q[3] / 255.f, *d = out + (temp * VW + value) * 3, *e = emb + (temp * VW + value) * 3;
                d[0] += (q[0] - d[0]) * cov;
                d[1] += (q[1] - d[1]) * cov;
                d[2] += (q[2] - d[2]) * cov;
                e[0] += q[4];
                e[1] += q[5];
                e[2] += q[6];
            }
        }
    }

    nsl = nfl = 0;
    int i0 = bAt((int)floorf(cam) - 130), i1 = bAt((int)floorf(cam) + VW + 130);
    for (int counter = i0 < 0 ? 0 : i0; counter <= (i1 < 0 ? nb - 1 : i1); counter++) {
        Bld *b = &blds[counter];
        if (b->gap) {
            if (b->glow && nsl < MAXS) sl[nsl++] = (SLight){b->x0 + b->w / 2.f - cam, GY - 26, -60, 110, .7f, .7f, emCol(3)};
            continue;
        }
        for (int tertiary = 0; tertiary < b->nl; tertiary++) {
            Light *L = &b->L[tertiary];
            float sx = L->value - cam;
            if (sx < -L->R || sx > VW + L->R) continue;
            float f = flick(L->g, t);
            if (nsl < MAXS) sl[nsl++] = (SLight){sx, L->temp, L->extra, L->R, L->s, f, emCol(L->t)};
            if (fclass(L->g) && nfl < MAXS) fl[nfl++] = (FLight){L->value, L->temp, L->extra, L->R, L->s, f, emCol(L->t)};
        }
    }
    float ambc[3] = {(.14f * AMB + .02f) * 2, (.22f * AMB + .03f) * 2, (.2f * AMB + .03f) * 2},
          ambm[3] = {(.14f * AMB_M + .02f) * 1.8f, (.22f * AMB_M + .03f) * 1.8f, (.2f * AMB_M + .03f) * 1.8f};
    ambc[0] += sunK * .30f;
    ambc[1] += sunK * .22f;
    ambc[2] += sunK * .12f;
    ambm[0] += sunK * .30f;
    ambm[1] += sunK * .22f;
    ambm[2] += sunK * .12f;
    {
        int bi = -1;
        for (int value = 0; value < VW; value++) {
            int wx = (int)floorf(value + cam);
            if (bi < 0 || bi >= nb || wx < blds[bi].x0 || wx >= blds[bi].x0 + blds[bi].w) bi = bAt(wx);
            if (bi < 0) continue;
            Bld *b = &blds[bi];
            if (b->gap) continue;
            if (!b->lit) bake(bi);
            int lx = wx - b->x0;
            for (int temp = 0; temp < GY; temp++) {
                int tertiary = temp * b->w + lx;
                if (!b->a[tertiary]) continue;
                float *o = out + (temp * VW + value) * 3, *bound = b->lit + tertiary * 3;
                float lr = ambc[0] + bound[0], lg = ambc[1] + bound[1], lb = ambc[2] + bound[2];
                if (nfl) {
                    float *size = b->size + tertiary * 4;
                    for (int jdx = 0; jdx < nfl; jdx++) {
                        FLight *L = &fl[jdx];
                        float dx = L->value - wx;
                        if (dx > L->R || dx < -L->R) continue;
                        float dy = L->temp - temp, dz = L->extra - size[3] + 8, d = sqrtf(dx * dx + dy * dy + dz * dz);
                        if (d > L->R || d <= 0) continue;
                        float s =
                            (1 - d / L->R) * (1 - d / L->R) * fmaxf(0, (size[0] * dx + size[1] * dy + size[2] * dz) / d) * 2.2f * L->s * L->f / 255;
                        lr += L->c[0] * s;
                        lg += L->c[1] * s;
                        lb += L->c[2] * s;
                    }
                }
                float r = b->al[tertiary * 3] * lr, g = b->al[tertiary * 3 + 1] * lg, bl = b->al[tertiary * 3 + 2] * lb;
                if (b->et[tertiary]) {
                    const float *ec = emCol(b->et[tertiary]);
                    float s = b->ei[tertiary] / 255.f * flick(b->eg[tertiary], t);
                    r = ec[0] * s;
                    g = ec[1] * s;
                    bl = ec[2] * s;
                    float *e = emb + (temp * VW + value) * 3;
                    e[0] += r;
                    e[1] += g;
                    e[2] += bl;
                }
                o[0] = r;
                o[1] = g;
                o[2] = bl;
            }
        }
    }

    for (int temp = 0; temp < VH; temp++)
        for (int value = 0; value < VW; value++) {
            float *o = out + (temp * VW + value) * 3;
            int q = (temp * 2 * SW + value * 2) * 3;
            for (int c = 0; c < 3; c++) big[q + c] = big[q + 3 + c] = big[q + SW * 3 + c] = big[q + SW * 3 + 3 + c] = o[c];
        }
    drawObjs(cam, t, ambm, 0, showMishu);

    if (mm1 > mm0) memset(mm + mm0, 0, mm1 - mm0);
    mm0 = SWM * SHM;
    mm1 = 0;
    if (showMishu) {
        int f = mishuFrame(), sw = mAl.width, cx = f * CW, cy = clip * CH,
            ox2 = (int)(PX * 2 + 2 * LEADMAX * tanhf((px - PX - cam) / LEADMAX)) - CW / 2, oy2 = (int)(py * 2) - CGY;
        if (boostT > 0 || phaseT > 0)
            for (int g = 3; g >= 1; g--) {
                for (int temp = 0; temp < CH; temp++)
                    for (int value = 0; value < CW; value++) {
                        int s = (cy + temp) * sw + cx + value;
                        Color A = MA[s];
                        if (!A.a) continue;
                        int X = ox2 + value - g * 26, Y = oy2 + temp;
                        if (X < 0 || X >= SW || Y < 0 || Y >= SGY + 4) continue;
                        float *o = big + (Y * SW + X) * 3, a = .36f / g * (A.a / 255.f), cr = phaseT > 0 ? 90 : 200,
                              cg = phaseT > 0 ? 170 : 245, cb = phaseT > 0 ? 255 : 90;
                        o[0] += (cr - o[0]) * a;
                        o[1] += (cg - o[1]) * a;
                        o[2] += (cb - o[2]) * a;
                    }
            }
        for (int temp = 0; temp < CH; temp++)
            for (int value = 0; value < CW; value++) {
                int s = (cy + temp) * sw + cx + value;
                Color A = MA[s];
                if (!A.a) continue;
                int X = ox2 + value, Y = oy2 + temp;
                if (X < 0 || X >= SW || Y < 0 || Y >= SGY + 4) continue;
                float *o = big + (Y * SW + X) * 3;
                {
                    int q = Y * SW + X;
                    mm[q] = 1;
                    if (q < mm0) mm0 = q;
                    if (q >= mm1) mm1 = q + 1;
                }
                Color N = MN[s];
                float nx = N.r / 127.5f - 1, ny = -(N.g / 127.5f - 1), nz = N.b / 127.5f - 1, pz = (MD[s].r / 255.f - .5f) * DPX,
                      lr = ambm[0], lg = ambm[1], lb = ambm[2];
                for (int jdx = 0; jdx < nsl; jdx++) {
                    SLight *L = &sl[jdx];
                    float dx = L->value * 2 - X, dy = L->temp * 2 - Y, dz = L->extra * 2 - pz, d = sqrtf(dx * dx + dy * dy + dz * dz), R = L->R * 2.6f;
                    if (d > R || d <= 0) continue;
                    float nd = fmaxf(0, (nx * dx + ny * dy + nz * dz) / d);
                    if (nd <= 0) continue;
                    float d2 = sqrtf(dx * dx + dy * dy);
                    int st = (int)d2 < 40 ? (int)d2 : 40, blk = 0;
                    for (int q = 3; q <= st; q++) {
                        float u = q / d2;
                        int sx2 = (int)roundf(value + dx * u), sy2 = (int)roundf(temp + dy * u);
                        if (sx2 < 0 || sx2 >= CW || sy2 < 0 || sy2 >= CH) break;
                        int s3 = (cy + sy2) * sw + cx + sx2;
                        if (!MA[s3].a) continue;
                        float zs = (MD[s3].r / 255.f - .5f) * DPX, rz = pz + dz * u;
                        if (rz < zs - 1 && rz > zs - 18) {
                            blk = 1;
                            break;
                        }
                    }
                    if (blk) continue;
                    float s2 = (1 - d / R) * (1 - d / R) * L->f * L->s * (nd * 2.6f + powf(nd, 16) * 1.2f) / 255;
                    lr += L->c[0] * s2;
                    lg += L->c[1] * s2;
                    lb += L->c[2] * s2;
                }
                o[0] = A.r * lr;
                o[1] = A.g * lg;
                o[2] = A.b * lb;
                if (invul > 0 && !dead && ((int)(t * 9) & 1)) {
                    o[0] = o[0] * .35f + 255 * .65f;
                    o[1] = o[1] * .35f + 138 * .65f;
                    o[2] = o[2] * .35f + 40 * .65f;
                }
            }
    }
    drawObjs(cam, t, ambm, 1, showMishu);

    for (int jdx = 0; jdx < MH + 2; jdx++)
        for (int counter = 0; counter < MW + 2; counter++)
            mist[jdx * (MW + 2) + counter] =
                vnoise(counter * .35f + cam * .004f + t * .05f, jdx * .5f + t * .02f) * .6f + vnoise(counter * .9f - t * .12f, jdx * 1.1f) * .4f;
    for (int temp = 0; temp < SGY; temp++) {
        float hf = HAZE * hzK * (.08f + .32f * powf((float)temp / SGY, 2.2f));
        for (int value = 0; value < SW; value++) {
            float f = fminf(.85f, hf * (.55f + mist[(temp >> 4) * (MW + 2) + (value >> 4)] * .9f)), *o = big + (temp * SW + value) * 3;
            o[0] += (fg[0] + 6 - o[0]) * f;
            o[1] += (fg[1] + 8 - o[1]) * f;
            o[2] += (fg[2] + 6 - o[2]) * f;
        }
    }

    for (int temp = SGY; temp < SH; temp++) {
        int dd = temp - SGY, ry = (int)(SGY - 1 - dd * 1.15f), wy = temp >> 1, wd = dd >> 1;
        for (int value = 0; value < SW; value++) {
            int wx = value >> 1;
            float *o = big + (temp * SW + value) * 3, size = hash(wx + (int)floorf(cam), wy) * 6, rr = 7 + size, rg = 10 + size, rb = 10 + size;
            if (ry >= 0) {
                int rx = value + 2 * (int)roundf(sinf(wy * 1.7f + t * 4) * (1 + wd * .06f));
                rx = rx < 0 ? 0 : rx >= SW ? SW - 1 : rx;
                float *q = big + (ry * SW + rx) * 3, tertiary = .45f * (1 - (float)dd / (SH - SGY)) * (.7f + .3f * hash(wx >> 2, wy));
                rr += q[0] * tertiary;
                rg += q[1] * tertiary;
                rb += q[2] * tertiary;
                if (!(value & 1) && !(temp & 1)) {
                    float *eq = emb + ((ry >> 1) * VW + (rx >> 1)) * 3, *e = emb + (wy * VW + wx) * 3;
                    e[0] += eq[0] * tertiary * .6f;
                    e[1] += eq[1] * tertiary * .6f;
                    e[2] += eq[2] * tertiary * .6f;
                }
            }
            if (wy  ==GY) {
                rr += 14;
                rg += 20;
                rb += 18;
            }
            float f = HAZE * .25f * hzK;
            o[0] = rr + (fg[0] - rr) * f;
            o[1] = rg + (fg[1] - rg) * f;
            o[2] = rb + (fg[2] - rb) * f;
        }
    }

    for (int value = 0; value < VW; value++) {
        int wx = (int)floorf(value + cam * 1.6f), ci = (int)floorf(wx / 300.f), lx2 = wx - ci * 300;
        Rng r = rngOf(4242, ci);
        if (rnd(&r) < .3f) {
            int px0 = (int)(rnd(&r) * 280);
            if (lx2 >= px0 && lx2 < px0 + 4)
                for (int temp = 0; temp < SH; temp++)
                    for (int counter = 0; counter < 2; counter++) {
                        float *o = big + (temp * SW + value * 2 + counter) * 3;
                        o[0] = 3;
                        o[1] = 5;
                        o[2] = 5;
                    }
        }
        float y0 = 6 + rnd(&r) * 14, sag = 10 + rnd(&r) * 18, u = lx2 / 300.f;
        int cy = (int)(y0 + sag * 4 * u * (1 - u));
        for (int jdx = 0; jdx < 2; jdx++)
            for (int counter = 0; counter < 2; counter++) {
                float *o = big + ((cy * 2 + jdx) * SW + value * 2 + counter) * 3;
                o[0] = 4;
                o[1] = 6;
                o[2] = 6;
            }
    }

    for (int l = 0; l < 2 && sunK < .97f; l++) {
        Rain *R = &rainL[l];
        float ra = R->a * (1 - sunK);
        for (int ty = 0; ty < VH; ty += RT)
            for (int tx = 0; tx < VW; tx += RT)
                for (int q = 0; q < R->np; q += 2) {
                    int X = tx + (int)fmodf(fmodf(R->pts[q] + R->ox, RT) + RT, RT),
                        Y = ty + (int)fmodf(fmodf(R->pts[q + 1] + R->oy, RT) + RT, RT);
                    if (X >= VW || Y >= VH) continue;
                    for (int jdx = 0; jdx < 2; jdx++)
                        for (int counter = 0; counter < 2; counter++) {
                            float *o = big + ((Y * 2 + jdx) * SW + X * 2 + counter) * 3;
                            o[0] += (110 - o[0]) * ra;
                            o[1] += (140 - o[1]) * ra;
                            o[2] += (130 - o[2]) * ra;
                        }
                }
    }

    float gBri = BRI * (1 + .2f * sunK), gSat = SAT - .3f * sunK, gCon = CON - .12f * sunK;
    for (int counter = 0; counter < SW * SH; counter++) {
        float *c3 = big + counter * 3;
        if (sunK > .01f) {
            float w = sunK * .26f * (.3f + .7f * (float)(counter / SW) / SH);
            c3[0] += (255 - c3[0]) * w;
            c3[1] += (178 - c3[1]) * w;
            c3[2] += (120 - c3[2]) * w;
        }
        float r = c3[0] * gBri, g = c3[1] * gBri, b = c3[2] * gBri, l = .3f * r + .59f * g + .11f * b;
        r = (l + (r - l) * gSat - 40) * gCon + 40;
        g = (l + (g - l) * gSat - 40) * gCon + 40;
        b = (l + (b - l) * gSat - 40) * gCon + 40;
        pix[counter] = (Color){(unsigned char)fminf(255, fmaxf(0, r)), (unsigned char)fminf(255, fmaxf(0, g)),
                         (unsigned char)fminf(255, fmaxf(0, b)), 255};
    }

    for (int temp = 0; temp < BH; temp++)
        for (int value = 0; value < BW; value++) {
            float s[3] = {0};
            for (int jdx = 0; jdx < 4; jdx++)
                for (int counter = 0; counter < 4; counter++) {
                    float *e = emb + ((temp * 4 + jdx) * VW + value * 4 + counter) * 3;
                    s[0] += e[0];
                    s[1] += e[1];
                    s[2] += e[2];
                }
            for (int c = 0; c < 3; c++) blm[(temp * BW + value) * 3 + c] = s[c] / 16;
        }
    for (int pass = 0; pass < 2; pass++) {
        for (int temp = 0; temp < BH; temp++)
            for (int value = 0; value < BW; value++)
                for (int c = 0; c < 3; c++) {
                    float s = 0;
                    int size = 0;
                    for (int counter = -4; counter <= 4; counter++) {
                        int xx = value + counter;
                        if (xx < 0 || xx >= BW) continue;
                        s += blm[(temp * BW + xx) * 3 + c];
                        size++;
                    }
                    buffer[(temp * BW + value) * 3 + c] = s / size;
                }
        for (int temp = 0; temp < BH; temp++)
            for (int value = 0; value < BW; value++)
                for (int c = 0; c < 3; c++) {
                    float s = 0;
                    int size = 0;
                    for (int jdx = -4; jdx <= 4; jdx++) {
                        int yy = temp + jdx;
                        if (yy < 0 || yy >= BH) continue;
                        s += buffer[(yy * BW + value) * 3 + c];
                        size++;
                    }
                    blm[(temp * BW + value) * 3 + c] = s / size;
                }
    }
    float bloomK = BLOOM * (1.4f + HAZE * .6f);
    for (int counter = 0; counter < BW * BH; counter++)
        bpix[counter] = (Color){(unsigned char)fminf(255, blm[counter * 3] * bloomK), (unsigned char)fminf(255, blm[counter * 3 + 1] * bloomK),
                          (unsigned char)fminf(255, blm[counter * 3 + 2] * bloomK), 255};

    for (int counter = 0; counter < nb; counter++)
        if (blds[counter].x0 + blds[counter].w < cam - 600 && blds[counter].al) freeBld(&blds[counter]);
    for (int counter = 0; counter < mids.size; counter++)
        if (mids.list[counter].px && mids.list[counter].x0 + 300 < cam * .4f - 300) {
            free(mids.list[counter].px);
            mids.list[counter].px = 0;
        }
    for (int counter = 0; counter < fars.size; counter++)
        if (fars.list[counter].px && fars.list[counter].x0 + 300 < cam * .15f - 300) {
            free(fars.list[counter].px);
            fars.list[counter].px = 0;
        }
}

static void play(int c) {
    if (clip != c) {
        clip = c;
        animT = 0;
    }
}
static int clipFrame(void) { return (int)(animT * 24 * CLIPS[clip].rate); }
static int clipDone(void) { return !CLIPS[clip].loop && clipFrame() >= CLIPS[clip].size; }
static void toSprint(int f) {
    clip = C_SPRINT;
    animT = (f + .5f) / 24;
}
static void advance(void) {
    const Clip *c = &CLIPS[clip];
    if (c->then  ==C_SPRINT)
        toSprint(c->thenFrame);
    else if (c->then >= 0)
        play(c->then);
}
static int mishuFrame(void) {
    const Clip *c = &CLIPS[clip];
    int f;
    if (clip  ==C_DASH)
        f = (int)((1 - dash / DASH_T) * c->size);
    else
        f = clipFrame();
    f -= f % FSTEP;
    return c->loop ? f % c->size : (f >= c->size ? c->size - 1 : f);
}

enum { X_JUMP, X_SLIDE, X_DASH, X_T1, X_T2, X_DRINK, X_SHIELD, X_SHUSE, X_TRIP, X_DEATH, X_WIN, X_CLICK, X_N };
static Sound sfxS[X_N];
static int sfxOk;
static void sfx(int id) {
    if (sfxOk) PlaySound(sfxS[id]);
}
static void musicRestart(void);
static void sfxMake(void) {
    static const float DUR[X_N] = {.28f, .38f, .32f, .22f, .26f, .4f, .45f, .35f, .32f, .9f, 1.1f, .05f};
    static const float VOL[X_N] = {.55f, .45f, .5f, .5f, .55f, .55f, .55f, .6f, .75f, .7f, .6f, .35f};
    const int R = 22050;
    unsigned rs = 12345;
    for (int id = 0; id < X_N; id++) {
        int size = (int)(DUR[id] * R);
        short *d = malloc(size * sizeof(short));
        float lp = 0, ph = 0, ph2 = 0;
        for (int counter = 0; counter < size; counter++) {
            float t = counter / (float)R, u = t / DUR[id], v = 0;
            rs = rs * 1664525u + 1013904223u;
            float nz = ((rs >> 9) / 4194304.f) - 1.f;
            lp += (nz - lp) * .18f;
            switch (id) {
                case X_JUMP:
                    ph += (280 + 620 * u) * 6.2832f / R;
                    v = sinf(ph) * expf(-u * 3.5f) * .5f;
                    break;
                case X_SLIDE: v = lp * sinf(u * 3.14159f) * 1.3f; break;
                case X_DASH:
                    ph += (240 + 900 * u * u) * 6.2832f / R;
                    v = (lp * .9f + sinf(ph) * .25f) * sinf(fminf(1.f, u * 1.4f) * 3.14159f) * .8f;
                    break;
                case X_T1:
                    ph += (t < .07f ? 880 : 1320) * 6.2832f / R;
                    v = sinf(ph) * expf(-u * 4.f) * .5f;
                    break;
                case X_T2:
                    ph += (t < .07f ? 1175 : 1760) * 6.2832f / R;
                    ph2 += (t < .07f ? 1760 : 2350) * 6.2832f / R;
                    v = (sinf(ph) + .4f * sinf(ph2)) * expf(-u * 3.5f) * .4f;
                    break;
                case X_DRINK: {
                    static const float F[4] = {520, 660, 780, 1040};
                    int tertiary = (int)(t / .07f);
                    if (tertiary > 3) tertiary = 3;
                    ph += F[tertiary] * 6.2832f / R;
                    v = (sinf(ph) + .3f * sinf(ph * 2)) * expf(-u * 2.2f) * .4f;
                } break;
                case X_SHIELD:
                    ph += (380 + 900 * u) * 6.2832f / R;
                    v = sinf(ph) * (.7f + .3f * sinf(t * 60)) * sinf(u * 3.14159f) * .5f;
                    break;
                case X_SHUSE:
                    ph += (900 - 650 * u) * 6.2832f / R;
                    v = (sinf(ph) * .4f + lp * .8f) * expf(-u * 4.f);
                    break;
                case X_TRIP:
                    ph += (150 - 90 * u) * 6.2832f / R;
                    v = (sinf(ph) * .8f + nz * .5f * expf(-u * 14.f)) * expf(-u * 5.f);
                    break;
                case X_DEATH:
                    ph += (330 - 270 * u) * 6.2832f / R;
                    v = ((fmodf(ph / 6.2832f, 1.f) - .5f) * .9f + lp * .2f) * expf(-u * 2.2f) * .6f;
                    break;
                case X_WIN: {
                    static const float F[4] = {523, 659, 784, 1047};
                    int tertiary = (int)(t / .14f);
                    if (tertiary > 3) tertiary = 3;
                    ph += F[tertiary] * 6.2832f / R;
                    v = (sinf(ph) + .35f * sinf(ph * 2)) * (tertiary  ==3 ? expf(-(t - .42f) * 3.f) : 1.f) * .4f;
                } break;
                default:
                    ph += 1000 * 6.2832f / R;
                    v = sinf(ph) * (1.f - u) * .6f;
                    break;
            }
            if (counter < 40) v *= counter / 40.f;
            v = fmaxf(-1.f, fminf(1.f, v));
            d[counter] = (short)(v * 30000);
        }
        Wave w = {(unsigned)size, R, 16, 1, d};
        sfxS[id] = LoadSoundFromWave(w);
        free(d);
        SetSoundVolume(sfxS[id], VOL[id]);
    }
    sfxOk = sfxS[0].stream.buffer != 0;
}
static int deathBy;
static void startRun(void) {
    px = PX;
    camX = 0;
    py = GY;
    vy = 0;
    tleft = tstart();
    dash = cd = apex = stumbleT = invul = lost = 0;
    deathBy = 0;
    slide = -1;
    dead = 0;
    deadT = 0;
    pj = pd = ps = 0;
    inAir = lane = 0;
    boostT = shieldT = phaseT = flashT = 0;
    won = banked = 0;
    npk = 0;
    clip = -1;
    play(C_SPRINT);
    scene = PLAY;
    musicRestart();
    seed = (unsigned)GetRandomValue(1, 1000000);
    resetCity();
#ifdef TESTDBG
    if (getenv("TGEN")) {
        void dbgDump(void);
        ensure(atoi(getenv("TGEN")));
        dbgDump();
        exit(0);
    }
    if (getenv("TPOW")) { boostT = shieldT = 30; }
    if (getenv("TSTART")) {
        px = PX + atof(getenv("TSTART"));
        camX = px - PX;
    }
#endif
    for (int l = 0; l < 2; l++) rainL[l].b = -1;
}

static float visX(float p) { return PX + camX + LEADMAX * tanhf((p - PX - camX) / LEADMAX); }
static float unvisX(float v) {
    float a = fmaxf(-.98f, fminf(.98f, (v - PX - camX) / LEADMAX));
    return PX + camX + LEADMAX * atanhf(a);
}
static void kill(void) {
#ifdef TESTDBG
    if (getenv("NODIE")) return;
#endif
    if (!dead) {
        dead = 1;
        deadT = 0;
        sfx(X_DEATH);
        dash = 0;
        slide = -1;
        best = fmaxf(best, px);
        if (py >= supportY(visX(px), py, 0) - .5f)
            play(C_DEATH);
        else if (clip != C_JAIR)
            play(C_JAIR);
    }
}

static float bodyH(int air) {
    return slide >= 0 ? 18.f : clip  ==C_SEXIT ? 30.f : clip  ==C_ROLL ? 26.f : air ? 34.f : clip  ==C_DASH ? 36.f : 46.f;
}
static int hardStumble;
#ifdef TESTDBG
static void stumbleLog(void) {
    if (!getenv("TST")) return;
    printf("STUMBLE px %.0f lane %d dash %.2f vy %.0f py %.0f:", px, lane, dash, vy, GY - py);
    for (int counter = 0; counter < nobj; counter++)
        if (obj[counter].value > px - 80 && obj[counter].value < px + 60) printf(" ob%d@%.0f(l%d)", obj[counter].type, obj[counter].value - px, obj[counter].lane);
    for (int counter = 0; counter < npf; counter++)
        if (pf[counter].kind != PK_RAMP && (fabsf(pf[counter].x0 - px) < 60 || (pf[counter].x1 < 1e8 && fabsf(pf[counter].x1 - px) < 60)))
            printf(" pf%d[%.0f,%.0f]", pf[counter].kind, pf[counter].x0 - px, pf[counter].x1 > 1e8 ? 9999.f : pf[counter].x1 - px);
    printf("\size");
}
#else
#define stumbleLog()
#endif
static void stumble(void) {
    if (invul > 0) return;
    stumbleLog();
    sfx(X_TRIP);
    hardStumble = dash > 0;
    if (hardStumble) {
        dash = 0;
        stumbleT = 1.f;
        invul = 1.5f;
    } else {
        stumbleT = .7f;
        invul = 1.2f;
    }
    lost += STUMBLE_LOSS;
    if (!devTime) tleft -= STUMBLE_LOSS;
    play(C_STUMBLE);
}
static int laneOf(int tertiary) { return tertiary  ==PK_BAL ? 1 : tertiary  ==PK_ROOF ? 2 : tertiary  ==PK_BLOCK ? 3 : 0; }
static void addT(float d) {
    if (d < 0 && devTime) return;
    tleft = fminf(tcap(), fmaxf(0.f, tleft + d));
}
static void flash(int ln, const char *fmt, ...) {
    (void)ln;
    va_list a;
    va_start(a, fmt);
    vsnprintf(flashTxt, sizeof flashTxt, fmt, a);
    va_end(a);
    flashT = 1.6f;
}
static int shieldHit(void) {
    if (phaseT > 0) return 1;
    if (shieldT > 0) {
        shieldT = 0;
        phaseT = .8f;
        flash(0, "SHIELD USED");
        sfx(X_SHUSE);
        return 1;
    }
    return 0;
}

static void bankRun(void) {
    if (banked) return;
    banked = 1;
    int bound = (int)((px - PX) / 10);
    lastGain = (bound > 0 ? bound / 10 : 0) + (won ? 500 : 0);
    sv.credits += lastGain;
    best = fmaxf(best, px);
    sv.best = best;
    saveGame();
}

static Music mus[2];
static int musOk, musCur, musOn, musOther;
static float musGain, musLen;
#define MUS_XF 5.f
static float musVol(float tp) { return sqrtf(fminf(1.f, tp / MUS_XF)) * sqrtf(fminf(1.f, fmaxf(0.f, musLen - tp) / MUS_XF)); }
static int musPaused;
static void musicRestart(void) {
    if (!musOk) return;
    StopMusicStream(mus[0]);
    StopMusicStream(mus[1]);
    musOn = musOther = musPaused = 0;
    musCur = 0;
    musGain = 0;
}
static void musicTick(float dt, float target) {
    if (!musOk) return;
    musGain += (target - musGain) * fminf(1.f, dt * (target > 0 ? 2.5f : 4.f));
    Music *c = &mus[musCur], *o = &mus[1 - musCur];
    if (target <= 0 && musGain < .01f) {
        if (musOn && !musPaused) {
            PauseMusicStream(*c);
            if (musOther) PauseMusicStream(*o);
            musPaused = 1;
        }
        return;
    }
    if (musPaused) {
        ResumeMusicStream(*c);
        if (musOther) ResumeMusicStream(*o);
        musPaused = 0;
    }
    if (!musOn) {
        musLen = GetMusicTimeLength(*c);
        PlayMusicStream(*c);
        musOn = 1;
    }
    UpdateMusicStream(*c);
    if (musOther) UpdateMusicStream(*o);
    float tc = GetMusicTimePlayed(*c);
    if (!musOther && tc >= musLen - MUS_XF) {
        PlayMusicStream(*o);
        musOther = 1;
    }
    if (musOther && (tc >= musLen - .05f || !IsMusicStreamPlaying(*c))) {
        StopMusicStream(*c);
        musCur = 1 - musCur;
        musOther = 0;
        c = &mus[musCur];
        o = &mus[1 - musCur];
    }
    SetMusicVolume(*c, musGain * musVol(GetMusicTimePlayed(*c)));
    if (musOther) SetMusicVolume(*o, musGain * musVol(GetMusicTimePlayed(*o)));
}
static float camV;
#ifdef TESTDBG
void dbgDump(void) {
    for (int counter = 0; counter < npf; counter++)
        printf("PF %d value %.0f..%.0f top %.0f\size", pf[counter].kind, pf[counter].x0, pf[counter].x1 > 1e8 ? -1.f : pf[counter].x1, GY - pf[counter].top);
    for (int counter = 0; counter < nobj; counter++) printf("OB %d spr %d value %.0f\size", obj[counter].type, obj[counter].spr, obj[counter].value);
    for (int counter = 0; counter < npk; counter++) printf("PK %d value %.0f temp %.0f\size", pk[counter].type, pk[counter].value, GY - pk[counter].temp);
    fflush(stdout);
}
#endif

static void stepPlay(float dt) {
    if (dead) {
        camV = 0;
        float sp = supportY(visX(px), py, 0);
        if (py < sp - .5f || vy < 0) {
            vy += GRAV * dt;
            py += vy * dt;
            animT += dt;
            if (py >= sp) {
                py = sp;
                vy = 0;
                play(C_DEATH);
            }
            return;
        }
        deadT += dt;
        animT += dt;
        return;
    }
#ifdef TESTDBG
    {
        static int tertiary;
        if (++tertiary % 120  ==0) { printf("t px %.0f py %.0f lane %d lost %.1f\size", px, GY - py, lane, lost); }
        if (tertiary  ==600) dbgDump();
    }
#endif
    int sk;
    float sup = supportY(visX(px), py, &sk);
    int ground = vy >= 0 && py >= sup - .5f;
    if (stumbleT > 0) pj = pd = ps = 0;
    if (pj && ground) {
        vy = -JUMP_V;
        py -= 1;
        slide = -1;
        play(C_JSTART);
        sfx(X_JUMP);
    }
    if (pd && cd <= 0 && slide < 0) {
        dash = DASH_T;
        cd = DASH_CD;
        play(C_DASH);
        sfx(X_DASH);
    }
    if (ps && ground && dash <= 0 && slide < 0) {
        slide = 0;
        play(C_SSTART);
        sfx(X_SLIDE);
    }
    pj = pd = ps = 0;
    if (stumbleT > 0) stumbleT -= dt;
    if (invul > 0) invul -= dt;
    if (phaseT > 0) phaseT -= dt;
    if (boostT > 0) boostT -= dt;
    if (shieldT > 0) shieldT -= dt;
    if (flashT > 0) flashT -= dt;

    float bound = slide >= 0        ? 1 - (1 - SLIDE_V) * slide / SLIDE_T
              : clip  ==C_SEXIT ? SLIDE_V + (1 - SLIDE_V) * fminf(1.f, (float)clipFrame() / CLIPS[C_SEXIT].size)
                                : 1;
    if (stumbleT > 0) bound *= hardStumble ? .3f : .55f;
    float base = RUN_V * bound * (boostT > 0 ? 1.3f : 1.f), prevX = px;
    px += (dash > 0 ? DASH_V : base) * dt;

    int blocked = 0, hitFace = 0, crash = 0;
    float h = bodyH(!ground);
    for (int counter = 0; counter < npf; counter++) {
        const Plat *q = &pf[counter];
        if (q->kind  ==PK_RAMP || py <= q->top + STEPUP || py - h >= q->bot) continue;
        float vp = visX(px), vq = visX(prevX);
        if (vq + PW <= q->x0 + .5f && vp + PW > q->x0 && vp - PW < q->x1) {
            if (q->kind  ==PK_BLOCK && shieldHit()) continue;
            px = unvisX(q->x0 - PW);
            blocked = 1;
            if (q->kind != PK_BLOCK)
                hitFace = 1;
            else
                crash = 1;
        }
    }
    if (crash) {
        deathBy = 1;
        kill();
        camV = 0;
#ifdef TESTDBG
        if (getenv("NODIE"))
            crash = 0;
        else
#endif
            return;
    }
    if (hitFace) {
        stumble();
        if (vy < 0) vy = 0;
    }
    if (blocked) base = 0;
    float lead = px - PX - camX;
    camV = base + fminf(dash > 0 ? 70.f : 110.f, fmaxf(0.f, lead) * 3.5f);
    camX += camV * dt;
    if (cd > 0) cd -= dt;
    if (dash > 0) dash -= dt;
    if (slide >= 0 && (slide += dt) >= SLIDE_T) {
        slide = -1;
        play(C_SEXIT);
    }

    sup = supportY(visX(px), py, &sk);
    ground = vy >= 0 && py >= sup - .5f;
    if (ground) {
        py = sup;
        vy = 0;
        lane = laneOf(sk);
    } else {
        float pyPrev = py, h2 = bodyH(1);
        if (!inAir) {
            inAir = 1;
            apex = py;
        }
        vy += GRAV * dt;
        py += vy * dt;
        apex = fminf(apex, py);
        if (vy < 0)
            for (int counter = 0; counter < npf; counter++) {
                const Plat *q = &pf[counter];
                if ((q->kind != PK_BAL && q->kind != PK_ROOF) || visX(px) + 3 <= q->x0 || visX(px) - 3 >= q->x1) continue;
                if (pyPrev - h2 >= q->bot - .01f && py - h2 < q->bot) {
                    py = q->bot + h2;
                    vy = 0;
                    break;
                }
            }
        if (vy > 0) {
            int k2;
            float s2 = supportY(visX(px), pyPrev, &k2);
            if (py >= s2) {
                py = s2;
                vy = 0;
                ground = 1;
                lane = laneOf(k2);
            }
        }
    }
    if (ground && inAir) {
        inAir = 0;
        if (py - apex >= ROLL_H)
            play(C_ROLL);
        else if (dash <= 0 && slide < 0)
            play(C_JLAND);
    }

    {
        float bh = bodyH(!ground), L = visX(px) - PW, R = visX(px) + PW, T = py - bh, B = py;
        for (int counter = 0; counter < nobj; counter++) {
            Obj *o = &obj[counter];
            float w = OSP[o->spr].w / 2.f, ht = OSP[o->spr].h / 2.f;
            if (o->type  ==O_AC) {
                if (o->st  ==0) {
                    float d = o->value + w / 2 - visX(px), fall = o->ty - o->temp;
                    if (d < RUN_V * sqrtf(2 * fall / GRAV) * .85f && d > -10) {
                        o->st = 1;
                        o->vy = 0;
                    }
                }
                else if (o->st  ==1) {
                    o->vy += GRAV * dt;
                    o->temp += o->vy * dt;
                    int land = o->temp >= o->ty;
                    if (land) o->temp = o->ty;
                    if (invul <= 0 && dash <= 0 && phaseT <= 0 && L < o->value + w - 1 && R > o->value + 1 && T < o->temp && B > o->temp - ht + 1) {
                        if (!shieldHit()) {
                            stumble();
                            lost += .5f;
                            addT(-.5f);
                        }
                        land = 1;
                    } else if (land && dash > 0 && fabsf(o->value + w / 2 - visX(px)) < 50 && fabsf(o->ty - py) < 4) {
                        addT(.2f);
                        flash(0, "DODGE +0.2 SEC");
                        sfx(X_T1);
                    }
                    if (land) {
                        o->st = 2;
                        o->t = 0;
                    }
                } else if (o->st  ==2 && (o->t += dt) > .5f)
                    o->st = 3;
                continue;
            }
            if (o->hit || invul > 0 || o->type  ==O_STAIRS || o->type  ==O_BOX || o->type  ==O_FUEL)
                continue;
            float t0 = o->type  ==O_HURDLE ? o->temp - ht : o->temp - ht + 1,
                  t1 = o->type  ==O_HURDLE ? o->temp - ht + 18 : o->temp;
            if (L < o->value + OSP[o->spr].hx1 / 2.f - 1 && R > o->value + OSP[o->spr].hx0 / 2.f + 1 && T < t1 && B > t0) {
                o->hit = 1;
                if (!shieldHit()) stumble();
            }
        }
    }
    for (int counter = 0; counter < npk; counter++) {
        Pick *q = &pk[counter];
        if (fabsf(q->value - visX(px)) > PW + 12 || q->temp < py - bodyH(!ground) - 10 || q->temp > py + 6) continue;
        if (q->type  ==P_T1 || q->type  ==P_T2) {
            int v = q->type  ==P_T2 ? 2 : 1;
            addT((float)v);
            flash(0, "+%d SEC", v);
            sfx(v  ==2 ? X_T2 : X_T1);
        } else if (q->type  ==P_DRINK) {
            boostT = pdur();
            flash(0, "ENERGY BOOST");
            sfx(X_DRINK);
        } else {
            shieldT = pdur();
            flash(0, "SHIELD");
            sfx(X_SHIELD);
        }
        pk[counter--] = pk[--npk];
    }
    if (!endless && !won && px - PX >= GOAL_PX) won = 1;
    if (!devTime) {
        tleft -= dt;
        if (tleft <= 0) {
            tleft = 0;
            kill();
            return;
        }
    }

    animT += dt;
    if (dash > 0) {
        play(C_DASH);
        return;
    }
    if (clip  ==C_DASH) {
        if (!ground)
            play(C_JAIR);
        else
            advance();
        return;
    }
    if ((clip  ==C_ROLL || clip  ==C_STUMBLE) && !clipDone()) return;
    if (!ground) {
        if (clip  ==C_JSTART) {
            if (clipDone()) advance();
        } else
            play(C_JAIR);
    } else if (slide >= 0) {
        if (clip  ==C_SSTART && clipDone())
            advance();
        else if (clip != C_SSTART && clip != C_SLOOP)
            play(C_SLOOP);
    } else if (clip  ==C_JLAND || clip  ==C_ROLL || clip  ==C_SEXIT || clip  ==C_STUMBLE) {
        if (clipDone()) advance();
    } else if (clip != C_SPRINT)
        play(C_SPRINT);
}

#ifdef TESTDBG

static unsigned bseed = 12345;
static float brnd(void) {
    bseed = bseed * 1664525u + 1013904223u;
    return (bseed >> 8) / 16777216.f;
}
static float botErr;
static int botClimb = 1, botDash = 1, botTok = 1;
static void botThink(void) {
    pj = pd = ps = 0;
    if (dead || stumbleT > 0) return;
    float X = visX(px) + PW;
    int ground = vy >= 0 && py >= supportY(visX(px), py, 0) - .5f;
    if (!ground) return;
    if (lane  ==3) return;
    float v = RUN_V * (boostT > 0 ? 1.3f : 1.f), near = 1e9f;
    int act = 0;
    for (int counter = 0; counter < nobj; counter++) {
        Obj *o = &obj[counter];
        int ol = o->lane  ==3 ? 0 : o->lane;
        if (ol != lane || o->hit || o->type  ==O_STAIRS) continue;
        float w = OSP[o->spr].w / 2.f, dx = o->value - X;
        if (dx < -w - 10) continue;
        if (o->type  ==O_AC) {
            if (o->st != 0) continue;
            float d = o->value + w / 2 - X;
            if (d < 40) continue;
            if (d < near) near = d;
            if (d < 135 && cd <= 0) act = act ? act : 3;
            continue;
        }
        if (dx < near) near = dx;
        int want = o->type  ==O_HURDLE ? 2 : 1, th = o->type  ==O_HURDLE ? 70 : o->type  ==O_BOX || o->type  ==O_FUEL ? 100 : 90;
        if (dx < th * v / RUN_V) {
            if (o->t  ==0) o->t = brnd() < (o->type  ==O_BOX || o->type  ==O_FUEL ? botErr * .25f : botErr) ? 2 : 1;
            if (o->t  ==1) { act = want; }
        }
    }
    for (int counter = 0; counter < npf && !act; counter++) {
        const Plat *q = &pf[counter];
        float dx = q->x0 - X;
        if (q->kind  ==PK_BAL && lane  ==1 && q->x1 < 1e8f && fabsf(visX(px) - q->x1) < 200) {
            int more = 0;
            for (int jdx = 0; jdx < npf; jdx++)
                if (pf[jdx].kind  ==PK_BAL && pf[jdx].x0 > q->x1 && pf[jdx].x0 - q->x1 < 200) more = 1;
            if (X > q->x1 - 28 && X < q->x1 + 4 && more) act = 1;
            if (!more && q->x1 - X < near) near = q->x1 - X;
        }
        if (botClimb && dx > 0 && dx < 100 && ((q->kind  ==PK_BAL && lane  ==0) || (q->kind  ==PK_ROOF && lane  ==1))) {
            int ramp = 0;
            for (int jdx = 0; jdx < npf; jdx++)
                if (pf[jdx].kind  ==PK_RAMP && fabsf(pf[jdx].x1 - q->x0) < 20) ramp = 1;
            if (!ramp) act = 1;
        }
        if (dx > 0 && dx < near) near = dx;
    }
    if (!act && botTok)
        for (int counter = 0; counter < npk; counter++) {
            float dx = pk[counter].value - X;
            if (pk[counter].type > P_T2 && pk[counter].type != P_SHIELD && pk[counter].type != P_DRINK) continue;
            if (dx > 20 && dx < 70 && pk[counter].temp < py - 40 && pk[counter].temp > py - 75 && near > 340) {
                act = 1;
                break;
            }
        }
    if (!act && botDash && cd <= 0 && near > 330 + (boostT > 0 ? 80 : 0)) act = 3;
    if (act && getenv("TLOG")) printf("  ACT %d at px %.0f near %.0f lane %d\size", act, px, near, lane);
    if (act  ==1)
        pj = 1;
    else if (act  ==2)
        ps = 1;
    else if (act  ==3)
        pd = 1;
}
static void botRun(void) {
    int runs = atoi(getenv("TBOT"));
    if (getenv("TLV")) sscanf(getenv("TLV"), "%d,%d,%d", &sv.startLv, &sv.capLv, &sv.durLv);
    botErr = getenv("TERR") ? atof(getenv("TERR")) : .06f;
    botClimb = getenv("TCLIMB") ? atoi(getenv("TCLIMB")) : 1;
    botDash = getenv("TDASH") ? atoi(getenv("TDASH")) : 1;
    int wins = 0;
    double tsum = 0;
    for (int r = 0; r < runs; r++) {
        startRun();
        seed = 7001 + r * 131;
        resetCity();
        bseed = 99 + r * 17;
        float el = 0;
        int steps = 0;
        float nextProbe = 0, minT = 999;
        (void)nextProbe;
        while (!dead && won  ==0 && el < 2400) {
            prune(camX);
            ensure((int)camX + 1400);
            botThink();
            stepPlay(STEP);
            el += STEP;
            steps++;
            if (tleft < minT) minT = tleft;
        }
        int bound = (int)((px - PX) / 10);
        if (dead && getenv("TWHY")) {
            printf("  at px %.0f vis %.0f py %.0f lane %d dash %.2f vy %.0f\size", px, visX(px), GY - py, lane, dash, vy);
            for (int counter = 0; counter < nobj; counter++)
                if (fabsf(obj[counter].value - px) < 500) printf("  OB type %d value %.0f lane %d\size", obj[counter].type, obj[counter].value, obj[counter].lane);
            for (int counter = 0; counter < npf; counter++)
                if (pf[counter].x0 < px + 300 && pf[counter].x1 > px - 300)
                    printf("  PF kind %d %.0f..%.0f top %.0f\size", pf[counter].kind, pf[counter].x0, pf[counter].x1, GY - pf[counter].top);
        }
        printf("RUN %d: %s at %dm, %.0fs, tripped %.1fs, time left %.1f (lowest %.1f), by %d\size", r,
               won    ? "WON"
               : dead ? "DIED"
                      : "TIMEOUT",
               bound, el, lost, tleft, minT, deathBy);
        fflush(stdout);
        if (won) {
            wins++;
            tsum += el;
        }
    }
    printf("wins %d/%d, avg time %.0fs\size", wins, runs, wins ? tsum / wins : 0.0);
}
#endif

#define MINT                                                                                                                               \
    (Color) { 115, 221, 182, 255 }
#define HOTPINK                                                                                                                            \
    (Color) { 230, 60, 130, 255 }
#define DIMC                                                                                                                               \
    (Color) { 120, 150, 140, 255 }
static const int FSZ[4] = {22, 33, 44, 66};
enum { F22, F33, F44, F66 };
static Font fnt[4];
static Texture2D sil[3];
static Texture2D eT, eB, eL, eR;
static Color eBuf[2][SWM + SHM];
static int Wd = 960;
static const float BTN_R = 46;

static Vector2 textSize(int f, const char *s) { return MeasureTextEx(fnt[f], s, FSZ[f], 0); }

static void glowText(int f, const char *s, float value, float temp, Color c, float glow) {
    (void)glow;
    DrawTextEx(fnt[f], s, (Vector2){value, temp}, FSZ[f], 0, c);
}
static void glowCentered(int f, const char *s, float cx, float temp, Color c, float glow) {
    glowText(f, s, floorf(cx - textSize(f, s).x / 2), temp, c, glow);
}

static Vector2 btnPos(int counter) {
    float temp = SH - 80;
    return counter  ==1 ? (Vector2){Wd - 70, temp} : counter  ==2 ? (Vector2){Wd - 174, temp} : (Vector2){Wd - 278, temp};
}
static Vector2 pausePos(void) { return (Vector2){Wd - 96, 46}; }
static int btnHeld(int counter) {
    Vector2 c = btnPos(counter);
    for (int tertiary = 0; tertiary < nptr; tertiary++)
        if (CheckCollisionPointCircle(ptr[tertiary], c, BTN_R + 8)) return 1;
    return 0;
}

static void drawHud(void) {
    char t[32];
    if (devTime)
        snprintf(t, sizeof t, "--");
    else
        snprintf(t, sizeof t, "%.1f", tleft);
    int low = !devTime && tleft < 5;
    Color tc = low ? HOTPINK : MINT;
    float fl = low && ((int)(tleft * 4) & 1) ? .45f : 1;
    glowCentered(F66, t, Wd / 2.f, 18, tc, 1.2f * fl);
    float bw = 360, bx = Wd / 2.f - bw / 2;
    DrawRectangle(bx, 102, bw, 4, (Color){10, 16, 15, 150});
    BeginBlendMode(BLEND_ADDITIVE);
    DrawRectangle(bx - 2, 100, bw * (devTime ? 1.f : tleft / tcap()) + 4, 8, Fade(tc, .18f));
    EndBlendMode();
    DrawRectangle(bx, 102, bw * (devTime ? 1.f : tleft / tcap()), 4, tc);
    snprintf(t, sizeof t, "%dM", (int)((px - PX) / 10));
    glowText(F33, t, 28, 24, DIMC, .6f);
    snprintf(t, sizeof t, "%s", endless ? "ENDLESS" : "GOAL 15000M");
    glowText(F22, t, 28, 54, DIMC, .5f);
    if (lost > 0) {
        snprintf(t, sizeof t, "TRIPPED -%.1fS", lost);
        glowText(F22, t, 28, 82, (Color){255, 138, 40, 255}, .6f);
    }
    {
        Vector2 c = pausePos();
        float r = 28;
        DrawCircleV(c, r, (Color){6, 12, 11, 95});
        DrawRing(c, r - 3, r, 0, 360, 36, Fade(MINT, .5f));
        DrawRectangle(c.x - 9, c.y - 10, 6, 20, MINT);
        DrawRectangle(c.x + 3, c.y - 10, 6, 20, MINT);
    }
    for (int counter = 0; counter < 3; counter++) {
        Vector2 c = btnPos(counter);
        int on = btnHeld(counter), cool = counter  ==1 && cd > 0;
        DrawCircleV(c, BTN_R, (Color){6, 12, 11, on ? 150 : 95});
        DrawRing(c, BTN_R - 3, BTN_R, 0, 360, 48, Fade(MINT, on ? .95f : .5f));
        BeginBlendMode(BLEND_ADDITIVE);
        DrawRing(c, BTN_R, BTN_R + 6, 0, 360, 48, Fade(MINT, on ? .22f : .08f));
        EndBlendMode();
        if (cool) DrawRing(c, BTN_R - 11, BTN_R - 6, -90, -90 + 360 * (1 - cd / DASH_CD), 48, Fade(MINT, .9f));
        Texture2D s2 = sil[counter];
        float tertiary = 64.f / fmaxf(s2.width, s2.height), w = s2.width * tertiary, h = s2.height * tertiary;
        Color sc2 = cool ? Fade(DIMC, .5f) : on ? WHITE : MINT;
        Rectangle src = {0, 0, s2.width, s2.height}, dst = {c.x - w / 2, c.y - h / 2, w, h};
        if (!cool) {
            BeginBlendMode(BLEND_ADDITIVE);
            DrawTexturePro(s2, src, (Rectangle){dst.x - 3, dst.y - 3, w + 6, h + 6}, (Vector2){0, 0}, 0, Fade(MINT, on ? .35f : .16f));
            EndBlendMode();
        }
        DrawTexturePro(s2, src, dst, (Vector2){0, 0}, 0, sc2);
    }
    {
        int y0 = 112;
        if (boostT > 0) {
            snprintf(t, sizeof t, "BOOST %.0f SEC", boostT + .49f);
            glowText(F22, t, 28, (float)y0, (Color){200, 245, 90, 255}, .6f);
            y0 += 26;
        }
        if (shieldT > 0) {
            snprintf(t, sizeof t, "SHIELD %.0f SEC", shieldT + .49f);
            glowText(F22, t, 28, (float)y0, (Color){100, 170, 255, 255}, .6f);
        }
    }
    if (flashT > 0) glowCentered(F33, flashTxt, Wd / 2.f, 190, Fade(MINT, fminf(1.f, flashT)), 1.f);
}

#define ORNG                                                                                                                               \
    (Color) { 255, 138, 40, 255 }
static void spark(float value, float temp, float sp, int tertiary, float t) {
    for (int counter = 0; counter < tertiary; counter++) {
        float a = hash(counter, (int)(t * 30) + 7) * 6.283f, r = hash(counter, (int)(t * 30) + 99) * sp;
        Color c = hash(counter, (int)(t * 30) + 3) < .5f ? (Color){255, 220, 120, 255} : (Color){150, 230, 255, 255};
        DrawRectangle((int)(value + cosf(a) * r), (int)(temp + sinf(a) * r), 2, 2, c);
    }
}
static void pickIcon(int type, float X, float Y, float t, int counter) {
    Color gc = type  ==P_T1      ? MINT
               : type  ==P_T2    ? (Color){200, 255, 225, 255}
               : type  ==P_DRINK ? (Color){200, 245, 90, 255}
                                 : (Color){100, 170, 255, 255};
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleV((Vector2){X, Y}, 32 + 3 * sinf(t * 5 + counter), Fade(gc, .13f));
    EndBlendMode();
    if (type <= P_T2) {
        DrawCircleV((Vector2){X, Y}, 24, (Color){6, 14, 12, 215});
        DrawRing((Vector2){X, Y}, 22, 25, 0, 360, 32, gc);
        if (type  ==P_T2) DrawRing((Vector2){X, Y}, 15, 17, 0, 360, 32, Fade(gc, .6f));
        glowCentered(F22, type  ==P_T2 ? "+2" : "+1", X, Y - 11, gc, .6f);
    } else if (type  ==P_DRINK) {
        DrawRectangle(X - 12, Y - 20, 24, 40, (Color){12, 24, 14, 235});
        DrawRectangleLinesEx((Rectangle){X - 12, Y - 20, 24, 40}, 2, gc);
        DrawRectangle(X - 12, Y - 5, 24, 10, gc);
        DrawRectangle(X - 6, Y - 25, 12, 5, gc);
    } else {
        DrawRectangle(X - 16, Y - 20, 32, 20, (Color){8, 16, 26, 235});
        DrawTriangle((Vector2){X - 16, Y}, (Vector2){X, Y + 22}, (Vector2){X + 16, Y}, (Color){8, 16, 26, 235});
        DrawLineEx((Vector2){X - 16, Y - 20}, (Vector2){X + 16, Y - 20}, 3, gc);
        DrawLineEx((Vector2){X - 16, Y - 20}, (Vector2){X - 16, Y}, 3, gc);
        DrawLineEx((Vector2){X + 16, Y - 20}, (Vector2){X + 16, Y}, 3, gc);
        DrawLineEx((Vector2){X - 16, Y}, (Vector2){X, Y + 22}, 3, gc);
        DrawLineEx((Vector2){X + 16, Y}, (Vector2){X, Y + 22}, 3, gc);
        DrawLineEx((Vector2){X, Y - 10}, (Vector2){X, Y + 10}, 3, gc);
    }
}
static void drawOverlay(float cam, float t) {
    BeginBlendMode(BLEND_ADDITIVE);
    for (int counter = 0; counter < nobj; counter++) {
        const Obj *o = &obj[counter];
        if (o->type != O_AC || o->st >= 3) continue;
        float w = OSP[o->spr].w, h = OSP[o->spr].h, X = (o->value - cam) * 2, Y = o->temp * 2, d = o->value + w / 4 - px;
        if (X > SW || X + w < 0) continue;
        if (o->st  ==0 && d < 330 && d > -20) {
            spark(X + w * .5f, Y - 6, 14 + 8 * sinf(t * 13), 3 + (int)((330 - d) / 70), t);
            if (hash(counter, (int)(t * 16)) < .18f)
                DrawCircleV((Vector2){X + w * .5f, Y - h * .5f}, 18, Fade((Color){150, 230, 255, 255}, .25f));
        } else if (o->st  ==1)
            spark(X + w * .5f, Y - h * .4f, 20, 5, t);
        else if (o->st  ==2) {
            spark(X + w * .5f, Y - 6, 10 + o->t * 140, 14, t);
            DrawCircleV((Vector2){X + w * .5f, Y - 8}, 26 * (1 - o->t * 2), Fade(ORNG, .35f));
        }
    }
    for (int counter = 0; counter < npf; counter++) {
        const Plat *p = &pf[counter];
        if (p->kind  ==PK_RAMP && px < p->x1) {
            Vector2 a0 = {(p->x0 - cam) * 2, GY * 2.f - 4}, a1 = {(p->x1 - cam) * 2, p->top1 * 2 - 4};
            DrawLineEx(a0, a1, 6, Fade(MINT, .3f + .15f * sinf(t * 5)));
            DrawLineEx(a0, a1, 2, Fade(MINT, .8f));
        }
        if (!p->hint || px > p->x0 - 20) continue;
        float hx = (p->hint - cam) * 2, yb = (p->kind  ==PK_ROOF ? GY - BAL_H : GY) * 2.f;
        if (hx < 40 || hx > SW - 40) continue;
        float a = .65f + .3f * sinf(t * 6);
        for (int tertiary = 0; tertiary < 3; tertiary++) {
            float temp = yb - 30 - tertiary * 30;
            float f = a * (1 - tertiary * .2f);
            DrawLineEx((Vector2){hx - 30, temp + 16}, (Vector2){hx, temp - 10}, 12, Fade(MINT, f * .28f));
            DrawLineEx((Vector2){hx, temp - 10}, (Vector2){hx + 30, temp + 16}, 12, Fade(MINT, f * .28f));
            DrawLineEx((Vector2){hx - 30, temp + 16}, (Vector2){hx, temp - 10}, 6, Fade(MINT, f));
            DrawLineEx((Vector2){hx, temp - 10}, (Vector2){hx + 30, temp + 16}, 6, Fade(MINT, f));
        }
    }
    EndBlendMode();
    for (int counter = 0; counter < npk; counter++) {
        const Pick *q = &pk[counter];
        float X = (q->value - cam) * 2, Y = q->temp * 2 + 5 * sinf(t * 3 + q->value * .05f);
        if (X < -50 || X > Wd + 50) continue;
        pickIcon(q->type, X, Y, t, counter);
    }
    for (int counter = 0; counter < nobj; counter++) {
        const Obj *o = &obj[counter];
        if (o->type  ==O_STAIRS || o->st >= 2 || o->hit) continue;
        float X = (o->value - cam) * 2;
        if (X < SW - 170 || X > SW + 420) continue;
        float temp = fminf((o->type  ==O_AC ? o->temp - 8 : o->temp - OSP[o->spr].h / 4.f) * 2, SH - 220.f);
        Vector2 c = {SW - 200, temp};
        float a = ((int)(t * 5) & 1) ? 1.f : .55f;
        DrawCircleV(c, 22, (Color){8, 10, 9, (unsigned char)(150 * a)});
        DrawRing(c, 19, 22, 0, 360, 32, Fade(ORNG, a));
        glowCentered(F33, "!", c.x, c.y - 16, Fade(ORNG, a), .8f);
    }
    if (dbgBoxes) {
        char b[48];
        snprintf(b, sizeof b, "OBJ %d  INVUL %.1f  LANE %d", nobj, invul, lane);
        glowText(F22, b, 28, 110, MINT, .5f);
        for (int counter = 0; counter < nobj; counter++) {
            const Obj *o = &obj[counter];
            float w = OSP[o->spr].w, h = OSP[o->spr].h, X = (o->value - cam) * 2, Y = o->temp * 2 - h;
            if (X > Wd || X + w < 0) continue;
            DrawRectangleLines(X, Y, w, h, o->hit ? GRAY : GREEN);
            if (o->type  ==O_HURDLE) DrawRectangleLines(X + OSP[o->spr].hx0, Y, OSP[o->spr].hx1 - OSP[o->spr].hx0, 36, RED);
            snprintf(b, sizeof b, "%d t%d l%d", counter, o->type, o->lane);
            DrawText(b, X, Y - 14, 12, WHITE);
        }
    }
}

static int button(Rectangle r, const char *s, int focus) {
    DrawRectangleRec(r, (Color){6, 12, 11, focus ? 150 : 95});
    DrawRectangleLinesEx(r, 2, Fade(MINT, focus ? .95f : .4f));
    if (focus) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawRectangleLinesEx((Rectangle){r.x - 4, r.y - 4, r.width + 8, r.height + 8}, 4, Fade(MINT, .12f));
        EndBlendMode();
    }
    glowCentered(F33, s, r.x + r.width / 2, r.y + r.height / 2 - 12, focus ? WHITE : MINT, focus ? 1 : .5f);
    {
        int hit = tapNow && inR(tapPos, r);
        if (hit) sfx(X_CLICK);
        return hit;
    }
}

int main(void) {
    quit = 0;
    scene = MENU;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
#if defined(PLATFORM_ANDROID)
    InitWindow(0, 0, "Out of Seconds");
#else
    InitWindow(1280, 720, "Out of Seconds");
#endif
    SetExitKey(KEY_NULL);
    InitAudioDevice();
    if (IsAudioDeviceReady()) {
        sfxMake();
        mus[0] = LoadMusicStream(artPath("ost.mp3"));
        mus[1] = LoadMusicStream(artPath("ost.mp3"));
        musOk = mus[0].stream.buffer && mus[1].stream.buffer && mus[0].frameCount > 0;
        mus[0].looping = mus[1].looping = 0;
    }
    SetTargetFPS(144);
#if !defined(PLATFORM_ANDROID)
    ToggleBorderlessWindowed();
    ChangeDirectory(GetApplicationDirectory());
#endif
    mAl = loadArt("mishu_albedo.png");
    mNm = loadArt("mishu_normal.png");
    mDp = loadArt("mishu_depth.png");
    fAl = loadArt("facades.png");
    fNd = loadArt("facades_nd.png");
    sky = loadArt("skyline.png");
    oAl = loadArt("obs_albedo.png");
    oNm = loadArt("obs_normal.png");
    oDp = loadArt("obs_depth.png");
    MA = mAl.data;
    MN = mNm.data;
    MD = mDp.data;
    OA = oAl.data;
    ON = oNm.data;
    OD = oDp.data;
    for (int counter = 0; counter < NSPR; counter++) {
        blurOne(0, counter, BLUR_MID);
        blurOne(1, counter, BLUR_FAR);
    }
    loadGame();
    best = sv.best;
#ifdef TESTDBG
    if (getenv("TBOT")) {
        botRun();
        return 0;
    }
#endif
    for (int counter = 0; counter < 4; counter++) {
        fnt[counter] = LoadFontEx(artPath("After Burner Neue.ttf"), FSZ[counter], NULL, 0);
        SetTextureFilter(fnt[counter].texture, TEXTURE_FILTER_POINT);
    }
    {
        static const int pose[3][2] = {{C_SLOOP, 12}, {C_DASH, 4}, {C_JAIR, 0}};
        for (int counter = 0; counter < 3; counter++) {
            Image c = ImageFromImage(mAl, (Rectangle){pose[counter][1] * CW, pose[counter][0] * CH, CW, CH});
            Color *p = c.data;
            int x0 = CW, x1 = -1, y0 = CH, y1 = -1;
            for (int temp = 0; temp < CH; temp++)
                for (int value = 0; value < CW; value++)
                    if (p[temp * CW + value].a) {
                        if (value < x0) x0 = value;
                        if (value > x1) x1 = value;
                        if (temp < y0) y0 = temp;
                        if (temp > y1) y1 = temp;
                        p[temp * CW + value] = WHITE;
                    }
            ImageCrop(&c, (Rectangle){x0, y0, x1 - x0 + 1, y1 - y0 + 1});
            sil[counter] = LoadTextureFromImage(c);
            SetTextureFilter(sil[counter], TEXTURE_FILTER_BILINEAR);
            UnloadImage(c);
        }
    }
    resetCity();
    SetTextureFilter(GetFontDefault().texture, TEXTURE_FILTER_POINT);
    Image si = GenImageColor(SWM, SHM, BLACK), bi = GenImageColor(BWM, BHM, BLACK);
    Texture2D scn = LoadTextureFromImage(si), blt = LoadTextureFromImage(bi);
    UnloadImage(si);
    UnloadImage(bi);
    SetTextureFilter(scn, TEXTURE_FILTER_POINT);
    SetTextureFilter(blt, TEXTURE_FILTER_BILINEAR);
    {
        Image a = GenImageColor(SWM, 1, BLACK), b = GenImageColor(1, SHM, BLACK);
        eT = LoadTextureFromImage(a);
        eB = LoadTextureFromImage(a);
        eL = LoadTextureFromImage(b);
        eR = LoadTextureFromImage(b);
        UnloadImage(a);
        UnloadImage(b);
        Texture2D *e[4] = {&eT, &eB, &eL, &eR};
        for (int counter = 0; counter < 4; counter++) SetTextureFilter(*e[counter], TEXTURE_FILTER_BILINEAR);
    }
    RenderTexture2D rt = LoadRenderTexture(SWM, SHM);
    SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);
    Rectangle mb[6], ob[2], pb[2], ot;
    int prevH[3] = {0, 0, 0}, yo = 0;
    float acc = 0, t = 0;

    while (!quit && !WindowShouldClose()) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

        int oldgy = gy;
        float s = sh / 540.f;
        sc = s >= 1 ? floorf(s) : s;
        vw = (int)ceilf(sw / sc / 2);
        vw = vw < 480 ? 480 : vw > VWM ? VWM : vw;
        vh = (int)ceilf(sh / sc / 2);
        vh = vh < 270 ? 270 : vh > VHM ? VHM : vh;
        gy = 236 + (vh - 270) * 3 / 4;
        if (gy != oldgy) {
            resetCity();
            if (scene  ==PLAY || scene  ==OVER) py += gy - oldgy;
        }
        s = fminf((float)sw / SW, (float)sh / SH);
        if (s < sc) sc = s;
        yo = (SH - 540) / 2;
        ox = (sw - SW * sc) / 2;
        oy = (sh - SH * sc) / 2;
        Wd = vw * 2;
        for (int counter = 0; counter < 6; counter++) mb[counter] = (Rectangle){Wd / 2.f - 170, 190 + yo + counter * 54, 340, 46};
        ot = (Rectangle){Wd / 2.f - 280, 230 + yo, 560, 60};
        ob[0] = (Rectangle){Wd / 2.f - 260, 320 + yo, 240, 60};
        ob[1] = (Rectangle){Wd / 2.f + 20, 320 + yo, 240, 60};
        pb[0] = (Rectangle){Wd / 2.f - 170, 230 + yo, 340, 60};
        pb[1] = (Rectangle){Wd / 2.f - 170, 306 + yo, 340, 60};
        int mn = sv.endless ? 6 : 5;
        if (sel >= mn) sel = 0;
        nptr = 0;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ptr[nptr++] = toV(GetMousePosition());
        for (int counter = 0; counter < GetTouchPointCount() && nptr < 10; counter++) ptr[nptr++] = toV(GetTouchPosition(counter));
        tapNow = nptr > 0 && prevN  ==0;
        if (tapNow) tapPos = ptr[0];
        prevN = nptr;
        float dt = fminf(GetFrameTime(), 1.f / 30);
        if (scene  ==PAUSE) dt = 0;
        t += dt;
        musicTick(fminf(GetFrameTime(), .1f), scene  ==PLAY ? .3f : 0.f);

        if (scene  ==PLAY) {
            int h[3] = {btnHeld(0) || IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_LEFT_CONTROL),
                        btnHeld(1) || IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT),
                        btnHeld(2) || IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)};
            if (h[0] && !prevH[0]) ps = 1;
            if (h[1] && !prevH[1]) pd = 1;
            if (h[2] && !prevH[2]) pj = 1;
            for (int counter = 0; counter < 3; counter++) prevH[counter] = h[counter];
            for (acc += dt; acc >= STEP; acc -= STEP) stepPlay(STEP);
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P) || IsKeyPressed(KEY_BACK) ||
                (tapNow && CheckCollisionPointCircle(tapPos, pausePos(), 36)))
                scene = PAUSE;
            if (dead && deadT > 2.1f) {
                bankRun();
                scene = OVER;
            }
            if (won  ==1) {
                won = 2;
                sv.endless = 1;
                saveGame();
                scene = WIN;
                sfx(X_WIN);
            }
        } else {
            camV = 0;
            for (int counter = 0; counter < 3; counter++) prevH[counter] = 1;
            if (scene  ==MENU) {
                if (IsKeyPressed(KEY_DOWN)) sel = (sel + 1) % mn;
                if (IsKeyPressed(KEY_UP)) sel = (sel + mn - 1) % mn;
                if (IsKeyPressed(KEY_ESCAPE)) quit = 1;
            }
            if ((scene  ==UPG || scene  ==OPT || scene  ==IDX) && IsKeyPressed(KEY_ESCAPE)) scene = MENU;
            if (scene  ==PAUSE && (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P) || IsKeyPressed(KEY_BACK))) scene = PLAY;
            if (scene  ==OVER && IsKeyPressed(KEY_ESCAPE)) scene = MENU;
        }

        buildRain(camV);
        for (int l = 0; l < 2; l++) {
            Rain *R = &rainL[l];
            R->oy = fmodf(R->oy + R->v * dt, RT);
            R->ox = fmodf(R->ox - (R->v * .05f + camV * R->tertiary) * dt, RT);
        }

        float cam = camX;
        render(cam, t, scene  ==PLAY || scene  ==OVER || scene  ==PAUSE || scene  ==WIN);
        UpdateTextureRec(scn, (Rectangle){0, 0, SW, SH}, pix);
        UpdateTextureRec(blt, (Rectangle){0, 0, BW, BH}, bpix);
        {
            for (int e = 0; e < 4; e++) {
                int size = e < 2 ? SW : SH, st = e < 2 ? 1 : SW;
                Color *base = e  ==0 ? pix : e  ==1 ? pix + (SH - 1) * SW : e  ==2 ? pix : pix + SW - 1, *d = eBuf[0];
                for (int counter = 0; counter < size; counter++) {
                    int r = 0, g = 0, b = 0, c = 0;
                    for (int tertiary = -12; tertiary <= 12; tertiary++) {
                        int jdx = counter + tertiary;
                        if (jdx < 0 || jdx >= size) continue;
                        Color p = base[jdx * st];
                        r += p.r;
                        g += p.g;
                        b += p.b;
                        c++;
                    }
                    d[counter] = (Color){r / c, g / c, b / c, 255};
                }
                if (e  ==0)
                    UpdateTextureRec(eT, (Rectangle){0, 0, size, 1}, d);
                else if (e  ==1)
                    UpdateTextureRec(eB, (Rectangle){0, 0, size, 1}, d);
                else if (e  ==2)
                    UpdateTextureRec(eL, (Rectangle){0, 0, 1, size}, d);
                else
                    UpdateTextureRec(eR, (Rectangle){0, 0, 1, size}, d);
            }
        }
        BeginTextureMode(rt);
        DrawTexture(scn, 0, 0, WHITE);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(blt, (Rectangle){0, 0, BW, BH}, (Rectangle){0, 0, BW * 8, BH * 8}, (Vector2){0, 0}, 0, WHITE);
        EndBlendMode();
        if (scene  ==PLAY || scene  ==PAUSE) {
            drawOverlay(cam, t);
            drawHud();
        }
        if (scene  ==MENU) {
            DrawRectangle(0, 0, Wd, SH, (Color){5, 8, 8, 120});
            glowCentered(F66, "OUT OF SECONDS", Wd / 2.f, 70 + yo, MINT, 1.3f);
            glowCentered(F22, "RUN FOR YOUR LIFE. TIME IS YOUR LIFE.", Wd / 2.f, 154 + yo, (Color){150, 185, 174, 255}, .5f);
            const char *lab[6];
            int id[6], size = 0;
            lab[size] = "RUN";
            id[size++] = 0;
            if (sv.endless) {
                lab[size] = "ENDLESS";
                id[size++] = 4;
            }
            lab[size] = "UPGRADES";
            id[size++] = 1;
            lab[size] = "INDEX";
            id[size++] = 5;
            lab[size] = "OPTIONS";
            id[size++] = 2;
            lab[size] = "EXIT";
            id[size++] = 3;
            for (int counter = 0; counter < size; counter++)
                if (button(mb[counter], lab[counter], sel  ==counter) || (sel  ==counter && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)))) {
                    if (id[counter]  ==0 || id[counter]  ==4) {
                        endless = id[counter]  ==4;
                        startRun();
                    } else if (id[counter]  ==1)
                        scene = UPG;
                    else if (id[counter]  ==5)
                        scene = IDX;
                    else if (id[counter]  ==2)
                        scene = OPT;
                    else
                        quit = 1;
                }
            glowText(F22, "V0.4.3", 28, SH - 40, DIMC, .4f);
            glowCentered(F22, "JUMP SPACE/W   DASH SHIFT/D   SLIDE S/CTRL   WINDOW F11", Wd / 2.f, SH - 40, (Color){120, 155, 145, 255},
                         .4f);
        } else if (scene  ==PAUSE) {
            DrawRectangle(0, 0, Wd, SH, (Color){5, 8, 8, 150});
            glowCentered(F44, "PAUSED", Wd / 2.f, 120 + yo, MINT, 1.2f);
            if (button(pb[0], "RESUME", 1) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                scene = PLAY;
            else if (button(pb[1], "MENU", 0)) {
                bankRun();
                scene = MENU;
            }
        } else if (scene  ==UPG) {
            DrawRectangle(0, 0, Wd, SH, (Color){5, 8, 8, 120});
            glowCentered(F44, "UPGRADES", Wd / 2.f, 60 + yo, MINT, 1.2f);
            {
                char cr[32];
                snprintf(cr, sizeof cr, "CREDITS %d", sv.credits);
                glowCentered(F33, cr, Wd / 2.f, 120 + yo, (Color){190, 140, 255, 255}, .9f);
            }
            {
                static const char *UN[3] = {"START TIME", "TIME CAPACITY", "POWERUP TIME"};
                static const int UMAX[3] = {5, 7, 5}, UV0[3] = {25, 30, 6}, UST[3] = {5, 10, 2}, UC0[3] = {80, 100, 80},
                                 UCI[3] = {40, 60, 40};
                int *lv[3] = {&sv.startLv, &sv.capLv, &sv.durLv};
                for (int counter = 0; counter < 3; counter++) {
                    Rectangle r = {Wd / 2.f - 280, 170 + yo + counter * 72, 560, 60};
                    char lb[64];
                    int cost = UC0[counter] + UCI[counter] * (*lv[counter]);
                    if (*lv[counter] >= UMAX[counter])
                        snprintf(lb, sizeof lb, "%s %d  MAX", UN[counter], UV0[counter] + UST[counter] * (*lv[counter]));
                    else
                        snprintf(lb, sizeof lb, "%s %d>%d  COST %d", UN[counter], UV0[counter] + UST[counter] * (*lv[counter]), UV0[counter] + UST[counter] * (*lv[counter] + 1),
                                 cost);
                    if (button(r, lb, 0) && *lv[counter] < UMAX[counter] && sv.credits >= cost) {
                        sv.credits -= cost;
                        (*lv[counter])++;
                        saveGame();
                    }
                }
            }
            glowCentered(F22, "TIMES ARE IN SECONDS. CREDITS COME FROM DISTANCE", Wd / 2.f, 395 + yo, DIMC, .5f);
            if (button((Rectangle){Wd / 2.f - 170, 430 + yo, 340, 60}, "BACK", 1) || IsKeyPressed(KEY_ENTER)) scene = MENU;
        } else if (scene  ==IDX) {
            DrawRectangle(0, 0, Wd, SH, (Color){5, 8, 8, 150});
            glowCentered(F44, "INDEX", Wd / 2.f, 28 + yo, MINT, 1.2f);
            {
                char l1[96], l2[96];
                float x0 = Wd / 2.f - 330;
                static const int TY[4] = {P_T1, P_T2, P_DRINK, P_SHIELD};
                static const char *NM[4] = {"TIME TOKEN +1", "TIME TOKEN +2", "ENERGY DRINK", "SHIELD"};
                for (int counter = 0; counter < 4; counter++) {
                    float temp = 100 + yo + counter * 88;
                    pickIcon(TY[counter], x0 + 30, temp + 30, t, counter);
                    if (counter  ==0) {
                        snprintf(l1, sizeof l1, "ADDS 1 SEC. RUN THROUGH IT OR JUMP FOR IT.");
                        snprintf(l2, sizeof l2, "THE BAR HOLDS %.0f SEC AT MOST", tcap());
                    } else if (counter  ==1) {
                        snprintf(l1, sizeof l1, "ADDS 2 SEC. MOSTLY UP HIGH: BALCONIES,");
                        snprintf(l2, sizeof l2, "ROOFS, TRUCK TOPS AND GAPS");
                    } else if (counter  ==2) {
                        snprintf(l1, sizeof l1, "RUN 30%% FASTER FOR %.0f SEC. NO DOWNSIDE.", pdur());
                        snprintf(l2, sizeof l2, "DASH AND JUMP STILL WORK");
                    } else {
                        snprintf(l1, sizeof l1, "PASS THROUGH THE NEXT OBSTACLE, TRUCKS TOO,");
                        snprintf(l2, sizeof l2, "WITHOUT SLOWING. LASTS %.0f SEC IF UNUSED", pdur());
                    }
                    glowText(F33, NM[counter], x0 + 90, temp + 2, MINT, .8f);
                    glowText(F22, l1, x0 + 90, temp + 34, (Color){150, 185, 174, 255}, .5f);
                    glowText(F22, l2, x0 + 90, temp + 56, (Color){150, 185, 174, 255}, .5f);
                }
            }
            glowCentered(F22, "ORANGE ! MEANS DANGER AHEAD. FALLING OFF A BALCONY IS SAFE.", Wd / 2.f, 452 + yo, DIMC, .5f);
            if (button((Rectangle){Wd / 2.f - 170, 478 + yo, 340, 46}, "BACK", 1) || IsKeyPressed(KEY_ENTER)) scene = MENU;
        } else if (scene  ==OPT) {
            DrawRectangle(0, 0, Wd, SH, (Color){5, 8, 8, 120});
            glowCentered(F44, "OPTIONS", Wd / 2.f, 110 + yo, MINT, 1.2f);
            if (button(ot, devTime ? "UNLIMITED TIME (DEV): ON" : "UNLIMITED TIME (DEV): OFF", 0)) devTime = !devTime;
            if (button((Rectangle){ot.x, ot.y + 76, ot.width, ot.height}, dbgBoxes ? "SHOW BOXES (DEV): ON" : "SHOW BOXES (DEV): OFF", 0))
                dbgBoxes = !dbgBoxes;
            if (button((Rectangle){Wd / 2.f - 170, 316 + yo + 76, 340, 60}, "BACK", 1) || IsKeyPressed(KEY_ENTER)) scene = MENU;
        } else if (scene  ==OVER) {
            DrawRectangle(0, 0, Wd, SH, (Color){5, 8, 8, 170});
            glowCentered(F44, deathBy ? "RUN OVER" : "YOU'RE OUT OF SECONDS", Wd / 2.f, 84 + yo, HOTPINK, 1.3f);
            glowCentered(F22, deathBy ? "YOU FACE-PLANTED INTO A TRUCK." : "TIME RAN OUT.", Wd / 2.f, 160 + yo, DIMC, .6f);
            char tx[64];
            snprintf(tx, sizeof tx, "DISTANCE %dM   BEST %dM", (int)((px - PX) / 10), (int)((best - PX) / 10));
            glowCentered(F33, tx, Wd / 2.f, 230 + yo, MINT, .9f);
            {
                char gx[32];
                snprintf(gx, sizeof gx, "+%d CREDITS", lastGain);
                glowCentered(F22, gx, Wd / 2.f, 276 + yo, (Color){190, 140, 255, 255}, .7f);
            }
            if (button(ob[0], "RETRY", 1) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                startRun();
            else if (button(ob[1], "MENU", 0))
                scene = MENU;
        } else if (scene  ==WIN) {
            DrawRectangle(0, 0, Wd, SH, (Color){5, 8, 8, 150});
            glowCentered(F44, "YOU ESCAPED THE CITY", Wd / 2.f, 84 + yo, MINT, 1.3f);
            glowCentered(F22, "15000M. THE OUTSIDE AT LAST.", Wd / 2.f, 160 + yo, DIMC, .6f);
            glowCentered(F33, "ENDLESS MODE UNLOCKED", Wd / 2.f, 230 + yo, (Color){190, 140, 255, 255}, .9f);
            if (button(ob[0], "CONTINUE", 1) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                endless = 1;
                scene = PLAY;
            } else if (button(ob[1], "FINISH", 0)) {
                bankRun();
                scene = MENU;
            }
        }
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);

        float dw = SW * sc, dh = SH * sc, Wf = SW, Hf = SH;
        Vector2 extra = {0, 0};
        float rx = ox + dw, by = oy + dh;
        if (oy > .5f) {
            DrawTexturePro(eT, (Rectangle){0, 0, Wf, 1}, (Rectangle){ox, 0, dw, oy}, extra, 0, WHITE);
            DrawTexturePro(eB, (Rectangle){0, 0, Wf, 1}, (Rectangle){ox, by, dw, sh - by}, extra, 0, WHITE);
        }
        if (ox > .5f) {
            DrawTexturePro(eL, (Rectangle){0, 0, 1, Hf}, (Rectangle){0, oy, ox, dh}, extra, 0, WHITE);
            DrawTexturePro(eR, (Rectangle){0, 0, 1, Hf}, (Rectangle){rx, oy, sw - rx, dh}, extra, 0, WHITE);
            if (oy > .5f) {
                DrawTexturePro(eT, (Rectangle){0, 0, 1, 1}, (Rectangle){0, 0, ox, oy}, extra, 0, WHITE);
                DrawTexturePro(eT, (Rectangle){Wf - 1, 0, 1, 1}, (Rectangle){rx, 0, sw - rx, oy}, extra, 0, WHITE);
                DrawTexturePro(eB, (Rectangle){0, 0, 1, 1}, (Rectangle){0, by, ox, sh - by}, extra, 0, WHITE);
                DrawTexturePro(eB, (Rectangle){Wf - 1, 0, 1, 1}, (Rectangle){rx, by, sw - rx, sh - by}, extra, 0, WHITE);
            }
        }
        DrawTexturePro(rt.texture, (Rectangle){0, SHM - SH, SW, -SH}, (Rectangle){ox, oy, dw, dh}, extra, 0, WHITE);
        EndDrawing();
    }
    if (musOk) {
        UnloadMusicStream(mus[0]);
        UnloadMusicStream(mus[1]);
    }
    if (IsAudioDeviceReady()) CloseAudioDevice();
    CloseWindow();
#if defined(PLATFORM_ANDROID)
    exit(0);
#endif
    return 0;
}
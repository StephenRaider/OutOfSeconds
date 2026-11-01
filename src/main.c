#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VWM 1024
#define VW vw
#define VHM 512
#define VH vh
#define GY gy
#define SW (VW*2)
#define SWM (VWM*2)
#define SHM (VHM*2)
#define SH (VH*2)
#define SGY (GY*2)
#define FL 64
#define PX 150
#define STEP (1.f/120)
#define LEADMAX 70.f
static int vw = 480, vh = 270, gy = 236;

#define TIME_LIMIT 30.f
#define RUN_V 250.f
#define DASH_V 650.f
#define DASH_T .35f
#define DASH_CD 1.2f
#define SLIDE_V .7f
#define SLIDE_T 1.f
#define JUMP_V 365.f
#define GRAV 900.f
#define ROLL_H 50.f
#define STUMBLE_LOSS 1.5f
#define FSTEP 2

#define HAZE .80f
#define BLOOM .30f
#define NEON .30f
#define AMB .10f
#define AMB_M .88f
#define TINT .80f
#define FOG_FAR .50f
#define FOG_MID .30f
#define DARK_FAR .82f
#define DARK_MID .52f
#define BLUR_FAR 1.5f
#define BLUR_MID .5f
#define SAT 1.60f
#define CON 1.25f
#define BRI .85f
static const float ACCENT[3] = { 230, 60, 130 };

#define CW 128
#define CH 120
#define CGY 118
#define DPX 61.7f
enum { C_SPRINT, C_JSTART, C_JAIR, C_JLAND, C_ROLL, C_SSTART, C_SLOOP, C_SEXIT, C_DEATH, C_DASH, C_STUMBLE };
typedef struct { int digit, loop, then, thenFrame; float rate; } Clip;
static const Clip CLIPS[] = {
    { 16, 1, -1, 0, 1 }, { 18, 0, C_JAIR, 0, 1.5f }, { 48, 1, -1, 0, 1 }, { 15, 0, C_SPRINT, 2, 1 }, { 18, 0, C_SPRINT, 2, 1 },
    { 21, 0, C_SLOOP, 0, 1.5f }, { 48, 1, -1, 0, 1 }, { 16, 0, C_SPRINT, 3, 1 }, { 47, 0, -1, 0, 1 }, { 9, 0, C_SPRINT, 0, 1 },
    { 17, 0, C_SPRINT, 11, 1 } };

enum { MENU, UPG, PLAY, OVER, PAUSE, OPT };
static int scene = MENU, quit, sel;
static float sc = 1, ox, oy;
static Vector2 ptr[11]; static int nptr, tapNow, prevN; static Vector2 tapPos;

static float camX, px, py, vy, tleft, dash, cd, slide, animT, deadT, best, apex, stumbleT, invul, lost;
static int clip, dead, pj, pd, ps, inAir, lane, devTime = 1;

static Vector2 toV(Vector2 p) { return (Vector2){ (p.x - ox)/sc, (p.y - oy)/sc }; }
static int inR(Vector2 p, Rectangle r) { return CheckCollisionPointRec(p, r); }

static Image mAl, mNm, mDp, fAl, fNd, sky, oAl, oNm, oDp;
static Color *MA, *MN, *MD, *OA, *ON, *OD;
static const char *artPath(const char *digit) {
#if defined(PLATFORM_ANDROID)
    return digit;
#else
    return FileExists(TextFormat("assets/%s", digit)) ? TextFormat("assets/%s", digit) : TextFormat("../assets/%s", digit);
#endif
}
static Image loadArt(const char *digit) {
    Image bound = LoadImage(artPath(digit));
    ImageFormat(&bound, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return bound;
}

static const unsigned char PAL[24][3] = { {137,158,147},{123,140,130},{114,130,119},{106,122,112},{100,113,103},{95,106,95},{88,100,91},{83,93,82},
    {78,89,81},{75,83,73},{69,79,71},{67,75,67},{63,71,63},{59,67,59},{57,62,55},{52,59,53},{48,53,47},{44,50,45},{40,46,41},{36,40,36},
    {32,37,33},{27,31,28},{20,22,20},{5,6,5} };
static const unsigned char *P(int pos) { return PAL[pos < 23 ? pos : 23]; }
static const unsigned char SIGN[3] = { 8, 12, 12 };

typedef struct { int y0, w, floors, ncol; int cols[6]; } Block;
static const Block BLK[3] = { { 0, 612, 6, 6, { 0, 132, 251, 395, 502, 612 } }, { 384, 511, 5, 6, { 0, 75, 203, 326, 450, 511 } },
                              { 704, 403, 4, 5, { 0, 96, 188, 283, 375 } } };

typedef struct { int value, result, w, h; } SprRect;
static const SprRect MIDR[] = { {0,0,100,210}, {101,0,76,210}, {178,0,89,186}, {268,0,68,186}, {337,0,156,185}, {494,0,81,185}, {576,0,74,120}, {651,0,52,120}, {704,0,73,115}, {778,0,55,115}, {834,0,162,112}, {0,211,204,112}, {205,211,94,111}, {300,211,52,111}, {523,211,71,92}, {595,211,70,92}, {786,211,79,91}, {866,211,71,91}, {325,324,149,53}, {475,324,129,53}, {605,324,233,50}, {0,382,205,49}, {206,382,161,47}, {368,382,233,46} };
static const SprRect FARR[] = { {353,211,50,105}, {404,211,38,105}, {443,211,44,93}, {488,211,34,93}, {666,211,78,92}, {745,211,40,92}, {938,211,37,60}, {976,211,26,60}, {0,324,36,57}, {37,324,27,57}, {65,324,81,56}, {147,324,102,56}, {250,324,47,55}, {298,324,26,55}, {602,382,35,46}, {638,382,35,46}, {674,382,39,45}, {714,382,35,45}, {750,382,74,26}, {825,382,64,26}, {890,382,116,25}, {0,432,102,24}, {103,432,116,23}, {220,432,80,23} };
#define NSPR 24

static unsigned seed = 1312;
static unsigned hashu(int a, int b) { unsigned h = (unsigned)a*374761393u + (unsigned)b*668265263u + seed*1442695041u; h = (h ^ (h >>13))*1274126177u; return h ^ (h >>16); }
static float hash(int a, int b) { return (hashu(a, b) >>8)/16777216.f; }
typedef struct { unsigned s; } Rng;
static Rng rngOf(int a, int b) { return (Rng){ hashu(a, b) }; }
static float rnd(Rng *r) { unsigned t = r->s += 0x6D2B79F5u; t = (t ^ (t >>15))*(1 | t); t = (t + (t ^ (t >>7))*(61 | t)) ^ t; return ((t ^ (t >>14)) >>8)/16777216.f; }
static float vnoise(float value, float result) {
    int xi = (int)floorf(value), yi = (int)floorf(result); float fx = value - xi, fy = result - yi, a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1);
    float u = fx*fx*(3 - 2*fx), v = fy*fy*(3 - 2*fy); return a + (b - a)*u + (c - a)*v + (a - b - c + d)*u*v;
}

static const float EMC[6][3] = { { 0, 0, 0 }, { 40, 225, 180 }, { 0, 0, 0 }, { 150, 215, 180 }, { 205, 175, 120 }, { 120, 165, 140 } };
static const float *emCol(int t) { return t == 2 ? ACCENT : EMC[t]; }

#define BEACON 0x80000000u
static int fclass(unsigned g) { if (!g) return 0; if (g & BEACON) return 3; float h = hash((int)g, 11); return h < .03f ? 2 : h < .08f ? 1 : 0; }
static float flick(unsigned g, float t) {
    int c = fclass(g); if (!c) return 1;
    if (c == 3) { float P = 2.4f, ph = fmodf(t + hash((int)g, 5)*P, P); return ph < .18f ? 1 : .05f; }
    float base = c == 1 ? 1 : .04f, P = 3.5f + hash((int)g, 3)*5; int kdx = (int)floorf(t/P); float lt = t - kdx*P;
    if (hash((int)g*7 + 1, kdx) > .6f) return base;
    int digit = 2 + (int)(hash((int)g*13 + 2, kdx)*2); float bs = hash((int)g*5 + 3, kdx)*(P - .5f), u = lt - bs;
    if (u < 0 || u >= digit*.13f) return base;
    return fmodf(u, .13f) < .06f ? (c == 1 ? .08f : 1) : base;
}

#define BAL_H 64.f
#define ROOF_H 128.f
#define PW 6.f
#define STEPUP 8.f
enum { PK_BAL, PK_ROOF, PK_BLOCK, PK_RAMP };
typedef struct { float x0, x1, top, bot, top1, hint; int kind; } Plat;
static Plat pf[160]; static int npf;
enum { S_TRASH, S_HURDLE, S_BARRIER, S_RAIL, S_AC1, S_AC2, S_STAIRS, S_FUEL, S_TRUCK, S_BIN1, S_BIN2, S_BIN3, NSP };
static const struct { int value, result, w, h; float dpx; int hx0, hx1; } OSP[NSP] = {
    { 0, 0, 37, 61, 26.1f, 1, 35 }, { 38, 0, 135, 88, 65.5f, 4, 131 }, { 174, 0, 96, 64, 37.3f, 0, 96 }, { 271, 0, 88, 54, 6.0f, 7, 88 }, { 360, 0, 35, 35, 28.0f, 0, 34 },
    { 396, 0, 71, 42, 41.5f, 6, 71 }, { 468, 0, 115, 128, 80.3f, 2, 113 }, { 0, 129, 583, 127, 105.8f, 1, 581 }, { 584, 129, 425, 132, 94.4f, 1, 419 },
    { 0, 262, 75, 72, 62.4f, 4, 71 }, { 76, 262, 103, 83, 88.4f, 3, 97 }, { 180, 262, 135, 75, 75.8f, 54, 125 } };
enum { O_TRASH, O_HURDLE, O_BARRIER, O_AC, O_STAIRS, O_BOX, O_FUEL };
typedef struct { int type, spr, st, hit, lane; float value, result, ty, vy, t; } Obj;
static Obj obj[160]; static int nobj;
static void addPlat(float x0, float x1, float top, float bot, float top1, int kind) { if (npf < 160) pf[npf++] = (Plat){ x0, x1, top, bot, top1, 0, kind }; }
static Obj *addObj(int type, int spr, float value, float result, int ln) { if (nobj >= 160) return 0; Obj *o = &obj[nobj++]; *o = (Obj){ type, spr, 0, 0, ln, value, result, result, 0, 0 }; return o; }
static float lastAC = -1e9f;
static void addAC(float xc, float ty, int ln, Rng *r) {
    if (xc - lastAC < 560) return;
    int s = rnd(r) < .5f ? S_AC1 : S_AC2; Obj *o = addObj(O_AC, s, xc - OSP[s].w/4.f, GY - BAL_H - 48, ln); if (o) { o->ty = ty; lastAC = xc; }
}
static void addTruck(int fuel, float value) {
    addObj(fuel ? O_FUEL : O_BOX, fuel ? S_FUEL : S_TRUCK, value, GY, 3);
    if (fuel) { addPlat(value, value + 40, GY - 61, GY, 0, PK_BLOCK); addPlat(value + 50, value + 250, GY - 62, GY, 0, PK_BLOCK); addPlat(value + 250, value + 291, GY - 52, GY, 0, PK_BLOCK); }
    else { addPlat(value, value + 170, GY - 66, GY, 0, PK_BLOCK); addPlat(value + 170, value + 212, GY - 54, GY, 0, PK_BLOCK); }
}
static void prune(float cam) {
    int kdx = 0; for (int pos = 0; pos < npf; pos++) if (pf[pos].x1 > 1e8f || pf[pos].x1 > cam - 900) pf[kdx++] = pf[pos]; npf = kdx;
    kdx = 0; for (int pos = 0; pos < nobj; pos++) if (obj[pos].value + 300 > cam - 900) obj[kdx++] = obj[pos]; nobj = kdx;
}

static float supportY(float value, float result, int *kind) {
    float f = GY; int kdx = -1;
    for (int pos = 0; pos < npf; pos++) {
        const Plat *p = &pf[pos]; if (value + 3 <= p->x0 || value - 3 >= p->x1) continue;
        float s = p->kind == PK_RAMP ? p->top + (p->top1 - p->top)*fminf(1.f, fmaxf(0.f, (value - p->x0)/(p->x1 - p->x0))) : p->top;
        if (s >= result - STEPUP && s < f) { f = s; kdx = p->kind; }
    }
    if (kind) *kind = kdx;
    return f;
}

typedef struct { float value, result, extra, R, s; int t; unsigned g; } Light;
#define MAXL 64
typedef struct {
    int gap, glow, x0, w, id, roof;
    unsigned char *al, *a, *et, *ei;
    unsigned *eg;
    float *digit;
    float *lit;
    Light L[MAXL]; int nl;
} Bld;
static Bld *blds; static int nb, cap, bEnd, bId;

static void addLight(Bld *b, Light l) { if (b->nl < MAXL) b->L[b->nl++] = l; }
static void bpx(Bld *b, int value, int result, const unsigned char *c, float nx, float ny, float extra) {
    if (value < 0 || value >= b->w || result < 0 || result >= GY) return;
    int pos = result*b->w + value; memcpy(b->al + pos*3, c, 3); b->a[pos] = 255;
    float *digit = b->digit + pos*4; digit[0] = nx; digit[1] = ny; digit[2] = sqrtf(fmaxf(0, 1 - nx*nx - ny*ny)); digit[3] = extra; b->et[pos] = 0;
}
static void brect(Bld *b, int value, int result, int w, int h, const unsigned char *c, float nx, float ny, float extra) { for (int jdx = result; jdx < result + h; jdx++) for (int pos = value; pos < value + w; pos++) bpx(b, pos, jdx, c, nx, ny, extra); }
static void bem(Bld *b, int value, int result, int w, int h, int t, int s, unsigned g) {
    if (!fclass(g)) g = 0;
    for (int jdx = result; jdx < result + h; jdx++) for (int pos = value; pos < value + w; pos++) { if (pos < 0 || pos >= b->w || jdx < 0 || jdx >= GY) continue; int kdx = jdx*b->w + pos; b->et[kdx] = t; b->ei[kdx] = s; b->eg[kdx] = g; b->a[kdx] = 255; }
}
static void glyph(Bld *b, int value, int result, int t, Rng *r, unsigned g) { int bound = (int)(rnd(r)*512); for (int jdx = 0; jdx < 3; jdx++) for (int pos = 0; pos < 3; pos++) if ((bound >>(jdx*3 + pos)) & 1 || (pos == 1 && jdx == 1)) bem(b, value + pos*2, result + jdx*2, 2, 2, t, 220, g); }

static void freeBld(Bld *b) { free(b->al); free(b->a); free(b->et); free(b->ei); free(b->eg); free(b->digit); free(b->lit); b->al = b->a = b->et = b->ei = 0; b->eg = 0; b->digit = b->lit = 0; }

static void genBuilding(Bld *b, int x0, Rng *r, int mode, float slabX) {
    const Block *B = &BLK[(int)(rnd(r)*3)]; int nbay = B->ncol - 1;
    int digit = 1 + (int)(rnd(r)*(nbay < 3 ? nbay : 3)), b0 = (int)(rnd(r)*(nbay - digit + 1)), w = 0, bsrc[3], bw[3];
    for (int jdx = 0; jdx < digit; jdx++) { bsrc[jdx] = B->cols[b0 + jdx]; bw[jdx] = B->cols[b0 + jdx + 1] - B->cols[b0 + jdx]; w += bw[jdx]; }
    int floors = mode == 2 ? 1 : mode == 1 ? 2 + (int)(rnd(r)*3) : rnd(r) < .45f ? 1 + (int)(rnd(r)*2) : 3 + (int)(rnd(r)*3), rows[5];
    for (int pos = 0; pos < floors; pos++) rows[pos] = (int)(rnd(r)*B->floors);
    int id = ++bId, N = w*GY;
    memset(b, 0, sizeof *b); b->x0 = x0; b->w = w; b->id = id; b->roof = floors == 1;
    b->al = calloc(N*3, 1); b->a = calloc(N, 1); b->et = calloc(N, 1); b->ei = calloc(N, 1); b->eg = calloc(N, sizeof(unsigned)); b->digit = malloc(N*4*sizeof(float));
    for (int pos = 0; pos < N; pos++) { float *q = b->digit + pos*4; q[0] = q[1] = 0; q[2] = 1; q[3] = -28; }
    Color *FA = fAl.data, *FN = fNd.data; int FW = fAl.width;
    int top = GY - FL - floors*FL;

    for (int pos = 0; pos < floors; pos++) {
        int y0 = GY - FL - (pos + 1)*FL; if (y0 + FL < 0) continue; int xo = 0;
        for (int bi = 0; bi < digit; bi++) {
            for (int yy = 0; yy < FL; yy++) {
                int Y = y0 + yy; if (Y < 0) continue; int sy = B->y0 + rows[pos]*FL + yy;
                for (int xx = 0; xx < bw[bi]; xx++) {
                    Color s = FA[sy*FW + bsrc[bi] + xx]; if (s.a < 128) continue; Color nd = FN[sy*FW + bsrc[bi] + xx]; int kdx = Y*w + xo + xx;
                    b->al[kdx*3] = s.r; b->al[kdx*3 + 1] = s.g; b->al[kdx*3 + 2] = s.b; b->a[kdx] = s.a == 254 ? 254 : 255;
                    float *q = b->digit + kdx*4; q[0] = nd.r/127.5f - 1; q[1] = -(nd.g/127.5f - 1); q[2] = nd.b/127.5f - 1; q[3] = -28 + (nd.a/255.f - .25f)*30;
                }
            }

            Rng lr = rngOf(id*31 + pos, xo);
            if (rnd(&lr) < .28f) {
                int t = rnd(&lr) < .5f ? 4 : 5, s = 45 + (int)(rnd(&lr)*50); unsigned g = id*1000 + pos*16 + xo, eg = fclass(g) ? g : 0; float cx = 0, cy = 0; int cn = 0;
                for (int yy = 0; yy < FL; yy++) { int Y = y0 + yy; if (Y < 0) continue; for (int xx = 0; xx < bw[bi]; xx++) { int kdx = Y*w + xo + xx; if (b->a[kdx] == 254) { b->et[kdx] = t; b->ei[kdx] = s; b->eg[kdx] = eg; cx += xo + xx; cy += Y; cn++; } } }
                if (cn > 20 && pos < 2) addLight(b, (Light){ x0 + cx/cn, cy/cn, -20, 80, .45f, t, 0 });
            }
            xo += bw[bi];
        }
    }

    if (top > 4) {
        brect(b, 0, top - 6, w, 6, P(5), 0, -.6f, -28); brect(b, 0, top - 6, w, 1, P(2), 0, -1, -28);
        if (rnd(r) < .7f) { int tx = 6 + (int)(rnd(r)*(w - 30)); brect(b, tx, top - 26, 18, 20, P(9), .3f, 0, -30); brect(b, tx - 1, top - 28, 20, 2, P(6), 0, -.8f, -28); for (int jdx = top - 6; jdx < top; jdx++) { bpx(b, tx + 2, jdx, P(12), 0, 0, -28); bpx(b, tx + 15, jdx, P(12), 0, 0, -28); } }
        for (int kdx = 0; kdx < 2; kdx++) if (rnd(r) < .6f) { int ax = (int)(rnd(r)*(w - 4)), ah = 10 + (int)(rnd(r)*22); for (int jdx = top - 6 - ah; jdx < top - 6; jdx++) bpx(b, ax, jdx, P(14), 0, 0, -28); if (rnd(r) < .5f) bem(b, ax, top - 7 - ah, 1, 1, 2, 200, BEACON | (unsigned)(id*7 + kdx)); }
        for (int kdx = 0; kdx < 2; kdx++) if (rnd(r) < .5f) { int ax = 4 + (int)(rnd(r)*(w - 20)); brect(b, ax, top - 14, 12, 8, P(7), 0, -.3f, -28); }
    }

    int gx = 0;
    while (gx < w) {
        int sw = 40 + (int)(rnd(r)*50); if (sw > w - gx) sw = w - gx; float kind = rnd(r); int y0 = GY - FL;
        brect(b, gx, y0, sw, FL, P(10), 0, 0, -28); brect(b, gx, y0, sw, 4, P(6), 0, -.7f, -28);
        if (kind < .45f) {
            for (int jdx = y0 + 12; jdx < GY; jdx++) for (int pos = gx + 3; pos < gx + sw - 3; pos++) { int s = (jdx - y0)%3; bpx(b, pos, jdx, s == 0 ? P(13) : s == 1 ? P(8) : P(11), 0, s == 0 ? -.5f : s == 2 ? .5f : 0, -29); }
            brect(b, gx + 3, y0 + 8, sw - 6, 4, P(15), 0, -.6f, -26);
            for (int pos = gx + 5; pos < gx + sw - 5; pos += 7) if (rnd(r) < .5f) { int gy = y0 + 20 + (int)(rnd(r)*30), gw = 3 + (int)(rnd(r)*4); brect(b, pos, gy, gw, 1, P(4), 0, 0, -29); }
        } else if (kind < .8f) {
            brect(b, gx + 3, y0 + 10, sw - 6, FL - 12, P(18), 0, 0, -34); int t = rnd(r) < .5f ? 3 : 4; unsigned g = id*1000 + 900 + gx;
            bem(b, gx + 4, y0 + 14, sw - 8, FL - 20, t, 30 + (int)(rnd(r)*30), g);
            for (int pos = gx + 10; pos < gx + sw - 6; pos += 12) brect(b, pos, y0 + 14, 1, FL - 20, P(12), 0, 0, -31);
            brect(b, gx + 3, y0 + 10, sw - 6, 3, P(4), 0, -.8f, -24);
            addLight(b, (Light){ x0 + gx + sw/2.f, y0 + 34, -26, 95, .6f, t, g });
        } else {
            brect(b, gx + sw/2 - 10, y0 + 16, 20, FL - 16, P(19), 0, 0, -40); brect(b, gx + sw/2 - 12, y0 + 14, 24, 2, P(6), 0, -.8f, -28);
            for (int jdx = GY - 8; jdx < GY; jdx += 3) brect(b, gx + sw/2 - 10, jdx, 20, 1, P(9), 0, -.6f, -34);
        }
        if (rnd(r) < .75f) { int tx = gx + sw/2 - 7; unsigned g = id*1000 + 800 + gx; brect(b, tx - 1, y0 + 5, 16, 2, P(12), 0, .6f, -22); bem(b, tx, y0 + 7, 14, 1, 3, 230, g); addLight(b, (Light){ x0 + tx + 7, y0 + 9, -12, 120, .9f, 3, g }); }
        gx += sw;
    }

    if (mode) {
        int sx = (int)(slabX - x0); if (sx < 0) sx = 0;
        if (sx < w) {
            int y0 = GY - FL; brect(b, sx, y0, w - sx, 6, P(2), 0, -.7f, -34); brect(b, sx, y0 + 5, w - sx, 1, P(20), 0, .6f, -34); bem(b, sx, y0, w - sx, 2, 1, 170, 0);
            for (int lx = sx + 30; lx < w; lx += 90) addLight(b, (Light){ x0 + lx, y0 - 6, -30, 85, .55f, 1, 0 });
        }
    }
    if (floors == 1) {
        brect(b, 0, top, w, 5, P(2), 0, -.7f, -34); brect(b, 0, top + 4, w, 1, P(20), 0, .6f, -34); bem(b, 0, top, w, 2, 1, 170, 0);
        for (int lx = 30; lx < w; lx += 90) addLight(b, (Light){ x0 + lx, top - 6, -30, 85, .55f, 1, 0 });
    }

    if (rnd(r) < NEON) {
        int sw = 14, sh = 36 + (int)(rnd(r)*30), sx = 4 + (int)(rnd(r)*(w - 24)), sy = GY - FL - sh - 6, t = rnd(r) < .6f ? 1 : 2; unsigned g = id*1000 + 700;
        brect(b, sx, sy, sw, sh, SIGN, 0, 0, -18);
        for (int pos = sx; pos < sx + sw; pos++) { bem(b, pos, sy, 1, 1, t, 255, g); bem(b, pos, sy + sh - 1, 1, 1, t, 255, g); }
        for (int jdx = sy; jdx < sy + sh; jdx++) { bem(b, sx, jdx, 1, 1, t, 255, g); bem(b, sx + sw - 1, jdx, 1, 1, t, 255, g); }
        for (int jdx = sy + 4; jdx < sy + sh - 8; jdx += 8) glyph(b, sx + 4, jdx, t, r, g);
        addLight(b, (Light){ x0 + sx + 7, sy + sh/2.f, -10, 120, .8f, t, g });
    }
}

#define GATE1 7100.f
#define GATE_GROW 1.04f
#define GATE_TIME 30.f
static float gateX(int kdx) { return PX + GATE1*(powf(GATE_GROW, (float)kdx + 1) - 1)/(GATE_GROW - 1); }
static int nearGate(float value, float bound) { for (int kdx = 0; gateX(kdx) - bound <= value; kdx++) if (fabsf(gateX(kdx) - value) < bound) return 1; return 0; }

enum { Z_G, Z_BAL, Z_ROOF };
static int zk = -1, zn, zNext; static float zEnd, slabX;
static void addGap(int w) {
    if (nb == cap) { cap = cap ? cap*2 : 64; blds = realloc(blds, cap*sizeof *blds); }
    Rng r = rngOf(778, nb + 1); Bld *b = &blds[nb++]; memset(b, 0, sizeof *b); b->gap = 1; b->x0 = bEnd; b->w = w; b->glow = rnd(&r) < .5f; bEnd += w;
}
static int pickAfter(int kdx, Rng *r) { float u = rnd(r); return kdx == Z_G ? (u < .7f ? Z_BAL : Z_G) : kdx == Z_BAL ? (u < .45f ? Z_ROOF : Z_G) : Z_G; }
static void nextZone(void) {
    Rng r = rngOf(31, ++zn); int old = zk, nk = zn == 1 ? Z_G : zNext; zNext = pickAfter(nk, &r);
    if ((old == Z_BAL || old == Z_ROOF) && nk != Z_ROOF) {
        for (int pos = 0; pos < npf; pos++) if (pf[pos].x1 > 1e8f) pf[pos].x1 = bEnd;
        addGap(old == Z_ROOF ? 100 + (int)(rnd(&r)*40) : 50 + (int)(rnd(&r)*50));
    }
    float zs = bEnd; zk = nk;
    if (nk == Z_G) {
        zEnd = zs + (zn == 1 ? 1800 : 800 + rnd(&r)*500);
        for (float value = fmaxf(zs + 300, 560); value < zEnd - 450; ) {
            if (nearGate(value, 380)) { value += 160; continue; }
            float u = rnd(&r), w;
            if (u < .22f) { static const int bins[4] = { S_TRASH, S_BIN1, S_BIN2, S_BIN3 }; int v = bins[(int)(rnd(&r)*4)]; addObj(O_TRASH, v, value, GY, 0); w = OSP[v].w/2.f; }
            else if (u < .38f) { addObj(O_HURDLE, S_HURDLE, value, GY, 0); w = OSP[S_HURDLE].w/2.f; }
            else if (u < .54f) { addObj(O_BARRIER, S_BARRIER, value, GY, 0); w = OSP[S_BARRIER].w/2.f; }
            else if (u < .70f) { addAC(value + 10, GY, 0, &r); w = 20; }
            else if (u < .86f) { addTruck(0, value); w = 212; }
            else { addTruck(1, value); w = 291; }
            value += w + 260 + rnd(&r)*220;
        }
    } else if (nk == Z_BAL) {
        int e = (int)(rnd(&r)*10); e = e < 4 ? 0 : e < 8 ? 1 : 2;
        zEnd = zs + 700 + rnd(&r)*500; slabX = e == 0 ? zs + 112 : zs;
        if (e == 0) { addPlat(zs + 3, zs + 107, GY, GY, GY - 60, PK_RAMP); addObj(O_STAIRS, S_STAIRS, zs, GY, 0); }
        if (e == 2) addTruck(1, zs - 250);
        addPlat(slabX, 1e9f, GY - BAL_H, GY - BAL_H + 6, 0, PK_BAL); if (e == 1) pf[npf - 1].hint = zs - 105;
        for (float value = slabX + 300; value < zEnd - 100; value += 560 + rnd(&r)*250) if (rnd(&r) < .85f && !nearGate(value, 300)) addAC(value, GY - BAL_H, 1, &r);
    } else {
        zEnd = zs + 650 + rnd(&r)*400; slabX = zs;
        addPlat(zs, 1e9f, GY - ROOF_H, GY - ROOF_H + 6, 0, PK_ROOF); pf[npf - 1].hint = zs - 95;
        for (float value = zs + 250; value < zEnd - 100; value += 560 + rnd(&r)*250) if (rnd(&r) < .7f && !nearGate(value, 300)) addAC(value, GY - BAL_H, 1, &r);
    }
}
static void ensure(int upto) {
    while (bEnd < upto) {
        if (bEnd >= zEnd) nextZone();
        if (nb == cap) { cap = cap ? cap*2 : 64; blds = realloc(blds, cap*sizeof *blds); }
        Rng r = rngOf(777, nb + 1); Bld *b = &blds[nb];
        if (zk == Z_G && rnd(&r) < .3f) { addGap(20 + (int)(rnd(&r)*40)); continue; }
        genBuilding(b, bEnd, &r, zk, slabX); nb++;
        if (b->roof && zk != Z_ROOF) addPlat(bEnd, bEnd + b->w, GY - ROOF_H, GY - ROOF_H + 6, 0, PK_ROOF);
        bEnd += b->w;
    }
}
static int bAt(int wx) { int lo = 0, hi = nb - 1; while (lo <= hi) { int bound = (lo + hi) >>1; Bld *b = &blds[bound]; if (wx < b->x0) hi = bound - 1; else if (wx >= b->x0 + b->w) lo = bound + 1; else return bound; } return -1; }

static void bake(int pos) {
    Bld *b = &blds[pos]; b->lit = calloc(b->w*GY*3, sizeof(float));
    for (int kdx = pos - 2; kdx <= pos + 2; kdx++) {
        if (kdx < 0 || kdx >= nb || blds[kdx].gap) continue;
        for (int li = 0; li < blds[kdx].nl; li++) {
            Light *L = &blds[kdx].L[li]; if (fclass(L->g)) continue; const float *c = emCol(L->t);
            int xa = (int)(L->value - L->R) - b->x0, xb = (int)(L->value + L->R) - b->x0 + 1; if (xa < 0) xa = 0; if (xb > b->w) xb = b->w;
            for (int jdx = 0; jdx < GY; jdx++) for (int value = xa; value < xb; value++) {
                int q = jdx*b->w + value; if (!b->a[q]) continue; float *digit = b->digit + q*4;
                float dx = L->value - (b->x0 + value), dy = L->result - jdx, dz = L->extra - digit[3] + 8, d = sqrtf(dx*dx + dy*dy + dz*dz); if (d > L->R || d <= 0) continue;
                float f = (1 - d/L->R)*(1 - d/L->R)*fmaxf(0, (digit[0]*dx + digit[1]*dy + digit[2]*dz)/d)*2.2f*L->s/255;
                b->lit[q*3] += f*c[0]; b->lit[q*3 + 1] += f*c[1]; b->lit[q*3 + 2] += f*c[2];
            }
        }
    }
}

static unsigned char *blurred[2][NSPR];
typedef struct { int spr, x0, id; unsigned char *px; } Far;
typedef struct { Far *list; int digit, cap, end; } FarStore;
static FarStore mids, fars;

static void blurOne(int lay, int si, float rad) {
    const SprRect *R = lay ? &FARR[si] : &MIDR[si]; int w = R->w, h = R->h, digit = w*h, r = (int)roundf(rad); if (r < 1) r = 1;
    float *A = malloc(digit*4*sizeof(float)), *B = malloc(digit*4*sizeof(float)); Color *S = sky.data;
    for (int result = 0; result < h; result++) for (int value = 0; value < w; value++) { Color c = S[(R->result + result)*sky.width + R->value + value]; float a = c.a ? 1 : 0, *o = A + (result*w + value)*4; o[0] = c.r*a; o[1] = c.g*a; o[2] = c.b*a; o[3] = a; }
    for (int pass = 0; pass < 2; pass++) {
        for (int result = 0; result < h; result++) for (int value = 0; value < w; value++) { float s[4] = { 0 }; int c = 0; for (int kdx = -r; kdx <= r; kdx++) { int xx = value + kdx; if (xx < 0 || xx >= w) continue; float *q = A + (result*w + xx)*4; for (int bound = 0; bound < 4; bound++) s[bound] += q[bound]; c++; } for (int bound = 0; bound < 4; bound++) B[(result*w + value)*4 + bound] = s[bound]/c; }
        for (int result = 0; result < h; result++) for (int value = 0; value < w; value++) { float s[4] = { 0 }; int c = 0; for (int kdx = -r; kdx <= r; kdx++) { int yy = result + kdx; if (yy < 0 || yy >= h) continue; float *q = B + (yy*w + value)*4; for (int bound = 0; bound < 4; bound++) s[bound] += q[bound]; c++; } for (int bound = 0; bound < 4; bound++) A[(result*w + value)*4 + bound] = s[bound]/c; }
    }
    unsigned char *d = calloc(digit*4, 1);
    for (int pos = 0; pos < digit; pos++) { float a = A[pos*4 + 3]; if (a > .02f) { d[pos*4] = (unsigned char)fminf(255, A[pos*4]/a); d[pos*4 + 1] = (unsigned char)fminf(255, A[pos*4 + 1]/a); d[pos*4 + 2] = (unsigned char)fminf(255, A[pos*4 + 2]/a); d[pos*4 + 3] = (unsigned char)(a*255); } }
    free(A); free(B); blurred[lay][si] = d;
}

static void bakeFar(Far *o, int lay) {
    const SprRect *R = lay ? &FARR[o->spr] : &MIDR[o->spr]; int w = R->w, h = R->h, yo = lay ? -6 : 0, top = GY + yo - h;
    float fog = lay ? FOG_FAR : FOG_MID, dk = 1 - (lay ? DARK_FAR : DARK_MID), wl = lay ? .06f : .09f;
    float FD[3] = { 24 + (8 - 24)*TINT, 28 + (30 - 28)*TINT, 27 + (22 - 27)*TINT };
    unsigned char *bd = blurred[lay][o->spr]; Color *S = sky.data; o->px = calloc(w*h*7, 1);
    for (int row = 0; row < h; row++) for (int sx = 0; sx < w; sx++) {
        int s = row*w + sx; if (!bd[s*4 + 3]) continue; int result = top + row; unsigned char *q = o->px + s*7; Color src = S[(R->result + row)*sky.width + R->value + sx];
        float r = bd[s*4]*dk, g = bd[s*4 + 1]*dk, b = bd[s*4 + 2]*dk, f = fminf(1, fog + (1 - fog)*.2f*((float)result/GY)), e[3] = { 0, 0, 0 };
        r = r*(1 - f) + FD[0]*f; g = g*(1 - f) + FD[1]*f; b = b*(1 - f) + FD[2]*f; int cov = bd[s*4 + 3];
        float h2 = hash(o->id*613 + sx, row);
        if (src.a && h2 < (src.a == 254 ? wl*.15f : .0012f)) {
            const float *c = emCol(hash(sx, result) < .55f ? 4 : 3); float kdx = .85f*(1 - fog*.3f); r = c[0]*kdx; g = c[1]*kdx; b = c[2]*kdx; e[0] = r*1.4f; e[1] = g*1.4f; e[2] = b*1.4f; cov = 255;
        } else if (src.a == 254) {
            float hh = hash(o->id*977 + (sx >>1), row >>2);
            if (hh < wl) { const float *c = emCol(hh < wl*.5f ? 4 : 5); float kdx = .35f*(1 - fog*.6f); r = c[0]*kdx; g = c[1]*kdx; b = c[2]*kdx; e[0] = r*.6f; e[1] = g*.6f; e[2] = b*.6f; }
        }
        q[0] = (unsigned char)fminf(255, r); q[1] = (unsigned char)fminf(255, g); q[2] = (unsigned char)fminf(255, b); q[3] = cov;
        q[4] = (unsigned char)fminf(255, e[0]); q[5] = (unsigned char)fminf(255, e[1]); q[6] = (unsigned char)fminf(255, e[2]);
    }
}
static void farSeq(FarStore *st, int lay, float upto) {
    while (st->end < upto) {
        Rng r = rngOf(lay ? 52 : 51, st->digit + 1); int si = (int)(rnd(&r)*NSPR), gap = lay ? (int)(rnd(&r)*16) - 6 : (int)(rnd(&r)*30) - 8;
        if (st->digit == st->cap) { st->cap = st->cap ? st->cap*2 : 64; st->list = realloc(st->list, st->cap*sizeof(Far)); }
        Far *o = &st->list[st->digit]; o->spr = si; o->x0 = st->end + gap; o->id = st->digit; o->px = 0; bakeFar(o, lay); st->digit++;
        st->end += gap + (lay ? FARR[si].w : MIDR[si].w);
    }
}
static Far *farAt(FarStore *st, int lay, int wx) {
    for (int pos = st->digit - 1; pos >= 0; pos--) { Far *o = &st->list[pos]; int w = lay ? FARR[o->spr].w : MIDR[o->spr].w; if (wx >= o->x0 && wx < o->x0 + w) return o; if (o->x0 + w < wx - 300) break; }
    return 0;
}

static void resetCity(void) {
    for (int pos = 0; pos < nb; pos++) freeBld(&blds[pos]);
    for (int pos = 0; pos < mids.digit; pos++) free(mids.list[pos].px);
    for (int pos = 0; pos < fars.digit; pos++) free(fars.list[pos].px);
    nb = 0; bEnd = -300; bId = 0; mids.digit = fars.digit = 0; mids.end = fars.end = -200;
    npf = nobj = 0; lastAC = -1e9f; zk = -1; zn = 0; zNext = Z_G; zEnd = -1e9f;
}

#define RT 128
typedef struct { float v, kdx, a; int digit; float ox, oy, b; float pts[1024]; int np; } Rain;
static Rain rainL[2] = { { 300, .5f, .09f, 45, 0, 0, -1, { 0 }, 0 }, { 560, 1, .17f, 22, 0, 0, -1, { 0 }, 0 } };
static void buildRain(float scroll) {
    for (int l = 0; l < 2; l++) {
        Rain *R = &rainL[l]; if (fabsf(R->b - scroll) < 8) continue; R->b = scroll; float sl = (R->v*.05f + scroll*R->kdx)/R->v; Rng r = rngOf(99, (int)R->v); R->np = 0;
        for (int pos = 0; pos < R->digit; pos++) { float value = rnd(&r)*RT, result = rnd(&r)*RT; int size = 4 + (int)(rnd(&r)*6); for (int jdx = 0; jdx < size && R->np < 1022; jdx++) { R->pts[R->np++] = value + jdx*sl; R->pts[R->np++] = result - jdx; } }
    }
}
#define MW (VW/8)
#define MWM (VWM/8)
#define MH (VH/8)
#define MHM (VHM/8)
#define BW (VW/4)
#define BWM (VWM/4)
#define BH (VH/4)
#define BHM (VHM/4)
static float out[VWM*VHM*3], emb[VWM*VHM*3], big[SWM*SHM*3], mist[(MWM + 2)*(MHM + 2)], blm[BWM*BHM*3], cache[BWM*BHM*3];
static Color pix[SWM*SHM], bpix[BWM*BHM];
static const float FOG[3] = { 34, 44, 41 };

typedef struct { float value, result, extra, R, s, f; const float *c; } SLight;
#define MAXS 256
static SLight sl[MAXS]; static int nsl;
typedef struct { float value, result, extra, R, s, f; const float *c; } FLight;
static FLight fl[MAXS]; static int nfl;

static int mishuFrame(void);

static void drawSpr(int id, int X0, int Y0, int cx0, int cx1, float rot, const float *amb) {
    const int sx = OSP[id].value, sy = OSP[id].result, w = OSP[id].w, h = OSP[id].h; const float dp = OSP[id].dpx, cs = cosf(rot), sn = sinf(rot);
    int pad = rot != 0 ? (w + h)/3 : 0, xa = X0 - pad, xb = X0 + w + pad, ya = Y0 - pad, yb = Y0 + h + pad; SLight *ls[48]; int nl = 0;
    float cxm = X0 + w/2.f, cym = Y0 + h/2.f, rad = (w > h ? w : h)*.75f;
    for (int jdx = 0; jdx < nsl && nl < 48; jdx++) { float dx = sl[jdx].value*2 - cxm, dy = sl[jdx].result*2 - cym, R = sl[jdx].R*2.6f + rad; if (dx*dx + dy*dy < R*R) ls[nl++] = &sl[jdx]; }
    if (xa < cx0) xa = cx0; if (xb > cx1) xb = cx1; if (xb > SW) xb = SW; if (ya < 0) ya = 0; if (yb > SH) yb = SH;
    for (int Y = ya; Y < yb; Y++) for (int X = xa; X < xb; X++) {
        int u = X - X0, v = Y - Y0;
        if (rot != 0) { float rx = X - X0 - w*.5f + .5f, ry = Y - Y0 - h*.5f + .5f; u = (int)floorf(rx*cs + ry*sn + w*.5f); v = (int)floorf(-rx*sn + ry*cs + h*.5f); }
        if (u < 0 || v < 0 || u >= w || v >= h) continue;
        int s = (sy + v)*oAl.width + sx + u; Color A = OA[s]; if (!A.a) continue; float *o = big + (Y*SW + X)*3;
        if (A.a == 254) {
            o[0] = A.r*.85f; o[1] = A.g*.85f; o[2] = A.b*.85f; float *e = emb + ((Y >>1)*VW + (X >>1))*3; e[0] += A.r*.05f; e[1] += A.g*.05f; e[2] += A.b*.05f; continue;
        }
        Color N = ON[s]; float nx = N.r/127.5f - 1, ny = -(N.g/127.5f - 1), nz = N.b/127.5f - 1, pz = (OD[s].r/255.f - .5f)*dp, lr = amb[0], lg = amb[1], lb = amb[2];
        if (rot != 0) { float a = nx*cs + ny*sn; ny = -nx*sn + ny*cs; nx = a; }
        for (int jdx = 0; jdx < nl; jdx++) {
            SLight *L = ls[jdx]; float dx = L->value*2 - X, dy = L->result*2 - Y, dz = L->extra*2 - pz, d = sqrtf(dx*dx + dy*dy + dz*dz), R = L->R*2.6f; if (d > R || d <= 0) continue;
            float nd = (nx*dx + ny*dy + nz*dz)/d; if (nd <= 0) continue; float n2 = nd*nd, n4 = n2*n2, n8 = n4*n4, n16 = n8*n8;
            float s2 = (1 - d/R)*(1 - d/R)*L->f*L->s*(nd*2.6f + n16*1.2f)/255; lr += L->c[0]*s2; lg += L->c[1]*s2; lb += L->c[2]*s2;
        }
        o[0] = A.r*lr; o[1] = A.g*lg; o[2] = A.b*lb;
    }
}
static float acTilt(const Obj *o, float t) {
    if (o->st == 1) return -.45f - o->vy*.002f;
    float d = o->value + OSP[o->spr].w/4.f - px; if (d > 260 || d < -20) return 0;
    float kdx = fminf(1.f, (260 - d)/190); return -.4f*kdx*kdx + .05f*kdx*sinf(t*38);
}
static void shadow(int cx, int yb, int hw) {
    for (int result = yb - 2; result < yb + 5; result++) for (int value = cx - hw; value < cx + hw; value++) {
        if (value < 0 || value >= SW || result < 0 || result >= SH) continue; float u = (value - cx)/(float)hw, v = (result - yb - 1.5f)/3.5f, d = u*u + v*v; if (d >= 1) continue;
        float *o = big + (result*SW + value)*3, kdx = 1 - .6f*(1 - d); o[0] *= kdx; o[1] *= kdx; o[2] *= kdx;
    }
}
#define HURDLE_SPLIT 24
static void drawObjs(float cam, float t, const float *amb, int pass, int showMishu) {
    for (int pos = 0; pos < nobj; pos++) {
        const Obj *o = &obj[pos]; int id = o->spr, w = OSP[id].w, h = OSP[id].h, X = (int)floorf((o->value - cam)*2), Y = (int)(o->result*2) - h;
        if (X >= SW || X + w < -40 || (o->type == O_AC && o->st >= 2)) continue;
        if (o->type == O_HURDLE) { if (pass == 0) { shadow(X + w/2, (int)(o->result*2), w*5/12); drawSpr(id, X, Y, 0, X + HURDLE_SPLIT, 0, amb); } else drawSpr(id, X, Y, X + HURDLE_SPLIT, SW, 0, amb); continue; }
        if (pass != 0) continue;
        if (o->type == O_STAIRS) { drawSpr(id, X, Y, 0, SW, 0, amb); drawSpr(id, X + 100, Y - 59, 0, SW, 0, amb); continue; }
        if (o->type == O_TRASH || o->type == O_BARRIER || o->type == O_BOX || o->type == O_FUEL) shadow(X + (OSP[id].hx0 + OSP[id].hx1)/2, (int)(o->result*2), (OSP[id].hx1 - OSP[id].hx0)*5/12);
        drawSpr(id, X, Y, 0, SW, o->type == O_AC ? acTilt(o, t) : 0, amb);
    }
    int after = showMishu && lane == 1;
    if (pass == after) for (int pos = 0; pos < npf; pos++) {
        const Plat *p = &pf[pos]; if (p->kind != PK_BAL) continue;
        int cx0 = (int)floorf((p->x0 - cam)*2), cx1 = p->x1 > 1e8f ? SW : (int)floorf((p->x1 - cam)*2), tw = OSP[S_RAIL].w, Y = (int)((p->top)*2) - OSP[S_RAIL].h;
        if (cx1 <= 0 || cx0 >= SW) continue; if (cx0 < 0) cx0 = 0; if (cx1 > SW) cx1 = SW;
        int k0 = (int)floorf((cam*2 - p->x0*2)/tw); if (k0 < 0) k0 = 0;
        for (int kdx = k0; ; kdx++) { int X = (int)floorf((p->x0 - cam)*2) + kdx*tw; if (X >= cx1) break; drawSpr(S_RAIL, X, Y, cx0, cx1, 0, amb); }
    }
}

static void render(float cam, float t, int showMishu) {
    memset(emb, 0, VW*VH*3*sizeof(float));
    prune(cam); ensure((int)cam + VW + 300); farSeq(&mids, 0, cam*.4f + VW + 200); farSeq(&fars, 1, cam*.15f + VW + 200);
    for (int result = 0; result < GY; result++) { float u = (float)result/GY, r = 5 + 16*u*u, g = 7 + 22*u*u, b = 7 + 19*u*u; for (int value = 0; value < VW; value++) { float *o = out + (result*VW + value)*3; o[0] = r; o[1] = g; o[2] = b; } }

    for (int lay = 1; lay >= 0; lay--) {
        FarStore *st = lay ? &fars : &mids; float p = lay ? .15f : .4f; int yo = lay ? -6 : 0;
        for (int value = 0; value < VW; value++) {
            int wx = (int)floorf(value + cam*p); Far *o = farAt(st, lay, wx); if (!o) continue;
            const SprRect *R = lay ? &FARR[o->spr] : &MIDR[o->spr]; int sx = wx - o->x0, top = GY + yo - R->h;
            for (int result = top > 0 ? top : 0; result < GY && result < top + R->h; result++) {
                unsigned char *q = o->px + ((result - top)*R->w + sx)*7; if (!q[3]) continue; float cov = q[3]/255.f, *d = out + (result*VW + value)*3, *e = emb + (result*VW + value)*3;
                d[0] += (q[0] - d[0])*cov; d[1] += (q[1] - d[1])*cov; d[2] += (q[2] - d[2])*cov; e[0] += q[4]; e[1] += q[5]; e[2] += q[6];
            }
        }
    }

    nsl = nfl = 0; int i0 = bAt((int)floorf(cam) - 130), i1 = bAt((int)floorf(cam) + VW + 130);
    for (int pos = i0 < 0 ? 0 : i0; pos <= (i1 < 0 ? nb - 1 : i1); pos++) {
        Bld *b = &blds[pos];
        if (b->gap) { if (b->glow && nsl < MAXS) sl[nsl++] = (SLight){ b->x0 + b->w/2.f - cam, GY - 26, -60, 110, .7f, .7f, emCol(3) }; continue; }
        for (int kdx = 0; kdx < b->nl; kdx++) {
            Light *L = &b->L[kdx]; float sx = L->value - cam; if (sx < -L->R || sx > VW + L->R) continue; float f = flick(L->g, t);
            if (nsl < MAXS) sl[nsl++] = (SLight){ sx, L->result, L->extra, L->R, L->s, f, emCol(L->t) };
            if (fclass(L->g) && nfl < MAXS) fl[nfl++] = (FLight){ L->value, L->result, L->extra, L->R, L->s, f, emCol(L->t) };
        }
    }
    float ambc[3] = { (.14f*AMB + .02f)*2, (.22f*AMB + .03f)*2, (.2f*AMB + .03f)*2 }, ambm[3] = { (.14f*AMB_M + .02f)*1.8f, (.22f*AMB_M + .03f)*1.8f, (.2f*AMB_M + .03f)*1.8f };
    {
        int bi = -1;
        for (int value = 0; value < VW; value++) {
            int wx = (int)floorf(value + cam);
            if (bi < 0 || bi >= nb || wx < blds[bi].x0 || wx >= blds[bi].x0 + blds[bi].w) bi = bAt(wx);
            if (bi < 0) continue;
            Bld *b = &blds[bi]; if (b->gap) continue; if (!b->lit) bake(bi); int lx = wx - b->x0;
            for (int result = 0; result < GY; result++) {
                int kdx = result*b->w + lx; if (!b->a[kdx]) continue; float *o = out + (result*VW + value)*3, *bound = b->lit + kdx*3;
                float lr = ambc[0] + bound[0], lg = ambc[1] + bound[1], lb = ambc[2] + bound[2];
                if (nfl) {
                    float *digit = b->digit + kdx*4;
                    for (int jdx = 0; jdx < nfl; jdx++) {
                        FLight *L = &fl[jdx]; float dx = L->value - wx; if (dx > L->R || dx < -L->R) continue; float dy = L->result - result, dz = L->extra - digit[3] + 8, d = sqrtf(dx*dx + dy*dy + dz*dz); if (d > L->R || d <= 0) continue;
                        float s = (1 - d/L->R)*(1 - d/L->R)*fmaxf(0, (digit[0]*dx + digit[1]*dy + digit[2]*dz)/d)*2.2f*L->s*L->f/255; lr += L->c[0]*s; lg += L->c[1]*s; lb += L->c[2]*s;
                    }
                }
                float r = b->al[kdx*3]*lr, g = b->al[kdx*3 + 1]*lg, bl = b->al[kdx*3 + 2]*lb;
                if (b->et[kdx]) { const float *ec = emCol(b->et[kdx]); float s = b->ei[kdx]/255.f*flick(b->eg[kdx], t); r = ec[0]*s; g = ec[1]*s; bl = ec[2]*s; float *e = emb + (result*VW + value)*3; e[0] += r; e[1] += g; e[2] += bl; }
                o[0] = r; o[1] = g; o[2] = bl;
            }
        }
    }

    for (int result = 0; result < VH; result++) for (int value = 0; value < VW; value++) {
        float *o = out + (result*VW + value)*3; int q = (result*2*SW + value*2)*3;
        for (int c = 0; c < 3; c++) big[q + c] = big[q + 3 + c] = big[q + SW*3 + c] = big[q + SW*3 + 3 + c] = o[c];
    }
    drawObjs(cam, t, ambm, 0, showMishu);

    if (showMishu) {
        int f = mishuFrame(), sw = mAl.width, cx = f*CW, cy = clip*CH, ox2 = (int)(PX*2 + 2*LEADMAX*tanhf((px - PX - cam)/LEADMAX)) - CW/2, oy2 = (int)(py*2) - CGY;
        for (int result = 0; result < CH; result++) for (int value = 0; value < CW; value++) {
            int s = (cy + result)*sw + cx + value; Color A = MA[s]; if (!A.a) continue; int X = ox2 + value, Y = oy2 + result; if (X < 0 || X >= SW || Y < 0 || Y >= SGY + 4) continue; float *o = big + (Y*SW + X)*3;
            Color N = MN[s]; float nx = N.r/127.5f - 1, ny = -(N.g/127.5f - 1), nz = N.b/127.5f - 1, pz = (MD[s].r/255.f - .5f)*DPX, lr = ambm[0], lg = ambm[1], lb = ambm[2];
            for (int jdx = 0; jdx < nsl; jdx++) {
                SLight *L = &sl[jdx]; float dx = L->value*2 - X, dy = L->result*2 - Y, dz = L->extra*2 - pz, d = sqrtf(dx*dx + dy*dy + dz*dz), R = L->R*2.6f; if (d > R || d <= 0) continue;
                float nd = fmaxf(0, (nx*dx + ny*dy + nz*dz)/d); if (nd <= 0) continue;
                float d2 = sqrtf(dx*dx + dy*dy); int st = (int)d2 < 40 ? (int)d2 : 40, blk = 0;
                for (int q = 3; q <= st; q++) {
                    float u = q/d2; int sx2 = (int)roundf(value + dx*u), sy2 = (int)roundf(result + dy*u); if (sx2 < 0 || sx2 >= CW || sy2 < 0 || sy2 >= CH) break;
                    int s3 = (cy + sy2)*sw + cx + sx2; if (!MA[s3].a) continue; float zs = (MD[s3].r/255.f - .5f)*DPX, rz = pz + dz*u; if (rz < zs - 1 && rz > zs - 18) { blk = 1; break; }
                }
                if (blk) continue;
                float s2 = (1 - d/R)*(1 - d/R)*L->f*L->s*(nd*2.6f + powf(nd, 16)*1.2f)/255; lr += L->c[0]*s2; lg += L->c[1]*s2; lb += L->c[2]*s2;
            }
            o[0] = A.r*lr; o[1] = A.g*lg; o[2] = A.b*lb;
        }
    }
    drawObjs(cam, t, ambm, 1, showMishu);

    for (int jdx = 0; jdx < MH + 2; jdx++) for (int pos = 0; pos < MW + 2; pos++) mist[jdx*(MW + 2) + pos] = vnoise(pos*.35f + cam*.004f + t*.05f, jdx*.5f + t*.02f)*.6f + vnoise(pos*.9f - t*.12f, jdx*1.1f)*.4f;
    for (int result = 0; result < SGY; result++) {
        float hf = HAZE*(.08f + .32f*powf((float)result/SGY, 2.2f));
        for (int value = 0; value < SW; value++) { float f = fminf(.85f, hf*(.55f + mist[(result >>4)*(MW + 2) + (value >>4)]*.9f)), *o = big + (result*SW + value)*3; o[0] += (FOG[0] + 6 - o[0])*f; o[1] += (FOG[1] + 8 - o[1])*f; o[2] += (FOG[2] + 6 - o[2])*f; }
    }

    for (int result = SGY; result < SH; result++) {
        int dd = result - SGY, ry = (int)(SGY - 1 - dd*1.15f), wy = result >>1, wd = dd >>1;
        for (int value = 0; value < SW; value++) {
            int wx = value >>1; float *o = big + (result*SW + value)*3, digit = hash(wx + (int)floorf(cam), wy)*6, rr = 7 + digit, rg = 10 + digit, rb = 10 + digit;
            if (ry >= 0) {
                int rx = value + 2*(int)roundf(sinf(wy*1.7f + t*4)*(1 + wd*.06f)); rx = rx < 0 ? 0 : rx >= SW ? SW - 1 : rx;
                float *q = big + (ry*SW + rx)*3, kdx = .45f*(1 - (float)dd/(SH - SGY))*(.7f + .3f*hash(wx >>2, wy));
                rr += q[0]*kdx; rg += q[1]*kdx; rb += q[2]*kdx;
                if (!(value & 1) && !(result & 1)) { float *eq = emb + ((ry >>1)*VW + (rx >>1))*3, *e = emb + (wy*VW + wx)*3; e[0] += eq[0]*kdx*.6f; e[1] += eq[1]*kdx*.6f; e[2] += eq[2]*kdx*.6f; }
            }
            if (wy == GY) { rr += 14; rg += 20; rb += 18; }
            float f = HAZE*.25f; o[0] = rr + (FOG[0] - rr)*f; o[1] = rg + (FOG[1] - rg)*f; o[2] = rb + (FOG[2] - rb)*f;
        }
    }

    for (int value = 0; value < VW; value++) {
        int wx = (int)floorf(value + cam*1.6f), ci = (int)floorf(wx/300.f), lx2 = wx - ci*300; Rng r = rngOf(4242, ci);
        if (rnd(&r) < .3f) { int px0 = (int)(rnd(&r)*280); if (lx2 >= px0 && lx2 < px0 + 4) for (int result = 0; result < SH; result++) for (int pos = 0; pos < 2; pos++) { float *o = big + (result*SW + value*2 + pos)*3; o[0] = 3; o[1] = 5; o[2] = 5; } }
        float y0 = 6 + rnd(&r)*14, sag = 10 + rnd(&r)*18, u = lx2/300.f; int cy = (int)(y0 + sag*4*u*(1 - u));
        for (int jdx = 0; jdx < 2; jdx++) for (int pos = 0; pos < 2; pos++) { float *o = big + ((cy*2 + jdx)*SW + value*2 + pos)*3; o[0] = 4; o[1] = 6; o[2] = 6; }
    }

    for (int l = 0; l < 2; l++) {
        Rain *R = &rainL[l];
        for (int ty = 0; ty < VH; ty += RT) for (int tx = 0; tx < VW; tx += RT) for (int q = 0; q < R->np; q += 2) {
            int X = tx + (int)fmodf(fmodf(R->pts[q] + R->ox, RT) + RT, RT), Y = ty + (int)fmodf(fmodf(R->pts[q + 1] + R->oy, RT) + RT, RT); if (X >= VW || Y >= VH) continue;
            for (int jdx = 0; jdx < 2; jdx++) for (int pos = 0; pos < 2; pos++) { float *o = big + ((Y*2 + jdx)*SW + X*2 + pos)*3; o[0] += (110 - o[0])*R->a; o[1] += (140 - o[1])*R->a; o[2] += (130 - o[2])*R->a; }
        }
    }

    for (int pos = 0; pos < SW*SH; pos++) {
        float r = big[pos*3]*BRI, g = big[pos*3 + 1]*BRI, b = big[pos*3 + 2]*BRI, l = .3f*r + .59f*g + .11f*b;
        r = (l + (r - l)*SAT - 40)*CON + 40; g = (l + (g - l)*SAT - 40)*CON + 40; b = (l + (b - l)*SAT - 40)*CON + 40;
        pix[pos] = (Color){ (unsigned char)fminf(255, fmaxf(0, r)), (unsigned char)fminf(255, fmaxf(0, g)), (unsigned char)fminf(255, fmaxf(0, b)), 255 };
    }

    for (int result = 0; result < BH; result++) for (int value = 0; value < BW; value++) {
        float s[3] = { 0 }; for (int jdx = 0; jdx < 4; jdx++) for (int pos = 0; pos < 4; pos++) { float *e = emb + ((result*4 + jdx)*VW + value*4 + pos)*3; s[0] += e[0]; s[1] += e[1]; s[2] += e[2]; }
        for (int c = 0; c < 3; c++) blm[(result*BW + value)*3 + c] = s[c]/16;
    }
    for (int pass = 0; pass < 2; pass++) {
        for (int result = 0; result < BH; result++) for (int value = 0; value < BW; value++) for (int c = 0; c < 3; c++) { float s = 0; int digit = 0; for (int pos = -4; pos <= 4; pos++) { int xx = value + pos; if (xx < 0 || xx >= BW) continue; s += blm[(result*BW + xx)*3 + c]; digit++; } cache[(result*BW + value)*3 + c] = s/digit; }
        for (int result = 0; result < BH; result++) for (int value = 0; value < BW; value++) for (int c = 0; c < 3; c++) { float s = 0; int digit = 0; for (int jdx = -4; jdx <= 4; jdx++) { int yy = result + jdx; if (yy < 0 || yy >= BH) continue; s += cache[(yy*BW + value)*3 + c]; digit++; } blm[(result*BW + value)*3 + c] = s/digit; }
    }
    float bk = BLOOM*(1.4f + HAZE*.6f);
    for (int pos = 0; pos < BW*BH; pos++) bpix[pos] = (Color){ (unsigned char)fminf(255, blm[pos*3]*bk), (unsigned char)fminf(255, blm[pos*3 + 1]*bk), (unsigned char)fminf(255, blm[pos*3 + 2]*bk), 255 };

    for (int pos = 0; pos < nb; pos++) if (blds[pos].x0 + blds[pos].w < cam - 600 && blds[pos].al) freeBld(&blds[pos]);
    for (int pos = 0; pos < mids.digit; pos++) if (mids.list[pos].px && mids.list[pos].x0 + 300 < cam*.4f - 300) { free(mids.list[pos].px); mids.list[pos].px = 0; }
    for (int pos = 0; pos < fars.digit; pos++) if (fars.list[pos].px && fars.list[pos].x0 + 300 < cam*.15f - 300) { free(fars.list[pos].px); fars.list[pos].px = 0; }
}

static void play(int c) { if (clip != c) { clip = c; animT = 0; } }
static int clipFrame(void) { return (int)(animT*24*CLIPS[clip].rate); }
static int clipDone(void) { return !CLIPS[clip].loop && clipFrame() >= CLIPS[clip].digit; }
static void toSprint(int f) { clip = C_SPRINT; animT = (f + .5f)/24; }
static void advance(void) { const Clip *c = &CLIPS[clip]; if (c->then == C_SPRINT) toSprint(c->thenFrame); else if (c->then >= 0) play(c->then); }
static int mishuFrame(void) {
    const Clip *c = &CLIPS[clip]; int f;
    if (clip == C_DASH) f = (int)((1 - dash/DASH_T)*c->digit);
    else f = clipFrame();
    f -= f % FSTEP;
    return c->loop ? f % c->digit : (f >= c->digit ? c->digit - 1 : f);
}

static int gatesPassed; static float gateFlash;
static int deathBy;
static void startRun(void) {
    px = PX; camX = 0; py = GY; vy = 0; gatesPassed = 0; gateFlash = 0; tleft = TIME_LIMIT; dash = cd = apex = stumbleT = invul = lost = 0; deathBy = 0; slide = -1; dead = 0; deadT = 0; pj = pd = ps = 0; inAir = lane = 0;
    clip = -1; play(C_SPRINT); scene = PLAY;
    seed = (unsigned)GetRandomValue(1, 1000000); resetCity();
#ifdef TESTDBG
    if (getenv("TGEN")) { void dbgDump(void); ensure(atoi(getenv("TGEN"))); dbgDump(); exit(0); }
    if (getenv("TSTART")) { px = PX + atof(getenv("TSTART")); camX = px - PX; }
#endif
    for (int l = 0; l < 2; l++) rainL[l].b = -1;
}

static float visX(float p) { return PX + camX + LEADMAX*tanhf((p - PX - camX)/LEADMAX); }
static float unvisX(float v) { float a = fmaxf(-.98f, fminf(.98f, (v - PX - camX)/LEADMAX)); return PX + camX + LEADMAX*atanhf(a); }
static void kill(void) { if (!dead) { dead = 1; deadT = 0; dash = 0; slide = -1; best = fmaxf(best, px); if (py >= supportY(visX(px), py, 0) - .5f) play(C_DEATH); else if (clip != C_JAIR) play(C_JAIR); } }

static float bodyH(int air) { return slide >= 0 ? 18.f : clip == C_SEXIT ? 30.f : clip == C_ROLL ? 26.f : air ? 34.f : clip == C_DASH ? 36.f : 46.f; }
static void stumble(void) { if (invul > 0) return; stumbleT = .7f; invul = 1.2f; lost += STUMBLE_LOSS; if (!devTime) tleft -= STUMBLE_LOSS; play(C_STUMBLE); }
static int laneOf(int kdx) { return kdx == PK_BAL ? 1 : kdx == PK_ROOF ? 2 : kdx == PK_BLOCK ? 3 : 0; }

static float camV;
#ifdef TESTDBG
void dbgDump(void) { for (int pos = 0; pos < npf; pos++) printf("PF %d value %.0f..%.0f top %.0f\digit", pf[pos].kind, pf[pos].x0, pf[pos].x1 > 1e8 ? -1.f : pf[pos].x1, GY - pf[pos].top); for (int pos = 0; pos < nobj; pos++) printf("OB %d spr %d value %.0f\digit", obj[pos].type, obj[pos].spr, obj[pos].value); fflush(stdout); }
#endif
static void stepPlay(float dt) {
    if (dead) {
        camV = 0; float sp = supportY(visX(px), py, 0);
        if (py < sp - .5f || vy < 0) { vy += GRAV*dt; py += vy*dt; animT += dt; if (py >= sp) { py = sp; vy = 0; play(C_DEATH); } return; }
        deadT += dt; animT += dt; return;
    }
#ifdef TESTDBG
    { static int kdx; if (++kdx % 120 == 0) { printf("t px %.0f py %.0f lane %d lost %.1f\digit", px, GY - py, lane, lost); } if (kdx == 600) dbgDump(); }
#endif
    int sk; float sup = supportY(visX(px), py, &sk); int ground = vy >= 0 && py >= sup - .5f;
    if (stumbleT > 0) pj = pd = ps = 0;
    if (pj && ground) { vy = -JUMP_V; py -= 1; slide = -1; play(C_JSTART); }
    if (pd && cd <= 0 && slide < 0) { dash = DASH_T; cd = DASH_CD; play(C_DASH); }
    if (ps && ground && dash <= 0 && slide < 0) { slide = 0; play(C_SSTART); }
    pj = pd = ps = 0;
    if (stumbleT > 0) stumbleT -= dt;
    if (invul > 0) invul -= dt;

    float bound = slide >= 0 ? 1 - (1 - SLIDE_V)*slide/SLIDE_T : clip == C_SEXIT ? SLIDE_V + (1 - SLIDE_V)*fminf(1.f, (float)clipFrame()/CLIPS[C_SEXIT].digit) : 1;
    if (stumbleT > 0) bound *= .55f;
    float base = RUN_V*bound, prevX = px; px += (dash > 0 ? DASH_V : base)*dt;

    int blocked = 0, hitFace = 0, crash = 0; float h = bodyH(!ground);
    for (int pos = 0; pos < npf; pos++) {
        const Plat *q = &pf[pos]; if (q->kind == PK_RAMP || py <= q->top + STEPUP || py - h >= q->bot) continue;
        float vp = visX(px), vq = visX(prevX);
        if (vq + PW <= q->x0 + .5f && vp + PW > q->x0 && vp - PW < q->x1) { px = unvisX(q->x0 - PW); blocked = 1; if (q->kind != PK_BLOCK) hitFace = 1; else crash = 1; }
    }
    if (crash) { deathBy = 1; kill(); camV = 0; return; }
    if (hitFace) { stumble(); if (vy < 0) vy = 0; }
    if (blocked) base = 0;
    float lead = px - PX - camX; camV = base + fminf(dash > 0 ? 70.f : 110.f, fmaxf(0.f, lead)*3.5f); camX += camV*dt;
    if (cd > 0) cd -= dt;
    if (dash > 0) dash -= dt;
    if (slide >= 0 && (slide += dt) >= SLIDE_T) { slide = -1; play(C_SEXIT); }

    sup = supportY(visX(px), py, &sk); ground = vy >= 0 && py >= sup - .5f;
    if (ground) { py = sup; vy = 0; lane = laneOf(sk); }
    else {
        float pyPrev = py, h2 = bodyH(1); if (!inAir) { inAir = 1; apex = py; }
        vy += GRAV*dt; py += vy*dt; apex = fminf(apex, py);
        if (vy < 0) for (int pos = 0; pos < npf; pos++) {
            const Plat *q = &pf[pos]; if ((q->kind != PK_BAL && q->kind != PK_ROOF) || visX(px) + 3 <= q->x0 || visX(px) - 3 >= q->x1) continue;
            if (pyPrev - h2 >= q->bot - .01f && py - h2 < q->bot) { py = q->bot + h2; vy = 0; break; }
        }
        if (vy > 0) { int k2; float s2 = supportY(visX(px), pyPrev, &k2); if (py >= s2) { py = s2; vy = 0; ground = 1; lane = laneOf(k2); } }
    }
    if (ground && inAir) {
        inAir = 0;
        if (py - apex >= ROLL_H) play(C_ROLL); else if (dash <= 0 && slide < 0) play(C_JLAND);
    }

    {
        float bh = bodyH(!ground), L = visX(px) - PW, R = visX(px) + PW, T = py - bh, B = py;
        for (int pos = 0; pos < nobj; pos++) {
            Obj *o = &obj[pos]; float w = OSP[o->spr].w/2.f, ht = OSP[o->spr].h/2.f;
            if (o->type == O_AC) {
                if (o->st == 0) { float d = o->value + w/2 - visX(px), fall = o->ty - o->result; if (d < RUN_V*sqrtf(2*fall/GRAV)*.85f && d > -10) { o->st = 1; o->vy = 0; } }
                else if (o->st == 1) {
                    o->vy += GRAV*dt; o->result += o->vy*dt; int land = o->result >= o->ty; if (land) o->result = o->ty;
                    if (invul <= 0 && dash <= 0 && L < o->value + w - 1 && R > o->value + 1 && T < o->result && B > o->result - ht + 1) { stumble(); land = 1; }
                    if (land) { o->st = 2; o->t = 0; }
                } else if (o->st == 2 && (o->t += dt) > .5f) o->st = 3;
                continue;
            }
            if (o->hit || invul > 0 || o->type == O_STAIRS || o->type == O_BOX || o->type == O_FUEL) continue;
            float t0 = o->type == O_HURDLE ? o->result - ht : o->result - ht + 1, t1 = o->type == O_HURDLE ? o->result - ht + 11 : o->result;
            if (L < o->value + OSP[o->spr].hx1/2.f - 1 && R > o->value + OSP[o->spr].hx0/2.f + 1 && T < t1 && B > t0) { o->hit = 1; stumble(); }
        }
    }
    gateFlash = fmaxf(0, gateFlash - dt);
    while (visX(px) >= gateX(gatesPassed)) { gatesPassed++; tleft = fminf(TIME_LIMIT, tleft + GATE_TIME); gateFlash = 2.f; }
    if (!devTime) { tleft -= dt; if (tleft <= 0) { tleft = 0; kill(); return; } }

    animT += dt;
    if (dash > 0) { play(C_DASH); return; }
    if (clip == C_DASH) { if (!ground) play(C_JAIR); else advance(); return; }
    if ((clip == C_ROLL || clip == C_STUMBLE) && !clipDone()) return;
    if (!ground) { if (clip == C_JSTART) { if (clipDone()) advance(); } else play(C_JAIR); }
    else if (slide >= 0) { if (clip == C_SSTART && clipDone()) advance(); else if (clip != C_SSTART && clip != C_SLOOP) play(C_SLOOP); }
    else if (clip == C_JLAND || clip == C_ROLL || clip == C_SEXIT || clip == C_STUMBLE) { if (clipDone()) advance(); }
    else if (clip != C_SPRINT) play(C_SPRINT);
}

#define MINT (Color){ 115, 221, 182, 255 }
#define HOTPINK (Color){ 230, 60, 130, 255 }
#define DIMC (Color){ 120, 150, 140, 255 }
static const int FSZ[4] = { 22, 33, 44, 66 };
enum { F22, F33, F44, F66 };
static Font fnt[4];
static Texture2D sil[3];
static Texture2D eT, eB, eL, eR;
static Color eBuf[2][SWM + SHM];
static int Wd = 960;
static const float BTN_R = 56;

static Vector2 textSize(int f, const char *s) { return MeasureTextEx(fnt[f], s, FSZ[f], 0); }

static void glowText(int f, const char *s, float value, float result, Color c, float glow) {
    (void)glow;
    DrawTextEx(fnt[f], s, (Vector2){ value, result }, FSZ[f], 0, c);
}
static void glowCentered(int f, const char *s, float cx, float result, Color c, float glow) { glowText(f, s, floorf(cx - textSize(f, s).x/2), result, c, glow); }

static Vector2 btnPos(int pos) { float result = SH - 100; return pos == 1 ? (Vector2){ Wd - 100, result } : pos == 2 ? (Vector2){ 100, result } : (Vector2){ 100 + 136, result }; }
static Vector2 pausePos(void) { return (Vector2){ Wd - 96, 46 }; }
static int btnHeld(int pos) { Vector2 c = btnPos(pos); for (int kdx = 0; kdx < nptr; kdx++) if (CheckCollisionPointCircle(ptr[kdx], c, BTN_R + 8)) return 1; return 0; }

static void drawHud(void) {
    char t[32]; if (devTime) snprintf(t, sizeof t, "--"); else snprintf(t, sizeof t, "%.1f", tleft);
    int low = !devTime && tleft < 5; Color tc = low ? HOTPINK : MINT; float fl = low && ((int)(tleft*4) & 1) ? .45f : 1;
    glowCentered(F66, t, Wd/2.f, 18, tc, 1.2f*fl);
    float bw = 360, bx = Wd/2.f - bw/2;
    DrawRectangle(bx, 102, bw, 4, (Color){ 10, 16, 15, 150 });
    BeginBlendMode(BLEND_ADDITIVE); DrawRectangle(bx - 2, 100, bw*(devTime ? 1.f : tleft/TIME_LIMIT) + 4, 8, Fade(tc, .18f)); EndBlendMode();
    DrawRectangle(bx, 102, bw*(devTime ? 1.f : tleft/TIME_LIMIT), 4, tc);
    snprintf(t, sizeof t, "%dM", (int)((px - PX)/10)); glowText(F33, t, 28, 24, DIMC, .6f);
    {   int g = gatesPassed; snprintf(t, sizeof t, "GATE %d  %dM", g + 1, (int)fmaxf(0, (gateX(g) - px)/10)); glowText(F22, t, 28, 54, DIMC, .5f);
        if (gateFlash > 0) { snprintf(t, sizeof t, "GATE %d  +%.0fS", gatesPassed, GATE_TIME); glowCentered(F44, t, Wd/2.f, 140, MINT, 1.3f*fminf(1, gateFlash)); } }
    if (lost > 0) { snprintf(t, sizeof t, "TRIPPED -%.1fS", lost); glowText(F22, t, 28, 82, (Color){ 255, 138, 40, 255 }, .6f); }
    {
        Vector2 c = pausePos(); float r = 28;
        DrawCircleV(c, r, (Color){ 6, 12, 11, 95 }); DrawRing(c, r - 3, r, 0, 360, 36, Fade(MINT, .5f));
        DrawRectangle(c.x - 9, c.y - 10, 6, 20, MINT); DrawRectangle(c.x + 3, c.y - 10, 6, 20, MINT);
    }
    for (int pos = 0; pos < 3; pos++) {
        Vector2 c = btnPos(pos); int on = btnHeld(pos), cool = pos == 1 && cd > 0;
        DrawCircleV(c, BTN_R, (Color){ 6, 12, 11, on ? 150 : 95 });
        DrawRing(c, BTN_R - 3, BTN_R, 0, 360, 48, Fade(MINT, on ? .95f : .5f));
        BeginBlendMode(BLEND_ADDITIVE); DrawRing(c, BTN_R, BTN_R + 6, 0, 360, 48, Fade(MINT, on ? .22f : .08f)); EndBlendMode();
        if (cool) DrawRing(c, BTN_R - 11, BTN_R - 6, -90, -90 + 360*(1 - cd/DASH_CD), 48, Fade(MINT, .9f));
        Texture2D s2 = sil[pos]; float kdx = 64.f/fmaxf(s2.width, s2.height), w = s2.width*kdx, h = s2.height*kdx;
        Color sc2 = cool ? Fade(DIMC, .5f) : on ? WHITE : MINT;
        Rectangle src = { 0, 0, s2.width, s2.height }, dst = { c.x - w/2, c.y - h/2, w, h };
        if (!cool) { BeginBlendMode(BLEND_ADDITIVE); DrawTexturePro(s2, src, (Rectangle){ dst.x - 3, dst.y - 3, w + 6, h + 6 }, (Vector2){ 0, 0 }, 0, Fade(MINT, on ? .35f : .16f)); EndBlendMode(); }
        DrawTexturePro(s2, src, dst, (Vector2){ 0, 0 }, 0, sc2);
    }
}

#define ORNG (Color){ 255, 138, 40, 255 }
static void spark(float value, float result, float sp, int kdx, float t) {
    for (int pos = 0; pos < kdx; pos++) {
        float a = hash(pos, (int)(t*30) + 7)*6.283f, r = hash(pos, (int)(t*30) + 99)*sp; Color c = hash(pos, (int)(t*30) + 3) < .5f ? (Color){ 255, 220, 120, 255 } : (Color){ 150, 230, 255, 255 };
        DrawRectangle((int)(value + cosf(a)*r), (int)(result + sinf(a)*r), 2, 2, c);
    }
}
static void drawOverlay(float cam, float t) {
    BeginBlendMode(BLEND_ADDITIVE);
    for (int pos = 0; pos < nobj; pos++) {
        const Obj *o = &obj[pos]; if (o->type != O_AC || o->st >= 3) continue;
        float w = OSP[o->spr].w, h = OSP[o->spr].h, X = (o->value - cam)*2, Y = o->result*2, d = o->value + w/4 - px; if (X > SW || X + w < 0) continue;
        if (o->st == 0 && d < 330 && d > -20) { spark(X + w*.5f, Y - 6, 14 + 8*sinf(t*13), 3 + (int)((330 - d)/70), t); if (hash(pos, (int)(t*16)) < .18f) DrawCircleV((Vector2){ X + w*.5f, Y - h*.5f }, 18, Fade((Color){ 150, 230, 255, 255 }, .25f)); }
        else if (o->st == 1) spark(X + w*.5f, Y - h*.4f, 20, 5, t);
        else if (o->st == 2) { spark(X + w*.5f, Y - 6, 10 + o->t*140, 14, t); DrawCircleV((Vector2){ X + w*.5f, Y - 8 }, 26*(1 - o->t*2), Fade(ORNG, .35f)); }
    }
    for (int pos = 0; pos < npf; pos++) {
        const Plat *p = &pf[pos];
        if (p->kind == PK_RAMP && px < p->x1) { Vector2 a0 = { (p->x0 - cam)*2, GY*2.f - 4 }, a1 = { (p->x1 - cam)*2, p->top1*2 - 4 }; DrawLineEx(a0, a1, 6, Fade(MINT, .3f + .15f*sinf(t*5))); DrawLineEx(a0, a1, 2, Fade(MINT, .8f)); }
        if (!p->hint || px > p->x0 - 20) continue;
        float hx = (p->hint - cam)*2, yb = (p->kind == PK_ROOF ? GY - BAL_H : GY)*2.f; if (hx < 40 || hx > SW - 40) continue;
        float a = .65f + .3f*sinf(t*6);
        for (int kdx = 0; kdx < 3; kdx++) {
            float result = yb - 30 - kdx*30; float f = a*(1 - kdx*.2f);
            DrawLineEx((Vector2){ hx - 30, result + 16 }, (Vector2){ hx, result - 10 }, 12, Fade(MINT, f*.28f)); DrawLineEx((Vector2){ hx, result - 10 }, (Vector2){ hx + 30, result + 16 }, 12, Fade(MINT, f*.28f));
            DrawLineEx((Vector2){ hx - 30, result + 16 }, (Vector2){ hx, result - 10 }, 6, Fade(MINT, f)); DrawLineEx((Vector2){ hx, result - 10 }, (Vector2){ hx + 30, result + 16 }, 6, Fade(MINT, f));
        }
    }
    for (int kdx = 0; ; kdx++) {
        float gx = gateX(kdx); if (gx < px - 200) continue; if (gx - cam > Wd/2 + 60) break; float X = (gx - cam)*2;
        for (int ln = 0; ln < 3; ln++) {
            float yl = ln == 0 ? GY : ln == 1 ? GY - BAL_H : GY - ROOF_H; int ok = ln == 0;
            for (int pos = 0; pos < npf && !ok; pos++) if ((pf[pos].kind == (ln == 1 ? PK_BAL : PK_ROOF)) && gx >= pf[pos].x0 && gx < pf[pos].x1) ok = 1;
            if (!ok) continue; float result = yl*2, h = 200, a = .8f + .2f*sinf(t*4 + kdx);
            DrawRectangle(X - 22, result - h, 8, h, Fade(MINT, a)); DrawRectangle(X + 14, result - h, 8, h, Fade(MINT, a));
            DrawRectangleGradientH(X - 14, result - h, 28, h, Fade(MINT, .0f), Fade(MINT, .22f*a));
            DrawRectangle(X - 22, result - h - 6, 44, 6, Fade(MINT, a));
            DrawRectangle(X - 90, result - 4, 180, 4, Fade(MINT, .35f*a));
            DrawRectangleGradientV(X - 14, result - h - 70, 28, 70, Fade(MINT, 0), Fade(MINT, .25f*a));
        }
    }
    EndBlendMode();
    for (int pos = 0; pos < nobj; pos++) {
        const Obj *o = &obj[pos]; if (o->type == O_STAIRS || o->st >= 2 || o->hit) continue;
        float X = (o->value - cam)*2; if (X < SW - 170 || X > SW + 420) continue;
        float result = (o->type == O_AC ? o->result - 8 : o->result - OSP[o->spr].h/4.f)*2; Vector2 c = { SW - 200, result }; float a = ((int)(t*5) & 1) ? 1.f : .55f;
        DrawCircleV(c, 22, (Color){ 8, 10, 9, (unsigned char)(150*a) }); DrawRing(c, 19, 22, 0, 360, 32, Fade(ORNG, a));
        glowCentered(F33, "!", c.x, c.y - 16, Fade(ORNG, a), .8f);
    }
}

static int button(Rectangle r, const char *s, int focus) {
    DrawRectangleRec(r, (Color){ 6, 12, 11, focus ? 150 : 95 });
    DrawRectangleLinesEx(r, 2, Fade(MINT, focus ? .95f : .4f));
    if (focus) { BeginBlendMode(BLEND_ADDITIVE); DrawRectangleLinesEx((Rectangle){ r.x - 4, r.y - 4, r.width + 8, r.height + 8 }, 4, Fade(MINT, .12f)); EndBlendMode(); }
    glowCentered(F33, s, r.x + r.width/2, r.y + r.height/2 - 12, focus ? WHITE : MINT, focus ? 1 : .5f);
    return tapNow && inR(tapPos, r);
}

int main(void) {
    quit = 0; scene = MENU;
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    #if defined(PLATFORM_ANDROID)
    InitWindow(0, 0, "Out of Seconds");
#else
    InitWindow(1280, 720, "Out of Seconds");
#endif
    SetExitKey(KEY_NULL);
    SetTargetFPS(144);
#if !defined(PLATFORM_ANDROID)
    ToggleBorderlessWindowed();
    ChangeDirectory(GetApplicationDirectory());
#endif
    mAl = loadArt("mishu_albedo.png"); mNm = loadArt("mishu_normal.png"); mDp = loadArt("mishu_depth.png");
    fAl = loadArt("facades.png"); fNd = loadArt("facades_nd.png"); sky = loadArt("skyline.png");
    oAl = loadArt("obs_albedo.png"); oNm = loadArt("obs_normal.png"); oDp = loadArt("obs_depth.png");
    MA = mAl.data; MN = mNm.data; MD = mDp.data; OA = oAl.data; ON = oNm.data; OD = oDp.data;
    for (int pos = 0; pos < NSPR; pos++) { blurOne(0, pos, BLUR_MID); blurOne(1, pos, BLUR_FAR); }
    for (int pos = 0; pos < 4; pos++) { fnt[pos] = LoadFontEx(artPath("After Burner Neue.ttf"), FSZ[pos], NULL, 0); SetTextureFilter(fnt[pos].texture, TEXTURE_FILTER_POINT); }
    {
        static const int pose[3][2] = { { C_SLOOP, 12 }, { C_DASH, 4 }, { C_JAIR, 0 } };
        for (int pos = 0; pos < 3; pos++) {
            Image c = ImageFromImage(mAl, (Rectangle){ pose[pos][1]*CW, pose[pos][0]*CH, CW, CH }); Color *p = c.data;
            int x0 = CW, x1 = -1, y0 = CH, y1 = -1;
            for (int result = 0; result < CH; result++) for (int value = 0; value < CW; value++) if (p[result*CW + value].a) { if (value < x0) x0 = value; if (value > x1) x1 = value; if (result < y0) y0 = result; if (result > y1) y1 = result; p[result*CW + value] = WHITE; }
            ImageCrop(&c, (Rectangle){ x0, y0, x1 - x0 + 1, y1 - y0 + 1 });
            sil[pos] = LoadTextureFromImage(c); SetTextureFilter(sil[pos], TEXTURE_FILTER_BILINEAR); UnloadImage(c);
        }
    }
    resetCity();
    SetTextureFilter(GetFontDefault().texture, TEXTURE_FILTER_POINT);
    Image si = GenImageColor(SWM, SHM, BLACK), bi = GenImageColor(BWM, BHM, BLACK);
    Texture2D scn = LoadTextureFromImage(si), blt = LoadTextureFromImage(bi); UnloadImage(si); UnloadImage(bi);
    SetTextureFilter(scn, TEXTURE_FILTER_POINT); SetTextureFilter(blt, TEXTURE_FILTER_BILINEAR);
    { Image a = GenImageColor(SWM, 1, BLACK), b = GenImageColor(1, SHM, BLACK); eT = LoadTextureFromImage(a); eB = LoadTextureFromImage(a); eL = LoadTextureFromImage(b); eR = LoadTextureFromImage(b); UnloadImage(a); UnloadImage(b);
      Texture2D *e[4] = { &eT, &eB, &eL, &eR }; for (int pos = 0; pos < 4; pos++) SetTextureFilter(*e[pos], TEXTURE_FILTER_BILINEAR); }
    RenderTexture2D rt = LoadRenderTexture(SWM, SHM); SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);
    Rectangle mb[4], ob[2], bk, pb[2], ot;
    int prevH[3] = { 0, 0, 0 }, yo = 0;
    float acc = 0, t = 0;

    while (!quit && !WindowShouldClose()) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

        int oldgy = gy; float s = sh/540.f; sc = s >= 1 ? floorf(s) : s;
        vw = (int)ceilf(sw/sc/2); vw = vw < 480 ? 480 : vw > VWM ? VWM : vw;
        vh = (int)ceilf(sh/sc/2); vh = vh < 270 ? 270 : vh > VHM ? VHM : vh;
        gy = 236 + (vh - 270)*3/4;
        if (gy != oldgy) { resetCity(); if (scene == PLAY || scene == OVER) py += gy - oldgy; }
        s = fminf((float)sw/SW, (float)sh/SH); if (s < sc) sc = s;
        yo = (SH - 540)/2;
        ox = (sw - SW*sc)/2; oy = (sh - SH*sc)/2;
        Wd = vw*2;
        for (int pos = 0; pos < 4; pos++) mb[pos] = (Rectangle){ Wd/2.f - 170, 206 + yo + pos*66, 340, 56 };
        ot = (Rectangle){ Wd/2.f - 280, 230 + yo, 560, 60 };
        ob[0] = (Rectangle){ Wd/2.f - 260, 320 + yo, 240, 60 }; ob[1] = (Rectangle){ Wd/2.f + 20, 320 + yo, 240, 60 };
        bk = (Rectangle){ Wd/2.f - 170, 320 + yo, 340, 60 };
        pb[0] = (Rectangle){ Wd/2.f - 170, 230 + yo, 340, 60 }; pb[1] = (Rectangle){ Wd/2.f - 170, 306 + yo, 340, 60 };
        nptr = 0;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ptr[nptr++] = toV(GetMousePosition());
        for (int pos = 0; pos < GetTouchPointCount() && nptr < 10; pos++) ptr[nptr++] = toV(GetTouchPosition(pos));
        tapNow = nptr > 0 && prevN == 0; if (tapNow) tapPos = ptr[0]; prevN = nptr;
        float dt = fminf(GetFrameTime(), 1.f/30); if (scene == PAUSE) dt = 0; t += dt;

        if (scene == PLAY) {
            int h[3] = { btnHeld(0) || IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_LEFT_CONTROL),
                         btnHeld(1) || IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT),
                         btnHeld(2) || IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) };
            if (h[0] && !prevH[0]) ps = 1;
            if (h[1] && !prevH[1]) pd = 1;
            if (h[2] && !prevH[2]) pj = 1;
            for (int pos = 0; pos < 3; pos++) prevH[pos] = h[pos];
            for (acc += dt; acc >= STEP; acc -= STEP) stepPlay(STEP);
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P) || IsKeyPressed(KEY_BACK) || (tapNow && CheckCollisionPointCircle(tapPos, pausePos(), 36))) scene = PAUSE;
            if (dead && deadT > 2.1f) scene = OVER;
        } else {
            camV = 0;
            for (int pos = 0; pos < 3; pos++) prevH[pos] = 1;
            if (scene == MENU) {
                if (IsKeyPressed(KEY_DOWN)) sel = (sel + 1) % 4;
                if (IsKeyPressed(KEY_UP)) sel = (sel + 3) % 4;
                if (IsKeyPressed(KEY_ESCAPE)) quit = 1;
            }
            if ((scene == UPG || scene == OPT) && IsKeyPressed(KEY_ESCAPE)) scene = MENU;
            if (scene == PAUSE && (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P) || IsKeyPressed(KEY_BACK))) scene = PLAY;
            if (scene == OVER && IsKeyPressed(KEY_ESCAPE)) scene = MENU;
        }

        buildRain(camV);
        for (int l = 0; l < 2; l++) { Rain *R = &rainL[l]; R->oy = fmodf(R->oy + R->v*dt, RT); R->ox = fmodf(R->ox - (R->v*.05f + camV*R->kdx)*dt, RT); }

        float cam = camX;
        render(cam, t, scene == PLAY || scene == OVER || scene == PAUSE);
        UpdateTextureRec(scn, (Rectangle){ 0, 0, SW, SH }, pix); UpdateTextureRec(blt, (Rectangle){ 0, 0, BW, BH }, bpix);
        {
            for (int e = 0; e < 4; e++) {
                int digit = e < 2 ? SW : SH, st = e < 2 ? 1 : SW; Color *base = e == 0 ? pix : e == 1 ? pix + (SH - 1)*SW : e == 2 ? pix : pix + SW - 1, *d = eBuf[0];
                for (int pos = 0; pos < digit; pos++) { int r = 0, g = 0, b = 0, c = 0; for (int kdx = -12; kdx <= 12; kdx++) { int jdx = pos + kdx; if (jdx < 0 || jdx >= digit) continue; Color p = base[jdx*st]; r += p.r; g += p.g; b += p.b; c++; } d[pos] = (Color){ r/c, g/c, b/c, 255 }; }
                if (e == 0) UpdateTextureRec(eT, (Rectangle){ 0, 0, digit, 1 }, d); else if (e == 1) UpdateTextureRec(eB, (Rectangle){ 0, 0, digit, 1 }, d);
                else if (e == 2) UpdateTextureRec(eL, (Rectangle){ 0, 0, 1, digit }, d); else UpdateTextureRec(eR, (Rectangle){ 0, 0, 1, digit }, d);
            }
        }
        BeginTextureMode(rt);
        DrawTexture(scn, 0, 0, WHITE);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(blt, (Rectangle){ 0, 0, BW, BH }, (Rectangle){ 0, 0, BW*8, BH*8 }, (Vector2){ 0, 0 }, 0, WHITE);
        EndBlendMode();
        if (scene == PLAY || scene == PAUSE) { drawOverlay(cam, t); drawHud(); }
        if (scene == MENU) {
            DrawRectangle(0, 0, Wd, SH, (Color){ 5, 8, 8, 120 });
            glowCentered(F66, "OUT OF SECONDS", Wd/2.f, 70 + yo, MINT, 1.3f);
            glowCentered(F22, "TIME IS YOUR LIFE. RUN. DON'T TRIP.", Wd/2.f, 154 + yo, (Color){ 150, 185, 174, 255 }, .5f);
            const char *lab[4] = { "RUN", "UPGRADES", "OPTIONS", "EXIT" };
            for (int pos = 0; pos < 4; pos++) if (button(mb[pos], lab[pos], sel == pos) || (sel == pos && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)))) {
                if (pos == 0) startRun(); else if (pos == 1) scene = UPG; else if (pos == 2) scene = OPT; else quit = 1;
            }
            glowCentered(F22, "JUMP SPACE/W   DASH SHIFT/D   SLIDE S/CTRL   WINDOW F11", Wd/2.f, SH - 40, (Color){ 120, 155, 145, 255 }, .4f);
        } else if (scene == PAUSE) {
            DrawRectangle(0, 0, Wd, SH, (Color){ 5, 8, 8, 150 });
            glowCentered(F44, "PAUSED", Wd/2.f, 120 + yo, MINT, 1.2f);
            if (button(pb[0], "RESUME", 1) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) scene = PLAY;
            else if (button(pb[1], "MENU", 0)) scene = MENU;
        } else if (scene == UPG) {
            DrawRectangle(0, 0, Wd, SH, (Color){ 5, 8, 8, 120 });
            glowCentered(F44, "UPGRADES", Wd/2.f, 110 + yo, MINT, 1.2f);
            glowCentered(F22, "NOTHING HERE YET.", Wd/2.f, 220 + yo, DIMC, .6f);
            if (button(bk, "BACK", 1) || IsKeyPressed(KEY_ENTER)) scene = MENU;
        } else if (scene == OPT) {
            DrawRectangle(0, 0, Wd, SH, (Color){ 5, 8, 8, 120 });
            glowCentered(F44, "OPTIONS", Wd/2.f, 110 + yo, MINT, 1.2f);
            if (button(ot, devTime ? "UNLIMITED TIME (DEV): ON" : "UNLIMITED TIME (DEV): OFF", 0)) devTime = !devTime;
            if (button((Rectangle){ Wd/2.f - 170, 316 + yo, 340, 60 }, "BACK", 1) || IsKeyPressed(KEY_ENTER)) scene = MENU;
        } else if (scene == OVER) {
            DrawRectangle(0, 0, Wd, SH, (Color){ 5, 8, 8, 170 });
            glowCentered(F44, deathBy ? "RUN OVER" : "YOU'RE OUT OF SECONDS", Wd/2.f, 84 + yo, HOTPINK, 1.3f);
            glowCentered(F22, deathBy ? "YOU FACE-PLANTED INTO A TRUCK." : "TIME RAN OUT.", Wd/2.f, 160 + yo, DIMC, .6f);
            char tx[64]; snprintf(tx, sizeof tx, "DISTANCE %dM   BEST %dM", (int)((px - PX)/10), (int)((best - PX)/10));
            glowCentered(F33, tx, Wd/2.f, 230 + yo, MINT, .9f);
            if (button(ob[0], "RETRY", 1) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) startRun();
            else if (button(ob[1], "MENU", 0)) scene = MENU;
        }
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);

        float dw = SW*sc, dh = SH*sc, Wf = SW, Hf = SH; Vector2 extra = { 0, 0 };
        float rx = ox + dw, by = oy + dh;
        if (oy > .5f) {
            DrawTexturePro(eT, (Rectangle){ 0, 0, Wf, 1 }, (Rectangle){ ox, 0, dw, oy }, extra, 0, WHITE);
            DrawTexturePro(eB, (Rectangle){ 0, 0, Wf, 1 }, (Rectangle){ ox, by, dw, sh - by }, extra, 0, WHITE);
        }
        if (ox > .5f) {
            DrawTexturePro(eL, (Rectangle){ 0, 0, 1, Hf }, (Rectangle){ 0, oy, ox, dh }, extra, 0, WHITE);
            DrawTexturePro(eR, (Rectangle){ 0, 0, 1, Hf }, (Rectangle){ rx, oy, sw - rx, dh }, extra, 0, WHITE);
            if (oy > .5f) {
                DrawTexturePro(eT, (Rectangle){ 0, 0, 1, 1 }, (Rectangle){ 0, 0, ox, oy }, extra, 0, WHITE);
                DrawTexturePro(eT, (Rectangle){ Wf - 1, 0, 1, 1 }, (Rectangle){ rx, 0, sw - rx, oy }, extra, 0, WHITE);
                DrawTexturePro(eB, (Rectangle){ 0, 0, 1, 1 }, (Rectangle){ 0, by, ox, sh - by }, extra, 0, WHITE);
                DrawTexturePro(eB, (Rectangle){ Wf - 1, 0, 1, 1 }, (Rectangle){ rx, by, sw - rx, sh - by }, extra, 0, WHITE);
            }
        }
        DrawTexturePro(rt.texture, (Rectangle){ 0, SHM - SH, SW, -SH }, (Rectangle){ ox, oy, dw, dh }, extra, 0, WHITE);
        EndDrawing();
    }
    CloseWindow();
#if defined(PLATFORM_ANDROID)
    exit(0);
#endif
    return 0;
}
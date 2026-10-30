#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VWM 1024
#define VW vw
#define VH 270
#define GY 236
#define SW (VW*2)
#define SWM (VWM*2)
#define SH (VH*2)
#define SGY (GY*2)
#define FL 64
#define PX 150
#define STEP (1.f/120)

#define TIME_LIMIT 30.f
#define RUN_V 250.f
#define DASH_V 650.f
#define DASH_T .35f
#define DASH_CD 1.2f
#define SLIDE_V .7f
#define SLIDE_T 1.f
#define JUMP_V 330.f
#define GRAV 900.f
#define ROLL_H 50.f
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

enum { MENU, UPG, PLAY, OVER };
static int scene = MENU, quit, sel;
static float sc = 1, ox, oy;
static Vector2 ptr[11]; static int nptr, tapNow, prevN; static Vector2 tapPos;

static float px, py, vy, tleft, dash, cd, slide, animT, deadT, best, peak;
static int clip, dead, pj, pd, ps;

static Vector2 toV(Vector2 p) { return (Vector2){ (p.x - ox)/sc/2, (p.y - oy)/sc/2 }; }
static int inR(Vector2 p, Rectangle r) { return CheckCollisionPointRec(p, r); }
static int held(Rectangle r) { for (int idx = 0; idx < nptr; idx++) if (inR(ptr[idx], r)) return 1; return 0; }
static int tapped(Rectangle r) { return tapNow && inR(tapPos, r); }

static Image mAl, mNm, mDp, fAl, fNd, sky;
static Color *MA, *MN, *MD;
static Image loadArt(const char *digit) {
#if defined(PLATFORM_ANDROID)
    Image bound = LoadImage(digit);
#else
    Image bound = LoadImage(FileExists(TextFormat("assets/%s", digit)) ? TextFormat("assets/%s", digit) : TextFormat("../assets/%s", digit));
#endif
    ImageFormat(&bound, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return bound;
}

static const unsigned char PAL[24][3] = { {137,158,147},{123,140,130},{114,130,119},{106,122,112},{100,113,103},{95,106,95},{88,100,91},{83,93,82},
    {78,89,81},{75,83,73},{69,79,71},{67,75,67},{63,71,63},{59,67,59},{57,62,55},{52,59,53},{48,53,47},{44,50,45},{40,46,41},{36,40,36},
    {32,37,33},{27,31,28},{20,22,20},{5,6,5} };
static const unsigned char *P(int idx) { return PAL[idx < 23 ? idx : 23]; }
static const unsigned char SIGN[3] = { 8, 12, 12 };

typedef struct { int y0, w, floors, ncol; int cols[6]; } Block;
static const Block BLK[3] = { { 0, 612, 6, 6, { 0, 132, 251, 395, 502, 612 } }, { 384, 511, 5, 6, { 0, 75, 203, 326, 450, 511 } },
                              { 704, 403, 4, 5, { 0, 96, 188, 283, 375 } } };

typedef struct { int item, result, w, h; } SprRect;
static const SprRect MIDR[] = { {0,0,100,210}, {101,0,76,210}, {178,0,89,186}, {268,0,68,186}, {337,0,156,185}, {494,0,81,185}, {576,0,74,120}, {651,0,52,120}, {704,0,73,115}, {778,0,55,115}, {834,0,162,112}, {0,211,204,112}, {205,211,94,111}, {300,211,52,111}, {523,211,71,92}, {595,211,70,92}, {786,211,79,91}, {866,211,71,91}, {325,324,149,53}, {475,324,129,53}, {605,324,233,50}, {0,382,205,49}, {206,382,161,47}, {368,382,233,46} };
static const SprRect FARR[] = { {353,211,50,105}, {404,211,38,105}, {443,211,44,93}, {488,211,34,93}, {666,211,78,92}, {745,211,40,92}, {938,211,37,60}, {976,211,26,60}, {0,324,36,57}, {37,324,27,57}, {65,324,81,56}, {147,324,102,56}, {250,324,47,55}, {298,324,26,55}, {602,382,35,46}, {638,382,35,46}, {674,382,39,45}, {714,382,35,45}, {750,382,74,26}, {825,382,64,26}, {890,382,116,25}, {0,432,102,24}, {103,432,116,23}, {220,432,80,23} };
#define NSPR 24

static unsigned seed = 1312;
static unsigned hashu(int a, int b) { unsigned h = (unsigned)a*374761393u + (unsigned)b*668265263u + seed*1442695041u; h = (h ^ (h >> 13))*1274126177u; return h ^ (h >> 16); }
static float hash(int a, int b) { return (hashu(a, b) >> 8)/16777216.f; }
typedef struct { unsigned s; } Rng;
static Rng rngOf(int a, int b) { return (Rng){ hashu(a, b) }; }
static float rnd(Rng *r) { unsigned t = r->s += 0x6D2B79F5u; t = (t ^ (t >> 15))*(1 | t); t = (t + (t ^ (t >> 7))*(61 | t)) ^ t; return ((t ^ (t >> 14)) >> 8)/16777216.f; }
static float vnoise(float item, float result) {
    int xi = (int)floorf(item), yi = (int)floorf(result); float fx = item - xi, fy = result - yi, a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1);
    float u = fx*fx*(3 - 2*fx), v = fy*fy*(3 - 2*fy); return a + (b - a)*u + (c - a)*v + (a - b - c + d)*u*v;
}

static const float EMC[6][3] = { { 0, 0, 0 }, { 40, 225, 180 }, { 0, 0, 0 }, { 150, 215, 180 }, { 205, 175, 120 }, { 120, 165, 140 } };
static const float *emCol(int t) { return t ==2 ? ACCENT : EMC[t]; }

#define BEACON 0x80000000u
static int fclass(unsigned g) { if (!g) return 0; if (g & BEACON) return 3; float h = hash((int)g, 11); return h < .03f ? 2 : h < .08f ? 1 : 0; }
static float flick(unsigned g, float t) {
    int c = fclass(g); if (!c) return 1;
    if (c ==3) { float P = 2.4f, ph = fmodf(t + hash((int)g, 5)*P, P); return ph < .18f ? 1 : .05f; }
    float base = c ==1 ? 1 : .04f, P = 3.5f + hash((int)g, 3)*5; int kdx = (int)floorf(t/P); float lt = t - kdx*P;
    if (hash((int)g*7 + 1, kdx) > .6f) return base;
    int digit = 2 + (int)(hash((int)g*13 + 2, kdx)*2); float bs = hash((int)g*5 + 3, kdx)*(P - .5f), u = lt - bs;
    if (u < 0 || u >= digit*.13f) return base;
    return fmodf(u, .13f) < .06f ? (c ==1 ? .08f : 1) : base;
}

typedef struct { float item, result, extra, R, s; int t; unsigned g; } Light;
#define MAXL 64
typedef struct {
    int gap, glow, x0, w, id;
    unsigned char *al, *a, *et, *ei;
    unsigned *eg;
    float *digit;
    float *lit;
    Light L[MAXL]; int nl;
} Bld;
static Bld *blds; static int nb, cap, bEnd, bId;

static void addLight(Bld *b, Light l) { if (b->nl < MAXL) b->L[b->nl++] = l; }
static void bpx(Bld *b, int item, int result, const unsigned char *c, float nx, float ny, float extra) {
    if (item < 0 || item >= b->w || result < 0 || result >= GY) return;
    int idx = result*b->w + item; memcpy(b->al + idx*3, c, 3); b->a[idx] = 255;
    float *digit = b->digit + idx*4; digit[0] = nx; digit[1] = ny; digit[2] = sqrtf(fmaxf(0, 1 - nx*nx - ny*ny)); digit[3] = extra; b->et[idx] = 0;
}
static void brect(Bld *b, int item, int result, int w, int h, const unsigned char *c, float nx, float ny, float extra) { for (int secondary = result; secondary < result + h; secondary++) for (int idx = item; idx < item + w; idx++) bpx(b, idx, secondary, c, nx, ny, extra); }
static void bem(Bld *b, int item, int result, int w, int h, int t, int s, unsigned g) {
    if (!fclass(g)) g = 0;
    for (int secondary = result; secondary < result + h; secondary++) for (int idx = item; idx < item + w; idx++) { if (idx < 0 || idx >= b->w || secondary < 0 || secondary >= GY) continue; int kdx = secondary*b->w + idx; b->et[kdx] = t; b->ei[kdx] = s; b->eg[kdx] = g; b->a[kdx] = 255; }
}
static void glyph(Bld *b, int item, int result, int t, Rng *r, unsigned g) { int bound = (int)(rnd(r)*512); for (int secondary = 0; secondary < 3; secondary++) for (int idx = 0; idx < 3; idx++) if ((bound >> (secondary*3 + idx)) & 1 || (idx ==1 && secondary ==1)) bem(b, item + idx*2, result + secondary*2, 2, 2, t, 220, g); }

static void freeBld(Bld *b) { free(b->al); free(b->a); free(b->et); free(b->ei); free(b->eg); free(b->digit); free(b->lit); b->al = b->a = b->et = b->ei = 0; b->eg = 0; b->digit = b->lit = 0; }

static void genBuilding(Bld *b, int x0, Rng *r) {
    const Block *B = &BLK[(int)(rnd(r)*3)]; int nbay = B->ncol - 1;
    int digit = 1 + (int)(rnd(r)*(nbay < 3 ? nbay : 3)), b0 = (int)(rnd(r)*(nbay - digit + 1)), w = 0, bsrc[3], bw[3];
    for (int secondary = 0; secondary < digit; secondary++) { bsrc[secondary] = B->cols[b0 + secondary]; bw[secondary] = B->cols[b0 + secondary + 1] - B->cols[b0 + secondary]; w += bw[secondary]; }
    int floors = rnd(r) < .45f ? 1 + (int)(rnd(r)*2) : 3 + (int)(rnd(r)*3), rows[5];
    for (int idx = 0; idx < floors; idx++) rows[idx] = (int)(rnd(r)*B->floors);
    int id = ++bId, N = w*GY;
    memset(b, 0, sizeof *b); b->x0 = x0; b->w = w; b->id = id;
    b->al = calloc(N*3, 1); b->a = calloc(N, 1); b->et = calloc(N, 1); b->ei = calloc(N, 1); b->eg = calloc(N, sizeof(unsigned)); b->digit = malloc(N*4*sizeof(float));
    for (int idx = 0; idx < N; idx++) { float *q = b->digit + idx*4; q[0] = q[1] = 0; q[2] = 1; q[3] = -28; }
    Color *FA = fAl.data, *FN = fNd.data; int FW = fAl.width;
    int top = GY - FL - floors*FL;

    for (int idx = 0; idx < floors; idx++) {
        int y0 = GY - FL - (idx + 1)*FL; if (y0 + FL < 0) continue; int xo = 0;
        for (int bi = 0; bi < digit; bi++) {
            for (int yy = 0; yy < FL; yy++) {
                int Y = y0 + yy; if (Y < 0) continue; int sy = B->y0 + rows[idx]*FL + yy;
                for (int xx = 0; xx < bw[bi]; xx++) {
                    Color s = FA[sy*FW + bsrc[bi] + xx]; if (s.a < 128) continue; Color nd = FN[sy*FW + bsrc[bi] + xx]; int kdx = Y*w + xo + xx;
                    b->al[kdx*3] = s.r; b->al[kdx*3 + 1] = s.g; b->al[kdx*3 + 2] = s.b; b->a[kdx] = s.a ==254 ? 254 : 255;
                    float *q = b->digit + kdx*4; q[0] = nd.r/127.5f - 1; q[1] = -(nd.g/127.5f - 1); q[2] = nd.b/127.5f - 1; q[3] = -28 + (nd.a/255.f - .25f)*30;
                }
            }

            Rng lr = rngOf(id*31 + idx, xo);
            if (rnd(&lr) < .28f) {
                int t = rnd(&lr) < .5f ? 4 : 5, s = 45 + (int)(rnd(&lr)*50); unsigned g = id*1000 + idx*16 + xo, eg = fclass(g) ? g : 0; float cx = 0, cy = 0; int cn = 0;
                for (int yy = 0; yy < FL; yy++) { int Y = y0 + yy; if (Y < 0) continue; for (int xx = 0; xx < bw[bi]; xx++) { int kdx = Y*w + xo + xx; if (b->a[kdx] ==254) { b->et[kdx] = t; b->ei[kdx] = s; b->eg[kdx] = eg; cx += xo + xx; cy += Y; cn++; } } }
                if (cn > 20 && idx < 2) addLight(b, (Light){ x0 + cx/cn, cy/cn, -20, 80, .45f, t, 0 });
            }
            xo += bw[bi];
        }
    }

    if (top > 4) {
        brect(b, 0, top - 6, w, 6, P(5), 0, -.6f, -28); brect(b, 0, top - 6, w, 1, P(2), 0, -1, -28);
        if (rnd(r) < .7f) { int tx = 6 + (int)(rnd(r)*(w - 30)); brect(b, tx, top - 26, 18, 20, P(9), .3f, 0, -30); brect(b, tx - 1, top - 28, 20, 2, P(6), 0, -.8f, -28); for (int secondary = top - 6; secondary < top; secondary++) { bpx(b, tx + 2, secondary, P(12), 0, 0, -28); bpx(b, tx + 15, secondary, P(12), 0, 0, -28); } }
        for (int kdx = 0; kdx < 2; kdx++) if (rnd(r) < .6f) { int ax = (int)(rnd(r)*(w - 4)), ah = 10 + (int)(rnd(r)*22); for (int secondary = top - 6 - ah; secondary < top - 6; secondary++) bpx(b, ax, secondary, P(14), 0, 0, -28); if (rnd(r) < .5f) bem(b, ax, top - 7 - ah, 1, 1, 2, 200, BEACON | (unsigned)(id*7 + kdx)); }
        for (int kdx = 0; kdx < 2; kdx++) if (rnd(r) < .5f) { int ax = 4 + (int)(rnd(r)*(w - 20)); brect(b, ax, top - 14, 12, 8, P(7), 0, -.3f, -28); }
    }

    int gx = 0;
    while (gx < w) {
        int sw = 40 + (int)(rnd(r)*50); if (sw > w - gx) sw = w - gx; float kind = rnd(r); int y0 = GY - FL;
        brect(b, gx, y0, sw, FL, P(10), 0, 0, -28); brect(b, gx, y0, sw, 4, P(6), 0, -.7f, -28);
        if (kind < .45f) {
            for (int secondary = y0 + 12; secondary < GY; secondary++) for (int idx = gx + 3; idx < gx + sw - 3; idx++) { int s = (secondary - y0)%3; bpx(b, idx, secondary, s ==0 ? P(13) : s ==1 ? P(8) : P(11), 0, s ==0 ? -.5f : s ==2 ? .5f : 0, -29); }
            brect(b, gx + 3, y0 + 8, sw - 6, 4, P(15), 0, -.6f, -26);
            for (int idx = gx + 5; idx < gx + sw - 5; idx += 7) if (rnd(r) < .5f) { int gy = y0 + 20 + (int)(rnd(r)*30), gw = 3 + (int)(rnd(r)*4); brect(b, idx, gy, gw, 1, P(4), 0, 0, -29); }
        } else if (kind < .8f) {
            brect(b, gx + 3, y0 + 10, sw - 6, FL - 12, P(18), 0, 0, -34); int t = rnd(r) < .5f ? 3 : 4; unsigned g = id*1000 + 900 + gx;
            bem(b, gx + 4, y0 + 14, sw - 8, FL - 20, t, 30 + (int)(rnd(r)*30), g);
            for (int idx = gx + 10; idx < gx + sw - 6; idx += 12) brect(b, idx, y0 + 14, 1, FL - 20, P(12), 0, 0, -31);
            brect(b, gx + 3, y0 + 10, sw - 6, 3, P(4), 0, -.8f, -24);
            addLight(b, (Light){ x0 + gx + sw/2.f, y0 + 34, -26, 95, .6f, t, g });
        } else {
            brect(b, gx + sw/2 - 10, y0 + 16, 20, FL - 16, P(19), 0, 0, -40); brect(b, gx + sw/2 - 12, y0 + 14, 24, 2, P(6), 0, -.8f, -28);
            for (int secondary = GY - 8; secondary < GY; secondary += 3) brect(b, gx + sw/2 - 10, secondary, 20, 1, P(9), 0, -.6f, -34);
        }
        if (rnd(r) < .75f) { int tx = gx + sw/2 - 7; unsigned g = id*1000 + 800 + gx; brect(b, tx - 1, y0 + 5, 16, 2, P(12), 0, .6f, -22); bem(b, tx, y0 + 7, 14, 1, 3, 230, g); addLight(b, (Light){ x0 + tx + 7, y0 + 9, -12, 120, .9f, 3, g }); }
        gx += sw;
    }

    if (rnd(r) < NEON) {
        int sw = 14, sh = 36 + (int)(rnd(r)*30), sx = 4 + (int)(rnd(r)*(w - 24)), sy = GY - FL - sh - 6, t = rnd(r) < .6f ? 1 : 2; unsigned g = id*1000 + 700;
        brect(b, sx, sy, sw, sh, SIGN, 0, 0, -18);
        for (int idx = sx; idx < sx + sw; idx++) { bem(b, idx, sy, 1, 1, t, 255, g); bem(b, idx, sy + sh - 1, 1, 1, t, 255, g); }
        for (int secondary = sy; secondary < sy + sh; secondary++) { bem(b, sx, secondary, 1, 1, t, 255, g); bem(b, sx + sw - 1, secondary, 1, 1, t, 255, g); }
        for (int secondary = sy + 4; secondary < sy + sh - 8; secondary += 8) glyph(b, sx + 4, secondary, t, r, g);
        addLight(b, (Light){ x0 + sx + 7, sy + sh/2.f, -10, 120, .8f, t, g });
    }
}
static void ensure(int upto) {
    while (bEnd < upto) {
        if (nb ==cap) { cap = cap ? cap*2 : 64; blds = realloc(blds, cap*sizeof *blds); }
        Rng r = rngOf(777, nb + 1); Bld *b = &blds[nb];
        if (rnd(&r) < .3f) { int gap = 20 + (int)(rnd(&r)*40); memset(b, 0, sizeof *b); b->gap = 1; b->x0 = bEnd; b->w = gap; b->glow = rnd(&r) < .5f; nb++; bEnd += gap; continue; }
        genBuilding(b, bEnd, &r); nb++; bEnd += b->w;
    }
}
static int bAt(int wx) { int lo = 0, hi = nb - 1; while (lo <= hi) { int bound = (lo + hi) >> 1; Bld *b = &blds[bound]; if (wx < b->x0) hi = bound - 1; else if (wx >= b->x0 + b->w) lo = bound + 1; else return bound; } return -1; }

static void bake(int idx) {
    Bld *b = &blds[idx]; b->lit = calloc(b->w*GY*3, sizeof(float));
    for (int kdx = idx - 2; kdx <= idx + 2; kdx++) {
        if (kdx < 0 || kdx >= nb || blds[kdx].gap) continue;
        for (int li = 0; li < blds[kdx].nl; li++) {
            Light *L = &blds[kdx].L[li]; if (fclass(L->g)) continue; const float *c = emCol(L->t);
            int xa = (int)(L->item - L->R) - b->x0, xb = (int)(L->item + L->R) - b->x0 + 1; if (xa < 0) xa = 0; if (xb > b->w) xb = b->w;
            for (int secondary = 0; secondary < GY; secondary++) for (int item = xa; item < xb; item++) {
                int q = secondary*b->w + item; if (!b->a[q]) continue; float *digit = b->digit + q*4;
                float dx = L->item - (b->x0 + item), dy = L->result - secondary, dz = L->extra - digit[3] + 8, d = sqrtf(dx*dx + dy*dy + dz*dz); if (d > L->R || d <= 0) continue;
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
    for (int result = 0; result < h; result++) for (int item = 0; item < w; item++) { Color c = S[(R->result + result)*sky.width + R->item + item]; float a = c.a ? 1 : 0, *o = A + (result*w + item)*4; o[0] = c.r*a; o[1] = c.g*a; o[2] = c.b*a; o[3] = a; }
    for (int pass = 0; pass < 2; pass++) {
        for (int result = 0; result < h; result++) for (int item = 0; item < w; item++) { float s[4] = { 0 }; int c = 0; for (int kdx = -r; kdx <= r; kdx++) { int xx = item + kdx; if (xx < 0 || xx >= w) continue; float *q = A + (result*w + xx)*4; for (int bound = 0; bound < 4; bound++) s[bound] += q[bound]; c++; } for (int bound = 0; bound < 4; bound++) B[(result*w + item)*4 + bound] = s[bound]/c; }
        for (int result = 0; result < h; result++) for (int item = 0; item < w; item++) { float s[4] = { 0 }; int c = 0; for (int kdx = -r; kdx <= r; kdx++) { int yy = result + kdx; if (yy < 0 || yy >= h) continue; float *q = B + (yy*w + item)*4; for (int bound = 0; bound < 4; bound++) s[bound] += q[bound]; c++; } for (int bound = 0; bound < 4; bound++) A[(result*w + item)*4 + bound] = s[bound]/c; }
    }
    unsigned char *d = calloc(digit*4, 1);
    for (int idx = 0; idx < digit; idx++) { float a = A[idx*4 + 3]; if (a > .02f) { d[idx*4] = (unsigned char)fminf(255, A[idx*4]/a); d[idx*4 + 1] = (unsigned char)fminf(255, A[idx*4 + 1]/a); d[idx*4 + 2] = (unsigned char)fminf(255, A[idx*4 + 2]/a); d[idx*4 + 3] = (unsigned char)(a*255); } }
    free(A); free(B); blurred[lay][si] = d;
}

static void bakeFar(Far *o, int lay) {
    const SprRect *R = lay ? &FARR[o->spr] : &MIDR[o->spr]; int w = R->w, h = R->h, yo = lay ? -6 : 0, top = GY + yo - h;
    float fog = lay ? FOG_FAR : FOG_MID, dk = 1 - (lay ? DARK_FAR : DARK_MID), wl = lay ? .06f : .09f;
    float FD[3] = { 24 + (8 - 24)*TINT, 28 + (30 - 28)*TINT, 27 + (22 - 27)*TINT };
    unsigned char *bd = blurred[lay][o->spr]; Color *S = sky.data; o->px = calloc(w*h*7, 1);
    for (int row = 0; row < h; row++) for (int sx = 0; sx < w; sx++) {
        int s = row*w + sx; if (!bd[s*4 + 3]) continue; int result = top + row; unsigned char *q = o->px + s*7; Color src = S[(R->result + row)*sky.width + R->item + sx];
        float r = bd[s*4]*dk, g = bd[s*4 + 1]*dk, b = bd[s*4 + 2]*dk, f = fminf(1, fog + (1 - fog)*.2f*((float)result/GY)), e[3] = { 0, 0, 0 };
        r = r*(1 - f) + FD[0]*f; g = g*(1 - f) + FD[1]*f; b = b*(1 - f) + FD[2]*f; int cov = bd[s*4 + 3];
        float h2 = hash(o->id*613 + sx, row);
        if (src.a && h2 < (src.a ==254 ? wl*.15f : .0012f)) {
            const float *c = emCol(hash(sx, result) < .55f ? 4 : 3); float kdx = .85f*(1 - fog*.3f); r = c[0]*kdx; g = c[1]*kdx; b = c[2]*kdx; e[0] = r*1.4f; e[1] = g*1.4f; e[2] = b*1.4f; cov = 255;
        } else if (src.a ==254) {
            float hh = hash(o->id*977 + (sx >> 1), row >> 2);
            if (hh < wl) { const float *c = emCol(hh < wl*.5f ? 4 : 5); float kdx = .35f*(1 - fog*.6f); r = c[0]*kdx; g = c[1]*kdx; b = c[2]*kdx; e[0] = r*.6f; e[1] = g*.6f; e[2] = b*.6f; }
        }
        q[0] = (unsigned char)fminf(255, r); q[1] = (unsigned char)fminf(255, g); q[2] = (unsigned char)fminf(255, b); q[3] = cov;
        q[4] = (unsigned char)fminf(255, e[0]); q[5] = (unsigned char)fminf(255, e[1]); q[6] = (unsigned char)fminf(255, e[2]);
    }
}
static void farSeq(FarStore *st, int lay, float upto) {
    while (st->end < upto) {
        Rng r = rngOf(lay ? 52 : 51, st->digit + 1); int si = (int)(rnd(&r)*NSPR), gap = lay ? (int)(rnd(&r)*16) - 6 : (int)(rnd(&r)*30) - 8;
        if (st->digit ==st->cap) { st->cap = st->cap ? st->cap*2 : 64; st->list = realloc(st->list, st->cap*sizeof(Far)); }
        Far *o = &st->list[st->digit]; o->spr = si; o->x0 = st->end + gap; o->id = st->digit; o->px = 0; bakeFar(o, lay); st->digit++;
        st->end += gap + (lay ? FARR[si].w : MIDR[si].w);
    }
}
static Far *farAt(FarStore *st, int lay, int wx) {
    for (int idx = st->digit - 1; idx >= 0; idx--) { Far *o = &st->list[idx]; int w = lay ? FARR[o->spr].w : MIDR[o->spr].w; if (wx >= o->x0 && wx < o->x0 + w) return o; if (o->x0 + w < wx - 300) break; }
    return 0;
}

static void resetCity(void) {
    for (int idx = 0; idx < nb; idx++) freeBld(&blds[idx]);
    for (int idx = 0; idx < mids.digit; idx++) free(mids.list[idx].px);
    for (int idx = 0; idx < fars.digit; idx++) free(fars.list[idx].px);
    nb = 0; bEnd = -300; bId = 0; mids.digit = fars.digit = 0; mids.end = fars.end = -200;
}

#define RT 128
typedef struct { float v, kdx, a; int digit; float ox, oy, b; float pts[1024]; int np; } Rain;
static Rain rainL[2] = { { 300, .5f, .09f, 45, 0, 0, -1, { 0 }, 0 }, { 560, 1, .17f, 22, 0, 0, -1, { 0 }, 0 } };
static void buildRain(float scroll) {
    for (int l = 0; l < 2; l++) {
        Rain *R = &rainL[l]; if (fabsf(R->b - scroll) < 8) continue; R->b = scroll; float sl = (R->v*.05f + scroll*R->kdx)/R->v; Rng r = rngOf(99, (int)R->v); R->np = 0;
        for (int idx = 0; idx < R->digit; idx++) { float item = rnd(&r)*RT, result = rnd(&r)*RT; int size = 4 + (int)(rnd(&r)*6); for (int secondary = 0; secondary < size && R->np < 1022; secondary++) { R->pts[R->np++] = item + secondary*sl; R->pts[R->np++] = result - secondary; } }
    }
}
#define MW (VW/8)
#define MWM (VWM/8)
#define MH (VH/8)
#define BW (VW/4)
#define BWM (VWM/4)
#define BH (VH/4)
static int vw = 480;
static float out[VWM*VH*3], emb[VWM*VH*3], big[SWM*SH*3], mist[(MWM + 2)*(MH + 2)], blm[BWM*BH*3], temporary[BWM*BH*3];
static Color pix[SWM*SH], bpix[BWM*BH];
static const float FOG[3] = { 34, 44, 41 };

typedef struct { float item, result, extra, R, s, f; const float *c; } SLight;
#define MAXS 256
static SLight sl[MAXS]; static int nsl;
typedef struct { float item, result, extra, R, s, f; const float *c; } FLight;
static FLight fl[MAXS]; static int nfl;

static int mishuFrame(void);
static void render(float cam, float t, int showMishu) {
    memset(emb, 0, sizeof emb);
    ensure((int)cam + VW + 300); farSeq(&mids, 0, cam*.4f + VW + 200); farSeq(&fars, 1, cam*.15f + VW + 200);
    for (int result = 0; result < GY; result++) { float u = (float)result/GY, r = 5 + 16*u*u, g = 7 + 22*u*u, b = 7 + 19*u*u; for (int item = 0; item < VW; item++) { float *o = out + (result*VW + item)*3; o[0] = r; o[1] = g; o[2] = b; } }

    for (int lay = 1; lay >= 0; lay--) {
        FarStore *st = lay ? &fars : &mids; float p = lay ? .15f : .4f; int yo = lay ? -6 : 0;
        for (int item = 0; item < VW; item++) {
            int wx = (int)floorf(item + cam*p); Far *o = farAt(st, lay, wx); if (!o) continue;
            const SprRect *R = lay ? &FARR[o->spr] : &MIDR[o->spr]; int sx = wx - o->x0, top = GY + yo - R->h;
            for (int result = top > 0 ? top : 0; result < GY && result < top + R->h; result++) {
                unsigned char *q = o->px + ((result - top)*R->w + sx)*7; if (!q[3]) continue; float cov = q[3]/255.f, *d = out + (result*VW + item)*3, *e = emb + (result*VW + item)*3;
                d[0] += (q[0] - d[0])*cov; d[1] += (q[1] - d[1])*cov; d[2] += (q[2] - d[2])*cov; e[0] += q[4]; e[1] += q[5]; e[2] += q[6];
            }
        }
    }

    nsl = nfl = 0; int i0 = bAt((int)floorf(cam) - 130), i1 = bAt((int)floorf(cam) + VW + 130);
    for (int idx = i0 < 0 ? 0 : i0; idx <= (i1 < 0 ? nb - 1 : i1); idx++) {
        Bld *b = &blds[idx];
        if (b->gap) { if (b->glow && nsl < MAXS) sl[nsl++] = (SLight){ b->x0 + b->w/2.f - cam, GY - 26, -60, 110, .7f, .7f, emCol(3) }; continue; }
        for (int kdx = 0; kdx < b->nl; kdx++) {
            Light *L = &b->L[kdx]; float sx = L->item - cam; if (sx < -L->R || sx > VW + L->R) continue; float f = flick(L->g, t);
            if (nsl < MAXS) sl[nsl++] = (SLight){ sx, L->result, L->extra, L->R, L->s, f, emCol(L->t) };
            if (fclass(L->g) && nfl < MAXS) fl[nfl++] = (FLight){ L->item, L->result, L->extra, L->R, L->s, f, emCol(L->t) };
        }
    }
    float ambc[3] = { (.14f*AMB + .02f)*2, (.22f*AMB + .03f)*2, (.2f*AMB + .03f)*2 }, ambm[3] = { (.14f*AMB_M + .02f)*1.8f, (.22f*AMB_M + .03f)*1.8f, (.2f*AMB_M + .03f)*1.8f };
    {
        int bi = -1;
        for (int item = 0; item < VW; item++) {
            int wx = (int)floorf(item + cam);
            if (bi < 0 || bi >= nb || wx < blds[bi].x0 || wx >= blds[bi].x0 + blds[bi].w) bi = bAt(wx);
            if (bi < 0) continue;
            Bld *b = &blds[bi]; if (b->gap) continue; if (!b->lit) bake(bi); int lx = wx - b->x0;
            for (int result = 0; result < GY; result++) {
                int kdx = result*b->w + lx; if (!b->a[kdx]) continue; float *o = out + (result*VW + item)*3, *bound = b->lit + kdx*3;
                float lr = ambc[0] + bound[0], lg = ambc[1] + bound[1], lb = ambc[2] + bound[2];
                if (nfl) {
                    float *digit = b->digit + kdx*4;
                    for (int secondary = 0; secondary < nfl; secondary++) {
                        FLight *L = &fl[secondary]; float dx = L->item - wx; if (dx > L->R || dx < -L->R) continue; float dy = L->result - result, dz = L->extra - digit[3] + 8, d = sqrtf(dx*dx + dy*dy + dz*dz); if (d > L->R || d <= 0) continue;
                        float s = (1 - d/L->R)*(1 - d/L->R)*fmaxf(0, (digit[0]*dx + digit[1]*dy + digit[2]*dz)/d)*2.2f*L->s*L->f/255; lr += L->c[0]*s; lg += L->c[1]*s; lb += L->c[2]*s;
                    }
                }
                float r = b->al[kdx*3]*lr, g = b->al[kdx*3 + 1]*lg, bl = b->al[kdx*3 + 2]*lb;
                if (b->et[kdx]) { const float *ec = emCol(b->et[kdx]); float s = b->ei[kdx]/255.f*flick(b->eg[kdx], t); r = ec[0]*s; g = ec[1]*s; bl = ec[2]*s; float *e = emb + (result*VW + item)*3; e[0] += r; e[1] += g; e[2] += bl; }
                o[0] = r; o[1] = g; o[2] = bl;
            }
        }
    }

    for (int result = 0; result < VH; result++) for (int item = 0; item < VW; item++) {
        float *o = out + (result*VW + item)*3; int q = (result*2*SW + item*2)*3;
        for (int c = 0; c < 3; c++) big[q + c] = big[q + 3 + c] = big[q + SW*3 + c] = big[q + SW*3 + 3 + c] = o[c];
    }

    if (showMishu) {
        int f = mishuFrame(), sw = mAl.width, cx = f*CW, cy = clip*CH, ox2 = PX*2 - CW/2, oy2 = (int)(py*2) - CGY;
        for (int result = 0; result < CH; result++) for (int item = 0; item < CW; item++) {
            int s = (cy + result)*sw + cx + item; Color A = MA[s]; if (!A.a) continue; int X = ox2 + item, Y = oy2 + result; if (X < 0 || X >= SW || Y < 0 || Y >= SGY + 4) continue; float *o = big + (Y*SW + X)*3;
            Color N = MN[s]; float nx = N.r/127.5f - 1, ny = -(N.g/127.5f - 1), nz = N.b/127.5f - 1, pz = (MD[s].r/255.f - .5f)*DPX, lr = ambm[0], lg = ambm[1], lb = ambm[2];
            for (int secondary = 0; secondary < nsl; secondary++) {
                SLight *L = &sl[secondary]; float dx = L->item*2 - X, dy = L->result*2 - Y, dz = L->extra*2 - pz, d = sqrtf(dx*dx + dy*dy + dz*dz), R = L->R*2.6f; if (d > R || d <= 0) continue;
                float nd = fmaxf(0, (nx*dx + ny*dy + nz*dz)/d); if (nd <= 0) continue;
                float d2 = sqrtf(dx*dx + dy*dy); int st = (int)d2 < 40 ? (int)d2 : 40, blk = 0;
                for (int q = 3; q <= st; q++) {
                    float u = q/d2; int sx2 = (int)roundf(item + dx*u), sy2 = (int)roundf(result + dy*u); if (sx2 < 0 || sx2 >= CW || sy2 < 0 || sy2 >= CH) break;
                    int s3 = (cy + sy2)*sw + cx + sx2; if (!MA[s3].a) continue; float zs = (MD[s3].r/255.f - .5f)*DPX, rz = pz + dz*u; if (rz < zs - 1 && rz > zs - 18) { blk = 1; break; }
                }
                if (blk) continue;
                float s2 = (1 - d/R)*(1 - d/R)*L->f*L->s*(nd*2.6f + powf(nd, 16)*1.2f)/255; lr += L->c[0]*s2; lg += L->c[1]*s2; lb += L->c[2]*s2;
            }
            o[0] = A.r*lr; o[1] = A.g*lg; o[2] = A.b*lb;
        }
    }

    for (int secondary = 0; secondary < MH + 2; secondary++) for (int idx = 0; idx < MW + 2; idx++) mist[secondary*(MW + 2) + idx] = vnoise(idx*.35f + cam*.004f + t*.05f, secondary*.5f + t*.02f)*.6f + vnoise(idx*.9f - t*.12f, secondary*1.1f)*.4f;
    for (int result = 0; result < SGY; result++) {
        float hf = HAZE*(.08f + .32f*powf((float)result/SGY, 2.2f));
        for (int item = 0; item < SW; item++) { float f = fminf(.85f, hf*(.55f + mist[(result >> 4)*(MW + 2) + (item >> 4)]*.9f)), *o = big + (result*SW + item)*3; o[0] += (FOG[0] + 6 - o[0])*f; o[1] += (FOG[1] + 8 - o[1])*f; o[2] += (FOG[2] + 6 - o[2])*f; }
    }

    for (int result = SGY; result < SH; result++) {
        int dd = result - SGY, ry = (int)(SGY - 1 - dd*1.15f), wy = result >> 1, wd = dd >> 1;
        for (int item = 0; item < SW; item++) {
            int wx = item >> 1; float *o = big + (result*SW + item)*3, digit = hash(wx + (int)floorf(cam), wy)*6, rr = 7 + digit, rg = 10 + digit, rb = 10 + digit;
            if (ry >= 0) {
                int rx = item + 2*(int)roundf(sinf(wy*1.7f + t*4)*(1 + wd*.06f)); rx = rx < 0 ? 0 : rx >= SW ? SW - 1 : rx;
                float *q = big + (ry*SW + rx)*3, kdx = .45f*(1 - (float)dd/(SH - SGY))*(.7f + .3f*hash(wx >> 2, wy));
                rr += q[0]*kdx; rg += q[1]*kdx; rb += q[2]*kdx;
                if (!(item & 1) && !(result & 1)) { float *eq = emb + ((ry >> 1)*VW + (rx >> 1))*3, *e = emb + (wy*VW + wx)*3; e[0] += eq[0]*kdx*.6f; e[1] += eq[1]*kdx*.6f; e[2] += eq[2]*kdx*.6f; }
            }
            if (wy ==GY) { rr += 14; rg += 20; rb += 18; }
            float f = HAZE*.25f; o[0] = rr + (FOG[0] - rr)*f; o[1] = rg + (FOG[1] - rg)*f; o[2] = rb + (FOG[2] - rb)*f;
        }
    }

    for (int item = 0; item < VW; item++) {
        int wx = (int)floorf(item + cam*1.6f), ci = (int)floorf(wx/300.f), lx2 = wx - ci*300; Rng r = rngOf(4242, ci);
        if (rnd(&r) < .3f) { int px0 = (int)(rnd(&r)*280); if (lx2 >= px0 && lx2 < px0 + 4) for (int result = 0; result < SH; result++) for (int idx = 0; idx < 2; idx++) { float *o = big + (result*SW + item*2 + idx)*3; o[0] = 3; o[1] = 5; o[2] = 5; } }
        float y0 = 6 + rnd(&r)*14, sag = 10 + rnd(&r)*18, u = lx2/300.f; int cy = (int)(y0 + sag*4*u*(1 - u));
        for (int secondary = 0; secondary < 2; secondary++) for (int idx = 0; idx < 2; idx++) { float *o = big + ((cy*2 + secondary)*SW + item*2 + idx)*3; o[0] = 4; o[1] = 6; o[2] = 6; }
    }

    for (int l = 0; l < 2; l++) {
        Rain *R = &rainL[l];
        for (int ty = 0; ty < VH; ty += RT) for (int tx = 0; tx < VW; tx += RT) for (int q = 0; q < R->np; q += 2) {
            int X = tx + (int)fmodf(fmodf(R->pts[q] + R->ox, RT) + RT, RT), Y = ty + (int)fmodf(fmodf(R->pts[q + 1] + R->oy, RT) + RT, RT); if (X >= VW || Y >= VH) continue;
            for (int secondary = 0; secondary < 2; secondary++) for (int idx = 0; idx < 2; idx++) { float *o = big + ((Y*2 + secondary)*SW + X*2 + idx)*3; o[0] += (110 - o[0])*R->a; o[1] += (140 - o[1])*R->a; o[2] += (130 - o[2])*R->a; }
        }
    }

    for (int idx = 0; idx < SW*SH; idx++) {
        float r = big[idx*3]*BRI, g = big[idx*3 + 1]*BRI, b = big[idx*3 + 2]*BRI, l = .3f*r + .59f*g + .11f*b;
        r = (l + (r - l)*SAT - 40)*CON + 40; g = (l + (g - l)*SAT - 40)*CON + 40; b = (l + (b - l)*SAT - 40)*CON + 40;
        pix[idx] = (Color){ (unsigned char)fminf(255, fmaxf(0, r)), (unsigned char)fminf(255, fmaxf(0, g)), (unsigned char)fminf(255, fmaxf(0, b)), 255 };
    }

    for (int result = 0; result < BH; result++) for (int item = 0; item < BW; item++) {
        float s[3] = { 0 }; for (int secondary = 0; secondary < 4; secondary++) for (int idx = 0; idx < 4; idx++) { float *e = emb + ((result*4 + secondary)*VW + item*4 + idx)*3; s[0] += e[0]; s[1] += e[1]; s[2] += e[2]; }
        for (int c = 0; c < 3; c++) blm[(result*BW + item)*3 + c] = s[c]/16;
    }
    for (int pass = 0; pass < 2; pass++) {
        for (int result = 0; result < BH; result++) for (int item = 0; item < BW; item++) for (int c = 0; c < 3; c++) { float s = 0; int digit = 0; for (int idx = -4; idx <= 4; idx++) { int xx = item + idx; if (xx < 0 || xx >= BW) continue; s += blm[(result*BW + xx)*3 + c]; digit++; } temporary[(result*BW + item)*3 + c] = s/digit; }
        for (int result = 0; result < BH; result++) for (int item = 0; item < BW; item++) for (int c = 0; c < 3; c++) { float s = 0; int digit = 0; for (int secondary = -4; secondary <= 4; secondary++) { int yy = result + secondary; if (yy < 0 || yy >= BH) continue; s += temporary[(yy*BW + item)*3 + c]; digit++; } blm[(result*BW + item)*3 + c] = s/digit; }
    }
    float bk = BLOOM*(1.4f + HAZE*.6f);
    for (int idx = 0; idx < BW*BH; idx++) bpix[idx] = (Color){ (unsigned char)fminf(255, blm[idx*3]*bk), (unsigned char)fminf(255, blm[idx*3 + 1]*bk), (unsigned char)fminf(255, blm[idx*3 + 2]*bk), 255 };

    for (int idx = 0; idx < nb; idx++) if (blds[idx].x0 + blds[idx].w < cam - 600 && blds[idx].al) freeBld(&blds[idx]);
    for (int idx = 0; idx < mids.digit; idx++) if (mids.list[idx].px && mids.list[idx].x0 + 300 < cam*.4f - 300) { free(mids.list[idx].px); mids.list[idx].px = 0; }
    for (int idx = 0; idx < fars.digit; idx++) if (fars.list[idx].px && fars.list[idx].x0 + 300 < cam*.15f - 300) { free(fars.list[idx].px); fars.list[idx].px = 0; }
}

static void play(int c) { if (clip != c) { clip = c; animT = 0; } }
static int clipFrame(void) { return (int)(animT*24*CLIPS[clip].rate); }
static int clipDone(void) { return !CLIPS[clip].loop && clipFrame() >= CLIPS[clip].digit; }
static void toSprint(int f) { clip = C_SPRINT; animT = (f + .5f)/24; }
static void advance(void) { const Clip *c = &CLIPS[clip]; if (c->then ==C_SPRINT) toSprint(c->thenFrame); else if (c->then >= 0) play(c->then); }
static int mishuFrame(void) {
    const Clip *c = &CLIPS[clip]; int f;
    if (clip ==C_DASH) f = (int)((1 - dash/DASH_T)*c->digit);
    else f = clipFrame();
    f -= f % FSTEP;
    return c->loop ? f % c->digit : (f >= c->digit ? c->digit - 1 : f);
}

static void startRun(void) {
    px = PX; py = GY; vy = 0; tleft = TIME_LIMIT; dash = cd = peak = 0; slide = -1; dead = 0; deadT = 0; pj = pd = ps = 0;
    clip = -1; play(C_SPRINT); scene = PLAY;
    seed = (unsigned)GetRandomValue(1, 1000000); resetCity();
    for (int l = 0; l < 2; l++) rainL[l].b = -1;
}

static void kill(void) { if (!dead) { dead = 1; deadT = 0; best = fmaxf(best, px); play(C_DEATH); } }

static float camV;
static void stepPlay(float dt) {
    if (dead) { deadT += dt; animT += dt; camV = 0; return; }
    int ground = py >= GY;
    if (pj && ground) { vy = -JUMP_V; py -= 1; slide = -1; play(C_JSTART); }
    if (pd && cd <= 0 && slide < 0) { dash = DASH_T; cd = DASH_CD; play(C_DASH); }
    if (ps && ground && dash <= 0 && slide < 0) { slide = 0; play(C_SSTART); }
    pj = pd = ps = 0;

    camV = dash > 0 ? DASH_V : slide >= 0 ? RUN_V*SLIDE_V : RUN_V; px += camV*dt;
    if (cd > 0) cd -= dt;
    if (dash > 0) dash -= dt;
    if (slide >= 0 && (slide += dt) >= SLIDE_T) { slide = -1; play(C_SEXIT); }
    if (py < GY || vy < 0) {
        vy += GRAV*dt; py += vy*dt; peak = fmaxf(peak, GY - py);
        if (py >= GY) {
            py = GY; vy = 0;
            if (peak >= ROLL_H) play(C_ROLL); else if (dash <= 0 && slide < 0) play(C_JLAND);
            peak = 0;
        }
    }
    if ((tleft -= dt) <= 0) { tleft = 0; kill(); return; }

    animT += dt;
    if (dash > 0) { play(C_DASH); return; }
    if (clip ==C_DASH) { if (py < GY) play(C_JAIR); else advance(); return; }
    if (clip ==C_ROLL && !clipDone()) return;
    if (py < GY) { if (clip ==C_JSTART) { if (clipDone()) advance(); } else play(C_JAIR); }
    else if (slide >= 0) { if (clip ==C_SSTART && clipDone()) advance(); else if (clip != C_SSTART && clip != C_SLOOP) play(C_SLOOP); }
    else if (clip ==C_JLAND || clip ==C_ROLL || clip ==C_SEXIT) { if (clipDone()) advance(); }
    else if (clip != C_SPRINT) play(C_SPRINT);
}

#define C_LINE (Color){ 200, 200, 200, 255 }
#define C_BTN  (Color){ 52, 52, 52, 170 }
#define C_SEL  (Color){ 96, 96, 96, 200 }
#define C_DIM  (Color){ 130, 130, 130, 255 }

static void drawHud(Rectangle *b) {
    char t[32]; snprintf(t, sizeof t, "%.1f", tleft);
    Color tc = tleft < 5 && ((int)(tleft*4) & 1) ? (Color){ 190, 190, 190, 255 } : WHITE;
    DrawText(t, VW/2 - MeasureText(t, 20)/2, 6, 20, tc);
    DrawRectangle(vw/2 - 150, 30, 300, 4, C_BTN);
    DrawRectangle(vw/2 - 150, 30, (int)(300*tleft/TIME_LIMIT), 4, tc);
    snprintf(t, sizeof t, "%dm", (int)((px - PX)/10)); DrawText(t, 9, 9, 10, C_DIM);
    const char *lab[3] = { "SLIDE", "DASH", "JUMP" };
    for (int idx = 0; idx < 3; idx++) {
        int on = held(b[idx]), off = idx ==1 && cd > 0;
        DrawRectangleRec(b[idx], off ? (Color){ 34, 34, 34, 170 } : on ? C_SEL : C_BTN);
        if (idx ==1 && cd > 0) DrawRectangle(b[idx].x, b[idx].y, b[idx].width*(1 - cd/DASH_CD), b[idx].height, C_BTN);
        DrawRectangleLinesEx(b[idx], 1, (Color){ 130, 130, 130, 160 });
        DrawText(lab[idx], b[idx].x + b[idx].width/2 - MeasureText(lab[idx], 10)/2, b[idx].y + b[idx].height/2 - 5, 10, WHITE);
    }
}

static int button(Rectangle r, const char *s, int focus) {
    DrawRectangleRec(r, focus ? C_SEL : C_BTN);
    DrawRectangleLinesEx(r, 1, C_DIM);
    DrawText(s, r.x + r.width/2 - MeasureText(s, 10)/2, r.y + r.height/2 - 5, 10, WHITE);
    return tapped(r);
}

static void centered(const char *s, int result, int size, Color c) { DrawText(s, VW/2 - MeasureText(s, size)/2, result, size, c); }

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Out of Seconds");
    SetExitKey(KEY_NULL);
    SetTargetFPS(144);
#if !defined(PLATFORM_ANDROID)
    ToggleBorderlessWindowed();
    ChangeDirectory(GetApplicationDirectory());
#endif
    mAl = loadArt("mishu_albedo.png"); mNm = loadArt("mishu_normal.png"); mDp = loadArt("mishu_depth.png");
    fAl = loadArt("facades.png"); fNd = loadArt("facades_nd.png"); sky = loadArt("skyline.png");
    MA = mAl.data; MN = mNm.data; MD = mDp.data;
    for (int idx = 0; idx < NSPR; idx++) { blurOne(0, idx, BLUR_MID); blurOne(1, idx, BLUR_FAR); }
    resetCity();
    SetTextureFilter(GetFontDefault().texture, TEXTURE_FILTER_POINT);
    Image si = GenImageColor(SWM, SH, BLACK), bi = GenImageColor(BWM, BH, BLACK);
    Texture2D scn = LoadTextureFromImage(si), blt = LoadTextureFromImage(bi); UnloadImage(si); UnloadImage(bi);
    SetTextureFilter(scn, TEXTURE_FILTER_POINT); SetTextureFilter(blt, TEXTURE_FILTER_BILINEAR);
    RenderTexture2D rt = LoadRenderTexture(SWM, SH); SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);
    Rectangle pb[3] = { { 9, 239, 138, 28 }, { 171, 239, 138, 28 }, { 333, 239, 138, 28 } };
    Rectangle mb[3] = { { 165, 117, 150, 30 }, { 165, 155, 150, 30 }, { 165, 193, 150, 30 } };
    Rectangle ob[2] = { { 82, 180, 150, 32 }, { 248, 180, 150, 32 } };
    int prevH[3] = { 0, 0, 0 };
    float acc = 0, t = 0;

    while (!quit && !WindowShouldClose()) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

        float s = (float)sh/SH; sc = s >= 1 ? floorf(s) : s;
        vw = (int)(sw/sc/2); vw = vw < 480 ? 480 : vw > VWM ? VWM : vw;
        s = fminf((float)sw/SW, (float)sh/SH); if (s < sc) sc = s;
        ox = (sw - SW*sc)/2; oy = (sh - SH*sc)/2;
        pb[0].x = 9; pb[1].x = vw/2 - 69; pb[2].x = vw - 147;
        for (int idx = 0; idx < 3; idx++) mb[idx].x = vw/2 - 75;
        ob[0].x = vw/2 - 158; ob[1].x = vw/2 + 8;
        nptr = 0;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ptr[nptr++] = toV(GetMousePosition());
        for (int idx = 0; idx < GetTouchPointCount() && nptr < 10; idx++) ptr[nptr++] = toV(GetTouchPosition(idx));
        tapNow = nptr > 0 && prevN ==0; if (tapNow) tapPos = ptr[0]; prevN = nptr;
        float dt = fminf(GetFrameTime(), 1.f/30); t += dt;

        if (scene ==PLAY) {
            int h[3] = { held(pb[0]) || IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_LEFT_CONTROL),
                         held(pb[1]) || IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT),
                         held(pb[2]) || IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) };
            if (h[0] && !prevH[0]) ps = 1;
            if (h[1] && !prevH[1]) pd = 1;
            if (h[2] && !prevH[2]) pj = 1;
            for (int idx = 0; idx < 3; idx++) prevH[idx] = h[idx];
            for (acc += dt; acc >= STEP; acc -= STEP) stepPlay(STEP);
            if (IsKeyPressed(KEY_ESCAPE)) scene = MENU;
            if (dead && deadT > 2.1f) scene = OVER;
        } else {
            camV = 0;
            for (int idx = 0; idx < 3; idx++) prevH[idx] = 1;
            if (scene ==MENU) {
                if (IsKeyPressed(KEY_DOWN)) sel = (sel + 1) % 3;
                if (IsKeyPressed(KEY_UP)) sel = (sel + 2) % 3;
                if (IsKeyPressed(KEY_ESCAPE)) quit = 1;
            }
            if (scene ==UPG && IsKeyPressed(KEY_ESCAPE)) scene = MENU;
            if (scene ==OVER && IsKeyPressed(KEY_ESCAPE)) scene = MENU;
        }

        buildRain(camV);
        for (int l = 0; l < 2; l++) { Rain *R = &rainL[l]; R->oy = fmodf(R->oy + R->v*dt, RT); R->ox = fmodf(R->ox - (R->v*.05f + camV*R->kdx)*dt, RT); }

        float cam = px - PX;
        render(cam, t, scene ==PLAY || scene ==OVER);
        UpdateTextureRec(scn, (Rectangle){ 0, 0, SW, SH }, pix); UpdateTextureRec(blt, (Rectangle){ 0, 0, BW, BH }, bpix);
        BeginTextureMode(rt);
        DrawTexture(scn, 0, 0, WHITE);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(blt, (Rectangle){ 0, 0, BW, BH }, (Rectangle){ 0, 0, BW*8, BH*8 }, (Vector2){ 0, 0 }, 0, WHITE);
        EndBlendMode();
        BeginMode2D((Camera2D){ .zoom = 2 });
        if (scene ==PLAY) drawHud(pb);
        if (scene ==MENU) {
            DrawRectangle(0, 0, VW, VH, (Color){ 5, 8, 8, 120 });
            centered("OUT OF SECONDS", 40, 40, WHITE);
            centered("30 seconds. Run. Don't stop.", 88, 10, C_DIM);
            const char *lab[3] = { "RUN", "UPGRADES", "EXIT" };
            for (int idx = 0; idx < 3; idx++) if (button(mb[idx], lab[idx], sel ==idx) || (sel ==idx && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)))) {
                if (idx ==0) startRun(); else if (idx ==1) scene = UPG; else quit = 1;
            }
            centered("Jump: Space/W   Dash: Shift/D   Slide: S/Ctrl   Window: F11", 248, 10, (Color){ 120, 120, 120, 255 });
        } else if (scene ==UPG) {
            DrawRectangle(0, 0, VW, VH, (Color){ 5, 8, 8, 120 });
            centered("UPGRADES", 45, 20, WHITE);
            centered("Nothing here yet.", 120, 10, C_DIM);
            if (button((Rectangle){ vw/2 - 75, 193, 150, 30 }, "BACK", 1) || IsKeyPressed(KEY_ENTER)) scene = MENU;
        } else if (scene ==OVER) {
            DrawRectangle(0, 0, VW, VH, (Color){ 5, 8, 8, 170 });
            centered("You're Out of Seconds", 36, 20, WHITE);
            centered("Time ran out.", 78, 10, C_DIM);
            char tx[48]; snprintf(tx, sizeof tx, "Distance %dm   Best %dm", (int)((px - PX)/10), (int)((best - PX)/10));
            centered(tx, 117, 10, WHITE);
            if (button(ob[0], "RETRY", 1) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) startRun();
            else if (button(ob[1], "MENU", 0)) scene = MENU;
        }
        EndMode2D();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(rt.texture, (Rectangle){ 0, 0, SW, -SH }, (Rectangle){ ox, oy, SW*sc, SH*sc }, (Vector2){ 0, 0 }, 0, WHITE);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
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
typedef struct { int length, loop, then, thenFrame; float rate; } Clip;
static const Clip CLIPS[] = {
    { 16, 1, -1, 0, 1 }, { 18, 0, C_JAIR, 0, 1.5f }, { 48, 1, -1, 0, 1 }, { 15, 0, C_SPRINT, 2, 1 }, { 18, 0, C_SPRINT, 2, 1 },
    { 21, 0, C_SLOOP, 0, 1.5f }, { 48, 1, -1, 0, 1 }, { 16, 0, C_SPRINT, 3, 1 }, { 47, 0, -1, 0, 1 }, { 9, 0, C_SPRINT, 0, 1 },
    { 17, 0, C_SPRINT, 11, 1 } };

enum { MENU, UPG, PLAY, OVER };
static int scene = MENU, quit, sel;
static float sc = 1, ox, oy;
static Vector2 ptr[11]; static int nptr, tapNow, prevN; static Vector2 tapPos;

static float camX, px, py, vy, tleft, dash, cd, slide, animT, deadT, best, peak;
static int clip, dead, pj, pd, ps;

static Vector2 toV(Vector2 p) { return (Vector2){ (p.x - ox)/sc, (p.y - oy)/sc }; }
static int inR(Vector2 p, Rectangle r) { return CheckCollisionPointRec(p, r); }

static Image mAl, mNm, mDp, fAl, fNd, sky;
static Color *MA, *MN, *MD;
static const char *artPath(const char *length) {
#if defined(PLATFORM_ANDROID)
    return length;
#else
    return FileExists(TextFormat("assets/%s", length)) ? TextFormat("assets/%s", length) : TextFormat("../assets/%s", length);
#endif
}
static Image loadArt(const char *length) {
    Image highest = LoadImage(artPath(length));
    ImageFormat(&highest, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return highest;
}

static const unsigned char PAL[24][3] = { {137,158,147},{123,140,130},{114,130,119},{106,122,112},{100,113,103},{95,106,95},{88,100,91},{83,93,82},
    {78,89,81},{75,83,73},{69,79,71},{67,75,67},{63,71,63},{59,67,59},{57,62,55},{52,59,53},{48,53,47},{44,50,45},{40,46,41},{36,40,36},
    {32,37,33},{27,31,28},{20,22,20},{5,6,5} };
static const unsigned char *P(int idx) { return PAL[idx < 23 ? idx : 23]; }
static const unsigned char SIGN[3] = { 8, 12, 12 };

typedef struct { int y0, w, floors, ncol; int cols[6]; } Block;
static const Block BLK[3] = { { 0, 612, 6, 6, { 0, 132, 251, 395, 502, 612 } }, { 384, 511, 5, 6, { 0, 75, 203, 326, 450, 511 } },
                              { 704, 403, 4, 5, { 0, 96, 188, 283, 375 } } };

typedef struct { int input, buffer, w, h; } SprRect;
static const SprRect MIDR[] = { {0,0,100,210}, {101,0,76,210}, {178,0,89,186}, {268,0,68,186}, {337,0,156,185}, {494,0,81,185}, {576,0,74,120}, {651,0,52,120}, {704,0,73,115}, {778,0,55,115}, {834,0,162,112}, {0,211,204,112}, {205,211,94,111}, {300,211,52,111}, {523,211,71,92}, {595,211,70,92}, {786,211,79,91}, {866,211,71,91}, {325,324,149,53}, {475,324,129,53}, {605,324,233,50}, {0,382,205,49}, {206,382,161,47}, {368,382,233,46} };
static const SprRect FARR[] = { {353,211,50,105}, {404,211,38,105}, {443,211,44,93}, {488,211,34,93}, {666,211,78,92}, {745,211,40,92}, {938,211,37,60}, {976,211,26,60}, {0,324,36,57}, {37,324,27,57}, {65,324,81,56}, {147,324,102,56}, {250,324,47,55}, {298,324,26,55}, {602,382,35,46}, {638,382,35,46}, {674,382,39,45}, {714,382,35,45}, {750,382,74,26}, {825,382,64,26}, {890,382,116,25}, {0,432,102,24}, {103,432,116,23}, {220,432,80,23} };
#define NSPR 24

static unsigned seed = 1312;
static unsigned hashu(int a, int b) { unsigned h = (unsigned)a*374761393u + (unsigned)b*668265263u + seed*1442695041u; h = (h ^ (h >> 13))*1274126177u; return h ^ (h >> 16); }
static float hash(int a, int b) { return (hashu(a, b) >> 8)/16777216.f; }
typedef struct { unsigned s; } Rng;
static Rng rngOf(int a, int b) { return (Rng){ hashu(a, b) }; }
static float rnd(Rng *r) { unsigned t = r->s += 0x6D2B79F5u; t = (t ^ (t >> 15))*(1 | t); t = (t + (t ^ (t >> 7))*(61 | t)) ^ t; return ((t ^ (t >> 14)) >> 8)/16777216.f; }
static float vnoise(float input, float buffer) {
    int xi = (int)floorf(input), yi = (int)floorf(buffer); float fx = input - xi, fy = buffer - yi, a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1);
    float u = fx*fx*(3 - 2*fx), v = fy*fy*(3 - 2*fy); return a + (b - a)*u + (c - a)*v + (a - b - c + d)*u*v;
}

static const float EMC[6][3] = { { 0, 0, 0 }, { 40, 225, 180 }, { 0, 0, 0 }, { 150, 215, 180 }, { 205, 175, 120 }, { 120, 165, 140 } };
static const float *emCol(int t) { return t == 2 ? ACCENT : EMC[t]; }

#define BEACON 0x80000000u
static int fclass(unsigned g) { if (!g) return 0; if (g & BEACON) return 3; float h = hash((int)g, 11); return h < .03f ? 2 : h < .08f ? 1 : 0; }
static float flick(unsigned g, float t) {
    int c = fclass(g); if (!c) return 1;
    if (c == 3) { float P = 2.4f, ph = fmodf(t + hash((int)g, 5)*P, P); return ph < .18f ? 1 : .05f; }
    float base = c == 1 ? 1 : .04f, P = 3.5f + hash((int)g, 3)*5; int level = (int)floorf(t/P); float lt = t - level*P;
    if (hash((int)g*7 + 1, level) > .6f) return base;
    int length = 2 + (int)(hash((int)g*13 + 2, level)*2); float bs = hash((int)g*5 + 3, level)*(P - .5f), u = lt - bs;
    if (u < 0 || u >= length*.13f) return base;
    return fmodf(u, .13f) < .06f ? (c == 1 ? .08f : 1) : base;
}

typedef struct { float input, buffer, extra, R, s; int t; unsigned g; } Light;
#define MAXL 64
typedef struct {
    int gap, glow, x0, w, id;
    unsigned char *al, *a, *et, *ei;
    unsigned *eg;
    float *length;
    float *lit;
    Light L[MAXL]; int nl;
} Bld;
static Bld *blds; static int nb, cap, bEnd, bId;

static void addLight(Bld *b, Light l) { if (b->nl < MAXL) b->L[b->nl++] = l; }
static void bpx(Bld *b, int input, int buffer, const unsigned char *c, float nx, float ny, float extra) {
    if (input < 0 || input >= b->w || buffer < 0 || buffer >= GY) return;
    int idx = buffer*b->w + input; memcpy(b->al + idx*3, c, 3); b->a[idx] = 255;
    float *length = b->length + idx*4; length[0] = nx; length[1] = ny; length[2] = sqrtf(fmaxf(0, 1 - nx*nx - ny*ny)); length[3] = extra; b->et[idx] = 0;
}
static void brect(Bld *b, int input, int buffer, int w, int h, const unsigned char *c, float nx, float ny, float extra) { for (int jdx = buffer; jdx < buffer + h; jdx++) for (int idx = input; idx < input + w; idx++) bpx(b, idx, jdx, c, nx, ny, extra); }
static void bem(Bld *b, int input, int buffer, int w, int h, int t, int s, unsigned g) {
    if (!fclass(g)) g = 0;
    for (int jdx = buffer; jdx < buffer + h; jdx++) for (int idx = input; idx < input + w; idx++) { if (idx < 0 || idx >= b->w || jdx < 0 || jdx >= GY) continue; int level = jdx*b->w + idx; b->et[level] = t; b->ei[level] = s; b->eg[level] = g; b->a[level] = 255; }
}
static void glyph(Bld *b, int input, int buffer, int t, Rng *r, unsigned g) { int highest = (int)(rnd(r)*512); for (int jdx = 0; jdx < 3; jdx++) for (int idx = 0; idx < 3; idx++) if ((highest >> (jdx*3 + idx)) & 1 || (idx == 1 && jdx == 1)) bem(b, input + idx*2, buffer + jdx*2, 2, 2, t, 220, g); }

static void freeBld(Bld *b) { free(b->al); free(b->a); free(b->et); free(b->ei); free(b->eg); free(b->length); free(b->lit); b->al = b->a = b->et = b->ei = 0; b->eg = 0; b->length = b->lit = 0; }

static void genBuilding(Bld *b, int x0, Rng *r) {
    const Block *B = &BLK[(int)(rnd(r)*3)]; int nbay = B->ncol - 1;
    int length = 1 + (int)(rnd(r)*(nbay < 3 ? nbay : 3)), b0 = (int)(rnd(r)*(nbay - length + 1)), w = 0, bsrc[3], bw[3];
    for (int jdx = 0; jdx < length; jdx++) { bsrc[jdx] = B->cols[b0 + jdx]; bw[jdx] = B->cols[b0 + jdx + 1] - B->cols[b0 + jdx]; w += bw[jdx]; }
    int floors = rnd(r) < .45f ? 1 + (int)(rnd(r)*2) : 3 + (int)(rnd(r)*3), rows[5];
    for (int idx = 0; idx < floors; idx++) rows[idx] = (int)(rnd(r)*B->floors);
    int id = ++bId, N = w*GY;
    memset(b, 0, sizeof *b); b->x0 = x0; b->w = w; b->id = id;
    b->al = calloc(N*3, 1); b->a = calloc(N, 1); b->et = calloc(N, 1); b->ei = calloc(N, 1); b->eg = calloc(N, sizeof(unsigned)); b->length = malloc(N*4*sizeof(float));
    for (int idx = 0; idx < N; idx++) { float *q = b->length + idx*4; q[0] = q[1] = 0; q[2] = 1; q[3] = -28; }
    Color *FA = fAl.data, *FN = fNd.data; int FW = fAl.width;
    int top = GY - FL - floors*FL;

    for (int idx = 0; idx < floors; idx++) {
        int y0 = GY - FL - (idx + 1)*FL; if (y0 + FL < 0) continue; int xo = 0;
        for (int bi = 0; bi < length; bi++) {
            for (int yy = 0; yy < FL; yy++) {
                int Y = y0 + yy; if (Y < 0) continue; int sy = B->y0 + rows[idx]*FL + yy;
                for (int xx = 0; xx < bw[bi]; xx++) {
                    Color s = FA[sy*FW + bsrc[bi] + xx]; if (s.a < 128) continue; Color nd = FN[sy*FW + bsrc[bi] + xx]; int level = Y*w + xo + xx;
                    b->al[level*3] = s.r; b->al[level*3 + 1] = s.g; b->al[level*3 + 2] = s.b; b->a[level] = s.a == 254 ? 254 : 255;
                    float *q = b->length + level*4; q[0] = nd.r/127.5f - 1; q[1] = -(nd.g/127.5f - 1); q[2] = nd.b/127.5f - 1; q[3] = -28 + (nd.a/255.f - .25f)*30;
                }
            }

            Rng lr = rngOf(id*31 + idx, xo);
            if (rnd(&lr) < .28f) {
                int t = rnd(&lr) < .5f ? 4 : 5, s = 45 + (int)(rnd(&lr)*50); unsigned g = id*1000 + idx*16 + xo, eg = fclass(g) ? g : 0; float cx = 0, cy = 0; int cn = 0;
                for (int yy = 0; yy < FL; yy++) { int Y = y0 + yy; if (Y < 0) continue; for (int xx = 0; xx < bw[bi]; xx++) { int level = Y*w + xo + xx; if (b->a[level] == 254) { b->et[level] = t; b->ei[level] = s; b->eg[level] = eg; cx += xo + xx; cy += Y; cn++; } } }
                if (cn > 20 && idx < 2) addLight(b, (Light){ x0 + cx/cn, cy/cn, -20, 80, .45f, t, 0 });
            }
            xo += bw[bi];
        }
    }

    if (top > 4) {
        brect(b, 0, top - 6, w, 6, P(5), 0, -.6f, -28); brect(b, 0, top - 6, w, 1, P(2), 0, -1, -28);
        if (rnd(r) < .7f) { int tx = 6 + (int)(rnd(r)*(w - 30)); brect(b, tx, top - 26, 18, 20, P(9), .3f, 0, -30); brect(b, tx - 1, top - 28, 20, 2, P(6), 0, -.8f, -28); for (int jdx = top - 6; jdx < top; jdx++) { bpx(b, tx + 2, jdx, P(12), 0, 0, -28); bpx(b, tx + 15, jdx, P(12), 0, 0, -28); } }
        for (int level = 0; level < 2; level++) if (rnd(r) < .6f) { int ax = (int)(rnd(r)*(w - 4)), ah = 10 + (int)(rnd(r)*22); for (int jdx = top - 6 - ah; jdx < top - 6; jdx++) bpx(b, ax, jdx, P(14), 0, 0, -28); if (rnd(r) < .5f) bem(b, ax, top - 7 - ah, 1, 1, 2, 200, BEACON | (unsigned)(id*7 + level)); }
        for (int level = 0; level < 2; level++) if (rnd(r) < .5f) { int ax = 4 + (int)(rnd(r)*(w - 20)); brect(b, ax, top - 14, 12, 8, P(7), 0, -.3f, -28); }
    }

    int gx = 0;
    while (gx < w) {
        int sw = 40 + (int)(rnd(r)*50); if (sw > w - gx) sw = w - gx; float kind = rnd(r); int y0 = GY - FL;
        brect(b, gx, y0, sw, FL, P(10), 0, 0, -28); brect(b, gx, y0, sw, 4, P(6), 0, -.7f, -28);
        if (kind < .45f) {
            for (int jdx = y0 + 12; jdx < GY; jdx++) for (int idx = gx + 3; idx < gx + sw - 3; idx++) { int s = (jdx - y0)%3; bpx(b, idx, jdx, s == 0 ? P(13) : s == 1 ? P(8) : P(11), 0, s == 0 ? -.5f : s == 2 ? .5f : 0, -29); }
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
            for (int jdx = GY - 8; jdx < GY; jdx += 3) brect(b, gx + sw/2 - 10, jdx, 20, 1, P(9), 0, -.6f, -34);
        }
        if (rnd(r) < .75f) { int tx = gx + sw/2 - 7; unsigned g = id*1000 + 800 + gx; brect(b, tx - 1, y0 + 5, 16, 2, P(12), 0, .6f, -22); bem(b, tx, y0 + 7, 14, 1, 3, 230, g); addLight(b, (Light){ x0 + tx + 7, y0 + 9, -12, 120, .9f, 3, g }); }
        gx += sw;
    }

    if (rnd(r) < NEON) {
        int sw = 14, sh = 36 + (int)(rnd(r)*30), sx = 4 + (int)(rnd(r)*(w - 24)), sy = GY - FL - sh - 6, t = rnd(r) < .6f ? 1 : 2; unsigned g = id*1000 + 700;
        brect(b, sx, sy, sw, sh, SIGN, 0, 0, -18);
        for (int idx = sx; idx < sx + sw; idx++) { bem(b, idx, sy, 1, 1, t, 255, g); bem(b, idx, sy + sh - 1, 1, 1, t, 255, g); }
        for (int jdx = sy; jdx < sy + sh; jdx++) { bem(b, sx, jdx, 1, 1, t, 255, g); bem(b, sx + sw - 1, jdx, 1, 1, t, 255, g); }
        for (int jdx = sy + 4; jdx < sy + sh - 8; jdx += 8) glyph(b, sx + 4, jdx, t, r, g);
        addLight(b, (Light){ x0 + sx + 7, sy + sh/2.f, -10, 120, .8f, t, g });
    }
}
static void ensure(int upto) {
    while (bEnd < upto) {
        if (nb == cap) { cap = cap ? cap*2 : 64; blds = realloc(blds, cap*sizeof *blds); }
        Rng r = rngOf(777, nb + 1); Bld *b = &blds[nb];
        if (rnd(&r) < .3f) { int gap = 20 + (int)(rnd(&r)*40); memset(b, 0, sizeof *b); b->gap = 1; b->x0 = bEnd; b->w = gap; b->glow = rnd(&r) < .5f; nb++; bEnd += gap; continue; }
        genBuilding(b, bEnd, &r); nb++; bEnd += b->w;
    }
}
static int bAt(int wx) { int lo = 0, hi = nb - 1; while (lo <= hi) { int highest = (lo + hi) >> 1; Bld *b = &blds[highest]; if (wx < b->x0) hi = highest - 1; else if (wx >= b->x0 + b->w) lo = highest + 1; else return highest; } return -1; }

static void bake(int idx) {
    Bld *b = &blds[idx]; b->lit = calloc(b->w*GY*3, sizeof(float));
    for (int level = idx - 2; level <= idx + 2; level++) {
        if (level < 0 || level >= nb || blds[level].gap) continue;
        for (int li = 0; li < blds[level].nl; li++) {
            Light *L = &blds[level].L[li]; if (fclass(L->g)) continue; const float *c = emCol(L->t);
            int xa = (int)(L->input - L->R) - b->x0, xb = (int)(L->input + L->R) - b->x0 + 1; if (xa < 0) xa = 0; if (xb > b->w) xb = b->w;
            for (int jdx = 0; jdx < GY; jdx++) for (int input = xa; input < xb; input++) {
                int q = jdx*b->w + input; if (!b->a[q]) continue; float *length = b->length + q*4;
                float dx = L->input - (b->x0 + input), dy = L->buffer - jdx, dz = L->extra - length[3] + 8, d = sqrtf(dx*dx + dy*dy + dz*dz); if (d > L->R || d <= 0) continue;
                float f = (1 - d/L->R)*(1 - d/L->R)*fmaxf(0, (length[0]*dx + length[1]*dy + length[2]*dz)/d)*2.2f*L->s/255;
                b->lit[q*3] += f*c[0]; b->lit[q*3 + 1] += f*c[1]; b->lit[q*3 + 2] += f*c[2];
            }
        }
    }
}

static unsigned char *blurred[2][NSPR];
typedef struct { int spr, x0, id; unsigned char *px; } Far;
typedef struct { Far *list; int length, cap, end; } FarStore;
static FarStore mids, fars;

static void blurOne(int lay, int si, float rad) {
    const SprRect *R = lay ? &FARR[si] : &MIDR[si]; int w = R->w, h = R->h, length = w*h, r = (int)roundf(rad); if (r < 1) r = 1;
    float *A = malloc(length*4*sizeof(float)), *B = malloc(length*4*sizeof(float)); Color *S = sky.data;
    for (int buffer = 0; buffer < h; buffer++) for (int input = 0; input < w; input++) { Color c = S[(R->buffer + buffer)*sky.width + R->input + input]; float a = c.a ? 1 : 0, *o = A + (buffer*w + input)*4; o[0] = c.r*a; o[1] = c.g*a; o[2] = c.b*a; o[3] = a; }
    for (int pass = 0; pass < 2; pass++) {
        for (int buffer = 0; buffer < h; buffer++) for (int input = 0; input < w; input++) { float s[4] = { 0 }; int c = 0; for (int level = -r; level <= r; level++) { int xx = input + level; if (xx < 0 || xx >= w) continue; float *q = A + (buffer*w + xx)*4; for (int highest = 0; highest < 4; highest++) s[highest] += q[highest]; c++; } for (int highest = 0; highest < 4; highest++) B[(buffer*w + input)*4 + highest] = s[highest]/c; }
        for (int buffer = 0; buffer < h; buffer++) for (int input = 0; input < w; input++) { float s[4] = { 0 }; int c = 0; for (int level = -r; level <= r; level++) { int yy = buffer + level; if (yy < 0 || yy >= h) continue; float *q = B + (yy*w + input)*4; for (int highest = 0; highest < 4; highest++) s[highest] += q[highest]; c++; } for (int highest = 0; highest < 4; highest++) A[(buffer*w + input)*4 + highest] = s[highest]/c; }
    }
    unsigned char *d = calloc(length*4, 1);
    for (int idx = 0; idx < length; idx++) { float a = A[idx*4 + 3]; if (a > .02f) { d[idx*4] = (unsigned char)fminf(255, A[idx*4]/a); d[idx*4 + 1] = (unsigned char)fminf(255, A[idx*4 + 1]/a); d[idx*4 + 2] = (unsigned char)fminf(255, A[idx*4 + 2]/a); d[idx*4 + 3] = (unsigned char)(a*255); } }
    free(A); free(B); blurred[lay][si] = d;
}

static void bakeFar(Far *o, int lay) {
    const SprRect *R = lay ? &FARR[o->spr] : &MIDR[o->spr]; int w = R->w, h = R->h, yo = lay ? -6 : 0, top = GY + yo - h;
    float fog = lay ? FOG_FAR : FOG_MID, dk = 1 - (lay ? DARK_FAR : DARK_MID), wl = lay ? .06f : .09f;
    float FD[3] = { 24 + (8 - 24)*TINT, 28 + (30 - 28)*TINT, 27 + (22 - 27)*TINT };
    unsigned char *bd = blurred[lay][o->spr]; Color *S = sky.data; o->px = calloc(w*h*7, 1);
    for (int row = 0; row < h; row++) for (int sx = 0; sx < w; sx++) {
        int s = row*w + sx; if (!bd[s*4 + 3]) continue; int buffer = top + row; unsigned char *q = o->px + s*7; Color src = S[(R->buffer + row)*sky.width + R->input + sx];
        float r = bd[s*4]*dk, g = bd[s*4 + 1]*dk, b = bd[s*4 + 2]*dk, f = fminf(1, fog + (1 - fog)*.2f*((float)buffer/GY)), e[3] = { 0, 0, 0 };
        r = r*(1 - f) + FD[0]*f; g = g*(1 - f) + FD[1]*f; b = b*(1 - f) + FD[2]*f; int cov = bd[s*4 + 3];
        float h2 = hash(o->id*613 + sx, row);
        if (src.a && h2 < (src.a == 254 ? wl*.15f : .0012f)) {
            const float *c = emCol(hash(sx, buffer) < .55f ? 4 : 3); float level = .85f*(1 - fog*.3f); r = c[0]*level; g = c[1]*level; b = c[2]*level; e[0] = r*1.4f; e[1] = g*1.4f; e[2] = b*1.4f; cov = 255;
        } else if (src.a == 254) {
            float hh = hash(o->id*977 + (sx >> 1), row >> 2);
            if (hh < wl) { const float *c = emCol(hh < wl*.5f ? 4 : 5); float level = .35f*(1 - fog*.6f); r = c[0]*level; g = c[1]*level; b = c[2]*level; e[0] = r*.6f; e[1] = g*.6f; e[2] = b*.6f; }
        }
        q[0] = (unsigned char)fminf(255, r); q[1] = (unsigned char)fminf(255, g); q[2] = (unsigned char)fminf(255, b); q[3] = cov;
        q[4] = (unsigned char)fminf(255, e[0]); q[5] = (unsigned char)fminf(255, e[1]); q[6] = (unsigned char)fminf(255, e[2]);
    }
}
static void farSeq(FarStore *st, int lay, float upto) {
    while (st->end < upto) {
        Rng r = rngOf(lay ? 52 : 51, st->length + 1); int si = (int)(rnd(&r)*NSPR), gap = lay ? (int)(rnd(&r)*16) - 6 : (int)(rnd(&r)*30) - 8;
        if (st->length == st->cap) { st->cap = st->cap ? st->cap*2 : 64; st->list = realloc(st->list, st->cap*sizeof(Far)); }
        Far *o = &st->list[st->length]; o->spr = si; o->x0 = st->end + gap; o->id = st->length; o->px = 0; bakeFar(o, lay); st->length++;
        st->end += gap + (lay ? FARR[si].w : MIDR[si].w);
    }
}
static Far *farAt(FarStore *st, int lay, int wx) {
    for (int idx = st->length - 1; idx >= 0; idx--) { Far *o = &st->list[idx]; int w = lay ? FARR[o->spr].w : MIDR[o->spr].w; if (wx >= o->x0 && wx < o->x0 + w) return o; if (o->x0 + w < wx - 300) break; }
    return 0;
}

static void resetCity(void) {
    for (int idx = 0; idx < nb; idx++) freeBld(&blds[idx]);
    for (int idx = 0; idx < mids.length; idx++) free(mids.list[idx].px);
    for (int idx = 0; idx < fars.length; idx++) free(fars.list[idx].px);
    nb = 0; bEnd = -300; bId = 0; mids.length = fars.length = 0; mids.end = fars.end = -200;
}

#define RT 128
typedef struct { float v, level, a; int length; float ox, oy, b; float pts[1024]; int np; } Rain;
static Rain rainL[2] = { { 300, .5f, .09f, 45, 0, 0, -1, { 0 }, 0 }, { 560, 1, .17f, 22, 0, 0, -1, { 0 }, 0 } };
static void buildRain(float scroll) {
    for (int l = 0; l < 2; l++) {
        Rain *R = &rainL[l]; if (fabsf(R->b - scroll) < 8) continue; R->b = scroll; float sl = (R->v*.05f + scroll*R->level)/R->v; Rng r = rngOf(99, (int)R->v); R->np = 0;
        for (int idx = 0; idx < R->length; idx++) { float input = rnd(&r)*RT, buffer = rnd(&r)*RT; int total = 4 + (int)(rnd(&r)*6); for (int jdx = 0; jdx < total && R->np < 1022; jdx++) { R->pts[R->np++] = input + jdx*sl; R->pts[R->np++] = buffer - jdx; } }
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
static float out[VWM*VHM*3], emb[VWM*VHM*3], big[SWM*SHM*3], mist[(MWM + 2)*(MHM + 2)], blm[BWM*BHM*3], tmp[BWM*BHM*3];
static Color pix[SWM*SHM], bpix[BWM*BHM];
static const float FOG[3] = { 34, 44, 41 };

typedef struct { float input, buffer, extra, R, s, f; const float *c; } SLight;
#define MAXS 256
static SLight sl[MAXS]; static int nsl;
typedef struct { float input, buffer, extra, R, s, f; const float *c; } FLight;
static FLight fl[MAXS]; static int nfl;

static int mishuFrame(void);
static void render(float cam, float t, int showMishu) {
    memset(emb, 0, VW*VH*3*sizeof(float));
    ensure((int)cam + VW + 300); farSeq(&mids, 0, cam*.4f + VW + 200); farSeq(&fars, 1, cam*.15f + VW + 200);
    for (int buffer = 0; buffer < GY; buffer++) { float u = (float)buffer/GY, r = 5 + 16*u*u, g = 7 + 22*u*u, b = 7 + 19*u*u; for (int input = 0; input < VW; input++) { float *o = out + (buffer*VW + input)*3; o[0] = r; o[1] = g; o[2] = b; } }

    for (int lay = 1; lay >= 0; lay--) {
        FarStore *st = lay ? &fars : &mids; float p = lay ? .15f : .4f; int yo = lay ? -6 : 0;
        for (int input = 0; input < VW; input++) {
            int wx = (int)floorf(input + cam*p); Far *o = farAt(st, lay, wx); if (!o) continue;
            const SprRect *R = lay ? &FARR[o->spr] : &MIDR[o->spr]; int sx = wx - o->x0, top = GY + yo - R->h;
            for (int buffer = top > 0 ? top : 0; buffer < GY && buffer < top + R->h; buffer++) {
                unsigned char *q = o->px + ((buffer - top)*R->w + sx)*7; if (!q[3]) continue; float cov = q[3]/255.f, *d = out + (buffer*VW + input)*3, *e = emb + (buffer*VW + input)*3;
                d[0] += (q[0] - d[0])*cov; d[1] += (q[1] - d[1])*cov; d[2] += (q[2] - d[2])*cov; e[0] += q[4]; e[1] += q[5]; e[2] += q[6];
            }
        }
    }

    nsl = nfl = 0; int i0 = bAt((int)floorf(cam) - 130), i1 = bAt((int)floorf(cam) + VW + 130);
    for (int idx = i0 < 0 ? 0 : i0; idx <= (i1 < 0 ? nb - 1 : i1); idx++) {
        Bld *b = &blds[idx];
        if (b->gap) { if (b->glow && nsl < MAXS) sl[nsl++] = (SLight){ b->x0 + b->w/2.f - cam, GY - 26, -60, 110, .7f, .7f, emCol(3) }; continue; }
        for (int level = 0; level < b->nl; level++) {
            Light *L = &b->L[level]; float sx = L->input - cam; if (sx < -L->R || sx > VW + L->R) continue; float f = flick(L->g, t);
            if (nsl < MAXS) sl[nsl++] = (SLight){ sx, L->buffer, L->extra, L->R, L->s, f, emCol(L->t) };
            if (fclass(L->g) && nfl < MAXS) fl[nfl++] = (FLight){ L->input, L->buffer, L->extra, L->R, L->s, f, emCol(L->t) };
        }
    }
    float ambc[3] = { (.14f*AMB + .02f)*2, (.22f*AMB + .03f)*2, (.2f*AMB + .03f)*2 }, ambm[3] = { (.14f*AMB_M + .02f)*1.8f, (.22f*AMB_M + .03f)*1.8f, (.2f*AMB_M + .03f)*1.8f };
    {
        int bi = -1;
        for (int input = 0; input < VW; input++) {
            int wx = (int)floorf(input + cam);
            if (bi < 0 || bi >= nb || wx < blds[bi].x0 || wx >= blds[bi].x0 + blds[bi].w) bi = bAt(wx);
            if (bi < 0) continue;
            Bld *b = &blds[bi]; if (b->gap) continue; if (!b->lit) bake(bi); int lx = wx - b->x0;
            for (int buffer = 0; buffer < GY; buffer++) {
                int level = buffer*b->w + lx; if (!b->a[level]) continue; float *o = out + (buffer*VW + input)*3, *highest = b->lit + level*3;
                float lr = ambc[0] + highest[0], lg = ambc[1] + highest[1], lb = ambc[2] + highest[2];
                if (nfl) {
                    float *length = b->length + level*4;
                    for (int jdx = 0; jdx < nfl; jdx++) {
                        FLight *L = &fl[jdx]; float dx = L->input - wx; if (dx > L->R || dx < -L->R) continue; float dy = L->buffer - buffer, dz = L->extra - length[3] + 8, d = sqrtf(dx*dx + dy*dy + dz*dz); if (d > L->R || d <= 0) continue;
                        float s = (1 - d/L->R)*(1 - d/L->R)*fmaxf(0, (length[0]*dx + length[1]*dy + length[2]*dz)/d)*2.2f*L->s*L->f/255; lr += L->c[0]*s; lg += L->c[1]*s; lb += L->c[2]*s;
                    }
                }
                float r = b->al[level*3]*lr, g = b->al[level*3 + 1]*lg, bl = b->al[level*3 + 2]*lb;
                if (b->et[level]) { const float *ec = emCol(b->et[level]); float s = b->ei[level]/255.f*flick(b->eg[level], t); r = ec[0]*s; g = ec[1]*s; bl = ec[2]*s; float *e = emb + (buffer*VW + input)*3; e[0] += r; e[1] += g; e[2] += bl; }
                o[0] = r; o[1] = g; o[2] = bl;
            }
        }
    }

    for (int buffer = 0; buffer < VH; buffer++) for (int input = 0; input < VW; input++) {
        float *o = out + (buffer*VW + input)*3; int q = (buffer*2*SW + input*2)*3;
        for (int c = 0; c < 3; c++) big[q + c] = big[q + 3 + c] = big[q + SW*3 + c] = big[q + SW*3 + 3 + c] = o[c];
    }

    if (showMishu) {
        int f = mishuFrame(), sw = mAl.width, cx = f*CW, cy = clip*CH, ox2 = (int)(PX*2 + 2*LEADMAX*tanhf((px - PX - cam)/LEADMAX)) - CW/2, oy2 = (int)(py*2) - CGY;
        for (int buffer = 0; buffer < CH; buffer++) for (int input = 0; input < CW; input++) {
            int s = (cy + buffer)*sw + cx + input; Color A = MA[s]; if (!A.a) continue; int X = ox2 + input, Y = oy2 + buffer; if (X < 0 || X >= SW || Y < 0 || Y >= SGY + 4) continue; float *o = big + (Y*SW + X)*3;
            Color N = MN[s]; float nx = N.r/127.5f - 1, ny = -(N.g/127.5f - 1), nz = N.b/127.5f - 1, pz = (MD[s].r/255.f - .5f)*DPX, lr = ambm[0], lg = ambm[1], lb = ambm[2];
            for (int jdx = 0; jdx < nsl; jdx++) {
                SLight *L = &sl[jdx]; float dx = L->input*2 - X, dy = L->buffer*2 - Y, dz = L->extra*2 - pz, d = sqrtf(dx*dx + dy*dy + dz*dz), R = L->R*2.6f; if (d > R || d <= 0) continue;
                float nd = fmaxf(0, (nx*dx + ny*dy + nz*dz)/d); if (nd <= 0) continue;
                float d2 = sqrtf(dx*dx + dy*dy); int st = (int)d2 < 40 ? (int)d2 : 40, blk = 0;
                for (int q = 3; q <= st; q++) {
                    float u = q/d2; int sx2 = (int)roundf(input + dx*u), sy2 = (int)roundf(buffer + dy*u); if (sx2 < 0 || sx2 >= CW || sy2 < 0 || sy2 >= CH) break;
                    int s3 = (cy + sy2)*sw + cx + sx2; if (!MA[s3].a) continue; float zs = (MD[s3].r/255.f - .5f)*DPX, rz = pz + dz*u; if (rz < zs - 1 && rz > zs - 18) { blk = 1; break; }
                }
                if (blk) continue;
                float s2 = (1 - d/R)*(1 - d/R)*L->f*L->s*(nd*2.6f + powf(nd, 16)*1.2f)/255; lr += L->c[0]*s2; lg += L->c[1]*s2; lb += L->c[2]*s2;
            }
            o[0] = A.r*lr; o[1] = A.g*lg; o[2] = A.b*lb;
        }
    }

    for (int jdx = 0; jdx < MH + 2; jdx++) for (int idx = 0; idx < MW + 2; idx++) mist[jdx*(MW + 2) + idx] = vnoise(idx*.35f + cam*.004f + t*.05f, jdx*.5f + t*.02f)*.6f + vnoise(idx*.9f - t*.12f, jdx*1.1f)*.4f;
    for (int buffer = 0; buffer < SGY; buffer++) {
        float hf = HAZE*(.08f + .32f*powf((float)buffer/SGY, 2.2f));
        for (int input = 0; input < SW; input++) { float f = fminf(.85f, hf*(.55f + mist[(buffer >> 4)*(MW + 2) + (input >> 4)]*.9f)), *o = big + (buffer*SW + input)*3; o[0] += (FOG[0] + 6 - o[0])*f; o[1] += (FOG[1] + 8 - o[1])*f; o[2] += (FOG[2] + 6 - o[2])*f; }
    }

    for (int buffer = SGY; buffer < SH; buffer++) {
        int dd = buffer - SGY, ry = (int)(SGY - 1 - dd*1.15f), wy = buffer >> 1, wd = dd >> 1;
        for (int input = 0; input < SW; input++) {
            int wx = input >> 1; float *o = big + (buffer*SW + input)*3, length = hash(wx + (int)floorf(cam), wy)*6, rr = 7 + length, rg = 10 + length, rb = 10 + length;
            if (ry >= 0) {
                int rx = input + 2*(int)roundf(sinf(wy*1.7f + t*4)*(1 + wd*.06f)); rx = rx < 0 ? 0 : rx >= SW ? SW - 1 : rx;
                float *q = big + (ry*SW + rx)*3, level = .45f*(1 - (float)dd/(SH - SGY))*(.7f + .3f*hash(wx >> 2, wy));
                rr += q[0]*level; rg += q[1]*level; rb += q[2]*level;
                if (!(input & 1) && !(buffer & 1)) { float *eq = emb + ((ry >> 1)*VW + (rx >> 1))*3, *e = emb + (wy*VW + wx)*3; e[0] += eq[0]*level*.6f; e[1] += eq[1]*level*.6f; e[2] += eq[2]*level*.6f; }
            }
            if (wy == GY) { rr += 14; rg += 20; rb += 18; }
            float f = HAZE*.25f; o[0] = rr + (FOG[0] - rr)*f; o[1] = rg + (FOG[1] - rg)*f; o[2] = rb + (FOG[2] - rb)*f;
        }
    }

    for (int input = 0; input < VW; input++) {
        int wx = (int)floorf(input + cam*1.6f), ci = (int)floorf(wx/300.f), lx2 = wx - ci*300; Rng r = rngOf(4242, ci);
        if (rnd(&r) < .3f) { int px0 = (int)(rnd(&r)*280); if (lx2 >= px0 && lx2 < px0 + 4) for (int buffer = 0; buffer < SH; buffer++) for (int idx = 0; idx < 2; idx++) { float *o = big + (buffer*SW + input*2 + idx)*3; o[0] = 3; o[1] = 5; o[2] = 5; } }
        float y0 = 6 + rnd(&r)*14, sag = 10 + rnd(&r)*18, u = lx2/300.f; int cy = (int)(y0 + sag*4*u*(1 - u));
        for (int jdx = 0; jdx < 2; jdx++) for (int idx = 0; idx < 2; idx++) { float *o = big + ((cy*2 + jdx)*SW + input*2 + idx)*3; o[0] = 4; o[1] = 6; o[2] = 6; }
    }

    for (int l = 0; l < 2; l++) {
        Rain *R = &rainL[l];
        for (int ty = 0; ty < VH; ty += RT) for (int tx = 0; tx < VW; tx += RT) for (int q = 0; q < R->np; q += 2) {
            int X = tx + (int)fmodf(fmodf(R->pts[q] + R->ox, RT) + RT, RT), Y = ty + (int)fmodf(fmodf(R->pts[q + 1] + R->oy, RT) + RT, RT); if (X >= VW || Y >= VH) continue;
            for (int jdx = 0; jdx < 2; jdx++) for (int idx = 0; idx < 2; idx++) { float *o = big + ((Y*2 + jdx)*SW + X*2 + idx)*3; o[0] += (110 - o[0])*R->a; o[1] += (140 - o[1])*R->a; o[2] += (130 - o[2])*R->a; }
        }
    }

    for (int idx = 0; idx < SW*SH; idx++) {
        float r = big[idx*3]*BRI, g = big[idx*3 + 1]*BRI, b = big[idx*3 + 2]*BRI, l = .3f*r + .59f*g + .11f*b;
        r = (l + (r - l)*SAT - 40)*CON + 40; g = (l + (g - l)*SAT - 40)*CON + 40; b = (l + (b - l)*SAT - 40)*CON + 40;
        pix[idx] = (Color){ (unsigned char)fminf(255, fmaxf(0, r)), (unsigned char)fminf(255, fmaxf(0, g)), (unsigned char)fminf(255, fmaxf(0, b)), 255 };
    }

    for (int buffer = 0; buffer < BH; buffer++) for (int input = 0; input < BW; input++) {
        float s[3] = { 0 }; for (int jdx = 0; jdx < 4; jdx++) for (int idx = 0; idx < 4; idx++) { float *e = emb + ((buffer*4 + jdx)*VW + input*4 + idx)*3; s[0] += e[0]; s[1] += e[1]; s[2] += e[2]; }
        for (int c = 0; c < 3; c++) blm[(buffer*BW + input)*3 + c] = s[c]/16;
    }
    for (int pass = 0; pass < 2; pass++) {
        for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) for (int c = 0; c < 3; c++) { float s = 0; int length = 0; for (int idx = -4; idx <= 4; idx++) { int xx = x + idx; if (xx < 0 || xx >= BW) continue; s += blm[(y*BW + xx)*3 + c]; length++; } tmp[(y*BW + x)*3 + c] = s/length; }
        for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) for (int c = 0; c < 3; c++) { float s = 0; int length = 0; for (int jdx = -4; jdx <= 4; jdx++) { int yy = y + jdx; if (yy < 0 || yy >= BH) continue; s += tmp[(yy*BW + x)*3 + c]; length++; } blm[(y*BW + x)*3 + c] = s/length; }
    }
    float bk = BLOOM*(1.4f + HAZE*.6f);
    for (int idx = 0; idx < BW*BH; idx++) bpix[idx] = (Color){ (unsigned char)fminf(255, blm[idx*3]*bk), (unsigned char)fminf(255, blm[idx*3 + 1]*bk), (unsigned char)fminf(255, blm[idx*3 + 2]*bk), 255 };

    for (int idx = 0; idx < nb; idx++) if (blds[idx].x0 + blds[idx].w < cam - 600 && blds[idx].al) freeBld(&blds[idx]);
    for (int idx = 0; idx < mids.length; idx++) if (mids.list[idx].px && mids.list[idx].x0 + 300 < cam*.4f - 300) { free(mids.list[idx].px); mids.list[idx].px = 0; }
    for (int idx = 0; idx < fars.length; idx++) if (fars.list[idx].px && fars.list[idx].x0 + 300 < cam*.15f - 300) { free(fars.list[idx].px); fars.list[idx].px = 0; }
}

static void play(int c) { if (clip != c) { clip = c; animT = 0; } }
static int clipFrame(void) { return (int)(animT*24*CLIPS[clip].rate); }
static int clipDone(void) { return !CLIPS[clip].loop && clipFrame() >= CLIPS[clip].length; }
static void toSprint(int f) { clip = C_SPRINT; animT = (f + .5f)/24; }
static void advance(void) { const Clip *c = &CLIPS[clip]; if (c->then == C_SPRINT) toSprint(c->thenFrame); else if (c->then >= 0) play(c->then); }
static int mishuFrame(void) {
    const Clip *c = &CLIPS[clip]; int f;
    if (clip == C_DASH) f = (int)((1 - dash/DASH_T)*c->length);
    else f = clipFrame();
    f -= f % FSTEP;
    return c->loop ? f % c->length : (f >= c->length ? c->length - 1 : f);
}

static void startRun(void) {
    px = PX; camX = 0; py = GY; vy = 0; tleft = TIME_LIMIT; dash = cd = peak = 0; slide = -1; dead = 0; deadT = 0; pj = pd = ps = 0;
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

    float highest = slide >= 0 ? 1 - (1 - SLIDE_V)*slide/SLIDE_T : clip == C_SEXIT ? SLIDE_V + (1 - SLIDE_V)*fminf(1.f, (float)clipFrame()/CLIPS[C_SEXIT].length) : 1;
    float base = RUN_V*highest; px += (dash > 0 ? DASH_V : base)*dt;
    float lead = px - PX - camX; camV = base + fminf(dash > 0 ? 70.f : 110.f, fmaxf(0.f, lead)*3.5f); camX += camV*dt;
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
    if (clip == C_DASH) { if (py < GY) play(C_JAIR); else advance(); return; }
    if (clip == C_ROLL && !clipDone()) return;
    if (py < GY) { if (clip == C_JSTART) { if (clipDone()) advance(); } else play(C_JAIR); }
    else if (slide >= 0) { if (clip == C_SSTART && clipDone()) advance(); else if (clip != C_SSTART && clip != C_SLOOP) play(C_SLOOP); }
    else if (clip == C_JLAND || clip == C_ROLL || clip == C_SEXIT) { if (clipDone()) advance(); }
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

static void glowText(int f, const char *s, float input, float buffer, Color c, float glow) {
    (void)glow;
    DrawTextEx(fnt[f], s, (Vector2){ input, buffer }, FSZ[f], 0, c);
}
static void glowCentered(int f, const char *s, float cx, float buffer, Color c, float glow) { glowText(f, s, floorf(cx - textSize(f, s).x/2), buffer, c, glow); }

static Vector2 btnPos(int idx) { return (Vector2){ Wd - 96, SH/2 + 16 + (idx - 1)*136 }; }
static int btnHeld(int idx) { Vector2 c = btnPos(idx); for (int level = 0; level < nptr; level++) if (CheckCollisionPointCircle(ptr[level], c, BTN_R + 8)) return 1; return 0; }

static void drawHud(void) {
    char t[32]; snprintf(t, sizeof t, "%.1f", tleft);
    int low = tleft < 5; Color tc = low ? HOTPINK : MINT; float fl = low && ((int)(tleft*4) & 1) ? .45f : 1;
    glowCentered(F66, t, Wd/2.f, 18, tc, 1.2f*fl);
    float bw = 360, bx = Wd/2.f - bw/2;
    DrawRectangle(bx, 102, bw, 4, (Color){ 10, 16, 15, 150 });
    BeginBlendMode(BLEND_ADDITIVE); DrawRectangle(bx - 2, 100, bw*tleft/TIME_LIMIT + 4, 8, Fade(tc, .18f)); EndBlendMode();
    DrawRectangle(bx, 102, bw*tleft/TIME_LIMIT, 4, tc);
    snprintf(t, sizeof t, "%dM", (int)((px - PX)/10)); glowText(F33, t, 28, 24, DIMC, .6f);
    for (int idx = 0; idx < 3; idx++) {
        Vector2 c = btnPos(idx); int on = btnHeld(idx), cool = idx == 1 && cd > 0;
        DrawCircleV(c, BTN_R, (Color){ 6, 12, 11, on ? 150 : 95 });
        DrawRing(c, BTN_R - 3, BTN_R, 0, 360, 48, Fade(MINT, on ? .95f : .5f));
        BeginBlendMode(BLEND_ADDITIVE); DrawRing(c, BTN_R, BTN_R + 6, 0, 360, 48, Fade(MINT, on ? .22f : .08f)); EndBlendMode();
        if (cool) DrawRing(c, BTN_R - 11, BTN_R - 6, -90, -90 + 360*(1 - cd/DASH_CD), 48, Fade(MINT, .9f));
        Texture2D s2 = sil[idx]; float level = 64.f/fmaxf(s2.width, s2.height), w = s2.width*level, h = s2.height*level;
        Color sc2 = cool ? Fade(DIMC, .5f) : on ? WHITE : MINT;
        Rectangle src = { 0, 0, s2.width, s2.height }, dst = { c.x - w/2, c.y - h/2, w, h };
        if (!cool) { BeginBlendMode(BLEND_ADDITIVE); DrawTexturePro(s2, src, (Rectangle){ dst.x - 3, dst.y - 3, w + 6, h + 6 }, (Vector2){ 0, 0 }, 0, Fade(MINT, on ? .35f : .16f)); EndBlendMode(); }
        DrawTexturePro(s2, src, dst, (Vector2){ 0, 0 }, 0, sc2);
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
    for (int idx = 0; idx < 4; idx++) { fnt[idx] = LoadFontEx(artPath("After Burner Neue.ttf"), FSZ[idx], NULL, 0); SetTextureFilter(fnt[idx].texture, TEXTURE_FILTER_POINT); }
    {
        static const int pose[3][2] = { { C_SLOOP, 12 }, { C_DASH, 4 }, { C_JAIR, 0 } };
        for (int idx = 0; idx < 3; idx++) {
            Image c = ImageFromImage(mAl, (Rectangle){ pose[idx][1]*CW, pose[idx][0]*CH, CW, CH }); Color *p = c.data;
            int x0 = CW, x1 = -1, y0 = CH, y1 = -1;
            for (int buffer = 0; buffer < CH; buffer++) for (int input = 0; input < CW; input++) if (p[buffer*CW + input].a) { if (input < x0) x0 = input; if (input > x1) x1 = input; if (buffer < y0) y0 = buffer; if (buffer > y1) y1 = buffer; p[buffer*CW + input] = WHITE; }
            ImageCrop(&c, (Rectangle){ x0, y0, x1 - x0 + 1, y1 - y0 + 1 });
            sil[idx] = LoadTextureFromImage(c); SetTextureFilter(sil[idx], TEXTURE_FILTER_BILINEAR); UnloadImage(c);
        }
    }
    resetCity();
    SetTextureFilter(GetFontDefault().texture, TEXTURE_FILTER_POINT);
    Image si = GenImageColor(SWM, SHM, BLACK), bi = GenImageColor(BWM, BHM, BLACK);
    Texture2D scn = LoadTextureFromImage(si), blt = LoadTextureFromImage(bi); UnloadImage(si); UnloadImage(bi);
    SetTextureFilter(scn, TEXTURE_FILTER_POINT); SetTextureFilter(blt, TEXTURE_FILTER_BILINEAR);
    { Image a = GenImageColor(SWM, 1, BLACK), b = GenImageColor(1, SHM, BLACK); eT = LoadTextureFromImage(a); eB = LoadTextureFromImage(a); eL = LoadTextureFromImage(b); eR = LoadTextureFromImage(b); UnloadImage(a); UnloadImage(b);
      Texture2D *e[4] = { &eT, &eB, &eL, &eR }; for (int idx = 0; idx < 4; idx++) SetTextureFilter(*e[idx], TEXTURE_FILTER_BILINEAR); }
    RenderTexture2D rt = LoadRenderTexture(SWM, SHM); SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);
    Rectangle mb[3], ob[2], bk;
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
        for (int idx = 0; idx < 3; idx++) mb[idx] = (Rectangle){ Wd/2.f - 170, 230 + yo + idx*76, 340, 60 };
        ob[0] = (Rectangle){ Wd/2.f - 260, 320 + yo, 240, 60 }; ob[1] = (Rectangle){ Wd/2.f + 20, 320 + yo, 240, 60 };
        bk = (Rectangle){ Wd/2.f - 170, 320 + yo, 340, 60 };
        nptr = 0;
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ptr[nptr++] = toV(GetMousePosition());
        for (int idx = 0; idx < GetTouchPointCount() && nptr < 10; idx++) ptr[nptr++] = toV(GetTouchPosition(idx));
        tapNow = nptr > 0 && prevN == 0; if (tapNow) tapPos = ptr[0]; prevN = nptr;
        float dt = fminf(GetFrameTime(), 1.f/30); t += dt;

        if (scene == PLAY) {
            int h[3] = { btnHeld(0) || IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_LEFT_CONTROL),
                         btnHeld(1) || IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT),
                         btnHeld(2) || IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) };
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
            if (scene == MENU) {
                if (IsKeyPressed(KEY_DOWN)) sel = (sel + 1) % 3;
                if (IsKeyPressed(KEY_UP)) sel = (sel + 2) % 3;
                if (IsKeyPressed(KEY_ESCAPE)) quit = 1;
            }
            if (scene == UPG && IsKeyPressed(KEY_ESCAPE)) scene = MENU;
            if (scene == OVER && IsKeyPressed(KEY_ESCAPE)) scene = MENU;
        }

        buildRain(camV);
        for (int l = 0; l < 2; l++) { Rain *R = &rainL[l]; R->oy = fmodf(R->oy + R->v*dt, RT); R->ox = fmodf(R->ox - (R->v*.05f + camV*R->level)*dt, RT); }

        float cam = camX;
        render(cam, t, scene == PLAY || scene == OVER);
        UpdateTextureRec(scn, (Rectangle){ 0, 0, SW, SH }, pix); UpdateTextureRec(blt, (Rectangle){ 0, 0, BW, BH }, bpix);
        {
            for (int e = 0; e < 4; e++) {
                int length = e < 2 ? SW : SH, st = e < 2 ? 1 : SW; Color *base = e == 0 ? pix : e == 1 ? pix + (SH - 1)*SW : e == 2 ? pix : pix + SW - 1, *d = eBuf[0];
                for (int idx = 0; idx < length; idx++) { int r = 0, g = 0, b = 0, c = 0; for (int level = -12; level <= 12; level++) { int jdx = idx + level; if (jdx < 0 || jdx >= length) continue; Color p = base[jdx*st]; r += p.r; g += p.g; b += p.b; c++; } d[idx] = (Color){ r/c, g/c, b/c, 255 }; }
                if (e == 0) UpdateTextureRec(eT, (Rectangle){ 0, 0, length, 1 }, d); else if (e == 1) UpdateTextureRec(eB, (Rectangle){ 0, 0, length, 1 }, d);
                else if (e == 2) UpdateTextureRec(eL, (Rectangle){ 0, 0, 1, length }, d); else UpdateTextureRec(eR, (Rectangle){ 0, 0, 1, length }, d);
            }
        }
        BeginTextureMode(rt);
        DrawTexture(scn, 0, 0, WHITE);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(blt, (Rectangle){ 0, 0, BW, BH }, (Rectangle){ 0, 0, BW*8, BH*8 }, (Vector2){ 0, 0 }, 0, WHITE);
        EndBlendMode();
        if (scene == PLAY) drawHud();
        if (scene == MENU) {
            DrawRectangle(0, 0, Wd, SH, (Color){ 5, 8, 8, 120 });
            glowCentered(F66, "OUT OF SECONDS", Wd/2.f, 70 + yo, MINT, 1.3f);
            glowCentered(F22, "30 SECONDS. RUN. DON'T STOP.", Wd/2.f, 154 + yo, (Color){ 150, 185, 174, 255 }, .5f);
            const char *lab[3] = { "RUN", "UPGRADES", "EXIT" };
            for (int idx = 0; idx < 3; idx++) if (button(mb[idx], lab[idx], sel == idx) || (sel == idx && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)))) {
                if (idx == 0) startRun(); else if (idx == 1) scene = UPG; else quit = 1;
            }
            glowCentered(F22, "JUMP SPACE/W   DASH SHIFT/D   SLIDE S/CTRL   WINDOW F11", Wd/2.f, SH - 40, (Color){ 120, 155, 145, 255 }, .4f);
        } else if (scene == UPG) {
            DrawRectangle(0, 0, Wd, SH, (Color){ 5, 8, 8, 120 });
            glowCentered(F44, "UPGRADES", Wd/2.f, 110 + yo, MINT, 1.2f);
            glowCentered(F22, "NOTHING HERE YET.", Wd/2.f, 220 + yo, DIMC, .6f);
            if (button(bk, "BACK", 1) || IsKeyPressed(KEY_ENTER)) scene = MENU;
        } else if (scene == OVER) {
            DrawRectangle(0, 0, Wd, SH, (Color){ 5, 8, 8, 170 });
            glowCentered(F44, "YOU'RE OUT OF SECONDS", Wd/2.f, 84 + yo, HOTPINK, 1.3f);
            glowCentered(F22, "TIME RAN OUT.", Wd/2.f, 160 + yo, DIMC, .6f);
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
    return 0;
}
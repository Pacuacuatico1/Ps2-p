/* Prisma Engine - port minimo a PS2 (v1): muestra un modelo .pm girando.
 * Compilar con ps2sdk + gsKit (ver README.md). Sin texturas, sin pad, sin scripts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <kernel.h>
#include <dmaKit.h>
#include <gsKit.h>

extern unsigned char model_pm[];     /* generado por bin2c a partir de model.pm */
extern unsigned int size_model_pm;

typedef struct { float p[3]; float n[3]; unsigned char c[4]; } PVert;

#define SCREEN_W   640
#define SCREEN_H   448
#define FOCAL      700.0f      /* "zoom" de la camara */
#define CAM_DIST   3.2f        /* distancia al modelo (radio del modelo = 1) */
#define NEAR_Z     0.3f
#define FAR_Z      12.0f
#define BATCH_TRIS 256         /* triangulos por envio a la GS */

static int nverts, ntris;
static PVert *verts;
static unsigned int *idx;
static float *sx, *sy;
static int *sz;
static unsigned char *sr, *sg, *sb, *svis;

static int load_model(void)
{
    const unsigned char *d = model_pm;
    unsigned int nv, nt;

    if (size_model_pm < 12 || memcmp(d, "PM01", 4) != 0) return 0;
    memcpy(&nv, d + 4, 4);
    memcpy(&nt, d + 8, 4);
    if (size_model_pm < 12 + nv * sizeof(PVert) + nt * 12) return 0;

    verts = (PVert *)malloc(nv * sizeof(PVert));
    idx = (unsigned int *)malloc(nt * 12);
    sx = (float *)malloc(nv * sizeof(float));
    sy = (float *)malloc(nv * sizeof(float));
    sz = (int *)malloc(nv * sizeof(int));
    sr = (unsigned char *)malloc(nv); sg = (unsigned char *)malloc(nv);
    sb = (unsigned char *)malloc(nv); svis = (unsigned char *)malloc(nv);
    if (!verts || !idx || !sx || !sy || !sz || !sr || !sg || !sb || !svis) return 0;

    memcpy(verts, d + 12, nv * sizeof(PVert));
    memcpy(idx, d + 12 + nv * sizeof(PVert), nt * 12);
    nverts = (int)nv;
    ntris = (int)nt;
    return 1;
}

static void transform(float yaw, float pitch)
{
    const float cy = cosf(yaw), sy_ = sinf(yaw);
    const float cp = cosf(pitch), sp = sinf(pitch);
    const float lx = 0.30f, ly = 0.50f, lz = 0.81f;   /* luz hacia la camara */
    int i;

    for (i = 0; i < nverts; i++) {
        const PVert *v = &verts[i];
        /* giro en Y y luego inclinacion en X */
        float x = cy * v->p[0] + sy_ * v->p[2];
        float z = -sy_ * v->p[0] + cy * v->p[2];
        float y = cp * v->p[1] - sp * z;
        float z2 = sp * v->p[1] + cp * z;
        float nx = cy * v->n[0] + sy_ * v->n[2];
        float nz = -sy_ * v->n[0] + cy * v->n[2];
        float ny = cp * v->n[1] - sp * nz;
        float nz2 = sp * v->n[1] + cp * nz;
        float w, k;

        z2 -= CAM_DIST;           /* camara en el origen mirando hacia -Z */
        w = -z2;
        svis[i] = (w > NEAR_Z);
        if (!svis[i]) continue;

        sx[i] = SCREEN_W * 0.5f + x / w * FOCAL;
        sy[i] = SCREEN_H * 0.5f - y / w * FOCAL;
        k = 1.0f - (w - NEAR_Z) / (FAR_Z - NEAR_Z);          /* mas cerca = mas grande */
        if (k < 0.0f) k = 0.0f;
        sz[i] = (int)(k * 16777215.0f);

        k = nx * lx + ny * ly + nz2 * lz;
        k = 0.25f + 0.75f * (k > 0.0f ? k : 0.0f);
        sr[i] = (unsigned char)(v->c[0] * k);
        sg[i] = (unsigned char)(v->c[1] * k);
        sb[i] = (unsigned char)(v->c[2] * k);
    }
}

static void draw_model(GSGLOBAL *gs)
{
    int t, batch = 0;

    for (t = 0; t < ntris; t++) {
        unsigned int a = idx[t * 3], b = idx[t * 3 + 1], c = idx[t * 3 + 2];
        float area;

        if (!svis[a] || !svis[b] || !svis[c]) continue;

        /* culling de caras traseras (frente = antihorario en el modelo) */
        area = (sx[b] - sx[a]) * (sy[c] - sy[a]) - (sx[c] - sx[a]) * (sy[b] - sy[a]);
        if (area >= 0.0f) continue;

        gsKit_prim_triangle_gouraud_3d(gs,
            sx[a], sy[a], sz[a], sx[b], sy[b], sz[b], sx[c], sy[c], sz[c],
            GS_SETREG_RGBAQ(sr[a], sg[a], sb[a], 0x80, 0x00),
            GS_SETREG_RGBAQ(sr[b], sg[b], sb[b], 0x80, 0x00),
            GS_SETREG_RGBAQ(sr[c], sg[c], sb[c], 0x80, 0x00));

        if (++batch >= BATCH_TRIS) { gsKit_queue_exec(gs); batch = 0; }
    }
}

int main(void)
{
    GSGLOBAL *gs;
    float yaw = 0.0f;

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gs = gsKit_init_global();
    gs->PSM = GS_PSM_CT24;
    gs->PSMZ = GS_PSMZ_24;
    gs->ZBuffering = GS_SETTING_ON;
    gs->DoubleBuffering = GS_SETTING_ON;
    gs->Width = SCREEN_W;
    gs->Height = SCREEN_H;

    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    gsKit_set_test(gs, GS_ZTEST_ON);

    if (!load_model()) {
        printf("Prisma PS2: model.pm invalido o sin memoria\n");
        SleepThread();
    }
    printf("Prisma PS2: %d vertices, %d triangulos\n", nverts, ntris);

    for (;;) {
        gsKit_clear(gs, GS_SETREG_RGBAQ(0x10, 0x14, 0x30, 0x00, 0x00));
        transform(yaw, 0.35f);
        draw_model(gs);
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
        yaw += 0.02f;
    }
    return 0;
}

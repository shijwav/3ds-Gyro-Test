#include <3ds.h>
#include <citro2d.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define TOP_W            400.0f
#define TOP_H            240.0f
#define BALL_R           22.0f
#define PIXELS_PER_DEG   4.0f
#define DEADZONE_DPS     0.6f
#define CALIBRATE_MS     1000
#define DEFAULT_COEFF    14.375f

static const char *AXIS_NAMES[3] = { "X roll ", "Y pitch", "Z yaw  " };

static const int MAPPINGS[6][2] = {
    { 2, 1 },
    { 2, 0 },
    { 0, 1 },
    { 0, 2 },
    { 1, 0 },
    { 1, 2 },
};

static float bias[3];
static float calib_sum[3];
static int calib_count;
static bool calibrating;
static u64 calib_start;

static void start_calibration(u64 now)
{
    calibrating = true;
    calib_start = now;
    calib_count = 0;
    memset(calib_sum, 0, sizeof(calib_sum));
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void draw_snowball(float x, float y)
{
    u32 outline = C2D_Color32(150, 190, 230, 255);
    u32 body    = C2D_Color32(255, 255, 255, 255);
    u32 shade   = C2D_Color32(214, 228, 246, 255);
    u32 shine   = C2D_Color32(255, 255, 255, 255);

    C2D_DrawCircleSolid(x, y, 0.5f, BALL_R + 2.0f, outline);
    C2D_DrawCircleSolid(x, y, 0.5f, BALL_R, shade);
    C2D_DrawCircleSolid(x - 3.0f, y - 3.0f, 0.5f, BALL_R - 4.0f, body);
    C2D_DrawCircleSolid(x - 9.0f, y - 10.0f, 0.5f, 4.0f, shine);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    gfxInitDefault();
    hidInit();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    consoleInit(GFX_BOTTOM, NULL);

    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    u32 sky = C2D_Color32(24, 52, 96, 255);
    u32 ground = C2D_Color32(228, 238, 250, 255);

    Result gyro_rc = HIDUSER_EnableGyroscope();
    Result accel_rc = HIDUSER_EnableAccelerometer();

    float coeff = 0.0f;
    if (R_FAILED(HIDUSER_GetGyroscopeRawToDpsCoefficient(&coeff)) || coeff == 0.0f)
        coeff = DEFAULT_COEFF;
    float scale = (coeff > 1.0f) ? (1.0f / coeff) : coeff;

    int map = 0;
    float flip_h = 1.0f, flip_v = 1.0f;
    float angle_h = 0.0f, angle_v = 0.0f;
    float max_h = (TOP_W / 2.0f - BALL_R) / PIXELS_PER_DEG;
    float max_v = (TOP_H / 2.0f - BALL_R) / PIXELS_PER_DEG;

    u64 last = osGetTime();
    start_calibration(last);
    int frame = 0;

    while (aptMainLoop())
    {
        hidScanInput();
        u32 down = hidKeysDown();
        if (down & KEY_START)
            break;

        u64 now = osGetTime();
        float dt = (float)(now - last) / 1000.0f;
        last = now;
        if (dt < 0.0f || dt > 0.1f)
            dt = 0.016f;

        angularRate g;
        hidGyroRead(&g);
        float raw[3] = { (float)g.x, (float)g.y, (float)g.z };

        accelVector a;
        hidAccelRead(&a);

        if (down & KEY_A)
        {
            angle_h = 0.0f;
            angle_v = 0.0f;
        }
        if ((down & KEY_B) && !calibrating)
            start_calibration(now);
        if (down & KEY_X)
            map = (map + 1) % 6;
        if (down & KEY_Y)
            flip_h = -flip_h;
        if (down & KEY_R)
            flip_v = -flip_v;

        float dps[3] = { 0.0f, 0.0f, 0.0f };

        if (calibrating)
        {
            for (int i = 0; i < 3; i++)
                calib_sum[i] += raw[i];
            calib_count++;
            if (now - calib_start >= CALIBRATE_MS)
            {
                if (calib_count > 0)
                {
                    for (int i = 0; i < 3; i++)
                        bias[i] = calib_sum[i] / (float)calib_count;
                }
                calibrating = false;
                angle_h = 0.0f;
                angle_v = 0.0f;
            }
        }
        else
        {
            for (int i = 0; i < 3; i++)
            {
                dps[i] = (raw[i] - bias[i]) * scale;
                if (fabsf(dps[i]) < DEADZONE_DPS)
                    dps[i] = 0.0f;
            }
            angle_h += dps[MAPPINGS[map][0]] * flip_h * dt;
            angle_v += dps[MAPPINGS[map][1]] * flip_v * dt;
            angle_h = clampf(angle_h, -max_h, max_h);
            angle_v = clampf(angle_v, -max_v, max_v);
        }

        float bx = TOP_W / 2.0f + angle_h * PIXELS_PER_DEG;
        float by = TOP_H / 2.0f + angle_v * PIXELS_PER_DEG;

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top, sky);
        C2D_SceneBegin(top);
        C2D_DrawRectSolid(0.0f, TOP_H - 40.0f, 0.0f, TOP_W, 40.0f, ground);
        draw_snowball(bx, by);
        C3D_FrameEnd(0);

        if (frame++ % 4 == 0)
        {
            printf("\x1b[0;0H");
            printf("SNOWBALL GYRO TEST            \n");
            printf("----------------------------------------\n");
            printf("gyro enable: %-4s accel enable: %-4s\n", R_SUCCEEDED(gyro_rc) ? "ok" : "FAIL", R_SUCCEEDED(accel_rc) ? "ok" : "FAIL");
            printf("coeff %.3f  scale %.5f          \n", coeff, scale);
            printf("\n");
            printf("GYRO raw   x %6d y %6d z %6d\n", g.x, g.y, g.z);
            printf("GYRO dps   x %6.1f y %6.1f z %6.1f\n", dps[0], dps[1], dps[2]);
            printf("ACCEL raw  x %6d y %6d z %6d\n", a.x, a.y, a.z);
            printf("\n");
            printf("horizontal: %s   \n", AXIS_NAMES[MAPPINGS[map][0]]);
            printf("vertical:   %s   \n", AXIS_NAMES[MAPPINGS[map][1]]);
            printf("flip H %s   flip V %s   \n", flip_h > 0 ? "no " : "yes", flip_v > 0 ? "no " : "yes");
            printf("angle  h %6.1f  v %6.1f deg    \n", angle_h, angle_v);
            printf("\n");
            if (calibrating)
                printf("HOLD STILL - calibrating...          \n");
            else
                printf("bias   %6.1f %6.1f %6.1f      \n", bias[0], bias[1], bias[2]);
            printf("\n");
            printf("A    recenter ball\n");
            printf("B    recalibrate (hold still)\n");
            printf("X    change which axes move it\n");
            printf("Y    flip left/right\n");
            printf("R    flip up/down\n");
            printf("START exit\n");
        }
    }

    HIDUSER_DisableGyroscope();
    HIDUSER_DisableAccelerometer();
    C2D_Fini();
    C3D_Fini();
    hidExit();
    gfxExit();
    return 0;
}

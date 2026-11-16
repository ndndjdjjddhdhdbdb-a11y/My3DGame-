#include "raymob.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOUCHES 10
#define MAX_BARRELS 14

static Color C(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return (Color){r, g, b, a};
}


#define SETTINGS_STORAGE_VERSION 1
#define SETTINGS_STORAGE_SLOTS 129

static const char *GetSettingsFilePath(void)
{
    return TextFormat("%sbolt_survival_settings.bin", GetApplicationDirectory());
}

static int LoadStorageValue(int position)
{
    if (position < 0 || position >= SETTINGS_STORAGE_SLOTS) return 0;

    int dataSize = 0;
    unsigned char *data = LoadFileData(GetSettingsFilePath(), &dataSize);
    if (data == NULL || dataSize != (int)(sizeof(int)*SETTINGS_STORAGE_SLOTS))
    {
        if (data != NULL) UnloadFileData(data);
        return 0;
    }

    int value = 0;
    memcpy(&value, data + position*sizeof(int), sizeof(value));
    UnloadFileData(data);
    return value;
}

static void SaveStorageValue(int position, int value)
{
    if (position < 0 || position >= SETTINGS_STORAGE_SLOTS) return;

    int values[SETTINGS_STORAGE_SLOTS] = {0};
    int dataSize = 0;
    unsigned char *data = LoadFileData(GetSettingsFilePath(), &dataSize);
    if (data != NULL && dataSize == (int)sizeof(values))
        memcpy(values, data, sizeof(values));
    if (data != NULL) UnloadFileData(data);

    values[position] = value;
    SaveFileData(GetSettingsFilePath(), values, sizeof(values));
}

static void SaveGameSettings(Vector2 joystickPosition, float joystickRadius,
                             Rectangle settingsButton, Rectangle cameraButton,
                             Vector2 actionCenters[3], float actionScale[3],
                             int actionAlpha[3], float sensitivity,
                             int quality, int fps, int details,
                             int resolution, int chunks)
{
    SaveStorageValue(100, SETTINGS_STORAGE_VERSION);
    SaveStorageValue(101, (int)(joystickPosition.x*10.0f));
    SaveStorageValue(102, (int)(joystickPosition.y*10.0f));
    SaveStorageValue(103, (int)(joystickRadius*10.0f));
    SaveStorageValue(104, (int)(settingsButton.x*10.0f));
    SaveStorageValue(105, (int)(settingsButton.y*10.0f));
    SaveStorageValue(106, (int)(cameraButton.x*10.0f));
    SaveStorageValue(107, (int)(cameraButton.y*10.0f));
    for (int i = 0; i < 3; i++)
    {
        SaveStorageValue(110+i*4, (int)(actionCenters[i].x*10.0f));
        SaveStorageValue(111+i*4, (int)(actionCenters[i].y*10.0f));
        SaveStorageValue(112+i*4, (int)(actionScale[i]*1000.0f));
        SaveStorageValue(113+i*4, actionAlpha[i]);
    }
    SaveStorageValue(123, (int)(sensitivity*100.0f));
    SaveStorageValue(124, quality);
    SaveStorageValue(125, fps);
    SaveStorageValue(126, details);
    SaveStorageValue(127, resolution);
    SaveStorageValue(128, chunks);
}

static void LoadGameSettings(Vector2 *joystickPosition, float *joystickRadius,
                             Rectangle *settingsButton, Rectangle *cameraButton,
                             Vector2 actionCenters[3], float actionScale[3],
                             int actionAlpha[3], float *sensitivity,
                             int *quality, int *fps, int *details,
                             int *resolution, int *chunks)
{
    if (LoadStorageValue(100) != SETTINGS_STORAGE_VERSION) return;

    joystickPosition->x = LoadStorageValue(101)/10.0f;
    joystickPosition->y = LoadStorageValue(102)/10.0f;
    *joystickRadius = LoadStorageValue(103)/10.0f;
    settingsButton->x = LoadStorageValue(104)/10.0f;
    settingsButton->y = LoadStorageValue(105)/10.0f;
    cameraButton->x = LoadStorageValue(106)/10.0f;
    cameraButton->y = LoadStorageValue(107)/10.0f;
    for (int i = 0; i < 3; i++)
    {
        actionCenters[i].x = LoadStorageValue(110+i*4)/10.0f;
        actionCenters[i].y = LoadStorageValue(111+i*4)/10.0f;
        actionScale[i] = LoadStorageValue(112+i*4)/1000.0f;
        actionAlpha[i] = LoadStorageValue(113+i*4);
    }
    *sensitivity = LoadStorageValue(123)/100.0f;
    *quality = LoadStorageValue(124);
    *fps = LoadStorageValue(125);
    *details = LoadStorageValue(126);
    *resolution = LoadStorageValue(127);
    *chunks = LoadStorageValue(128);
}


static void DrawSurvivalButton(Rectangle r, const char *label, int fontSize, bool selected)
{
    Color fill = selected ? C(39, 83, 119, 250) : C(17, 31, 55, 248);
    Color edge = selected ? C(157, 225, 255, 255) : C(105, 157, 202, 235);
    DrawRectangleRounded((Rectangle){r.x+3, r.y+5, r.width, r.height}, 0.14f, 10, C(0, 0, 0, 115));
    DrawRectangleRounded(r, 0.14f, 10, fill);
    DrawRectangleRounded((Rectangle){r.x+4, r.y+4, r.width-8, 3}, 0.5f, 4, C(180, 224, 255, selected ? 115 : 55));
    DrawRectangleRoundedLinesEx(r, 0.14f, 10, selected ? 2.6f : 1.7f, edge);
    DrawRectangle((int)(r.x+9), (int)(r.y+8), 4, (int)(r.height-16), edge);
    int tw = MeasureText(label, fontSize);
    DrawText(label, (int)(r.x+r.width/2-tw/2), (int)(r.y+r.height/2-fontSize/2), fontSize, C(231, 244, 255, 255));
}

static void DrawSurvivalButtonAlpha(Rectangle r, const char *label, int fontSize,
                                    bool selected, unsigned char alpha)
{
    Color fill = selected ? C(39, 83, 119, alpha) : C(17, 31, 55, alpha);
    Color edge = selected ? C(157, 225, 255, alpha) : C(105, 157, 202, alpha);
    DrawRectangleRounded((Rectangle){r.x+3, r.y+5, r.width, r.height}, 0.14f, 10, C(0, 0, 0, (unsigned char)(alpha*0.45f)));
    DrawRectangleRounded(r, 0.14f, 10, fill);
    DrawRectangleRoundedLinesEx(r, 0.14f, 10, selected ? 2.6f : 1.7f, edge);
    int tw = MeasureText(label, fontSize);
    DrawText(label, (int)(r.x+r.width/2-tw/2), (int)(r.y+r.height/2-fontSize/2), fontSize, C(231, 244, 255, alpha));
}

static bool IsSettingsButtonHit(Vector2 point, Rectangle button)
{
    Rectangle hitArea = {
        button.x - 65,
        button.y - 32,
        button.width + 130,
        button.height + 32
    };
    return CheckCollisionPointRec(point, hitArea);
}

static void DrawTree(float x, float z, float scale, int quality)
{
    DrawCylinder((Vector3){x, 0.85f*scale, z},
                 0.18f*scale, 0.23f*scale, 1.7f*scale,
                 8, C(112, 75, 45, 255));

    DrawSphere((Vector3){x, 2.0f*scale, z},
               0.95f*scale, C(44, 132, 69, 255));

    if (quality >= 1)
        DrawSphere((Vector3){x-0.48f*scale, 1.85f*scale, z+0.08f*scale},
                   0.62f*scale, C(57, 153, 77, 255));

    if (quality >= 2)
        DrawSphere((Vector3){x+0.48f*scale, 1.88f*scale, z-0.08f*scale},
                   0.62f*scale, C(35, 117, 61, 255));

    // Светлые верхушки кроны добавляют объём деревьям.
    if (quality >= 1)
        DrawSphere((Vector3){x-0.16f*scale, 2.42f*scale, z-0.20f*scale},
                   0.34f*scale, C(83, 166, 91, 255));

    // Небольшие корни у основания ствола.
    DrawCylinderEx(
        (Vector3){x, 0.20f*scale, z},
        (Vector3){x-0.34f*scale, 0.04f*scale, z+0.12f*scale},
        0.10f*scale, 0.035f*scale, 6, C(99, 69, 43, 255)
    );
    DrawCylinderEx(
        (Vector3){x, 0.20f*scale, z},
        (Vector3){x+0.30f*scale, 0.04f*scale, z-0.14f*scale},
        0.10f*scale, 0.035f*scale, 6, C(99, 69, 43, 255)
    );
}

static void DrawRustHouse(float x, float z, float scale, int variant)
{
    float w = 5.2f*scale;
    float h = 3.0f*scale;
    float d = 4.6f*scale;

    Color wall = (variant % 3 == 0) ? C(112, 91, 69, 255) :
                 (variant % 3 == 1) ? C(132, 105, 76, 255) :
                                      C(91, 99, 91, 255);

    Color roof = (variant % 2 == 0) ? C(91, 83, 73, 255) :
                                      C(105, 78, 62, 255);

    DrawCube((Vector3){x, h*0.5f, z}, w, h, d, wall);

    DrawCube((Vector3){x, h+0.18f*scale, z},
             w+0.55f*scale, 0.38f*scale,
             d+0.55f*scale, roof);

    DrawCube((Vector3){x, h+0.48f*scale, z},
             w*0.72f, 0.28f*scale,
             d+0.25f*scale, C(76, 72, 66, 255));

    DrawCube((Vector3){x, 1.05f*scale, z-d*0.505f},
             0.85f*scale, 2.1f*scale, 0.10f*scale,
             C(58, 48, 39, 255));

    DrawCube((Vector3){x-w*0.30f, 1.85f*scale, z-d*0.515f},
             0.78f*scale, 0.72f*scale, 0.12f*scale,
             C(72, 112, 122, 255));

    DrawCube((Vector3){x+w*0.30f, 1.85f*scale, z-d*0.515f},
             0.78f*scale, 0.72f*scale, 0.12f*scale,
             C(72, 112, 122, 255));

    DrawCube((Vector3){x-w*0.18f, 1.5f*scale, z+d*0.515f},
             0.12f*scale, 2.8f*scale, 0.10f*scale,
             C(65, 54, 43, 255));

    DrawCube((Vector3){x+w*0.18f, 1.5f*scale, z+d*0.515f},
             0.12f*scale, 2.8f*scale, 0.10f*scale,
             C(65, 54, 43, 255));

    DrawCube((Vector3){x+w*0.30f, h+0.75f*scale, z},
             0.42f*scale, 1.2f*scale, 0.42f*scale,
             C(69, 65, 59, 255));

    // Рамы окон и перекладины делают фасады заметнее.
    DrawCube((Vector3){x-w*0.30f-0.42f*scale, 1.85f*scale, z-d*0.545f},
             0.055f*scale, 0.84f*scale, 0.045f*scale,
             C(75, 57, 40, 255));
    DrawCube((Vector3){x-w*0.30f+0.42f*scale, 1.85f*scale, z-d*0.545f},
             0.055f*scale, 0.84f*scale, 0.045f*scale,
             C(75, 57, 40, 255));
    DrawCube((Vector3){x-w*0.30f, 1.85f*scale-0.39f*scale, z-d*0.545f},
             0.90f*scale, 0.055f*scale, 0.045f*scale,
             C(75, 57, 40, 255));
    DrawCube((Vector3){x-w*0.30f, 1.85f*scale+0.39f*scale, z-d*0.545f},
             0.90f*scale, 0.055f*scale, 0.045f*scale,
             C(75, 57, 40, 255));
    DrawCube((Vector3){x-w*0.30f, 1.85f*scale, z-d*0.565f},
             0.055f*scale, 0.72f*scale, 0.025f*scale,
             C(185, 190, 171, 255));
    DrawCube((Vector3){x-w*0.30f, 1.85f*scale, z-d*0.565f},
             0.78f*scale, 0.055f*scale, 0.025f*scale,
             C(185, 190, 171, 255));

    DrawCube((Vector3){x+w*0.30f-0.42f*scale, 1.85f*scale, z-d*0.545f},
             0.055f*scale, 0.84f*scale, 0.045f*scale,
             C(75, 57, 40, 255));
    DrawCube((Vector3){x+w*0.30f+0.42f*scale, 1.85f*scale, z-d*0.545f},
             0.055f*scale, 0.84f*scale, 0.045f*scale,
             C(75, 57, 40, 255));
    DrawCube((Vector3){x+w*0.30f, 1.85f*scale-0.39f*scale, z-d*0.545f},
             0.90f*scale, 0.055f*scale, 0.045f*scale,
             C(75, 57, 40, 255));
    DrawCube((Vector3){x+w*0.30f, 1.85f*scale+0.39f*scale, z-d*0.545f},
             0.90f*scale, 0.055f*scale, 0.045f*scale,
             C(75, 57, 40, 255));
    DrawCube((Vector3){x+w*0.30f, 1.85f*scale, z-d*0.565f},
             0.055f*scale, 0.72f*scale, 0.025f*scale,
             C(185, 190, 171, 255));
    DrawCube((Vector3){x+w*0.30f, 1.85f*scale, z-d*0.565f},
             0.78f*scale, 0.055f*scale, 0.025f*scale,
             C(185, 190, 171, 255));

    // Ручка двери и деревянная ступенька.
    DrawSphere((Vector3){x+0.23f*scale, 1.05f*scale, z-d*0.545f},
               0.055f*scale, C(205, 166, 83, 255));
    DrawCube((Vector3){x, 0.08f*scale, z-d*0.58f},
             1.25f*scale, 0.16f*scale, 0.42f*scale,
             C(91, 72, 52, 255));
}

static bool IsVillageClear(float x, float z)
{
    if (fabsf(x) < 7.0f || fabsf(z) < 7.0f) return false;

    if (fabsf(z + 120.0f) < 5.0f ||
        fabsf(z - 120.0f) < 5.0f) return false;

    for (int i = -3; i <= 3; i++)
    {
        float hz = i*55.0f;

        if (fabsf(z-hz) < 12.0f &&
            (fabsf(x-18.0f) < 13.0f ||
             fabsf(x+18.0f) < 13.0f))
            return false;
    }

    return true;
}

static void DrawRock(float x, float y, float z, float size)
{
    DrawSphere((Vector3){x, y, z},
               size, C(112, 121, 119, 255));

    DrawSphere((Vector3){x-size*0.42f, y+size*0.2f, z-size*0.1f},
               size*0.52f, C(145, 151, 145, 255));
}

static void DrawGrassPatch(float x, float z, float size, int seed)
{
    Color grassA = C(55, 125, 62, 235);
    Color grassB = C(76, 151, 72, 230);
    for (int i = 0; i < 3; i++)
    {
        float offset = ((seed*(i+3))%7 - 3)*0.035f*size;
        DrawCylinderEx(
            (Vector3){x+offset, 0.02f, z+(i-1)*0.10f*size},
            (Vector3){x+offset+0.07f*size, 0.22f*size, z+(i-1)*0.10f*size},
            0.018f*size, 0.008f*size, 4,
            (i == 1) ? grassB : grassA
        );
    }
}

static void DrawPlayerShadow(Vector3 player)
{
    DrawCylinder(
        (Vector3){player.x, 0.025f, player.z},
        0.48f, 0.48f, 0.025f, 16,
        C(25, 42, 28, 90)
    );
}

static void DrawLootBarrel(Vector3 p)
{
    DrawCylinder((Vector3){p.x, 0.48f, p.z},
                 0.39f, 0.39f, 0.88f, 12, C(126, 78, 42, 255));
    DrawCylinder((Vector3){p.x, 0.48f, p.z},
                 0.405f, 0.405f, 0.12f, 12, C(91, 58, 37, 255));
    DrawCylinder((Vector3){p.x, 0.20f, p.z},
                 0.415f, 0.415f, 0.075f, 12, C(85, 91, 98, 255));
    DrawCylinder((Vector3){p.x, 0.76f, p.z},
                 0.415f, 0.415f, 0.075f, 12, C(85, 91, 98, 255));
    DrawCylinder((Vector3){p.x, 0.94f, p.z},
                 0.38f, 0.38f, 0.08f, 12, C(105, 67, 39, 255));
}

static bool IsBlockedAt(float x, float z,
                           Vector3 *barrels, bool *barrelDestroyed)
{
    const float playerRadius = 0.42f;

    // Домики основной деревни.
    for (int i = -3; i <= 3; i++)
    {
        float hz = i*55.0f;
        float sc = 0.82f + (float)((i*i+3)%3)*0.08f;

        float houses[2][3] = {
            {-18.0f, hz, sc},
            {18.0f, hz+22.0f, sc*0.92f}
        };

        for (int h = 0; h < 2; h++)
        {
            float halfW = 2.6f*houses[h][2] + playerRadius;
            float halfD = 2.3f*houses[h][2] + playerRadius;

            if (fabsf(x-houses[h][0]) < halfW &&
                fabsf(z-houses[h][1]) < halfD)
                return true;
        }
    }

    // Дополнительные дома.
    const float extra[3][2] = {
        {-42.0f, -120.0f}, {48.0f, 120.0f}, {42.0f, -120.0f}
    };
    for (int h = 0; h < 3; h++)
    {
        float sc = (h == 1) ? 0.84f : 0.80f;
        if (fabsf(x-extra[h][0]) < 2.6f*sc+playerRadius &&
            fabsf(z-extra[h][1]) < 2.3f*sc+playerRadius)
            return true;
    }

    // Бочки.
    for (int b = 0; b < MAX_BARRELS; b++)
    {
        if (barrelDestroyed[b]) continue;
        float dx = x-barrels[b].x;
        float dz = z-barrels[b].z;
        float minDist = 0.39f + playerRadius;
        if (dx*dx + dz*dz < minDist*minDist)
            return true;
    }

    return false;
}

static Vector3 PlayerPoint(Vector3 player, float angle,
                           float x, float y, float z)
{
    float c = cosf(angle);
    float sn = sinf(angle);

    return (Vector3){
        player.x + x*c - z*sn,
        player.y + y,
        player.z + x*sn + z*c
    };
}

static void DrawFirstPersonArms(Vector3 eye, float yaw,
                                float walkPhase, bool moving, float time,
                                float attackProgress)
{
    Vector3 forward = {sinf(yaw), 0.0f, -cosf(yaw)};
    Vector3 right = {cosf(yaw), 0.0f, sinf(yaw)};

    Color shirt = C(40, 112, 205, 255);
    Color skin = C(225, 183, 143, 255);

    for (int side = -1; side <= 1; side += 2)
    {
        float s = (float)side;

        float armSwing = moving ?
            sinf(walkPhase)*s*0.13f :
            sinf(time*1.6f)*0.012f*s;

        float armLift = moving ?
            cosf(walkPhase*2.0f)*0.025f :
            sinf(time*1.6f)*0.012f;

        Vector3 shoulder = {
            eye.x + right.x*s*0.42f + forward.x*(0.18f+armSwing*0.25f),
            eye.y - 0.30f + armLift,
            eye.z + right.z*s*0.42f + forward.z*(0.18f+armSwing*0.25f)
        };

        Vector3 elbow = {
            eye.x + right.x*s*0.48f + forward.x*(0.48f+armSwing*0.65f),
            eye.y - 0.52f + armLift*0.8f,
            eye.z + right.z*s*0.48f + forward.z*(0.48f+armSwing*0.65f)
        };

        float punch = (side == 1) ? attackProgress*0.95f : 0.0f;
        Vector3 wrist = {
            eye.x + right.x*s*0.34f + forward.x*(0.78f+armSwing+punch),
            eye.y - 0.48f + armLift + punch*0.10f,
            eye.z + right.z*s*0.34f + forward.z*(0.78f+armSwing+punch)
        };

        Vector3 hand = {
            eye.x + right.x*s*0.30f + forward.x*(0.88f+armSwing+punch),
            eye.y - 0.45f + armLift + punch*0.10f,
            eye.z + right.z*s*0.30f + forward.z*(0.88f+armSwing+punch)
        };

        DrawCylinderEx(shoulder, elbow, 0.16f, 0.13f, 8, shirt);
        DrawSphere(elbow, 0.13f, shirt);
        DrawCylinderEx(elbow, wrist, 0.105f, 0.085f, 8, skin);
        DrawSphere(hand, 0.105f, skin);
    }
}

static void DrawPlayer(Vector3 player, float angle,
                       float walkPhase, bool moving, float attackProgress)
{
    float swing = moving ? sinf(walkPhase) * 0.34f : 0.0f;

    Color skin = C(225, 183, 143, 255);
    Color shirt = C(40, 112, 205, 255);
    Color pants = C(48, 55, 82, 255);
    Color shoes = C(35, 38, 43, 255);

    Vector3 hipL = PlayerPoint(player, angle, -0.17f, 0.66f, 0.0f);
    Vector3 hipR = PlayerPoint(player, angle,  0.17f, 0.66f, 0.0f);

    Vector3 kneeL = PlayerPoint(player, angle, -0.18f, 0.34f, swing*0.55f);
    Vector3 kneeR = PlayerPoint(player, angle,  0.18f, 0.34f, -swing*0.55f);

    Vector3 footL = PlayerPoint(player, angle, -0.18f, 0.10f, swing);
    Vector3 footR = PlayerPoint(player, angle,  0.18f, 0.10f, -swing);

    DrawCylinderEx(hipL, kneeL, 0.14f, 0.12f, 8, pants);
    DrawCylinderEx(kneeL, footL, 0.11f, 0.10f, 8, pants);
    DrawCylinderEx(hipR, kneeR, 0.14f, 0.12f, 8, pants);
    DrawCylinderEx(kneeR, footR, 0.11f, 0.10f, 8, pants);

    DrawSphere(footL, 0.12f, shoes);
    DrawSphere(footR, 0.12f, shoes);

    // Небольшой походный рюкзак за спиной персонажа.
    DrawCube(
        PlayerPoint(player, angle, 0, 1.12f, 0.235f),
        0.43f, 0.55f, 0.25f, C(48, 82, 65, 255)
    );
    DrawCube(
        PlayerPoint(player, angle, 0, 1.12f, 0.37f),
        0.25f, 0.22f, 0.06f, C(72, 105, 76, 255)
    );

    DrawCylinder(PlayerPoint(player, angle, 0, 1.05f, 0),
                 0.31f, 0.36f, 0.78f, 12, shirt);

    // Передняя панель жилета и пояс.
    DrawCube(
        PlayerPoint(player, angle, 0, 1.12f, -0.185f),
        0.34f, 0.34f, 0.035f, C(31, 91, 132, 255)
    );
    DrawCube(
        PlayerPoint(player, angle, 0, 0.69f, 0),
        0.56f, 0.10f, 0.40f, C(91, 75, 52, 255)
    );
    DrawCube(
        PlayerPoint(player, angle, 0, 0.69f, -0.215f),
        0.13f, 0.11f, 0.035f, C(190, 157, 91, 255)
    );

    DrawCylinder(PlayerPoint(player, angle, 0, 1.48f, 0),
                 0.11f, 0.11f, 0.18f, 8, skin);

    Vector3 shoulderL = PlayerPoint(player, angle, -0.39f, 1.35f, 0);
    Vector3 elbowL = PlayerPoint(player, angle, -0.48f, 0.98f, -swing*0.65f);
    Vector3 handL = PlayerPoint(player, angle, -0.47f, 0.76f, -swing*0.9f);

    Vector3 shoulderR = PlayerPoint(player, angle, 0.39f, 1.35f, 0);
    Vector3 elbowR = PlayerPoint(player, angle, 0.48f, 0.98f, swing*0.65f-attackProgress*0.30f);
    Vector3 handR = PlayerPoint(player, angle, 0.47f, 0.76f, swing*0.9f-attackProgress*0.85f);

    DrawCylinderEx(shoulderL, elbowL, 0.12f, 0.095f, 8, shirt);
    DrawCylinderEx(elbowL, handL, 0.09f, 0.075f, 8, skin);

    DrawCylinderEx(shoulderR, elbowR, 0.12f, 0.095f, 8, shirt);
    DrawCylinderEx(elbowR, handR, 0.09f, 0.075f, 8, skin);

    DrawSphere(handL, 0.085f, skin);
    DrawSphere(handR, 0.085f, skin);

    DrawSphere(PlayerPoint(player, angle, 0, 1.78f, 0),
               0.29f, skin);

    DrawSphere(PlayerPoint(player, angle, 0, 1.94f, 0.015f),
               0.255f, C(65, 43, 30, 255));

    // Небольшой нос вместо крупной лишней лицевой сферы,
    // которая визуально выглядела как второй человечек/голова.
    DrawSphere(PlayerPoint(player, angle, 0, 1.80f, -0.285f),
               0.055f, skin);

    // Походная кепка и козырёк персонажа.
    DrawCube(
        PlayerPoint(player, angle, 0, 2.035f, 0.005f),
        0.49f, 0.12f, 0.48f, C(47, 77, 57, 255)
    );
    DrawCube(
        PlayerPoint(player, angle, 0, 1.99f, -0.22f),
        0.46f, 0.045f, 0.24f, C(35, 61, 45, 255)
    );

    // Ремни рюкзака и небольшие плечевые накладки.
    DrawCube(
        PlayerPoint(player, angle, -0.19f, 1.20f, -0.205f),
        0.075f, 0.48f, 0.045f, C(177, 148, 91, 255)
    );
    DrawCube(
        PlayerPoint(player, angle, 0.19f, 1.20f, -0.205f),
        0.075f, 0.48f, 0.045f, C(177, 148, 91, 255)
    );
    DrawSphere(PlayerPoint(player, angle, -0.39f, 1.34f, -0.015f),
               0.145f, C(52, 91, 76, 255));
    DrawSphere(PlayerPoint(player, angle, 0.39f, 1.34f, -0.015f),
               0.145f, C(52, 91, 76, 255));

    DrawSphere(PlayerPoint(player, angle, -0.095f, 1.83f, -0.365f),
               0.035f, C(30, 35, 42, 255));

    DrawSphere(PlayerPoint(player, angle, 0.095f, 1.83f, -0.365f),
               0.035f, C(30, 35, 42, 255));
}

static void DrawCloud(float x, float y, float z, float s)
{
    Color cloud = C(245, 250, 255, 255);

    DrawSphere((Vector3){x, y, z}, s, cloud);
    DrawSphere((Vector3){x+s*0.85f, y+0.12f*s, z}, s*0.72f, cloud);
    DrawSphere((Vector3){x-s*0.8f, y+0.05f*s, z}, s*0.65f, cloud);
}

int main(void)
{
    InitWindow(0, 0, "boltSurvival");
    SetTargetFPS(60);

    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    Vector3 player = {0, 0, 0};

    float playerAngle = 0.0f;
    float cameraYaw = 0.0f;
    float cameraPitch = 0.0f;
    float lookSensitivity = 3.0f;
    float playerSpeed = 0.08f;
    float walkPhase = 0.0f;

    // Система выживания: запас еды и воды постепенно уменьшается.
    float foodLevel = 100.0f;
    float waterLevel = 100.0f;
    const float foodDrainRate = 0.035f;
    const float waterDrainRate = 0.055f;

    bool playerMoving = false;
    bool firstPerson = false;
    bool settingsOpen = false;
    bool editControls = false;
    bool sprintEnabled = false;
    bool inventoryOpen = false;
    int selectedHotbarSlot = 0;
    // 0 AXE, 1 MED, 2 FOOD, 3 WATER, 4 WOOD, 5 STONE.
    const char *itemNames[6] = {"AXE", "MED", "FOOD", "WATER", "WOOD", "STONE"};
    int itemCounts[6] = {1, 1, 3, 3, 0, 0};
    const int hotbarItemIds[4] = {0, 1, 2, 3};

    // Главное меню.
    bool mainMenuOpen = true;
    int menuPage = 0;

    Rectangle menuPlayButton;
    Rectangle menuServersButton;
    Rectangle menuShopButton;
    Rectangle menuSettingsButton;
    Rectangle menuBackButton;
    Rectangle menuReturnButton;

    int graphicsQuality = 0;
    int targetFps = 30;
    int detailLevel = 2;
    int resolutionLevel = 2;

    int chunkDistance = 4;

    SetTargetFPS(30);

    int renderWidth = (int)(screenWidth * 1.0f);
    int renderHeight = (int)(screenHeight * 1.0f);

    if (renderWidth < 320) renderWidth = 320;
    if (renderHeight < 240) renderHeight = 240;

    RenderTexture2D sceneTexture =
        LoadRenderTexture(renderWidth, renderHeight);

    SetTextureFilter(sceneTexture.texture, TEXTURE_FILTER_BILINEAR);

    Camera3D camera = {0};
    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 65.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float joystickRadius = 120.0f;
    float knobRadius = 50.0f;

    Vector2 joystickPosition = {150, screenHeight - 160};
    Vector2 joystickKnob = joystickPosition;

    bool joystickActive = false;
    int joystickTouchId = -1;

    Rectangle settingsButton = {screenWidth - 190, 18, 170, 64};
    Rectangle cameraButton = {screenWidth - 190, 92, 170, 64};

    float panelWidth = fminf(540, screenWidth - 30);
    float panelHeight = 650;

    float panelX = (screenWidth - panelWidth)/2;
    float panelY = (screenHeight - panelHeight)/2;

    Rectangle panel, moveButtonsButton, sizeButton, slider;
    Rectangle lowButton, mediumButton, highButton;
    Rectangle chunkButton;

    Rectangle fpsButtons[5];
    const int fpsValues[5] = {30, 60, 90, 120, 144};

    Rectangle detailMoreButton, detailSomeButton, detailFewButton;
    Rectangle resolutionFullButton, resolution85Button, resolution70Button;

    Rectangle resetButton, closeButton, doneButton;

    int dragTarget = 0;
    int dragTouchId = -1;
    Vector2 dragOffset = {0, 0};

    int lookTouchId = -1;
    Vector2 previousLookPosition = {0, 0};

    bool draggingSlider = false;
    int sliderTouchId = -1;

    int previousTouchIds[MAX_TOUCHES];

    for (int i = 0; i < MAX_TOUCHES; i++)
        previousTouchIds[i] = -1;

    Vector3 barrelPositions[MAX_BARRELS];
    int barrelHealth[MAX_BARRELS];
    bool barrelDestroyed[MAX_BARRELS] = { false };
    int targetedBarrel = -1;
    Rectangle interactButton = {0};
    Rectangle sprintButton = {0};
    Rectangle inventoryButton = {0};
    Rectangle hotbarSlots[4] = {{0}};

    // Настраиваемые положения, размеры и прозрачность игровых кнопок.
    Vector2 actionButtonCenters[3] = {
        {0, 0}, {0, 0}, {0, 0}
    };
    float actionButtonScale[3] = {1.0f, 1.0f, 1.0f};
    int actionButtonAlpha[3] = {245, 245, 245};
    int selectedActionControl = 0;
    Rectangle editSelectButtons[3];
    Rectangle editSizeMinus, editSizePlus, editAlphaMinus, editAlphaPlus;
    bool attackRequested = false;
    float attackCooldown = 0.0f;
    const float attackCooldownDuration = 1.0f;
    float attackAnimation = 0.0f;

    LoadGameSettings(&joystickPosition, &joystickRadius,
                     &settingsButton, &cameraButton,
                     actionButtonCenters, actionButtonScale,
                     actionButtonAlpha, &lookSensitivity,
                     &graphicsQuality, &targetFps, &detailLevel,
                     &resolutionLevel, &chunkDistance);
    knobRadius = joystickRadius*0.42f;
    if (targetFps < 30 || targetFps > 144) targetFps = 30;
    SetTargetFPS(targetFps);

    for (int i = 0; i < 7; i++)
    {
        float hz = (i-3)*55.0f;
        barrelPositions[i*2] = (Vector3){-13.5f, 0.0f, hz+3.4f};
        barrelPositions[i*2+1] = (Vector3){13.5f, 0.0f, hz+25.4f};
        barrelHealth[i*2] = 50;
        barrelHealth[i*2+1] = 50;
    }

    while (!WindowShouldClose())
    {
        float frameTime = GetFrameTime();
        if (attackCooldown > 0.0f) attackCooldown -= frameTime;
        if (attackCooldown < 0.0f) attackCooldown = 0.0f;
        if (attackAnimation > 0.0f) attackAnimation -= frameTime;
        if (attackAnimation < 0.0f) attackAnimation = 0.0f;

        if (!mainMenuOpen && !settingsOpen && !editControls)
        {
            foodLevel -= foodDrainRate*frameTime;
            waterLevel -= waterDrainRate*frameTime;
            if (foodLevel < 0.0f) foodLevel = 0.0f;
            if (waterLevel < 0.0f) waterLevel = 0.0f;
        }
        attackRequested = false;
        screenWidth = GetScreenWidth();
        screenHeight = GetScreenHeight();

        panelWidth = fminf(540, screenWidth - 30);
        panelHeight = fminf(650, screenHeight - 20);

        panelX = (screenWidth - panelWidth)/2;
        panelY = (screenHeight - panelHeight)/2;

        if (settingsButton.x + settingsButton.width > screenWidth)
            settingsButton.x = screenWidth-settingsButton.width-12;

        if (settingsButton.x < 8) settingsButton.x = 8;
        if (settingsButton.y < 8) settingsButton.y = 8;

        if (settingsButton.y + settingsButton.height > screenHeight)
            settingsButton.y = screenHeight-settingsButton.height-8;

        if (cameraButton.x + cameraButton.width > screenWidth)
            cameraButton.x = screenWidth-cameraButton.width-12;

        if (cameraButton.x < 8) cameraButton.x = 8;
        if (cameraButton.y < 8) cameraButton.y = 8;

        if (cameraButton.y + cameraButton.height > screenHeight)
            cameraButton.y = screenHeight-cameraButton.height-8;

        panel = (Rectangle){panelX, panelY, panelWidth, panelHeight};

        // Компактная сетка главного меню в стиле sci-fi панели.
        float menuWidth = fminf(390.0f, screenWidth - 40.0f);
        float menuX = (screenWidth - menuWidth) / 2.0f;
        float menuGap = 10.0f;
        float menuButtonWidth = (menuWidth-menuGap)/2.0f;
        float menuButtonHeight = fminf(58.0f, fmaxf(42.0f, screenHeight*0.15f));
        float menuGridHeight = menuButtonHeight*2.0f+menuGap;
        float menuStartY = fmaxf(128.0f, (screenHeight+90.0f-menuGridHeight)/2.0f);

        menuPlayButton = (Rectangle){menuX, menuStartY, menuButtonWidth, menuButtonHeight};
        menuServersButton = (Rectangle){menuX+menuButtonWidth+menuGap, menuStartY, menuButtonWidth, menuButtonHeight};
        menuShopButton = (Rectangle){menuX, menuStartY+menuButtonHeight+menuGap, menuButtonWidth, menuButtonHeight};
        menuSettingsButton = (Rectangle){menuX+menuButtonWidth+menuGap, menuStartY+menuButtonHeight+menuGap, menuButtonWidth, menuButtonHeight};

        menuBackButton = menuShopButton;

        menuReturnButton = (Rectangle){
            18, 18, 135, 55
        };

        moveButtonsButton = (Rectangle){
            panelX+20, panelY+75, panelWidth-40, 44
        };

        sizeButton = (Rectangle){
            panelX+20, panelY+128, panelWidth-40, 44
        };

        float qualityGap = 8;
        float qualityW = (panelWidth-40-qualityGap*2)/3;

        lowButton = (Rectangle){
            panelX+20, panelY+184, qualityW, 46
        };

        mediumButton = (Rectangle){
            panelX+20+qualityW+qualityGap, panelY+184, qualityW, 46
        };

        highButton = (Rectangle){
            panelX+20+(qualityW+qualityGap)*2, panelY+184, qualityW, 46
        };

        float fpsGap = 5;
        float fpsW = (panelWidth-40-fpsGap*4)/5;

        for (int fi = 0; fi < 5; fi++)
        {
            fpsButtons[fi] = (Rectangle){
                panelX+20+fi*(fpsW+fpsGap), panelY+260, fpsW, 42
            };
        }

        detailMoreButton = (Rectangle){
            panelX+20, panelY+340, qualityW, 42
        };

        detailSomeButton = (Rectangle){
            panelX+20+qualityW+qualityGap, panelY+340, qualityW, 42
        };

        detailFewButton = (Rectangle){
            panelX+20+(qualityW+qualityGap)*2, panelY+340, qualityW, 42
        };

        resolutionFullButton = (Rectangle){
            panelX+20, panelY+420, qualityW, 42
        };

        resolution85Button = (Rectangle){
            panelX+20+qualityW+qualityGap, panelY+420, qualityW, 42
        };

        resolution70Button = (Rectangle){
            panelX+20+(qualityW+qualityGap)*2, panelY+420, qualityW, 42
        };

        chunkButton = (Rectangle){
            panelX+20, panelY+465, panelWidth-40, 38
        };

        slider = (Rectangle){
            panelX+40, panelY+540, panelWidth-80, 12
        };

        resetButton = (Rectangle){
            panelX+20, panelY+590, (panelWidth-48)/2, 42
        };

        closeButton = (Rectangle){
            panelX+28+(panelWidth-48)/2, panelY+590,
            (panelWidth-48)/2, 42
        };

        doneButton = (Rectangle){
            screenWidth/2-105, screenHeight-100, 210, 62
        };

        if (actionButtonCenters[0].x == 0)
        {
            actionButtonCenters[0] = (Vector2){screenWidth-105.0f, screenHeight-119.0f};
            actionButtonCenters[1] = (Vector2){screenWidth-105.0f, screenHeight-199.0f};
            actionButtonCenters[2] = (Vector2){screenWidth-38.0f, screenHeight-56.0f};
        }

        float hitW = 170.0f*actionButtonScale[0];
        float hitH = 62.0f*actionButtonScale[0];
        float runW = 170.0f*actionButtonScale[1];
        float runH = 62.0f*actionButtonScale[1];
        float bagW = 52.0f*actionButtonScale[2];
        float bagH = 52.0f*actionButtonScale[2];

        for (int control = 0; control < 3; control++)
        {
            float halfW = (control == 2 ? bagW : (control == 0 ? hitW : runW))*0.5f;
            float halfH = (control == 2 ? bagH : (control == 0 ? hitH : runH))*0.5f;
            if (actionButtonCenters[control].x < halfW+5) actionButtonCenters[control].x = halfW+5;
            if (actionButtonCenters[control].x > screenWidth-halfW-5) actionButtonCenters[control].x = screenWidth-halfW-5;
            if (actionButtonCenters[control].y < halfH+5) actionButtonCenters[control].y = halfH+5;
            if (actionButtonCenters[control].y > screenHeight-halfH-5) actionButtonCenters[control].y = screenHeight-halfH-5;
        }

        interactButton = (Rectangle){actionButtonCenters[0].x-hitW/2, actionButtonCenters[0].y-hitH/2, hitW, hitH};
        sprintButton = (Rectangle){actionButtonCenters[1].x-runW/2, actionButtonCenters[1].y-runH/2, runW, runH};
        inventoryButton = (Rectangle){actionButtonCenters[2].x-bagW/2, actionButtonCenters[2].y-bagH/2, bagW, bagH};

        float editSelectW = (screenWidth-40.0f)/3.0f;
        for (int control = 0; control < 3; control++)
            editSelectButtons[control] = (Rectangle){20.0f+control*editSelectW, 52.0f, editSelectW-5.0f, 38.0f};
        editSizeMinus = (Rectangle){screenWidth/2.0f-112.0f, 102.0f, 92.0f, 38.0f};
        editSizePlus = (Rectangle){screenWidth/2.0f+20.0f, 102.0f, 92.0f, 38.0f};
        editAlphaMinus = (Rectangle){screenWidth/2.0f-112.0f, 148.0f, 92.0f, 38.0f};
        editAlphaPlus = (Rectangle){screenWidth/2.0f+20.0f, 148.0f, 92.0f, 38.0f};
        float slotSize = fminf(54.0f, (screenWidth-165.0f)/4.0f);
        float slotGap = 6.0f;
        float hotbarWidth = slotSize*4.0f + slotGap*3.0f;
        float hotbarX = (screenWidth-hotbarWidth)/2.0f;
        for (int slot = 0; slot < 4; slot++)
            hotbarSlots[slot] = (Rectangle){hotbarX+slot*(slotSize+slotGap),
                                           screenHeight-78.0f, slotSize, 58.0f};

        int touchCount = GetTouchPointCount();

        if (touchCount > MAX_TOUCHES)
            touchCount = MAX_TOUCHES;

        int currentTouchIds[MAX_TOUCHES];

        for (int i = 0; i < MAX_TOUCHES; i++)
            currentTouchIds[i] = -1;

        for (int i = 0; i < touchCount; i++)
        {
            int id = GetTouchPointId(i);
            Vector2 pos = GetTouchPosition(i);

            currentTouchIds[i] = id;

            bool newTouch = true;

            for (int j = 0; j < MAX_TOUCHES; j++)
            {
                if (previousTouchIds[j] == id)
                {
                    newTouch = false;
                    break;
                }
            }

            if (!newTouch) continue;

            if (editControls)
            {
                if (CheckCollisionPointRec(pos, doneButton))
                {
                    editControls = false;
                    dragTarget = 0;
                    dragTouchId = -1;
                    SaveGameSettings(joystickPosition, joystickRadius,
                        settingsButton, cameraButton, actionButtonCenters,
                        actionButtonScale, actionButtonAlpha, lookSensitivity,
                        graphicsQuality, targetFps, detailLevel,
                        resolutionLevel, chunkDistance);
                }
                else if (CheckCollisionPointRec(pos, editSelectButtons[0]))
                    selectedActionControl = 0;
                else if (CheckCollisionPointRec(pos, editSelectButtons[1]))
                    selectedActionControl = 1;
                else if (CheckCollisionPointRec(pos, editSelectButtons[2]))
                    selectedActionControl = 2;
                else if (CheckCollisionPointRec(pos, editSizeMinus))
                    actionButtonScale[selectedActionControl] = fmaxf(0.55f, actionButtonScale[selectedActionControl]-0.10f);
                else if (CheckCollisionPointRec(pos, editSizePlus))
                    actionButtonScale[selectedActionControl] = fminf(1.70f, actionButtonScale[selectedActionControl]+0.10f);
                else if (CheckCollisionPointRec(pos, editAlphaMinus))
                    actionButtonAlpha[selectedActionControl] = (int)fmaxf(60.0f, actionButtonAlpha[selectedActionControl]-20.0f);
                else if (CheckCollisionPointRec(pos, editAlphaPlus))
                    actionButtonAlpha[selectedActionControl] = (int)fminf(255.0f, actionButtonAlpha[selectedActionControl]+20.0f);
                else if (CheckCollisionPointRec(pos, interactButton))
                {
                    dragTarget = 4;
                    dragTouchId = id;
                    dragOffset = (Vector2){actionButtonCenters[0].x-pos.x, actionButtonCenters[0].y-pos.y};
                }
                else if (CheckCollisionPointRec(pos, sprintButton))
                {
                    dragTarget = 5;
                    dragTouchId = id;
                    dragOffset = (Vector2){actionButtonCenters[1].x-pos.x, actionButtonCenters[1].y-pos.y};
                }
                else if (CheckCollisionPointRec(pos, inventoryButton))
                {
                    dragTarget = 6;
                    dragTouchId = id;
                    dragOffset = (Vector2){actionButtonCenters[2].x-pos.x, actionButtonCenters[2].y-pos.y};
                }
                else
                {
                    float dx = pos.x - joystickPosition.x;
                    float dy = pos.y - joystickPosition.y;

                    if (sqrtf(dx*dx+dy*dy) < joystickRadius + 45)
                    {
                        dragTarget = 1;
                        dragTouchId = id;

                        dragOffset = (Vector2){
                            joystickPosition.x-pos.x,
                            joystickPosition.y-pos.y
                        };
                    }
                    else if (IsSettingsButtonHit(pos, settingsButton))
                    {
                        dragTarget = 2;
                        dragTouchId = id;

                        dragOffset = (Vector2){
                            settingsButton.x-pos.x,
                            settingsButton.y-pos.y
                        };
                    }
                    else if (CheckCollisionPointRec(pos, cameraButton))
                    {
                        dragTarget = 3;
                        dragTouchId = id;

                        dragOffset = (Vector2){
                            cameraButton.x-pos.x,
                            cameraButton.y-pos.y
                        };
                    }
                }
            }
            else if (mainMenuOpen && !settingsOpen)
            {
                if (menuPage == 0)
                {
                    if (CheckCollisionPointRec(pos, menuPlayButton))
                    {
                        mainMenuOpen = false;
                    }
                    else if (CheckCollisionPointRec(pos, menuServersButton))
                    {
                        menuPage = 1;
                    }
                    else if (CheckCollisionPointRec(pos, menuShopButton))
                    {
                        menuPage = 2;
                    }
                    else if (CheckCollisionPointRec(pos, menuSettingsButton))
                    {
                        settingsOpen = true;
                    }
                }
                else
                {
                    if (CheckCollisionPointRec(pos, menuBackButton))
                        menuPage = 0;
                }
            }
            else if (!mainMenuOpen && !settingsOpen && !inventoryOpen &&
                     CheckCollisionPointRec(pos, menuReturnButton))
            {
                mainMenuOpen = true;
                menuPage = 0;
                joystickActive = false;
                lookTouchId = -1;
                joystickKnob = joystickPosition;
            }
            else if (!mainMenuOpen &&
                     IsSettingsButtonHit(pos, settingsButton))
            {
                if (settingsOpen)
                    SaveGameSettings(joystickPosition, joystickRadius,
                        settingsButton, cameraButton, actionButtonCenters,
                        actionButtonScale, actionButtonAlpha, lookSensitivity,
                        graphicsQuality, targetFps, detailLevel,
                        resolutionLevel, chunkDistance);
                settingsOpen = !settingsOpen;
                joystickActive = false;
                lookTouchId = -1;
            }
            else if (!mainMenuOpen &&
                     CheckCollisionPointRec(pos, cameraButton))
            {
                firstPerson = !firstPerson;
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen &&
                     CheckCollisionPointRec(pos, sprintButton))
            {
                sprintEnabled = !sprintEnabled;
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls &&
                     CheckCollisionPointRec(pos, inventoryButton))
            {
                inventoryOpen = !inventoryOpen;
                joystickActive = false;
                lookTouchId = -1;
            }
            else if (inventoryOpen)
            {
                // Еду и воду можно использовать касанием карточки в рюкзаке.
                Rectangle invPanel = {screenWidth*0.08f, screenHeight*0.20f,
                                      screenWidth*0.84f, screenHeight*0.60f};
                for (int item = 2; item <= 3; item++)
                {
                    float w = (invPanel.width-50.0f)/2.0f;
                    Rectangle r = {invPanel.x+16+(item%2)*(w+12),
                                   invPanel.y+72+(item/2)*52, w, 44};
                    if (CheckCollisionPointRec(pos, r) && itemCounts[item] > 0)
                    {
                        if (item == 2)
                        {
                            foodLevel += 25.0f;
                            if (foodLevel > 100.0f) foodLevel = 100.0f;
                        }
                        else
                        {
                            waterLevel += 25.0f;
                            if (waterLevel > 100.0f) waterLevel = 100.0f;
                        }
                        itemCounts[item]--;
                        break;
                    }
                }
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen &&
                     CheckCollisionPointRec(pos, interactButton))
            {
                attackRequested = true;
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen)
            {
                // Сначала проверяем хотбар. Если касание не попало в слот,
                // передаём его джойстику или управлению обзором камеры.
                bool touchedHotbar = false;
                for (int slot = 0; slot < 4; slot++)
                {
                    if (CheckCollisionPointRec(pos, hotbarSlots[slot]))
                    {
                        selectedHotbarSlot = slot;
                        touchedHotbar = true;
                        break;
                    }
                }

                if (!touchedHotbar)
                {
                    if (pos.x < screenWidth*0.48f &&
                        pos.y > screenHeight*0.25f)
                    {
                        float dx = pos.x-joystickPosition.x;
                        float dy = pos.y-joystickPosition.y;

                        if (sqrtf(dx*dx+dy*dy) < joystickRadius+150.0f)
                        {
                            joystickActive = true;
                            joystickTouchId = id;
                        }
                    }
                    else if (pos.x >= screenWidth*0.48f && pos.y > 155.0f)
                    {
                        lookTouchId = id;
                        previousLookPosition = pos;
                    }
                }
            }
            else if (settingsOpen)
            {
                if (CheckCollisionPointRec(pos, moveButtonsButton))
                {
                    settingsOpen = false;
                    editControls = true;
                }
                else if (CheckCollisionPointRec(pos, sizeButton))
                {
                    joystickRadius += 15;

                    if (joystickRadius > 180)
                        joystickRadius = 90;

                    knobRadius = joystickRadius*0.42f;
                }
                else if (CheckCollisionPointRec(pos, lowButton))
                {
                    graphicsQuality = 0;
                    detailLevel = 2;
                    resolutionLevel = 2;
                }
                else if (CheckCollisionPointRec(pos, mediumButton))
                {
                    graphicsQuality = 1;
                    detailLevel = 1;
                    resolutionLevel = 1;
                }
                else if (CheckCollisionPointRec(pos, highButton))
                {
                    graphicsQuality = 2;
                }
                else if (CheckCollisionPointRec(pos, fpsButtons[0]))
                {
                    targetFps = 30;
                    SetTargetFPS(targetFps);
                    SaveGameSettings(joystickPosition, joystickRadius,
                        settingsButton, cameraButton, actionButtonCenters,
                        actionButtonScale, actionButtonAlpha, lookSensitivity,
                        graphicsQuality, targetFps, detailLevel,
                        resolutionLevel, chunkDistance);
                }
                else if (CheckCollisionPointRec(pos, fpsButtons[1]))
                {
                    targetFps = 60;
                    SetTargetFPS(targetFps);
                }
                else if (CheckCollisionPointRec(pos, fpsButtons[2]))
                {
                    targetFps = 90;
                    SetTargetFPS(targetFps);
                }
                else if (CheckCollisionPointRec(pos, fpsButtons[3]))
                {
                    targetFps = 120;
                    SetTargetFPS(targetFps);
                }
                else if (CheckCollisionPointRec(pos, fpsButtons[4]))
                {
                    targetFps = 144;
                    SetTargetFPS(targetFps);
                }
                else if (CheckCollisionPointRec(pos, detailMoreButton))
                    detailLevel = 0;
                else if (CheckCollisionPointRec(pos, detailSomeButton))
                    detailLevel = 1;
                else if (CheckCollisionPointRec(pos, detailFewButton))
                    detailLevel = 2;
                else if (CheckCollisionPointRec(pos, resolutionFullButton))
                    resolutionLevel = 0;
                else if (CheckCollisionPointRec(pos, resolution85Button))
                    resolutionLevel = 1;
                else if (CheckCollisionPointRec(pos, resolution70Button))
                    resolutionLevel = 2;
                else if (CheckCollisionPointRec(pos, chunkButton))
                {
                    chunkDistance++;

                    if (chunkDistance > 20)
                        chunkDistance = 2;
                }
                else if (CheckCollisionPointRec(pos, slider))
                {
                    draggingSlider = true;
                    sliderTouchId = id;
                }
                else if (CheckCollisionPointRec(pos, resetButton))
                {
                    joystickRadius = 120;
                    knobRadius = 50;
                    actionButtonScale[0] = actionButtonScale[1] = actionButtonScale[2] = 1.0f;
                    actionButtonAlpha[0] = actionButtonAlpha[1] = actionButtonAlpha[2] = 245;
                    actionButtonCenters[0] = (Vector2){screenWidth-105.0f, screenHeight-119.0f};
                    actionButtonCenters[1] = (Vector2){screenWidth-105.0f, screenHeight-199.0f};
                    actionButtonCenters[2] = (Vector2){screenWidth-38.0f, screenHeight-56.0f};

                    joystickPosition = (Vector2){
                        150, screenHeight-160
                    };

                    joystickKnob = joystickPosition;

                    settingsButton = (Rectangle){
                        screenWidth-190, 18, 170, 64
                    };

                    cameraButton = (Rectangle){
                        screenWidth-190, 92, 170, 64
                    };

                    lookSensitivity = 3.0f;
                    graphicsQuality = 1;
                    detailLevel = 1;
                    resolutionLevel = 0;
                    chunkDistance = 4;

                    targetFps = 30;
                    SetTargetFPS(targetFps);
                }
                else if (CheckCollisionPointRec(pos, closeButton))
                {
                    settingsOpen = false;
                    SaveGameSettings(joystickPosition, joystickRadius,
                        settingsButton, cameraButton, actionButtonCenters,
                        actionButtonScale, actionButtonAlpha, lookSensitivity,
                        graphicsQuality, targetFps, detailLevel,
                        resolutionLevel, chunkDistance);
                }
            }
            else
            {
                if (pos.x < screenWidth*0.48f &&
                    pos.y > screenHeight*0.25f)
                {
                    float dx = pos.x-joystickPosition.x;
                    float dy = pos.y-joystickPosition.y;

                    if (sqrtf(dx*dx+dy*dy) < joystickRadius+100)
                    {
                        joystickActive = true;
                        joystickTouchId = id;
                    }
                }
                else if (pos.x > screenWidth*0.48f && pos.y > 155)
                {
                    lookTouchId = id;
                    previousLookPosition = pos;
                }
            }
        }

        if (touchCount == 0 &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            Vector2 pos = GetMousePosition();

            if (mainMenuOpen && !settingsOpen && !editControls)
            {
                if (menuPage == 0)
                {
                    if (CheckCollisionPointRec(pos, menuPlayButton))
                        mainMenuOpen = false;
                    else if (CheckCollisionPointRec(pos, menuServersButton))
                        menuPage = 1;
                    else if (CheckCollisionPointRec(pos, menuShopButton))
                        menuPage = 2;
                    else if (CheckCollisionPointRec(pos, menuSettingsButton))
                        settingsOpen = true;
                }
                else if (CheckCollisionPointRec(pos, menuBackButton))
                {
                    menuPage = 0;
                }
            }
            else if (!mainMenuOpen && !settingsOpen && !inventoryOpen &&
                     CheckCollisionPointRec(pos, menuReturnButton))
            {
                mainMenuOpen = true;
                menuPage = 0;
                joystickActive = false;
                lookTouchId = -1;
                joystickKnob = joystickPosition;
            }
            else if (!mainMenuOpen && !editControls && !inventoryOpen &&
                     IsSettingsButtonHit(pos, settingsButton))
            {
                settingsOpen = !settingsOpen;
                joystickActive = false;
                lookTouchId = -1;
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen &&
                     CheckCollisionPointRec(pos, cameraButton))
            {
                firstPerson = !firstPerson;
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen &&
                     CheckCollisionPointRec(pos, sprintButton))
            {
                sprintEnabled = !sprintEnabled;
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls &&
                     CheckCollisionPointRec(pos, inventoryButton))
            {
                inventoryOpen = !inventoryOpen;
                joystickActive = false;
                lookTouchId = -1;
            }
            else if (inventoryOpen)
            {
                // Еду и воду можно использовать касанием карточки в рюкзаке.
                Rectangle invPanel = {screenWidth*0.08f, screenHeight*0.20f,
                                      screenWidth*0.84f, screenHeight*0.60f};
                for (int item = 2; item <= 3; item++)
                {
                    float w = (invPanel.width-50.0f)/2.0f;
                    Rectangle r = {invPanel.x+16+(item%2)*(w+12),
                                   invPanel.y+72+(item/2)*52, w, 44};
                    if (CheckCollisionPointRec(pos, r) && itemCounts[item] > 0)
                    {
                        if (item == 2)
                        {
                            foodLevel += 25.0f;
                            if (foodLevel > 100.0f) foodLevel = 100.0f;
                        }
                        else
                        {
                            waterLevel += 25.0f;
                            if (waterLevel > 100.0f) waterLevel = 100.0f;
                        }
                        itemCounts[item]--;
                        break;
                    }
                }
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen &&
                     CheckCollisionPointRec(pos, interactButton))
            {
                attackRequested = true;
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen)
            {
                for (int slot = 0; slot < 4; slot++)
                    if (CheckCollisionPointRec(pos, hotbarSlots[slot])) selectedHotbarSlot = slot;
            }
            else if (settingsOpen)
            {
                if (CheckCollisionPointRec(pos, moveButtonsButton))
                {
                    settingsOpen = false;
                    editControls = true;
                }
                else if (CheckCollisionPointRec(pos, sizeButton))
                {
                    joystickRadius += 15;

                    if (joystickRadius > 180)
                        joystickRadius = 90;

                    knobRadius = joystickRadius*0.42f;
                }
                else if (CheckCollisionPointRec(pos, lowButton))
                {
                    graphicsQuality = 0;
                    detailLevel = 2;
                    resolutionLevel = 2;
                }
                else if (CheckCollisionPointRec(pos, mediumButton))
                {
                    graphicsQuality = 1;
                    detailLevel = 1;
                    resolutionLevel = 1;
                }
                else if (CheckCollisionPointRec(pos, highButton))
                {
                    graphicsQuality = 2;
                }
                else if (CheckCollisionPointRec(pos, fpsButtons[0]))
                {
                    targetFps = 30;
                    SetTargetFPS(targetFps);
                }
                else if (CheckCollisionPointRec(pos, fpsButtons[1]))
                {
                    targetFps = 60;
                    SetTargetFPS(targetFps);
                }
                else if (CheckCollisionPointRec(pos, fpsButtons[2]))
                {
                    targetFps = 90;
                    SetTargetFPS(targetFps);
                }
                else if (CheckCollisionPointRec(pos, fpsButtons[3]))
                {
                    targetFps = 120;
                    SetTargetFPS(targetFps);
                }
                else if (CheckCollisionPointRec(pos, fpsButtons[4]))
                {
                    targetFps = 144;
                    SetTargetFPS(targetFps);
                }
                else if (CheckCollisionPointRec(pos, detailMoreButton))
                    detailLevel = 0;
                else if (CheckCollisionPointRec(pos, detailSomeButton))
                    detailLevel = 1;
                else if (CheckCollisionPointRec(pos, detailFewButton))
                    detailLevel = 2;
                else if (CheckCollisionPointRec(pos, resolutionFullButton))
                    resolutionLevel = 0;
                else if (CheckCollisionPointRec(pos, resolution85Button))
                    resolutionLevel = 1;
                else if (CheckCollisionPointRec(pos, resolution70Button))
                    resolutionLevel = 2;
                else if (CheckCollisionPointRec(pos, chunkButton))
                {
                    chunkDistance++;

                    if (chunkDistance > 20)
                        chunkDistance = 2;
                }
                else if (CheckCollisionPointRec(pos, slider))
                {
                    float v = (pos.x-slider.x)/slider.width;

                    if (v < 0) v = 0;
                    if (v > 1) v = 1;

                    lookSensitivity = 0.1f+v*9.9f;
                }
                else if (CheckCollisionPointRec(pos, resetButton))
                {
                    joystickRadius = 120;
                    knobRadius = 50;
                    actionButtonScale[0] = actionButtonScale[1] = actionButtonScale[2] = 1.0f;
                    actionButtonAlpha[0] = actionButtonAlpha[1] = actionButtonAlpha[2] = 245;
                    actionButtonCenters[0] = (Vector2){screenWidth-105.0f, screenHeight-119.0f};
                    actionButtonCenters[1] = (Vector2){screenWidth-105.0f, screenHeight-199.0f};
                    actionButtonCenters[2] = (Vector2){screenWidth-38.0f, screenHeight-56.0f};

                    joystickPosition = (Vector2){
                        150, screenHeight-160
                    };

                    joystickKnob = joystickPosition;

                    settingsButton = (Rectangle){
                        screenWidth-190, 18, 170, 64
                    };

                    cameraButton = (Rectangle){
                        screenWidth-190, 92, 170, 64
                    };

                    lookSensitivity = 3.0f;
                    graphicsQuality = 1;
                    detailLevel = 1;
                    resolutionLevel = 0;
                    chunkDistance = 4;

                    targetFps = 30;
                    SetTargetFPS(targetFps);
                    SaveGameSettings(joystickPosition, joystickRadius,
                        settingsButton, cameraButton, actionButtonCenters,
                        actionButtonScale, actionButtonAlpha, lookSensitivity,
                        graphicsQuality, targetFps, detailLevel,
                        resolutionLevel, chunkDistance);
                }
                else if (CheckCollisionPointRec(pos, closeButton))
                {
                    settingsOpen = false;
                    SaveGameSettings(joystickPosition, joystickRadius,
                        settingsButton, cameraButton, actionButtonCenters,
                        actionButtonScale, actionButtonAlpha, lookSensitivity,
                        graphicsQuality, targetFps, detailLevel,
                        resolutionLevel, chunkDistance);
                }
            }
        }

        if (editControls && dragTarget != 0)
        {
            bool found = false;

            for (int i = 0; i < touchCount; i++)
            {
                if (GetTouchPointId(i) == dragTouchId)
                {
                    Vector2 pos = GetTouchPosition(i);

                    float nx = pos.x+dragOffset.x;
                    float ny = pos.y+dragOffset.y;

                    if (dragTarget == 1)
                    {
                        nx = fmaxf(joystickRadius+8.0f,
                                   fminf(nx, screenWidth-joystickRadius-8.0f));
                        ny = fmaxf(joystickRadius+8.0f,
                                   fminf(ny, screenHeight-joystickRadius-8.0f));
                        joystickPosition = (Vector2){nx, ny};
                        joystickKnob = joystickPosition;
                    }
                    else if (dragTarget == 2)
                    {
                        settingsButton.x = fmaxf(
                            8, fminf(nx, screenWidth-settingsButton.width-8)
                        );
                        settingsButton.y = fmaxf(
                            8, fminf(ny, screenHeight-settingsButton.height-8)
                        );
                    }
                    else if (dragTarget == 3)
                    {
                        cameraButton.x = fmaxf(
                            8, fminf(nx, screenWidth-cameraButton.width-8)
                        );
                        cameraButton.y = fmaxf(
                            8, fminf(ny, screenHeight-cameraButton.height-8)
                        );
                    }
                    else if (dragTarget >= 4 && dragTarget <= 6)
                    {
                        int control = dragTarget-4;
                        float halfW = interactButton.width*0.5f;
                        float halfH = interactButton.height*0.5f;
                        if (control == 1) { halfW = sprintButton.width*0.5f; halfH = sprintButton.height*0.5f; }
                        if (control == 2) { halfW = inventoryButton.width*0.5f; halfH = inventoryButton.height*0.5f; }
                        actionButtonCenters[control] = (Vector2){
                            fmaxf(halfW+5.0f, fminf(nx, screenWidth-halfW-5.0f)),
                            fmaxf(halfH+5.0f, fminf(ny, screenHeight-halfH-5.0f))
                        };
                    }

                    found = true;
                    break;
                }
            }

            if (!found)
            {
                dragTarget = 0;
                dragTouchId = -1;
            }
        }

        playerMoving = false;

        if (joystickActive && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen)
        {
            bool found = false;

            for (int i = 0; i < touchCount; i++)
            {
                if (GetTouchPointId(i) == joystickTouchId)
                {
                    Vector2 pos = GetTouchPosition(i);

                    float dx = pos.x-joystickPosition.x;
                    float dy = pos.y-joystickPosition.y;

                    float len = sqrtf(dx*dx+dy*dy);

                    if (len > joystickRadius)
                    {
                        dx = dx/len*joystickRadius;
                        dy = dy/len*joystickRadius;
                    }

                    joystickKnob = (Vector2){
                        joystickPosition.x+dx,
                        joystickPosition.y+dy
                    };

                    float moveX = dx/joystickRadius;
                    float moveForward = -dy/joystickRadius;

                    if (fabsf(moveX)+fabsf(moveForward) > 0.12f)
                    {
                        playerMoving = true;
                        walkPhase += 0.22f;
                    }

                    float worldMoveX =
                        cosf(cameraYaw)*moveX +
                        sinf(cameraYaw)*moveForward;

                    float worldMoveZ =
                        sinf(cameraYaw)*moveX -
                        cosf(cameraYaw)*moveForward;

                    float survivalSpeed = playerSpeed;
                    if (foodLevel < 20.0f || waterLevel < 20.0f)
                        survivalSpeed *= 0.72f;
                    if (sprintEnabled) survivalSpeed *= 1.65f;

                    float nextX = player.x + worldMoveX*survivalSpeed;
                    float nextZ = player.z + worldMoveZ*survivalSpeed;

                    // Проверяем оси отдельно, чтобы можно было скользить вдоль стен.
                    if (!IsBlockedAt(nextX, player.z,
                                     barrelPositions, barrelDestroyed))
                        player.x = nextX;

                    if (!IsBlockedAt(player.x, nextZ,
                                     barrelPositions, barrelDestroyed))
                        player.z = nextZ;

                    if (playerMoving)
                    {
                        float targetAngle = atan2f(worldMoveX, -worldMoveZ);
                        float angleDiff = targetAngle-playerAngle;

                        while (angleDiff > 3.14159265f)
                            angleDiff -= 6.28318530f;

                        while (angleDiff < -3.14159265f)
                            angleDiff += 6.28318530f;

                        playerAngle += angleDiff*0.22f;
                    }

                    found = true;
                    break;
                }
            }

            if (!found)
            {
                joystickActive = false;
                joystickTouchId = -1;
                joystickKnob = joystickPosition;
            }
        }
        else if (mainMenuOpen || settingsOpen || editControls)
        {
            joystickActive = false;
            joystickKnob = joystickPosition;
        }

        if (draggingSlider)
        {
            bool found = false;

            for (int i = 0; i < touchCount; i++)
            {
                if (GetTouchPointId(i) == sliderTouchId)
                {
                    Vector2 pos = GetTouchPosition(i);

                    float value = (pos.x-slider.x)/slider.width;

                    if (value < 0) value = 0;
                    if (value > 1) value = 1;

                    lookSensitivity = 0.1f+value*9.9f;

                    found = true;
                    break;
                }
            }

            if (!found)
            {
                draggingSlider = false;
                sliderTouchId = -1;
            }
        }

        if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen &&
            lookTouchId != -1)
        {
            bool found = false;

            for (int i = 0; i < touchCount; i++)
            {
                if (GetTouchPointId(i) == lookTouchId)
                {
                    Vector2 pos = GetTouchPosition(i);

                    float dx = pos.x-previousLookPosition.x;
                    float dy = pos.y-previousLookPosition.y;

                    float sensitivity = lookSensitivity*0.004f;

                    cameraYaw += dx*sensitivity;
                    cameraPitch -= dy*sensitivity;

                    if (cameraPitch > 1.2f) cameraPitch = 1.2f;
                    if (cameraPitch < -1.2f) cameraPitch = -1.2f;

                    previousLookPosition = pos;

                    found = true;
                    break;
                }
            }

            if (!found)
                lookTouchId = -1;
        }

        if (settingsOpen || editControls || mainMenuOpen || inventoryOpen)
            lookTouchId = -1;

        for (int i = 0; i < MAX_TOUCHES; i++)
            previousTouchIds[i] = currentTouchIds[i];

        float dirX = sinf(cameraYaw)*cosf(cameraPitch);
        float dirY = sinf(cameraPitch);
        float dirZ = -cosf(cameraYaw)*cosf(cameraPitch);

        if (firstPerson)
        {
            camera.position = (Vector3){
                player.x, player.y+1.6f, player.z
            };

            camera.target = (Vector3){
                player.x+dirX*10,
                player.y+1.6f+dirY*10,
                player.z+dirZ*10
            };
        }
        else
        {
            Vector3 desiredCamera = {
                player.x-dirX*7,
                player.y+3.0f-dirY*7,
                player.z-dirZ*7
            };

            camera.position = desiredCamera;

            // Не даём камере проходить сквозь стены или опускаться под землю.
            Vector3 cameraStart = {player.x, player.y+1.4f, player.z};
            for (int step = 1; step <= 24; step++)
            {
                float t = step/24.0f;
                float cx = cameraStart.x+(desiredCamera.x-cameraStart.x)*t;
                float cy = cameraStart.y+(desiredCamera.y-cameraStart.y)*t;
                float cz = cameraStart.z+(desiredCamera.z-cameraStart.z)*t;

                if (cy < 0.25f ||
                    IsBlockedAt(cx, cz, barrelPositions, barrelDestroyed))
                {
                    float safeT = fmaxf(0.0f, t-0.06f);
                    camera.position = (Vector3){
                        cameraStart.x+(desiredCamera.x-cameraStart.x)*safeT,
                        fmaxf(0.35f, cameraStart.y+(desiredCamera.y-cameraStart.y)*safeT),
                        cameraStart.z+(desiredCamera.z-cameraStart.z)*safeT
                    };
                    break;
                }
            }

            camera.target = (Vector3){
                player.x, player.y+1.4f, player.z
            };
        }

        targetedBarrel = -1;
        Vector3 aimOrigin = {player.x, player.y+1.6f, player.z};
        Vector3 aimDirection = {dirX, dirY, dirZ};
        float closestBarrelDistance = 9999.0f;

        for (int b = 0; b < MAX_BARRELS; b++)
        {
            if (barrelDestroyed[b]) continue;

            float dx = barrelPositions[b].x-aimOrigin.x;
            float dy = barrelPositions[b].y+0.48f-aimOrigin.y;
            float dz = barrelPositions[b].z-aimOrigin.z;
            float distance = sqrtf(dx*dx+dy*dy+dz*dz);

            // Бочку можно выбрать только на расстоянии удара.
            if (distance < 0.1f || distance > 2.2f) continue;

            float dot = (dx*aimDirection.x +
                         dy*aimDirection.y +
                         dz*aimDirection.z)/distance;

            if (dot > 0.965f && distance < closestBarrelDistance)
            {
                closestBarrelDistance = distance;
                targetedBarrel = b;
            }
        }

        if (attackRequested && attackCooldown <= 0.0f &&
            !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen)
        {
            attackCooldown = attackCooldownDuration;
            attackAnimation = 0.24f;
            if (targetedBarrel >= 0 && !barrelDestroyed[targetedBarrel])
            {
                barrelHealth[targetedBarrel] -= 10;
                if (barrelHealth[targetedBarrel] <= 0)
                {
                    barrelHealth[targetedBarrel] = 0;
                    barrelDestroyed[targetedBarrel] = true;

                    // Бочки выдают припасы в инвентарь: еду, воду, дерево или камень.
                    int lootType = 2 + (targetedBarrel % 4);
                    int lootAmount = 2 + (targetedBarrel % 3);
                    itemCounts[lootType] += lootAmount;
                    if (itemCounts[lootType] > 99) itemCounts[lootType] = 99;
                    targetedBarrel = -1;
                }
            }
        }

        float renderScale =
            (resolutionLevel == 0) ? 1.0f :
            ((resolutionLevel == 1) ? 0.85f : 0.70f);

        int wantedWidth = (int)(screenWidth*renderScale);
        int wantedHeight = (int)(screenHeight*renderScale);

        if (wantedWidth < 320) wantedWidth = 320;
        if (wantedHeight < 240) wantedHeight = 240;

        if (wantedWidth != renderWidth ||
            wantedHeight != renderHeight)
        {
            UnloadRenderTexture(sceneTexture);

            renderWidth = wantedWidth;
            renderHeight = wantedHeight;

            sceneTexture = LoadRenderTexture(renderWidth, renderHeight);

            SetTextureFilter(
                sceneTexture.texture,
                TEXTURE_FILTER_BILINEAR
            );
        }

        BeginTextureMode(sceneTexture);

        ClearBackground(C(135, 194, 235, 255));

        BeginMode3D(camera);

        // Мягкая многослойная поверхность мира.
        DrawPlane((Vector3){0, -0.08f, 0},
                  (Vector2){520, 520},
                  C(73, 139, 76, 255));

        // Широкая грунтовая дорога через поселение.
        DrawPlane((Vector3){0, -0.055f, 0},
                  (Vector2){12.5f, 520},
                  C(126, 111, 83, 255));
        DrawPlane((Vector3){0, -0.049f, 0},
                  (Vector2){9.2f, 520},
                  C(157, 137, 101, 255));
        DrawPlane((Vector3){0, -0.055f, 0},
                  (Vector2){520, 12.5f},
                  C(126, 111, 83, 255));
        DrawPlane((Vector3){0, -0.049f, 0},
                  (Vector2){520, 9.2f},
                  C(157, 137, 101, 255));

        // Небольшие естественные пятна травы вокруг игрока.
        if (graphicsQuality >= 1)
        {
            int grassStep = (detailLevel == 0) ? 7 : 11;
            int centerX = (int)(player.x/grassStep)*grassStep;
            int centerZ = (int)(player.z/grassStep)*grassStep;
            for (int gx = -5; gx <= 5; gx++)
            {
                for (int gz = -5; gz <= 5; gz++)
                {
                    float px = (float)(centerX+gx*grassStep);
                    float pz = (float)(centerZ+gz*grassStep);
                    if (!IsVillageClear(px, pz)) continue;
                    int seed = abs((int)(px*13.0f+pz*7.0f));
                    DrawGrassPatch(px, pz, 0.8f, seed);
                }
            }
        }

        DrawPlane((Vector3){0, -0.052f, 0},
                  (Vector2){9.5f, 520},
                  C(116, 105, 84, 255));

        DrawPlane((Vector3){0, -0.048f, 0},
                  (Vector2){7.0f, 520},
                  C(164, 145, 111, 255));

        DrawPlane((Vector3){0, -0.052f, 0},
                  (Vector2){520, 9.5f},
                  C(116, 105, 84, 255));

        DrawPlane((Vector3){0, -0.048f, 0},
                  (Vector2){520, 7.0f},
                  C(164, 145, 111, 255));

        DrawPlane((Vector3){0, -0.052f, -120},
                  (Vector2){520, 7.5f},
                  C(116, 105, 84, 255));

        DrawPlane((Vector3){0, -0.048f, -120},
                  (Vector2){520, 5.2f},
                  C(164, 145, 111, 255));

        DrawPlane((Vector3){0, -0.052f, 120},
                  (Vector2){520, 7.5f},
                  C(116, 105, 84, 255));

        DrawPlane((Vector3){0, -0.048f, 120},
                  (Vector2){520, 5.2f},
                  C(164, 145, 111, 255));

        float chunkView = chunkDistance*50.0f;
        float chunkView2 = chunkView*chunkView;

        int houseStep =
            (graphicsQuality == 0 || detailLevel >= 2) ? 2 : 1;

        for (int i = -3; i <= 3; i += houseStep)
        {
            float hz = i*55.0f;

            float sc = 0.82f +
                (float)((i*i+3)%3)*0.08f;

            float hx1 = -18.0f-player.x;
            float hz1 = hz-player.z;

            if (hx1*hx1+hz1*hz1 < chunkView2)
                DrawRustHouse(-18.0f, hz, sc, i+4);

            float hx2 = 18.0f-player.x;
            float hz2 = hz+22.0f-player.z;

            if (hx2*hx2+hz2*hz2 < chunkView2)
                DrawRustHouse(18.0f, hz+22.0f, sc*0.92f, i+7);
        }

        const Vector3 extraHouses[3] = {
            {-42.0f, 0, -120.0f},
            {48.0f, 0, 120.0f},
            {42.0f, 0, -120.0f}
        };

        for (int h = 0; h < 3; h++)
        {
            float dx = extraHouses[h].x-player.x;
            float dz = extraHouses[h].z-player.z;

            if (dx*dx+dz*dz < chunkView2)
            {
                DrawRustHouse(
                    extraHouses[h].x,
                    extraHouses[h].z,
                    (h == 1) ? 0.84f : 0.80f,
                    h
                );
            }
        }

        for (int b = 0; b < MAX_BARRELS; b++)
        {
            if (barrelDestroyed[b]) continue;

            float bx = barrelPositions[b].x-player.x;
            float bz = barrelPositions[b].z-player.z;

            if (bx*bx+bz*bz < chunkView2)
                DrawLootBarrel(barrelPositions[b]);
        }

        for (int i = -5; i <= 5; i++)
        {
            DrawSphere(
                (Vector3){i*48.0f, -8.0f, -265.0f},
                34.0f, C(101, 174, 105, 255)
            );

            DrawSphere(
                (Vector3){i*48.0f, -8.0f, 265.0f},
                34.0f, C(91, 160, 94, 255)
            );

            DrawSphere(
                (Vector3){-265.0f, -8.0f, i*48.0f},
                34.0f, C(101, 174, 105, 255)
            );

            DrawSphere(
                (Vector3){265.0f, -8.0f, i*48.0f},
                34.0f, C(91, 160, 94, 255)
            );
        }

        DrawSphere(
            (Vector3){-28, 28, -45},
            5.0f, C(255, 226, 125, 255)
        );

        if (graphicsQuality >= 1)
        {
            DrawCloud(-15, 18, -35, 2.4f);
            DrawCloud(15, 22, -48, 2.8f);
        }

        if (graphicsQuality >= 2)
            DrawCloud(32, 17, 12, 2.1f);

        int treeStep =
            (graphicsQuality == 0) ? 46 :
            ((graphicsQuality == 1) ? 34 : 27);

        treeStep += detailLevel*5;

        int treeLimit = 245;

        float treeView = fminf(chunkDistance*50.0f, 245.0f);
        float treeView2 = treeView*treeView;

        for (int x = -treeLimit; x <= treeLimit; x += treeStep)
        {
            for (int z = -treeLimit; z <= treeLimit; z += treeStep)
            {
                float tx = (float)x-player.x;
                float tz = (float)z-player.z;

                if (tx*tx+tz*tz > treeView2)
                    continue;

                if (!IsVillageClear((float)x, (float)z))
                    continue;

                float scale =
                    0.85f+(float)((x*x+z*z)%5)*0.07f;

                DrawTree(
                    (float)x, (float)z,
                    scale, graphicsQuality
                );
            }
        }

        if (graphicsQuality >= 1)
        {
            DrawRock(-5, 0.35f, -8, 0.65f);
            DrawRock(7, 0.28f, 5, 0.48f);

            DrawSphere(
                (Vector3){-7, 0.45f, 3},
                0.65f, C(48, 137, 67, 255)
            );

            if (detailLevel < 2)
            {
                DrawSphere(
                    (Vector3){9, 0.42f, -7},
                    0.58f, C(58, 147, 72, 255)
                );
            }
        }

        if (graphicsQuality >= 2)
            DrawRock(-10, 0.25f, 9, 0.42f);

        if (graphicsQuality >= 1 && detailLevel < 2)
        {
            int flowerStep =
                (graphicsQuality == 1) ? 9 : 6;

            flowerStep += detailLevel*3;

            for (int i = -12; i <= 12; i += flowerStep)
            {
                DrawCylinder(
                    (Vector3){-3.6f, 0.18f, i*2.0f},
                    0.035f, 0.035f, 0.35f,
                    5, C(49, 120, 54, 255)
                );

                DrawSphere(
                    (Vector3){-3.6f, 0.38f, i*2.0f},
                    0.13f, C(255, 220, 95, 255)
                );

                DrawCylinder(
                    (Vector3){3.6f, 0.18f, i*2.0f},
                    0.035f, 0.035f, 0.35f,
                    5, C(49, 120, 54, 255)
                );

                DrawSphere(
                    (Vector3){3.6f, 0.38f, i*2.0f},
                    0.13f, C(245, 150, 175, 255)
                );
            }
        }

        if (!firstPerson)
        {
            DrawPlayerShadow(player);
            DrawPlayer(
                player, playerAngle, walkPhase, playerMoving,
                attackAnimation > 0.0f ? attackAnimation/0.24f : 0.0f
            );
        }
        else
        {
            DrawFirstPersonArms(
                camera.position,
                cameraYaw,
                walkPhase,
                playerMoving,
                (float)GetTime(),
                attackAnimation > 0.0f ? attackAnimation/0.24f : 0.0f
            );
        }

        EndMode3D();
        EndTextureMode();

        BeginDrawing();

        ClearBackground(BLACK);

        DrawTexturePro(
            sceneTexture.texture,
            (Rectangle){
                0, 0,
                (float)sceneTexture.texture.width,
                -(float)sceneTexture.texture.height
            },
            (Rectangle){
                0, 0,
                (float)screenWidth,
                (float)screenHeight
            },
            (Vector2){0, 0},
            0.0f,
            WHITE
        );

        // Игровой интерфейс скрывается в главном меню.
        if (!mainMenuOpen)
        {
            // Компактная панель выживания в стиле военного HUD.
            DrawRectangleRounded(
                (Rectangle){14, 72, 244, 88},
                0.16f, 14, C(12, 24, 34, 218)
            );
            DrawRectangleRoundedLinesEx(
                (Rectangle){14, 72, 244, 88},
                0.16f, 14, 1.5f, C(119, 159, 166, 190)
            );
            DrawText("SURVIVAL STATUS", 27, 78, 11,
                     C(135, 190, 194, 255));
            DrawText("FOOD", 28, 98, 12, RAYWHITE);
            DrawRectangleRounded(
                (Rectangle){82, 88, 112, 9},
                0.5f, 6, C(65, 70, 72, 230)
            );
            DrawRectangleRounded(
                (Rectangle){82, 88, 112*(foodLevel/100.0f), 9},
                0.5f, 6, C(215, 160, 75, 255)
            );
            DrawText(TextFormat("%d%%", (int)foodLevel),
                     203, 85, 12, C(245, 220, 170, 255));

            DrawText("WATER", 28, 124, 12, RAYWHITE);
            DrawRectangleRounded(
                (Rectangle){82, 126, 112, 9},
                0.5f, 6, C(65, 70, 72, 230)
            );
            DrawRectangleRounded(
                (Rectangle){82, 126, 112*(waterLevel/100.0f), 9},
                0.5f, 6, C(75, 170, 235, 255)
            );
            DrawText(TextFormat("%d%%", (int)waterLevel),
                     203, 123, 12, C(170, 220, 250, 255));

            if (foodLevel < 20.0f || waterLevel < 20.0f)
                DrawText("LOW SUPPLIES - FIND BARRELS", 28, 145, 10,
                         C(255, 190, 105, 255));

            DrawRectangleRounded(
                (Rectangle){14, 14, 210, 46},
                0.22f, 12, C(12, 25, 37, 205)
            );
            DrawRectangleRoundedLinesEx(
                (Rectangle){14, 14, 210, 46},
                0.22f, 12, 1.5f, C(116, 169, 180, 190)
            );
            DrawCircle(34, 37, 7, C(103, 230, 135, 255));
            DrawText("GREEN VALLEY", 49, 24, 18, RAYWHITE);
            DrawText("SURVIVAL ZONE  /  DAY 01", 49, 43, 9,
                     C(139, 179, 190, 255));

            DrawRectangleRounded(
                settingsButton, 0.30f, 16,
                C(20, 34, 52, 235)
            );

            DrawRectangleRoundedLinesEx(
                settingsButton, 0.30f, 16, 2,
                C(130, 205, 255, 240)
            );

            DrawText(
                "SETTINGS",
                settingsButton.x+18,
                settingsButton.y+22,
                19,
                RAYWHITE
            );

            DrawRectangleRounded(
                cameraButton, 0.30f, 16,
                C(20, 34, 52, 235)
            );

            DrawRectangleRoundedLinesEx(
                cameraButton, 0.30f, 16, 2,
                C(130, 205, 255, 220)
            );

            DrawText(
                firstPerson ? "FIRST PERSON" : "THIRD PERSON",
                cameraButton.x+10,
                cameraButton.y+19,
                17,
                RAYWHITE
            );

            DrawCircleV(
                joystickPosition,
                joystickRadius+5,
                C(10, 25, 35, 65)
            );

            DrawCircleV(
                joystickPosition,
                joystickRadius,
                C(20, 35, 48, 105)
            );

            DrawCircleLines(
                (int)joystickPosition.x,
                (int)joystickPosition.y,
                joystickRadius-4,
                C(220, 240, 255, 145)
            );

            DrawCircleV(
                joystickKnob,
                knobRadius+5,
                C(10, 20, 30, 90)
            );

            DrawCircleV(
                joystickKnob,
                knobRadius,
                C(220, 239, 255, 205)
            );

            // Панель быстрого доступа: четыре выбираемых слота.
            if (!settingsOpen && !editControls && !inventoryOpen)
            {
                for (int slot = 0; slot < 4; slot++)
                {
                    Rectangle r = hotbarSlots[slot];
                    DrawRectangleRounded((Rectangle){r.x+2, r.y+3, r.width, r.height}, 0.16f, 8, C(0, 0, 0, 110));
                    DrawRectangleRounded(r, 0.16f, 8, slot == selectedHotbarSlot ? C(36, 91, 139, 245) : C(13, 28, 49, 235));
                    DrawRectangleRoundedLinesEx(r, 0.16f, 8, slot == selectedHotbarSlot ? 2.5f : 1.4f,
                        slot == selectedHotbarSlot ? C(117, 210, 255, 255) : C(91, 145, 184, 220));
                    DrawText(TextFormat("%d", slot+1), (int)r.x+5, (int)r.y+4, 11, C(160, 205, 235, 255));
                    const char *slotName = itemNames[hotbarItemIds[slot]];
                    int nw = MeasureText(slotName, 10);
                    DrawText(slotName, (int)(r.x+r.width/2-nw/2), (int)r.y+22, 10, RAYWHITE);
                    DrawText(TextFormat("x%d", itemCounts[hotbarItemIds[slot]]), (int)(r.x+r.width-22), (int)(r.y+r.height-14), 10, C(175, 215, 245, 255));
                }
                DrawSurvivalButtonAlpha(inventoryButton, "BAG", 13, false, (unsigned char)actionButtonAlpha[2]);
                DrawSurvivalButtonAlpha(sprintButton, sprintEnabled ? "RUN: ON" : "RUN", 18, sprintEnabled, (unsigned char)actionButtonAlpha[1]);
            }

            // Кнопка удара всегда видна во время игры.
            if (!settingsOpen && !editControls && !inventoryOpen)
            {
                unsigned char hitAlpha = (unsigned char)actionButtonAlpha[0];
                Color hitColor = attackCooldown <= 0.0f ?
                    C(185, 75, 55, hitAlpha) : C(75, 83, 91, hitAlpha);
                DrawRectangleRounded(interactButton, 0.25f, 14, hitColor);
                DrawRectangleRoundedLinesEx(
                    interactButton, 0.25f, 14, 2,
                    C(230, 190, 160, hitAlpha)
                );
                DrawText(
                    "HIT",
                    (int)(interactButton.x+interactButton.width/2-
                          MeasureText("HIT", 20)/2),
                    (int)(interactButton.y+7), 20, C(255, 255, 255, hitAlpha)
                );

                float cooldownRatio = attackCooldown/attackCooldownDuration;
                DrawRectangleRounded(
                    (Rectangle){interactButton.x+12, interactButton.y+34,
                                (interactButton.width-24)*(1.0f-cooldownRatio), 7},
                    0.5f, 6, C(105, 225, 150, hitAlpha)
                );
                const char *cooldownLabel = attackCooldown <= 0.0f ?
                    "READY" : TextFormat("%.1f s", attackCooldown);
                DrawText(
                    cooldownLabel,
                    (int)(interactButton.x+interactButton.width/2-
                          MeasureText(cooldownLabel, 13)/2),
                    (int)(interactButton.y+44), 13, C(255, 255, 255, hitAlpha)
                );
            }

            if (targetedBarrel >= 0 && !barrelDestroyed[targetedBarrel])
            {
                float hudX = screenWidth/2.0f-130.0f;
                Rectangle barrelHud = {hudX, 105, 260, 76};

                DrawRectangleRounded(
                    barrelHud, 0.22f, 12, C(18, 31, 44, 225)
                );
                DrawRectangleRoundedLinesEx(
                    barrelHud, 0.22f, 12, 2,
                    C(190, 145, 90, 240)
                );
                DrawText(
                    "LOOT BARREL", (int)hudX+18, 116, 19, RAYWHITE
                );
                DrawText(
                    TextFormat("HP: %d/50", barrelHealth[targetedBarrel]),
                    (int)hudX+18, 145, 18,
                    C(255, 218, 145, 255)
                );

                DrawText(
                    "AIM AT BARREL",
                    (int)hudX+18, 166, 14, C(190, 215, 230, 255)
                );
            }

            DrawCircleLines(
                (int)joystickKnob.x,
                (int)joystickKnob.y,
                knobRadius-3,
                C(255, 255, 255, 230)
            );

            // Кнопка возврата в главное меню.
            DrawSurvivalButton(menuReturnButton, "MENU", 19, false);

            if (inventoryOpen)
            {
                DrawRectangle(0, 0, screenWidth, screenHeight, C(0, 5, 16, 175));
                Rectangle invPanel = {screenWidth*0.08f, screenHeight*0.20f, screenWidth*0.84f, screenHeight*0.60f};
                DrawRectangleRounded(invPanel, 0.08f, 14, C(10, 24, 45, 248));
                DrawRectangleRoundedLinesEx(invPanel, 0.08f, 14, 2.2f, C(112, 190, 235, 245));
                DrawText("INVENTORY", (int)invPanel.x+18, (int)invPanel.y+16, 22, RAYWHITE);
                DrawText("ITEMS / QUICK ACCESS", (int)invPanel.x+20, (int)invPanel.y+45, 11, C(145, 190, 220, 255));
                for (int item = 0; item < 6; item++)
                {
                    float w = (invPanel.width-50.0f)/2.0f;
                    Rectangle r = {invPanel.x+16+(item%2)*(w+12),
                                   invPanel.y+72+(item/2)*52, w, 44};
                    DrawRectangleRounded(r, 0.12f, 8,
                        (item == 2 || item == 3) ? C(24, 61, 86, 255) : C(19, 42, 67, 255));
                    DrawRectangleRoundedLinesEx(r, 0.12f, 8, 1.5f, C(100, 170, 215, 230));
                    DrawText(itemNames[item], (int)r.x+9, (int)r.y+6, 13, RAYWHITE);
                    DrawText(TextFormat("COUNT: %d", itemCounts[item]),
                        (int)r.x+9, (int)r.y+25, 10, C(160, 205, 235, 255));
                }
                DrawSurvivalButton(inventoryButton, "BAG", 13, true);
                DrawText("TAP FOOD / WATER TO CONSUME",
                    (int)invPanel.x+18, (int)(invPanel.y+invPanel.height-26),
                    10, C(145, 190, 220, 255));
            }
        }

        // Главное меню boltSurvival.
        if (mainMenuOpen)
        {
            // Холодный сине-графитовый фон с техническими линиями.
            DrawRectangleGradientV(0, 0, screenWidth, screenHeight,
                                   C(13, 24, 48, 255), C(29, 43, 72, 255));
            DrawRectangle(0, 0, screenWidth, screenHeight, C(2, 8, 22, 75));
            for (int stripe = 0; stripe < 9; stripe++)
            {
                float sx = (float)stripe*screenWidth/8.0f;
                DrawLineEx((Vector2){sx, 0}, (Vector2){sx, screenHeight},
                           1.0f, C(105, 158, 205, 22));
            }
            DrawRectangle(12, 0, 18, screenHeight, C(75, 125, 177, 35));
            DrawRectangle(screenWidth-30, 0, 18, screenHeight, C(75, 125, 177, 35));
            DrawRectangleLinesEx((Rectangle){28, 12, screenWidth-56, screenHeight-24},
                                 2.0f, C(111, 174, 220, 90));

            const char *title = "boltSurvival";
            int titleSize = (int)fminf(48.0f, screenWidth*0.095f);
            int titleWidth = MeasureText(title, titleSize);

            DrawText(
                title,
                screenWidth/2-titleWidth/2,
                42,
                titleSize,
                RAYWHITE
            );

            const char *subtitle = "SURVIVE  -  EXPLORE  -  BUILD";
            int subtitleWidth = MeasureText(subtitle, 15);

            DrawText(
                subtitle,
                screenWidth/2-subtitleWidth/2,
                100,
                15,
                C(157, 204, 239, 255)
            );

            if (menuPage == 0)
            {
                Rectangle menuPanel = {menuX-14, menuStartY-15,
                                        menuWidth+28, menuGridHeight+30};
                DrawRectangleRounded(menuPanel, 0.08f, 12, C(5, 13, 29, 190));
                DrawRectangleRoundedLinesEx(menuPanel, 0.08f, 12, 2.0f,
                                            C(112, 177, 222, 165));

                Rectangle buttons[4] = {menuPlayButton, menuServersButton,
                                        menuShopButton, menuSettingsButton};
                const char *labels[4] = {"PLAY", "SERVERS", "SHOP", "SETTINGS"};
                for (int i = 0; i < 4; i++)
                    DrawSurvivalButton(buttons[i], labels[i], 18, i == 0);
            }
            else
            {
                const char *pageTitle =
                    (menuPage == 1) ? "SERVERS" : "SHOP";

                int pageTitleWidth = MeasureText(pageTitle, 27);

                DrawText(
                    pageTitle,
                    screenWidth/2-pageTitleWidth/2,
                    menuStartY+15,
                    27,
                    RAYWHITE
                );

                if (menuPage == 1)
                {
                    float cardWidth = fminf(360.0f, screenWidth-40.0f);
                    Rectangle serverCard = {
                        (screenWidth-cardWidth)/2.0f,
                        menuStartY+5.0f, cardWidth, 105.0f
                    };

                    DrawRectangleRounded(
                        serverCard, 0.20f, 14, C(30, 52, 68, 250)
                    );
                    DrawRectangleRoundedLinesEx(
                        serverCard, 0.20f, 14, 2,
                        C(100, 175, 215, 230)
                    );
                    DrawText(
                        "SERVER #1",
                        (int)serverCard.x+18,
                        (int)serverCard.y+14, 20, RAYWHITE
                    );
                    DrawCircle(
                        (int)(serverCard.x+serverCard.width-26),
                        (int)serverCard.y+25, 6,
                        C(80, 220, 135, 255)
                    );
                    DrawText(
                        "ONLINE",
                        (int)serverCard.x+18,
                        (int)serverCard.y+49, 16,
                        C(100, 225, 145, 255)
                    );
                    DrawText(
                        "0/20 PLAYERS",
                        (int)serverCard.x+18,
                        (int)serverCard.y+75, 18,
                        C(190, 215, 235, 255)
                    );
                }
                else
                {
                    const char *message = "COMING SOON";
                    int messageWidth = MeasureText(message, 18);
                    DrawText(
                        message,
                        screenWidth/2-messageWidth/2,
                        menuStartY+75,
                        18,
                        C(165, 190, 210, 255)
                    );
                }
            
                DrawSurvivalButton(menuBackButton, "BACK", 19, false);
            }

            DrawText(
                "ALPHA VERSION",
                18,
                screenHeight-35,
                14,
                C(130, 160, 180, 255)
            );
        }

        // Меню настроек.
        if (settingsOpen)
        {
            DrawRectangle(
                0, 0, screenWidth, screenHeight,
                C(0, 0, 0, 155)
            );

            DrawRectangleRounded(
                panel, 0.08f, 20,
                C(13, 25, 45, 252)
            );

            DrawRectangleRoundedLinesEx(
                panel, 0.08f, 20, 2,
                C(111, 177, 222, 235)
            );

            DrawText(
                "GAME SETTINGS",
                panelX+24, panelY+18,
                25, RAYWHITE
            );

            DrawText(
                "Controls and graphics",
                panelX+26, panelY+47,
                15, C(155, 177, 198, 255)
            );

            DrawRectangleRounded(
                moveButtonsButton, 0.25f, 14,
                C(42, 54, 43, 255)
            );

            DrawText(
                "MOVE BUTTONS",
                moveButtonsButton.x+18,
                moveButtonsButton.y+17,
                19, RAYWHITE
            );

            DrawRectangleRounded(
                sizeButton, 0.25f, 14,
                C(42, 65, 91, 255)
            );

            DrawText(
                "JOYSTICK SIZE",
                sizeButton.x+18,
                sizeButton.y+17,
                19, RAYWHITE
            );

            DrawText(
                "GRAPHICS QUALITY",
                panelX+25, panelY+166,
                16, C(205, 220, 238, 255)
            );

            Color lowColor =
                graphicsQuality == 0 ?
                C(83, 103, 67, 255) :
                C(42, 65, 91, 255);

            Color medColor =
                graphicsQuality == 1 ?
                C(42, 150, 100, 255) :
                C(42, 65, 91, 255);

            Color highColor =
                graphicsQuality == 2 ?
                C(42, 150, 100, 255) :
                C(42, 65, 91, 255);

            DrawRectangleRounded(lowButton, 0.22f, 12, lowColor);
            DrawRectangleRounded(mediumButton, 0.22f, 12, medColor);
            DrawRectangleRounded(highButton, 0.22f, 12, highColor);

            DrawText(
                "LOW",
                lowButton.x+lowButton.width/2-19,
                lowButton.y+14,
                16, RAYWHITE
            );

            DrawText(
                "MEDIUM",
                mediumButton.x+mediumButton.width/2-34,
                mediumButton.y+14,
                16, RAYWHITE
            );

            DrawText(
                "HIGH",
                highButton.x+highButton.width/2-19,
                highButton.y+14,
                16, RAYWHITE
            );

            DrawText(
                "FPS LIMIT",
                panelX+25, panelY+240,
                16, RAYWHITE
            );

            for (int fi = 0; fi < 5; fi++)
            {
                Color fpsColor =
                    (targetFps == fpsValues[fi]) ?
                    C(42, 150, 100, 255) :
                    C(42, 65, 91, 255);

                DrawRectangleRounded(
                    fpsButtons[fi], 0.22f, 12, fpsColor
                );

                const char *fpsLabel =
                    (fi == 0) ? "30" :
                    (fi == 1) ? "60" :
                    (fi == 2) ? "90" :
                    (fi == 3) ? "120" : "144";

                int tw = MeasureText(fpsLabel, 16);

                DrawText(
                    fpsLabel,
                    fpsButtons[fi].x+fpsButtons[fi].width/2-tw/2,
                    fpsButtons[fi].y+13,
                    16, RAYWHITE
                );
            }

            DrawText(
                "WORLD DETAILS",
                panelX+25, panelY+320,
                16, RAYWHITE
            );

            Color moreColor =
                detailLevel == 0 ?
                C(42, 150, 100, 255) :
                C(42, 65, 91, 255);

            Color someColor =
                detailLevel == 1 ?
                C(42, 150, 100, 255) :
                C(42, 65, 91, 255);

            Color fewColor =
                detailLevel == 2 ?
                C(42, 150, 100, 255) :
                C(42, 65, 91, 255);

            DrawRectangleRounded(
                detailMoreButton, 0.22f, 12, moreColor
            );

            DrawRectangleRounded(
                detailSomeButton, 0.22f, 12, someColor
            );

            DrawRectangleRounded(
                detailFewButton, 0.22f, 12, fewColor
            );

            DrawText(
                "MORE",
                detailMoreButton.x+detailMoreButton.width/2-25,
                detailMoreButton.y+13,
                15, RAYWHITE
            );

            DrawText(
                "SOME",
                detailSomeButton.x+detailSomeButton.width/2-24,
                detailSomeButton.y+13,
                15, RAYWHITE
            );

            DrawText(
                "LESS",
                detailFewButton.x+detailFewButton.width/2-20,
                detailFewButton.y+13,
                15, RAYWHITE
            );

            DrawText(
                "RENDER RESOLUTION",
                panelX+25, panelY+400,
                16, RAYWHITE
            );

            Color fullColor =
                resolutionLevel == 0 ?
                C(42, 150, 100, 255) :
                C(42, 65, 91, 255);

            Color midResColor =
                resolutionLevel == 1 ?
                C(42, 150, 100, 255) :
                C(42, 65, 91, 255);

            Color lowResColor =
                resolutionLevel == 2 ?
                C(42, 150, 100, 255) :
                C(42, 65, 91, 255);

            DrawRectangleRounded(
                resolutionFullButton, 0.22f, 12, fullColor
            );

            DrawRectangleRounded(
                resolution85Button, 0.22f, 12, midResColor
            );

            DrawRectangleRounded(
                resolution70Button, 0.22f, 12, lowResColor
            );

            DrawText(
                "100%",
                resolutionFullButton.x+resolutionFullButton.width/2-22,
                resolutionFullButton.y+13,
                15, RAYWHITE
            );

            DrawText(
                "85%",
                resolution85Button.x+resolution85Button.width/2-17,
                resolution85Button.y+13,
                15, RAYWHITE
            );

            DrawText(
                "70%",
                resolution70Button.x+resolution70Button.width/2-17,
                resolution70Button.y+13,
                15, RAYWHITE
            );

            DrawRectangleRounded(
                chunkButton, 0.22f, 12,
                C(42, 75, 105, 255)
            );

            DrawText(
                TextFormat("CHUNK DISTANCE: %d / 20", chunkDistance),
                chunkButton.x+15,
                chunkButton.y+11,
                17, RAYWHITE
            );

            DrawText(
                "CAMERA SENSITIVITY",
                panelX+25, panelY+510,
                16, RAYWHITE
            );

            DrawText(
                TextFormat("%.1f", lookSensitivity),
                panelX+panelWidth-70,
                panelY+510,
                20, C(95, 195, 255, 255)
            );

            float sliderValue =
                (lookSensitivity-0.1f)/9.9f;

            DrawRectangleRounded(
                (Rectangle){
                    slider.x, slider.y-5,
                    slider.width, 22
                },
                0.5f, 12,
                C(53, 69, 91, 255)
            );

            DrawRectangleRounded(
                (Rectangle){
                    slider.x, slider.y-5,
                    slider.width*sliderValue, 22
                },
                0.5f, 12,
                C(52, 164, 238, 255)
            );

            DrawCircle(
                (int)(slider.x+slider.width*sliderValue),
                (int)(slider.y+6),
                15, RAYWHITE
            );

            DrawText(
                "0.1",
                slider.x, slider.y+18,
                14, C(160, 180, 200, 255)
            );

            DrawText(
                "10",
                slider.x+slider.width-20,
                slider.y+18,
                14, C(160, 180, 200, 255)
            );

            DrawRectangleRounded(
                resetButton, 0.25f, 14,
                C(57, 75, 98, 255)
            );

            DrawText(
                "RESET",
                resetButton.x+resetButton.width/2-34,
                resetButton.y+15,
                18, RAYWHITE
            );

            DrawRectangleRounded(
                closeButton, 0.25f, 14,
                C(35, 133, 218, 255)
            );

            DrawText(
                "CLOSE",
                closeButton.x+closeButton.width/2-34,
                closeButton.y+15,
                18, RAYWHITE
            );
        }

        if (editControls)
        {
            DrawRectangle(
                0, 0, screenWidth, screenHeight,
                C(0, 0, 0, 125)
            );

            DrawText(
                "MOVE YOUR CONTROLS",
                screenWidth/2-145, 12,
                20, RAYWHITE
            );

            const char *controlNames[3] = {"HIT", "RUN", "BAG"};
            for (int control = 0; control < 3; control++)
            {
                DrawRectangleRounded(editSelectButtons[control], 0.18f, 8,
                    selectedActionControl == control ? C(39, 112, 158, 245) : C(20, 39, 58, 235));
                DrawRectangleRoundedLinesEx(editSelectButtons[control], 0.18f, 8, 1.5f,
                    selectedActionControl == control ? C(150, 220, 255, 255) : C(100, 150, 190, 220));
                int labelW = MeasureText(controlNames[control], 15);
                DrawText(controlNames[control],
                    (int)(editSelectButtons[control].x+editSelectButtons[control].width/2-labelW/2),
                    (int)(editSelectButtons[control].y+11), 15, RAYWHITE);
            }

            Rectangle adjustButtons[4] = {editSizeMinus, editSizePlus, editAlphaMinus, editAlphaPlus};
            const char *adjustLabels[4] = {"SIZE -", "SIZE +", "ALPHA -", "ALPHA +"};
            for (int control = 0; control < 4; control++)
            {
                DrawRectangleRounded(adjustButtons[control], 0.18f, 8, C(24, 55, 78, 245));
                DrawRectangleRoundedLinesEx(adjustButtons[control], 0.18f, 8, 1.2f, C(115, 180, 215, 230));
                DrawText(adjustLabels[control], (int)adjustButtons[control].x+12,
                    (int)adjustButtons[control].y+11, 13, RAYWHITE);
            }

            DrawText(TextFormat("SIZE: %d%%   OPACITY: %d%%",
                (int)(actionButtonScale[selectedActionControl]*100.0f),
                actionButtonAlpha[selectedActionControl]*100/255),
                12, 194, 14, C(190, 220, 240, 255));

            // Показываем сами кнопки в режиме редактирования, чтобы было
            // понятно, какую именно область пользователь перетаскивает.
            DrawRectangleRounded(interactButton, 0.25f, 14,
                C(185, 75, 55, (unsigned char)actionButtonAlpha[0]));
            DrawText("HIT",
                (int)(interactButton.x+interactButton.width/2-MeasureText("HIT", 20)/2),
                (int)(interactButton.y+interactButton.height/2-10), 20,
                C(255, 255, 255, (unsigned char)actionButtonAlpha[0]));
            DrawSurvivalButtonAlpha(sprintButton, "RUN", 18, false,
                (unsigned char)actionButtonAlpha[1]);
            DrawSurvivalButtonAlpha(inventoryButton, "BAG", 13, false,
                (unsigned char)actionButtonAlpha[2]);

            DrawRectangleLinesEx(interactButton, 2, C(255, 210, 120, 230));
            DrawRectangleLinesEx(sprintButton, 2, C(255, 210, 120, 230));
            DrawRectangleLinesEx(inventoryButton, 2, C(255, 210, 120, 230));

            DrawRectangleLinesEx(
                settingsButton, 3,
                C(80, 200, 255, 255)
            );

            DrawRectangleLinesEx(
                cameraButton, 3,
                C(80, 200, 255, 255)
            );

            DrawCircleLines(
                (int)joystickPosition.x,
                (int)joystickPosition.y,
                joystickRadius+5,
                C(80, 200, 255, 255)
            );

            DrawRectangleRounded(
                doneButton, 0.25f, 14,
                C(35, 133, 218, 255)
            );

            DrawText(
                "DONE",
                doneButton.x+74,
                doneButton.y+20,
                22, RAYWHITE
            );
        }

        EndDrawing();
    }

    UnloadRenderTexture(sceneTexture);
    CloseWindow();

    return 0;
}

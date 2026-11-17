#include "raymob.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOUCHES 10
#define MAX_BARRELS 14
#define MAX_BUILD_PIECES 64
#define MAX_TREES 256
#define MAX_WOOD_DROPS 64
#define MAX_ORE_NODES 16
#define MAX_ORE_DROPS 16

// Текущие залежи руды также участвуют в коллизиях игрока.
static Vector3 gOrePositions[MAX_ORE_NODES];
static bool gOreActive[MAX_ORE_NODES];

typedef struct BuildSaveData
{
    int count, woodCount;
    int types[MAX_BUILD_PIECES], doorOpen[MAX_BUILD_PIECES];
    float x[MAX_BUILD_PIECES], z[MAX_BUILD_PIECES];
} BuildSaveData;

static const char *GetBuildFilePath(void)
{
    return TextFormat("%sbolt_survival_build.bin", GetApplicationDirectory());
}

static void SaveBuildData(Vector3 pieces[MAX_BUILD_PIECES], int types[MAX_BUILD_PIECES],
                          bool doors[MAX_BUILD_PIECES], int count, int wood)
{
    BuildSaveData data = {0};
    data.count = count; data.woodCount = wood;
    for (int i = 0; i < count && i < MAX_BUILD_PIECES; i++)
    {
        data.types[i] = types[i]; data.doorOpen[i] = doors[i] ? 1 : 0;
        data.x[i] = pieces[i].x; data.z[i] = pieces[i].z;
    }
    SaveFileData(GetBuildFilePath(), &data, sizeof(data));
}

static void LoadBuildData(Vector3 pieces[MAX_BUILD_PIECES], int types[MAX_BUILD_PIECES],
                          bool doors[MAX_BUILD_PIECES], int *count, int *wood)
{
    int size = 0;
    unsigned char *bytes = LoadFileData(GetBuildFilePath(), &size);
    if (bytes == NULL) return;
    if (size == (int)sizeof(BuildSaveData))
    {
        BuildSaveData data;
        memcpy(&data, bytes, sizeof(data));
        if (data.count >= 0 && data.count <= MAX_BUILD_PIECES &&
            data.woodCount >= 0 && data.woodCount <= 99)
        {
            *count = data.count; *wood = data.woodCount;
            for (int i = 0; i < data.count; i++)
            {
                if (data.types[i] < 0 || data.types[i] > 4) continue;
                pieces[i] = (Vector3){data.x[i], 0.0f, data.z[i]};
                types[i] = data.types[i]; doors[i] = data.doorOpen[i] != 0;
            }
        }
    }
    UnloadFileData(bytes);
}



static int FindNearestWoodDrop(Vector3 player, Vector3 drops[MAX_WOOD_DROPS], bool active[MAX_WOOD_DROPS])
{
    int nearest=-1; float best=2.8f*2.8f;
    for(int i=0;i<MAX_WOOD_DROPS;i++){if(!active[i])continue;float dx=drops[i].x-player.x,dz=drops[i].z-player.z,d2=dx*dx+dz*dz;if(d2<best){best=d2;nearest=i;}}
    return nearest;
}

static int FindNearestOreDrop(Vector3 player, Vector3 drops[MAX_ORE_DROPS], bool active[MAX_ORE_DROPS])
{
    int nearest=-1; float best=2.8f*2.8f;
    for(int i=0;i<MAX_ORE_DROPS;i++){if(!active[i])continue;float dx=drops[i].x-player.x,dz=drops[i].z-player.z,d2=dx*dx+dz*dz;if(d2<best){best=d2;nearest=i;}}
    return nearest;
}

static Color GetOreColor(int type)
{
    if (type == 0) return (Color){184, 111, 67, 255};   // железо
    if (type == 1) return (Color){48, 91, 205, 255};    // лазурит
    if (type == 2) return (Color){224, 194, 49, 255};   // сера
    return (Color){205, 211, 218, 255};                 // кремний
}

static const char *GetOreName(int type)
{
    if (type == 0) return "IRON";
    if (type == 1) return "LAPIS";
    if (type == 2) return "SULFUR";
    return "SILICON";
}

static Color C(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return (Color){r, g, b, a};
}


#define SETTINGS_STORAGE_VERSION 3
#define SETTINGS_STORAGE_SLOTS 131

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
                             Rectangle settingsButton, Rectangle cameraButton, Rectangle mapButton,
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
    SaveStorageValue(129, (int)(mapButton.x*10.0f));
    SaveStorageValue(130, (int)(mapButton.y*10.0f));
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
                             Rectangle *settingsButton, Rectangle *cameraButton, Rectangle *mapButton,
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
    mapButton->x = LoadStorageValue(129)/10.0f;
    mapButton->y = LoadStorageValue(130)/10.0f;
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

static void DrawTree(float x, float z, float scale, int quality, int biome)
{
    // 0 — лесная ель, 1 — заснеженная ель, 2 — песочная пальма.
    if (biome == 2)
    {
        Color trunk = C(133, 101, 61, 255);
        Color leaf = C(83, 132, 65, 255);
        DrawCylinderEx((Vector3){x, 0.08f, z},
                       (Vector3){x+0.20f*scale, 2.75f*scale, z},
                       0.22f*scale, 0.13f*scale, 7, trunk);
        for (int arm=0; arm<5; arm++)
        {
            float a = arm*1.2566f;
            Vector3 start = {x+0.20f*scale, 2.55f*scale, z};
            Vector3 end = {x+0.20f*scale+cosf(a)*1.35f*scale,
                           2.75f*scale+sinf(a)*0.20f*scale,
                           z+sinf(a)*1.35f*scale};
            DrawCylinderEx(start, end, 0.10f*scale, 0.025f*scale, 5, leaf);
            if (quality >= 1)
                DrawSphere((Vector3){end.x, end.y+0.06f*scale, end.z},
                           0.20f*scale, C(111, 151, 73, 255));
        }
        return;
    }

    Color bark = C(105, 69, 40, 255);
    Color lower = (biome == 1) ? C(202, 218, 222, 255) : C(35, 105, 54, 255);
    Color middle = (biome == 1) ? C(222, 233, 235, 255) : C(47, 128, 62, 255);
    Color upper = (biome == 1) ? C(238, 244, 244, 255) : C(66, 145, 72, 255);
    DrawCylinder((Vector3){x, 1.12f*scale, z},
                 0.16f*scale, 0.23f*scale, 2.25f*scale, 7, bark);
    DrawCylinder((Vector3){x, 0.18f*scale, z},
                 0.28f*scale, 0.18f*scale, 0.36f*scale, 7, C(91, 59, 36, 255));
    DrawCylinder((Vector3){x, 1.75f*scale, z},
                 0.0f, 1.05f*scale, 1.85f*scale, 7, lower);
    DrawCylinder((Vector3){x, 2.30f*scale, z},
                 0.0f, 0.82f*scale, 1.55f*scale, 7, middle);
    if (quality >= 1)
        DrawCylinder((Vector3){x, 2.82f*scale, z},
                     0.0f, 0.58f*scale, 1.15f*scale, 7, upper);
    if (quality >= 2)
    {
        DrawSphere((Vector3){x-0.28f*scale, 1.75f*scale, z+0.18f*scale},
                   0.34f*scale, lower);
        DrawSphere((Vector3){x+0.25f*scale, 2.25f*scale, z-0.16f*scale},
                   0.29f*scale, middle);
    }
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

static void DrawGasCrate(Vector3 p, bool weapon)
{
    Color base=weapon?C(75,91,76,255):C(119,83,49,255);
    Color edge=weapon?C(42,55,47,255):C(75,51,34,255);
    DrawCube(p,1.05f,0.82f,0.9f,base);
    DrawCube((Vector3){p.x,p.y+0.43f,p.z},1.1f,0.10f,0.95f,edge);
    DrawCube((Vector3){p.x,p.y,p.z-0.47f},0.12f,0.72f,0.045f,edge);
    DrawCube((Vector3){p.x-0.48f,p.y,p.z},0.045f,0.72f,0.12f,edge);
    if(weapon) DrawCube((Vector3){p.x,p.y+0.02f,p.z-0.51f},0.48f,0.24f,0.025f,C(166,133,66,255));
}

static void DrawMiniGasStation(float x,float z,int quality)
{
    Color wall=C(116,112,99,255),red=C(151,61,48,255),metal=C(48,54,55,255);
    Color shelf=C(83,88,83,255),wood=C(112,83,55,255);

    // Основной корпус: передний фасад с широким входом и витринами.
    DrawCube((Vector3){x,1.75f,z+9.0f},24.0f,3.5f,0.18f,wall);
    DrawCube((Vector3){x-12.0f,1.75f,z},0.18f,3.5f,18.2f,wall);
    DrawCube((Vector3){x+12.0f,1.75f,z},0.18f,3.5f,18.2f,wall);
    DrawCube((Vector3){x-8.0f,1.75f,z-9.0f},8.0f,3.5f,0.18f,wall);
    DrawCube((Vector3){x+8.0f,1.75f,z-9.0f},8.0f,3.5f,0.18f,wall);
    DrawCube((Vector3){x,3.58f,z},24.4f,0.24f,18.4f,metal);
    DrawCube((Vector3){x,3.86f,z-0.1f},23.8f,0.26f,0.38f,red);

    // Передние витрины, вход остаётся свободным по центру.
    for(int side=-1;side<=1;side+=2)
    {
        float wx=x+side*8.0f;
        DrawCube((Vector3){wx,2.0f,z-9.12f},5.2f,2.45f,0.055f,C(75,139,151,175));
        DrawCube((Vector3){wx-2.65f,2.0f,z-9.16f},0.10f,2.65f,0.10f,metal);
        DrawCube((Vector3){wx+2.65f,2.0f,z-9.16f},0.10f,2.65f,0.10f,metal);
        DrawCube((Vector3){wx,3.30f,z-9.16f},5.4f,0.10f,0.10f,metal);
        DrawCube((Vector3){wx,0.70f,z-9.16f},5.4f,0.10f,0.10f,metal);
        DrawCube((Vector3){wx,2.0f,z-9.18f},0.07f,2.45f,0.07f,metal);
    }
    // Металлическая рама входа.
    DrawCube((Vector3){x-2.0f,1.55f,z-9.12f},0.12f,3.05f,0.12f,metal);
    DrawCube((Vector3){x+2.0f,1.55f,z-9.12f},0.12f,3.05f,0.12f,metal);
    DrawCube((Vector3){x,3.08f,z-9.12f},4.1f,0.12f,0.12f,metal);
    DrawCube((Vector3){x+1.85f,1.45f,z-9.18f},0.055f,0.85f,0.045f,C(190,198,190,255));

    // Внутренние перегородки: торговый зал и две небольшие подсобки.
    // Три отдельные комнаты в задней части большой заправки, с дверными проёмами.
    for(int side=-1;side<=1;side+=2)
    {
        float rx=x+side*4.0f;
        DrawCube((Vector3){rx,1.35f,z+3.15f},0.14f,2.7f,1.3f,wall);
        DrawCube((Vector3){rx,1.35f,z+7.25f},0.14f,2.7f,2.5f,wall);
        DrawCube((Vector3){rx,1.35f,z+4.45f},0.12f,2.7f,0.12f,metal);
        DrawCube((Vector3){rx,1.35f,z+5.15f},0.12f,2.7f,0.12f,metal);
    }
    DrawCube((Vector3){x,1.35f,z+8.0f},7.9f,2.7f,0.14f,wall);

    // Полки вдоль стен торгового зала, на них позже появится отдельный лут.
    for(int side=-1;side<=1;side+=2)
    {
        float sx=x+side*10.7f;
        for(int row=0;row<3;row++)
        {
            float sz=z-3.0f+row*2.0f;
            DrawCube((Vector3){sx,0.95f,sz},0.16f,1.85f,0.16f,shelf);
            DrawCube((Vector3){sx+side*0.62f,0.95f,sz},0.16f,1.85f,0.16f,shelf);
            for(int level=0;level<3;level++)
                DrawCube((Vector3){sx+side*0.31f,0.42f+level*0.62f,sz},
                         1.35f,0.10f,1.05f,wood);
        }
    }
    // Стеллажи в подсобках.
    for(int side=-1;side<=1;side+=2)
    {
        float sx=x+side*8.0f;
        DrawCube((Vector3){sx,1.0f,z+6.5f},3.0f,0.12f,0.75f,shelf);
        DrawCube((Vector3){sx,1.75f,z+6.5f},3.0f,0.12f,0.75f,shelf);
        DrawCube((Vector3){sx-1.4f,1.0f,z+6.5f},0.12f,2.0f,0.8f,metal);
        DrawCube((Vector3){sx+1.4f,1.0f,z+6.5f},0.12f,2.0f,0.8f,metal);
    }

    // Навес и топливные колонки.
    DrawCube((Vector3){x,4.35f,z+15.0f},29.0f,0.38f,19.0f,C(89,86,76,255));
    DrawCube((Vector3){x,4.58f,z+15.0f},28.7f,0.14f,18.7f,red);
    for(int side=-1;side<=1;side+=2)for(int row=0;row<2;row++)
        DrawCube((Vector3){x+side*13.0f,2.15f,z+6.0f+row*9.5f},0.22f,4.1f,0.22f,metal);
    for(int pump=0;pump<2;pump++)
    {
        float px=x+(pump==0?-6.0f:6.0f),pz=z+13.0f;
        DrawCube((Vector3){px,1.05f,pz},0.95f,1.9f,0.82f,C(151,145,126,255));
        DrawCube((Vector3){px,1.63f,pz-0.43f},0.58f,0.42f,0.035f,C(29,43,42,255));
        DrawCube((Vector3){px,1.64f,pz-0.46f},0.42f,0.19f,0.025f,C(95,174,132,255));
        DrawCylinderEx((Vector3){px+0.49f,1.7f,pz},(Vector3){px+0.72f,1.0f,pz-0.18f},0.055f,0.045f,6,C(25,27,27,255));
    }

    // Лестница на правой стене. Ступени развёрнуты поперёк стены.
    for(int rung=0;rung<9;rung++)
        DrawCube((Vector3){x+12.25f,0.28f+rung*0.40f,z-4.5f},0.16f,0.10f,1.0f,C(105,108,101,255));
    DrawCylinderEx((Vector3){x+12.25f,0.1f,z-5.0f},(Vector3){x+12.25f,3.85f,z-5.0f},0.075f,0.075f,6,metal);
    DrawCylinderEx((Vector3){x+12.25f,0.1f,z-4.0f},(Vector3){x+12.25f,3.85f,z-4.0f},0.075f,0.075f,6,metal);
    DrawCube((Vector3){x+11.7f,3.55f,z-4.5f},1.1f,0.14f,1.25f,metal);

    // Старые три ящика пока оставлены в магазине; будущий лут будет на полках.
    DrawGasCrate((Vector3){x-3.8f,0.48f,z+1.2f},false);
    DrawGasCrate((Vector3){x,0.48f,z+1.2f},false);
    DrawGasCrate((Vector3){x+3.8f,0.48f,z+1.2f},false);
    DrawGasCrate((Vector3){x,4.05f,z},true);

    // Сломанный грузовик возле главного входа: кузов, кабина, капот и колёса.
    float tx=x-3.8f,tz=z-7.3f;
    DrawCube((Vector3){tx,0.82f,tz},3.7f,0.72f,1.65f,C(91,103,91,255));
    DrawCube((Vector3){tx+0.85f,1.48f,tz+0.05f},1.55f,1.15f,1.55f,C(82,94,87,255));
    DrawCube((Vector3){tx+1.75f,1.15f,tz-0.02f},0.55f,0.35f,1.7f,C(111,104,84,255));
    DrawCube((Vector3){tx+0.85f,1.65f,tz-0.78f},0.92f,0.52f,0.035f,C(52,75,79,170));
    DrawCube((Vector3){tx-0.9f,1.12f,tz},1.65f,0.08f,1.72f,C(68,73,68,255));
    for(int side=-1;side<=1;side+=2)
        for(int axle=0;axle<2;axle++)
        {
            float wx=tx+(axle==0?-1.15f:1.15f);
            float wz=tz+side*0.82f;
            DrawCylinderEx((Vector3){wx-0.20f,0.48f,wz},
                           (Vector3){wx+0.20f,0.48f,wz},
                           0.43f,0.43f,8,C(29,31,31,255));
            DrawCylinderEx((Vector3){wx-0.22f,0.48f,wz},
                           (Vector3){wx+0.22f,0.48f,wz},
                           0.19f,0.19f,8,C(105,107,101,255));
        }
    DrawCube((Vector3){tx+1.8f,0.65f,tz-0.82f},0.65f,0.12f,0.18f,C(120,48,39,255));
    DrawCube((Vector3){tx+1.8f,0.65f,tz+0.82f},0.65f,0.12f,0.18f,C(120,48,39,255));

    DrawCube((Vector3){x,0.015f,z+8.0f},34.0f,0.035f,36.0f,C(62,65,61,255));
}

static bool IsGasStationBlocked(float x,float z,float radius,float playerY)
{
    // На крыше можно пройти за край и упасть. На земле стены здания блокируют проход.
    if(playerY<2.9f)
    {
        if(fabsf(x)<12.0f+radius && fabsf(z+13.0f)<0.24f+radius) return true;
        if(fabsf(x)<12.0f+radius && fabsf(z+31.0f)<0.24f+radius && fabsf(x)>2.0f) return true;
        if(fabsf(z+22.0f)<9.0f+radius && fabsf(fabsf(x)-12.0f)<0.24f+radius &&
           !(fabsf(x-12.25f)<0.9f && z>-28.0f && z<-25.0f)) return true;
        // Перегородки трёх задних комнат с дверными проёмами.
        for(int side=-1;side<=1;side+=2)
        {
            float wallX=side*4.0f;
            if(fabsf(x-wallX)<0.24f+radius && z>-19.5f && z<-18.2f) return true;
            if(fabsf(x-wallX)<0.24f+radius && z>-16.0f && z<-13.5f) return true;
        }
        if(fabsf(z+14.0f)<0.24f+radius && fabsf(x)<4.0f+radius) return true;
    }
    // Коллизии трёх ящиков в торговом зале.
    if(playerY<1.35f)
    {
        const float crateX[3]={-3.8f,0.0f,3.8f};
        for(int i=0;i<3;i++)
        {
            float dx=x-crateX[i], dz=z+20.8f;
            float crateRadius=0.72f+radius;
            if(dx*dx+dz*dz<crateRadius*crateRadius) return true;
        }
    }
    // Опоры навеса и бензоколонки.
    const float posts[4][2]={{-9.5f,-18.3f},{9.5f,-18.3f},{-9.5f,-8.8f},{9.5f,-8.8f}};
    for(int i=0;i<4;i++){float dx=x-posts[i][0],dz=z-posts[i][1];if(dx*dx+dz*dz<(0.22f+radius)*(0.22f+radius))return true;}
    for(int i=0;i<2;i++){float px=(i==0)?-3.4f:3.4f,dx=x-px,dz=z+15.0f;if(dx*dx+dz*dz<(0.58f+radius)*(0.58f+radius))return true;}
    return false;
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

// Невысокие холмы с пологими склонами и небольшими каменными уступами.
static void DrawMountain(float x, float z, float scale, bool snowy)
{
    Color slope = snowy ? C(116, 127, 126, 255) : C(91, 112, 83, 255);
    Color ledge = snowy ? C(153, 163, 160, 255) : C(112, 119, 105, 255);
    Color rock = snowy ? C(190, 199, 196, 255) : C(126, 132, 122, 255);

    // Низкие широкие основания формируют плавные подгорки.
    DrawCylinderEx((Vector3){x, 0.0f, z},
                   (Vector3){x+1.0f*scale, 4.2f*scale, z+1.0f*scale},
                   15.0f*scale, 5.0f*scale, 10, slope);
    DrawCylinderEx((Vector3){x-10.0f*scale, 0.0f, z+7.0f*scale},
                   (Vector3){x-9.0f*scale, 2.7f*scale, z+7.5f*scale},
                   9.0f*scale, 3.0f*scale, 9, slope);
    DrawCylinderEx((Vector3){x+11.0f*scale, 0.0f, z-5.0f*scale},
                   (Vector3){x+11.5f*scale, 3.2f*scale, z-5.0f*scale},
                   10.0f*scale, 3.5f*scale, 9, slope);

    // Небольшие уступы и обнажённые каменные пласты.
    DrawCube((Vector3){x-5.5f*scale, 1.65f*scale, z+5.0f*scale},
             8.0f*scale, 0.65f*scale, 2.0f*scale, ledge);
    DrawCube((Vector3){x+5.0f*scale, 2.35f*scale, z-2.5f*scale},
             6.0f*scale, 0.55f*scale, 2.2f*scale, ledge);
    DrawRock(x+2.5f*scale, 2.0f*scale, z-7.0f*scale, 1.25f*scale);
    DrawRock(x-8.0f*scale, 1.0f*scale, z-3.0f*scale, 0.9f*scale);

    if (snowy)
        DrawSphere((Vector3){x+1.0f*scale, 4.0f*scale, z+1.0f*scale},
                   2.2f*scale, rock);
}

// Более крупные залежи с хорошо заметными цветными вкраплениями.
static void DrawOreNode(float x, float z, int type)
{
    // Увеличиваем всю модель залежи ровно в 2 раза от основания.
    const float s = 2.0f;
    Color ore = GetOreColor(type);
    DrawSphere((Vector3){x, 0.34f*s, z}, 0.53f*s, C(79, 84, 87, 255));
    DrawSphere((Vector3){x-0.30f*s, 0.25f*s, z+0.19f*s}, 0.32f*s, C(103, 108, 108, 255));
    DrawSphere((Vector3){x+0.29f*s, 0.23f*s, z-0.20f*s}, 0.28f*s, C(91, 97, 98, 255));
    DrawCube((Vector3){x-0.15f*s, 0.67f*s, z-0.14f*s}, 0.23f*s, 0.20f*s, 0.21f*s, ore);
    DrawCube((Vector3){x+0.22f*s, 0.49f*s, z+0.18f*s}, 0.20f*s, 0.17f*s, 0.19f*s, ore);
    DrawCube((Vector3){x+0.02f*s, 0.78f*s, z+0.16f*s}, 0.15f*s, 0.14f*s, 0.15f*s, ore);
}

static void DrawOreDrop(Vector3 p, int type)
{
    Color ore = GetOreColor(type);
    DrawSphere((Vector3){p.x, 0.16f, p.z}, 0.19f, C(83, 89, 91, 255));
    DrawCube((Vector3){p.x, 0.27f, p.z}, 0.18f, 0.15f, 0.18f, ore);
}

static void DrawCave(float x, float z)
{
    Color dark = C(24, 29, 31, 255);
    Color rock = C(89, 96, 94, 255);
    DrawCube((Vector3){x, 1.25f, z+2.1f}, 2.65f, 2.45f, 0.18f, dark);
    DrawCube((Vector3){x-1.55f, 1.0f, z}, 0.72f, 2.0f, 4.8f, rock);
    DrawCube((Vector3){x+1.55f, 1.0f, z}, 0.72f, 2.0f, 4.8f, rock);
    DrawCube((Vector3){x, 2.12f, z}, 3.65f, 0.72f, 4.8f, rock);
    DrawRock(x-1.15f, 1.85f, z-1.1f, 0.62f);
    DrawRock(x+1.18f, 1.88f, z-0.8f, 0.58f);
    DrawRock(x-0.65f, 2.35f, z+0.2f, 0.48f);
    DrawRock(x+0.72f, 2.32f, z+0.1f, 0.52f);
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

static bool IsBlockedAt(float x, float z, float playerY,
                           Vector3 *barrels, bool *barrelDestroyed)
{
    const float playerRadius = 0.42f;
    // Берег острова: игрок не может уйти за пределы игровой карты.
    if (x*x + z*z > 260.0f*260.0f) return true;

    // Маленькие рудные залежи имеют физическую коллизию.
    for (int ore = 0; ore < MAX_ORE_NODES; ore++)
    {
        if (!gOreActive[ore]) continue;
        float dx = x-gOrePositions[ore].x;
        float dz = z-gOrePositions[ore].z;
        float radius = playerRadius+0.66f;
        if (dx*dx+dz*dz < radius*radius) return true;
    }

    if(IsGasStationBlocked(x,z,playerRadius,playerY)) return true;

    // Стены небольших пещер: центр входа оставлен свободным.
    const Vector3 caveSites[3] = {{-140,0,-42}, {140,0,18}, {122,0,122}};
    for (int cv=0; cv<3; cv++)
    {
        float dx=x-caveSites[cv].x, dz=z-caveSites[cv].z;
        if (fabsf(dz) < 2.65f && fabsf(dx) > 1.05f && fabsf(dx) < 2.15f) return true;
        if (fabsf(dx) < 1.75f && dz > 2.15f && dz < 2.65f) return true;
    }

    const Vector3 oreSites[15] = {
        {-112,0,-72}, {-145,0,42}, {-82,0,132}, {-34,0,158},
        {78,0,-142}, {142,0,-76}, {154,0,78}, {92,0,138},
        {-156,0,-8}, {42,0,148},
        {118,0,-112}, {154,0,36}, {-132,0,112}, {-92,0,-142}, {32,0,-166}
    };
    for (int o=0; o<15; o++)
    {
        float dx=x-oreSites[o].x, dz=z-oreSites[o].z;
        if (dx*dx+dz*dz < 0.62f*0.62f) return true;
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

static bool IsCameraBlockedAt(float x, float z,
                                 Vector3 *barrels, bool *barrelDestroyed)
{
    if (x*x + z*z > 265.0f*265.0f) return true;
    // Камера проходит ближе к объектам, чем игрок: небольшой отступ
    // не позволяет ей резко отскакивать от невидимой широкой зоны.
    const float cameraRadius = 0.12f;

    if(IsGasStationBlocked(x,z,cameraRadius,0.0f)) return true;

    for (int b = 0; b < MAX_BARRELS; b++)
    {
        if (barrelDestroyed[b]) continue;
        float dx = x-barrels[b].x;
        float dz = z-barrels[b].z;
        float minDist = 0.39f+cameraRadius;
        if (dx*dx+dz*dz < minDist*minDist) return true;
    }
    return false;
}

static void GetBuildPlacement(Vector3 player, float angle, int type,
                              Vector3 *pieces, int *pieceTypes, int pieceCount,
                              float *outX, float *outZ, int *outStoredType)
{
    float x = roundf((player.x + sinf(angle)*3.2f)/2.5f)*2.5f;
    float z = roundf((player.z - cosf(angle)*3.2f)/2.5f)*2.5f;
    int storedType = type;

    // Стена должна вставать на край уже установленного фундамента,
    // а не в его центр. Поворот стены выбирается по стороне подхода.
    if (type == 1)
    {
        // Стена разрешена только рядом с существующим фундаментом.
        // Ищем фундамент около точки прицеливания, а не разрешаем
        // оставить стену на произвольной клетке в воздухе.
        int foundation = -1;
        float nearest = 4.25f*4.25f;
        for (int i = 0; i < pieceCount; i++)
        {
            if (pieceTypes[i] != 0) continue;
            float dx = x-pieces[i].x;
            float dz = z-pieces[i].z;
            float d2 = dx*dx + dz*dz;
            if (d2 < nearest) { nearest = d2; foundation = i; }
        }

        if (foundation < 0)
        {
            storedType = -1; // нет опоры — предпросмотр будет красным
        }
        else
        {
            // Выбираем ближайшую из четырёх сторон фундамента.
            // Направление определяется положением игрока, поэтому
            // игроку достаточно подойти к нужной стороне и нажать.
            float dx = player.x-pieces[foundation].x;
            float dz = player.z-pieces[foundation].z;
            if (fabsf(dx) > fabsf(dz))
            {
                x = pieces[foundation].x + (dx >= 0.0f ? 1.25f : -1.25f);
                z = pieces[foundation].z;
                storedType = 4;
            }
            else
            {
                x = pieces[foundation].x;
                z = pieces[foundation].z + (dz >= 0.0f ? 1.25f : -1.25f);
                storedType = 1;
            }
        }
    }

    *outX = x;
    *outZ = z;
    *outStoredType = storedType;
}

static bool PlaceBuildPiece(Vector3 player, float angle, int type,
                               Vector3 *pieces, int *pieceTypes, int *pieceCount,
                               int *woodCount, Vector3 *barrels,
                               bool *barrelDestroyed)
{
    if (*pieceCount >= MAX_BUILD_PIECES) return false;
    int cost = (type == 0) ? 30 : ((type == 1 || type == 3) ? 15 : 0);
    if (type != 2 && *woodCount < cost) return false;

    float x, z;
    int storedType;
    GetBuildPlacement(player, angle, type, pieces, pieceTypes, *pieceCount,
                      &x, &z, &storedType);
    if (storedType < 0) return false;
    if (fabsf(x-player.x) < 2.0f && fabsf(z-player.z) < 2.0f) return false;
    if (IsBlockedAt(x, z, player.y, barrels, barrelDestroyed)) return false;
    for (int i = 0; i < *pieceCount; i++)
    {
        float dx = pieces[i].x-x, dz = pieces[i].z-z;
        if (dx*dx+dz*dz < 0.01f && pieceTypes[i] == storedType) return false;
    }
    pieces[*pieceCount] = (Vector3){x, 0.0f, z};
    pieceTypes[*pieceCount] = storedType;
    (*pieceCount)++;
    if (type != 2) *woodCount -= cost;
    return true;
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
    float swing = moving ? sinf(walkPhase)*0.30f : sinf(walkPhase)*0.025f;
    float bob = moving ? fabsf(cosf(walkPhase))*0.035f : 0.0f;

    Color skin = C(211, 164, 125, 255);
    Color skinLight = C(231, 190, 151, 255);
    Color underwear = C(67, 79, 105, 255);
    Color underwearDark = C(43, 53, 75, 255);

    // Ноги и ступни — персонаж без обуви и верхней одежды.
    Vector3 hipL = PlayerPoint(player, angle, -0.17f, 0.73f, 0.0f);
    Vector3 hipR = PlayerPoint(player, angle,  0.17f, 0.73f, 0.0f);
    Vector3 kneeL = PlayerPoint(player, angle, -0.18f, 0.39f, swing*0.55f);
    Vector3 kneeR = PlayerPoint(player, angle,  0.18f, 0.39f, -swing*0.55f);
    Vector3 ankleL = PlayerPoint(player, angle, -0.18f, 0.13f, swing);
    Vector3 ankleR = PlayerPoint(player, angle,  0.18f, 0.13f, -swing);

    DrawCylinderEx(hipL, kneeL, 0.145f, 0.115f, 10, skin);
    DrawCylinderEx(kneeL, ankleL, 0.115f, 0.09f, 10, skin);
    DrawCylinderEx(hipR, kneeR, 0.145f, 0.115f, 10, skin);
    DrawCylinderEx(kneeR, ankleR, 0.115f, 0.09f, 10, skin);
    DrawSphere(kneeL, 0.105f, skin);
    DrawSphere(kneeR, 0.105f, skin);

    // Простые шорты-бельё без ремней и карманов.
    DrawCube(PlayerPoint(player, angle, 0, 0.73f, 0),
             0.53f, 0.31f, 0.37f, underwear);
    DrawCube(PlayerPoint(player, angle, 0, 0.565f, -0.012f),
             0.49f, 0.035f, 0.375f, underwearDark);

    // Торс от третьего лица: отдельная цельная геометрия от талии
    // до плеч, чтобы корпус не терялся из-за тонкого цилиндра.
    DrawCylinderEx(
        PlayerPoint(player, angle, 0, 0.82f+bob, 0),
        PlayerPoint(player, angle, 0, 1.48f+bob, 0),
        0.245f,  0.315f, 12, skin
    );
    DrawSphere(PlayerPoint(player, angle, 0, 1.20f+bob, 0),
               0.285f, skin);
    DrawSphere(PlayerPoint(player, angle, 0, 1.49f+bob, 0),
               0.22f, skin);
    DrawCylinderEx(
        PlayerPoint(player, angle, 0, 1.45f+bob, 0),
        PlayerPoint(player, angle, 0, 1.66f+bob, 0),
        0.12f, 0.105f, 10, skin
    );

    // Руки полностью без рукавов.
    Vector3 shoulderL = PlayerPoint(player, angle, -0.36f, 1.42f+bob, 0);
    Vector3 elbowL = PlayerPoint(player, angle, -0.45f, 1.05f+bob, -swing*0.65f);
    Vector3 wristL = PlayerPoint(player, angle, -0.43f, 0.82f+bob, -swing*0.9f);
    Vector3 shoulderR = PlayerPoint(player, angle, 0.36f, 1.42f+bob, 0);
    Vector3 elbowR = PlayerPoint(player, angle, 0.45f, 1.05f+bob,
                                 swing*0.65f-attackProgress*0.24f);
    Vector3 wristR = PlayerPoint(player, angle, 0.43f, 0.82f+bob,
                                 swing*0.9f-attackProgress*0.72f);

    DrawSphere(shoulderL, 0.145f, skin);
    DrawSphere(shoulderR, 0.145f, skin);
    DrawCylinderEx(shoulderL, elbowL, 0.115f, 0.09f, 10, skin);
    DrawCylinderEx(shoulderR, elbowR, 0.115f, 0.09f, 10, skin);
    DrawSphere(elbowL, 0.09f, skin);
    DrawSphere(elbowR, 0.09f, skin);
    DrawCylinderEx(elbowL, wristL, 0.085f, 0.068f, 10, skin);
    DrawCylinderEx(elbowR, wristR, 0.085f, 0.068f, 10, skin);
    DrawSphere(wristL, 0.075f, skinLight);
    DrawSphere(wristR, 0.075f, skinLight);

    // Голова без кепки, с простыми волосами и лицом.
    DrawSphere(PlayerPoint(player, angle, 0, 1.86f+bob, 0),
               0.265f, skin);
    DrawSphere(PlayerPoint(player, angle, -0.265f, 1.86f+bob, 0),
               0.065f, skin);
    DrawSphere(PlayerPoint(player, angle, 0.265f, 1.86f+bob, 0),
               0.065f, skin);
    DrawSphere(PlayerPoint(player, angle, 0, 2.045f+bob, 0.035f),
               0.205f, C(65, 48, 35, 255));

    DrawSphere(PlayerPoint(player, angle, -0.092f, 1.88f+bob, -0.245f),
               0.027f, C(37, 42, 37, 255));
    DrawSphere(PlayerPoint(player, angle, 0.092f, 1.88f+bob, -0.245f),
               0.027f, C(37, 42, 37, 255));
    DrawSphere(PlayerPoint(player, angle, 0, 1.81f+bob, -0.275f),
               0.045f, skinLight);
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
    Vector3 thirdPersonCameraPosition = {0.0f, 3.0f, -7.0f};
    bool thirdPersonCameraReady = false;
    float lookSensitivity = 3.0f;
    float playerSpeed = 0.08f;
    float walkPhase = 0.0f;
    float thirdPersonBobPhase = 0.0f;
    float thirdPersonBobBlend = 0.0f;

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
    bool mapOpen = false;
    bool buildMode = false;
    int selectedBuildType = 0; // 0 FOUNDATION, 1 WALL, 2 DOOR, 3 ROOF.
    int selectedHotbarSlot = 0;
    // 0 AXE, 1 MED, 2 FOOD, 3 WATER, 4 WOOD, 5 STONE, 6 DOOR, 7 ORF PIXCADE,
    // 8 IRON, 9 LAPIS, 10 SULFUR, 11 SILICON.
    const char *itemNames[12] = {"AXE", "MED", "FOOD", "WATER", "WOOD", "STONE", "DOOR", "ORF PIXCADE",
                                 "IRON", "LAPIS", "SULFUR", "SILICON"};
    int itemCounts[12] = {1, 1, 3, 3, 0, 0, 0, 1, 0, 0, 0, 0};
    const int hotbarItemIds[4] = {0, 7, 2, 3}; // AXE, ORF PIXCADE, FOOD, WATER.

    // Главное меню.
    bool mainMenuOpen = true;
    bool loadingScreen = true;
    float loadingProgress = 0.0f;
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

    // Сразу создаём буфер в выбранном разрешении, чтобы при первом
    // входе в игру не было лишнего пересоздания текстуры и скачка памяти.
    float initialRenderScale =
        (resolutionLevel == 0) ? 1.0f :
        ((resolutionLevel == 1) ? 0.85f : 0.70f);

    int renderWidth = (int)(screenWidth * initialRenderScale);
    int renderHeight = (int)(screenHeight * initialRenderScale);

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
    Rectangle mapButton = {screenWidth - 190, 166, 170, 64};

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
    int targetedGasCrate = -1, openedGasCrate = -1;
    bool gasCrateOpen = false;
    int gasCrateItems[4][8] = {{2,3,4,5,8,9,10,11},{2,3,4,5,8,9,10,11},{2,3,4,5,8,9,10,11},{0,1,7,8,9,10,11,4}};
    int gasCrateAmounts[4][8] = {{3,2,18,12,4,2,3,4},{2,3,14,10,3,4,2,3},{3,2,20,8,5,3,2,4},{1,2,1,6,4,5,5,25}};
    Rectangle interactButton = {0};
    Rectangle sprintButton = {0};
    Rectangle inventoryButton = {0};
    Rectangle buildButton = {0};
    Rectangle buildFloorButton = {0}, buildWallButton = {0}, buildDoorButton = {0}, buildRoofButton = {0}, buildPlaceButton = {0};
    Rectangle takeButton = {0};
    Vector3 treePositions[MAX_TREES] = {{0}}, woodDrops[MAX_WOOD_DROPS] = {{0}};
    int treeBiome[MAX_TREES] = {0};
    int treeHealth[MAX_TREES] = {0}, treeCount = 0;
    bool treeChopped[MAX_TREES] = {false}, woodDropActive[MAX_WOOD_DROPS] = {false};
    Vector3 oreDrops[MAX_ORE_DROPS] = {{0}};
    int oreDropType[MAX_ORE_DROPS] = {0}, oreDropAmount[MAX_ORE_DROPS] = {0};
    bool oreDropActive[MAX_ORE_DROPS] = {false};
    int oreHealth[MAX_ORE_NODES] = {0};
    double oreRespawnAt[MAX_ORE_NODES] = {0};
    int nearbyWoodDrop = -1, nearbyOreDrop = -1, targetedTree = -1, targetedOre = -1;
    Vector3 buildPieces[MAX_BUILD_PIECES] = {{0}};
    int buildPieceTypes[MAX_BUILD_PIECES] = {0};
    bool buildDoorOpen[MAX_BUILD_PIECES] = {false};
    int buildPieceCount = 0;
    Rectangle hotbarSlots[4] = {{0}};

    // Настраиваемые положения, размеры и прозрачность игровых кнопок.
    Vector2 actionButtonCenters[3] = {
        {0, 0}, {0, 0}, {0, 0}
    };
    float actionButtonScale[3] = {1.0f, 1.0f, 1.0f};
    int actionButtonAlpha[3] = {245, 245, 245};
    int selectedActionControl = 0;
    Rectangle editSelectButtons[3];
    Rectangle editSizeSlider, editAlphaSlider;
    bool attackRequested = false;
    float attackCooldown = 0.0f;
    const float attackCooldownDuration = 1.0f;
    float attackAnimation = 0.0f;

    LoadGameSettings(&joystickPosition, &joystickRadius,
                     &settingsButton, &cameraButton, &mapButton,
                     actionButtonCenters, actionButtonScale,
                     actionButtonAlpha, &lookSensitivity,
                     &graphicsQuality, &targetFps, &detailLevel,
                     &resolutionLevel, &chunkDistance);
    knobRadius = joystickRadius*0.42f;
    if (targetFps < 30 || targetFps > 144) targetFps = 30;
    SetTargetFPS(targetFps);

    for(int i=0;i<MAX_BARRELS;i++){barrelPositions[i]=(Vector3){0};barrelHealth[i]=0;barrelDestroyed[i]=true;}

    LoadBuildData(buildPieces, buildPieceTypes, buildDoorOpen,
                  &buildPieceCount, &itemCounts[4]);
    srand((unsigned int)(GetTime()*100000.0) ^ 0x51A7u);
    for (int tx=-238; tx<=238 && treeCount<MAX_TREES; tx+=38)
        for (int tz=-238; tz<=238 && treeCount<MAX_TREES; tz+=38)
            if (tx*tx+tz*tz < 248*248 && IsVillageClear((float)tx,(float)tz))
            {
                treePositions[treeCount]=(Vector3){(float)tx,0,(float)tz};
                treeBiome[treeCount] = (tz < -82) ? 1 : ((tz > 82) ? 2 : 0);
                treeHealth[treeCount++]=3;
            }

    // По четыре залежи каждого вида. Руды размещаются вне деревни и дорог.
    for (int ore = 0; ore < MAX_ORE_NODES; ore++)
    {
        int type = ore/4;
        bool placed = false;
        for (int attempt = 0; attempt < 250 && !placed; attempt++)
        {
            float angle = ((float)rand()/(float)RAND_MAX)*6.2831853f;
            float radius = 24.0f + ((float)rand()/(float)RAND_MAX)*205.0f;
            float x = cosf(angle)*radius, z = sinf(angle)*radius;
            if (!IsVillageClear(x,z)) continue;
            bool clear = true;
            for (int t=0; t<treeCount; t++)
            {
                float dx=x-treePositions[t].x, dz=z-treePositions[t].z;
                if (dx*dx+dz*dz < 16.0f) { clear=false; break; }
            }
            for (int b=0; b<MAX_BARRELS && clear; b++)
            {
                float dx=x-barrelPositions[b].x, dz=z-barrelPositions[b].z;
                if (dx*dx+dz*dz < 36.0f) clear=false;
            }
            for (int other=0; other<ore && clear; other++)
            {
                if (!gOreActive[other]) continue;
                float dx=x-gOrePositions[other].x, dz=z-gOrePositions[other].z;
                if (dx*dx+dz*dz < 64.0f) clear=false;
            }
            if (!clear) continue;
            gOrePositions[ore]=(Vector3){x,0,z};
            gOreActive[ore]=true;
            oreHealth[ore]=(type==2)?10:((type==3)?30:15);
            placed=true;
        }
        if (!placed) oreRespawnAt[ore]=GetTime()+5.0;
    }

    while (!WindowShouldClose())
    {
        float frameTime = GetFrameTime();
        if (loadingScreen)
        {
            loadingProgress += frameTime/2.4f;
            if (loadingProgress >= 1.0f) { loadingProgress = 1.0f; loadingScreen = false; }
        }
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
        // Возрождение каждой залежи через 60 секунд в новой случайной точке.
        for (int ore=0; ore<MAX_ORE_NODES; ore++)
        {
            if (gOreActive[ore] || GetTime() < oreRespawnAt[ore]) continue;
            bool placed = false;
            int type = ore/4;
            for (int attempt=0; attempt<250 && !placed; attempt++)
            {
                float angle=((float)rand()/(float)RAND_MAX)*6.2831853f;
                float radius=24.0f+((float)rand()/(float)RAND_MAX)*205.0f;
                float x=cosf(angle)*radius, z=sinf(angle)*radius;
                if (!IsVillageClear(x,z)) continue;
                bool clear=true;
                for(int t=0;t<treeCount;t++){float dx=x-treePositions[t].x,dz=z-treePositions[t].z;if(dx*dx+dz*dz<16.0f){clear=false;break;}}
                for(int b=0;b<MAX_BARRELS&&clear;b++){float dx=x-barrelPositions[b].x,dz=z-barrelPositions[b].z;if(dx*dx+dz*dz<36.0f)clear=false;}
                for(int other=0;other<MAX_ORE_NODES&&clear;other++){if(other==ore||!gOreActive[other])continue;float dx=x-gOrePositions[other].x,dz=z-gOrePositions[other].z;if(dx*dx+dz*dz<64.0f)clear=false;}
                if(!clear)continue;
                gOrePositions[ore]=(Vector3){x,0,z};
                gOreActive[ore]=true;
                oreHealth[ore]=(type==2)?10:((type==3)?30:15);
                placed=true;
            }
            if(!placed) oreRespawnAt[ore]=GetTime()+5.0;
        }
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
        if (mapButton.x + mapButton.width > screenWidth) mapButton.x = screenWidth-mapButton.width-12;
        if (mapButton.x < 8) mapButton.x = 8;
        if (mapButton.y < 8) mapButton.y = 8;
        if (mapButton.y + mapButton.height > screenHeight) mapButton.y = screenHeight-mapButton.height-8;

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
            screenWidth/2.0f-67.5f, 18, 135, 55
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
        editSizeSlider = (Rectangle){screenWidth*0.18f, 119.0f, screenWidth*0.64f, 16.0f};
        editAlphaSlider = (Rectangle){screenWidth*0.18f, 174.0f, screenWidth*0.64f, 16.0f};
        float slotSize = fminf(54.0f, (screenWidth-165.0f)/4.0f);
        float slotGap = 6.0f;
        float hotbarWidth = slotSize*4.0f + slotGap*3.0f;
        float hotbarX = (screenWidth-hotbarWidth)/2.0f;
        for (int slot = 0; slot < 4; slot++)
            hotbarSlots[slot] = (Rectangle){hotbarX+slot*(slotSize+slotGap),
                                           screenHeight-78.0f, slotSize, 58.0f};
        buildButton = (Rectangle){14, 170, 142, 42};
        float buildChoiceW = fminf(82.0f, (screenWidth-50.0f)/4.0f);
        buildFloorButton = (Rectangle){10, 220, buildChoiceW, 38};
        buildWallButton = (Rectangle){16+buildChoiceW, 220, buildChoiceW, 38};
        buildRoofButton = (Rectangle){22+buildChoiceW*2, 220, buildChoiceW, 38};
        buildDoorButton = (Rectangle){28+buildChoiceW*3, 220, buildChoiceW, 38};
        buildPlaceButton = (Rectangle){14, 266, 142, 42};
        takeButton = (Rectangle){screenWidth/2.0f-76.0f, screenHeight-150.0f, 152.0f, 46.0f};

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
                        settingsButton, cameraButton, mapButton, actionButtonCenters,
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
                else if (CheckCollisionPointRec(pos, editSizeSlider))
                {
                    dragTarget = 8; dragTouchId = id;
                    float t = (pos.x-editSizeSlider.x)/editSizeSlider.width;
                    actionButtonScale[selectedActionControl] = 0.55f+fmaxf(0.0f,fminf(1.0f,t))*1.15f;
                }
                else if (CheckCollisionPointRec(pos, editAlphaSlider))
                {
                    dragTarget = 9; dragTouchId = id;
                    float t = (pos.x-editAlphaSlider.x)/editAlphaSlider.width;
                    actionButtonAlpha[selectedActionControl] = (int)(60.0f+fmaxf(0.0f,fminf(1.0f,t))*195.0f);
                }
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

                        dragOffset = (Vector2){cameraButton.x-pos.x,cameraButton.y-pos.y};
                    }
                    else if (CheckCollisionPointRec(pos, mapButton))
                    { dragTarget=7; dragTouchId=id; dragOffset=(Vector2){mapButton.x-pos.x,mapButton.y-pos.y}; }
                }
            }
            else if (mainMenuOpen && !settingsOpen && !loadingScreen)
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
                        settingsButton, cameraButton, mapButton, actionButtonCenters,
                        actionButtonScale, actionButtonAlpha, lookSensitivity,
                        graphicsQuality, targetFps, detailLevel,
                        resolutionLevel, chunkDistance);
                settingsOpen = !settingsOpen;
                joystickActive = false;
                lookTouchId = -1;
            }
            else if (!mainMenuOpen && !mapOpen && CheckCollisionPointRec(pos, mapButton))
            { mapOpen=true; joystickActive=false; lookTouchId=-1; }
            else if (mapOpen)
            { if (CheckCollisionPointRec(pos,mapButton)) mapOpen=false; }
            else if (!mainMenuOpen &&
                     CheckCollisionPointRec(pos, cameraButton))
            {
                firstPerson = !firstPerson;
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen &&
                     CheckCollisionPointRec(pos, buildButton))
            {
                buildMode = !buildMode;
            }
            else if (buildMode && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, buildFloorButton))
                selectedBuildType = 0;
            else if (buildMode && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, buildWallButton))
                selectedBuildType = 1;
            else if (buildMode && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, buildRoofButton))
                selectedBuildType = 3;
            else if (buildMode && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, buildDoorButton))
                selectedBuildType = 2;
            else if (buildMode && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, buildPlaceButton))
            {
                if (selectedBuildType != 2 || itemCounts[6] > 0)
                {
                    bool placed = PlaceBuildPiece(player, cameraYaw, selectedBuildType,
                        buildPieces, buildPieceTypes, &buildPieceCount,
                        &itemCounts[4], barrelPositions, barrelDestroyed);
                    if (placed && selectedBuildType == 2) itemCounts[6]--;
                }
                SaveBuildData(buildPieces, buildPieceTypes, buildDoorOpen,
                              buildPieceCount, itemCounts[4]);
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen &&
                     CheckCollisionPointRec(pos, sprintButton))
            {
                sprintEnabled = !sprintEnabled;
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !gasCrateOpen &&
                     CheckCollisionPointRec(pos, inventoryButton))
            {
                inventoryOpen = !inventoryOpen;
                joystickActive = false;
                lookTouchId = -1;
            }
            else if (gasCrateOpen)
            {
                Rectangle cp={screenWidth*0.14f,screenHeight*0.30f,screenWidth*0.72f,screenHeight*0.40f};
                Rectangle close={cp.x+cp.width-76,cp.y+9,66,29};
                if(CheckCollisionPointRec(pos,close)){gasCrateOpen=false;openedGasCrate=-1;}
                else if(openedGasCrate>=0){
                    float gap=7.0f,cw=fminf((cp.width-32.0f-gap*3.0f)/4.0f,(cp.height-76.0f-gap)/2.0f),ch=cw; float gridX=cp.x+(cp.width-(cw*4.0f+gap*3.0f))*0.5f;
                    for(int slot=0;slot<8;slot++){
                        Rectangle cell={gridX+(slot%4)*(cw+gap),cp.y+52+(slot/4)*(ch+gap),cw,ch};
                        if(CheckCollisionPointRec(pos,cell)&&gasCrateAmounts[openedGasCrate][slot]>0){
                            int item=gasCrateItems[openedGasCrate][slot],room=99-itemCounts[item];
                            int take=gasCrateAmounts[openedGasCrate][slot]<room?gasCrateAmounts[openedGasCrate][slot]:room;
                            if(take>0){itemCounts[item]+=take;gasCrateAmounts[openedGasCrate][slot]-=take;} break;
                        }
                    }
                }
            }
            else if (inventoryOpen)
            {
                // Еду и воду можно использовать касанием карточки в рюкзаке.
                Rectangle invPanel = {screenWidth*0.10f, screenHeight*0.12f,
                                      screenWidth*0.80f, screenHeight*0.76f};
                Rectangle craftAxeButton = {invPanel.x+16, invPanel.y+260, invPanel.width-32, 32};
                Rectangle craftMedButton = {invPanel.x+16, invPanel.y+296, invPanel.width-32, 32};
                Rectangle craftDoorButton = {invPanel.x+16, invPanel.y+332, invPanel.width-32, 32};
                if (CheckCollisionPointRec(pos, craftAxeButton) &&
                    itemCounts[4] >= 3 && itemCounts[5] >= 2 && itemCounts[0] < 99)
                {
                    itemCounts[4] -= 3;
                    itemCounts[5] -= 2;
                    itemCounts[0]++;
                }
                else if (CheckCollisionPointRec(pos, craftMedButton) &&
                         itemCounts[4] >= 2 && itemCounts[5] >= 1 && itemCounts[1] < 99)
                {
                    itemCounts[4] -= 2;
                    itemCounts[5] -= 1;
                    itemCounts[1]++;
                }
                else if (CheckCollisionPointRec(pos, craftDoorButton) && itemCounts[4] >= 10 && itemCounts[6] < 99)
                { itemCounts[4] -= 10; itemCounts[6]++; SaveBuildData(buildPieces,buildPieceTypes,buildDoorOpen,buildPieceCount,itemCounts[4]); }
                for (int item = 2; item <= 3; item++)
                {
                    float gap = 8.0f;
                    float cellSize = fminf(54.0f, (invPanel.width-32.0f-gap*3.0f)/4.0f);
                    float gridWidth = cellSize*4.0f+gap*3.0f;
                    float gridX = invPanel.x+(invPanel.width-gridWidth)*0.5f;
                    Rectangle r = {gridX+(item%4)*(cellSize+gap),
                                   invPanel.y+72+(item/4)*58, cellSize, cellSize};
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
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, takeButton) && (nearbyOreDrop = FindNearestOreDrop(player, oreDrops, oreDropActive)) >= 0)
            { int item=8+oreDropType[nearbyOreDrop]; itemCounts[item]+=oreDropAmount[nearbyOreDrop]; if(itemCounts[item]>99)itemCounts[item]=99; oreDropActive[nearbyOreDrop]=false; }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, takeButton) && (nearbyWoodDrop = FindNearestWoodDrop(player, woodDrops, woodDropActive)) >= 0)
            { itemCounts[4] += 15; if(itemCounts[4]>99)itemCounts[4]=99; woodDropActive[nearbyWoodDrop]=false; SaveBuildData(buildPieces,buildPieceTypes,buildDoorOpen,buildPieceCount,itemCounts[4]); }
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
                        pos.y > screenHeight*0.18f)
                    {
                        float dx = pos.x-joystickPosition.x;
                        float dy = pos.y-joystickPosition.y;

                        if (sqrtf(dx*dx+dy*dy) < joystickRadius+260.0f)
                        {
                            joystickActive = true;
                            joystickTouchId = id;
                        }
                    }
                    else if (pos.x >= screenWidth*0.48f && pos.y > screenHeight*0.12f)
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
                    detailLevel = 0;
                    resolutionLevel = 0;
                }
                else if (CheckCollisionPointRec(pos, fpsButtons[0]))
                {
                    targetFps = 30;
                    SetTargetFPS(targetFps);
                    SaveGameSettings(joystickPosition, joystickRadius,
                        settingsButton, cameraButton, mapButton, actionButtonCenters,
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
                        settingsButton, cameraButton, mapButton, actionButtonCenters,
                        actionButtonScale, actionButtonAlpha, lookSensitivity,
                        graphicsQuality, targetFps, detailLevel,
                        resolutionLevel, chunkDistance);
                }
            }
            else
            {
                if (pos.x < screenWidth*0.48f &&
                    pos.y > screenHeight*0.18f)
                {
                    float dx = pos.x-joystickPosition.x;
                    float dy = pos.y-joystickPosition.y;

                    if (sqrtf(dx*dx+dy*dy) < joystickRadius+260)
                    {
                        joystickActive = true;
                        joystickTouchId = id;
                    }
                }
                else if (pos.x > screenWidth*0.48f && pos.y > screenHeight*0.12f)
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

            if (mainMenuOpen && !settingsOpen && !editControls && !loadingScreen)
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
                     CheckCollisionPointRec(pos, buildButton))
            {
                buildMode = !buildMode;
            }
            else if (buildMode && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, buildFloorButton))
                selectedBuildType = 0;
            else if (buildMode && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, buildWallButton))
                selectedBuildType = 1;
            else if (buildMode && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, buildRoofButton))
                selectedBuildType = 3;
            else if (buildMode && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, buildDoorButton))
                selectedBuildType = 2;
            else if (buildMode && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, buildPlaceButton))
            {
                if (selectedBuildType != 2 || itemCounts[6] > 0)
                {
                    bool placed = PlaceBuildPiece(player, cameraYaw, selectedBuildType,
                        buildPieces, buildPieceTypes, &buildPieceCount,
                        &itemCounts[4], barrelPositions, barrelDestroyed);
                    if (placed && selectedBuildType == 2) itemCounts[6]--;
                }
                SaveBuildData(buildPieces, buildPieceTypes, buildDoorOpen,
                              buildPieceCount, itemCounts[4]);
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen &&
                     CheckCollisionPointRec(pos, sprintButton))
            {
                sprintEnabled = !sprintEnabled;
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !gasCrateOpen &&
                     CheckCollisionPointRec(pos, inventoryButton))
            {
                inventoryOpen = !inventoryOpen;
                joystickActive = false;
                lookTouchId = -1;
            }
            else if (inventoryOpen)
            {
                // Еду и воду можно использовать касанием карточки в рюкзаке.
                Rectangle invPanel = {screenWidth*0.10f, screenHeight*0.12f,
                                      screenWidth*0.80f, screenHeight*0.76f};
                for (int item = 2; item <= 3; item++)
                {
                    float gap = 8.0f;
                    float cellSize = fminf(54.0f, (invPanel.width-32.0f-gap*3.0f)/4.0f);
                    float gridWidth = cellSize*4.0f+gap*3.0f;
                    float gridX = invPanel.x+(invPanel.width-gridWidth)*0.5f;
                    Rectangle r = {gridX+(item%4)*(cellSize+gap),
                                   invPanel.y+72+(item/4)*58, cellSize, cellSize};
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
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, takeButton) && (nearbyOreDrop = FindNearestOreDrop(player, oreDrops, oreDropActive)) >= 0)
            { int item=8+oreDropType[nearbyOreDrop]; itemCounts[item]+=oreDropAmount[nearbyOreDrop]; if(itemCounts[item]>99)itemCounts[item]=99; oreDropActive[nearbyOreDrop]=false; }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, takeButton) && (nearbyWoodDrop = FindNearestWoodDrop(player, woodDrops, woodDropActive)) >= 0)
            { itemCounts[4] += 15; if(itemCounts[4]>99)itemCounts[4]=99; woodDropActive[nearbyWoodDrop]=false; SaveBuildData(buildPieces,buildPieceTypes,buildDoorOpen,buildPieceCount,itemCounts[4]); }
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
                        settingsButton, cameraButton, mapButton, actionButtonCenters,
                        actionButtonScale, actionButtonAlpha, lookSensitivity,
                        graphicsQuality, targetFps, detailLevel,
                        resolutionLevel, chunkDistance);
                }
                else if (CheckCollisionPointRec(pos, closeButton))
                {
                    settingsOpen = false;
                    SaveGameSettings(joystickPosition, joystickRadius,
                        settingsButton, cameraButton, mapButton, actionButtonCenters,
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
                    else if (dragTarget == 7)
                    { mapButton.x=fmaxf(8,fminf(nx,screenWidth-mapButton.width-8)); mapButton.y=fmaxf(8,fminf(ny,screenHeight-mapButton.height-8)); }
                    else if (dragTarget == 8)
                    {
                        float t = fmaxf(0.0f, fminf(1.0f, (pos.x-editSizeSlider.x)/editSizeSlider.width));
                        actionButtonScale[selectedActionControl] = 0.55f+t*1.15f;
                    }
                    else if (dragTarget == 9)
                    {
                        float t = fmaxf(0.0f, fminf(1.0f, (pos.x-editAlphaSlider.x)/editAlphaSlider.width));
                        actionButtonAlpha[selectedActionControl] = (int)(60.0f+t*195.0f);
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
                    if (sprintEnabled) survivalSpeed *= 1.80f;

                    float nextX = player.x + worldMoveX*survivalSpeed;
                    float nextZ = player.z + worldMoveZ*survivalSpeed;

                    // Лестница находится на правой стене заправки:
                    // центр примерно X=12.25, Z=-26.5. Узкая зона не даёт
                    // начинать подъём задолго до контакта с лестницей.
                    bool ladderZone = fabsf(player.x-12.25f)<0.78f &&
                                      player.z>-27.35f && player.z<-25.65f;
                    if (ladderZone &&
                        ((player.y<3.72f && moveForward>0.12f) ||
                         (player.y>0.0f && moveForward< -0.12f)))
                    {
                        player.y += moveForward*survivalSpeed*1.15f;
                        if (player.y<0.0f) player.y=0.0f;
                        if (player.y>3.72f) player.y=3.72f;
                        // На время подъёма удерживаем персонажа по центру лестницы.
                        nextX=12.25f;
                        nextZ=-26.5f;
                    }
                    else if (player.y>0.0f)
                    {
                        // Крыша заправки — сплошная поверхность над всем корпусом.
                        // На ней высота сохраняется; за внешним краем начинается падение.
                        bool roofSurface = fabsf(player.x)<11.55f &&
                                           player.z>-30.75f && player.z<-13.25f &&
                                           player.y>=3.35f;
                        bool ladderTop = fabsf(player.x-12.25f)<0.95f &&
                                         player.z>-27.35f && player.z<-25.65f &&
                                         player.y>=3.35f;
                        bool standingOnRoof = roofSurface || ladderTop;
                        if (!standingOnRoof)
                        {
                            player.y -= GetFrameTime()*4.0f;
                            if (player.y<0.0f) player.y=0.0f;
                        }
                    }

                    // Проверяем оси отдельно, чтобы можно было скользить вдоль стен.
                    if (!IsBlockedAt(nextX, player.z,player.y,barrelPositions,barrelDestroyed))
                        player.x = nextX;
                    if (!IsBlockedAt(player.x, nextZ,player.y,barrelPositions,barrelDestroyed))
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

        if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && !mapOpen &&
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

        if (settingsOpen || editControls || mainMenuOpen || inventoryOpen || mapOpen)
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

            // Проверяем путь камеры отдельно от коллизии игрока.
            // Мелкие шаги уменьшают скачки при пересечении края дома.
            Vector3 cameraStart = {player.x, player.y+1.4f, player.z};
            Vector3 safeCamera = desiredCamera;
            for (int step = 1; step <= 48; step++)
            {
                float t = step/48.0f;
                float cx = cameraStart.x+(desiredCamera.x-cameraStart.x)*t;
                float cy = cameraStart.y+(desiredCamera.y-cameraStart.y)*t;
                float cz = cameraStart.z+(desiredCamera.z-cameraStart.z)*t;

                if (cy < 0.35f ||
                    IsCameraBlockedAt(cx, cz, barrelPositions, barrelDestroyed))
                {
                    float safeT = fmaxf(0.0f, t-0.025f);
                    safeCamera = (Vector3){
                        cameraStart.x+(desiredCamera.x-cameraStart.x)*safeT,
                        fmaxf(0.45f, cameraStart.y+(desiredCamera.y-cameraStart.y)*safeT),
                        cameraStart.z+(desiredCamera.z-cameraStart.z)*safeT
                    };
                    break;
                }
            }

            // Плавно подтягиваем камеру к безопасной точке вместо резкого
            // телепортирования при каждом кадре столкновения.
            if (!thirdPersonCameraReady)
            {
                thirdPersonCameraPosition = safeCamera;
                thirdPersonCameraReady = true;
            }
            float cameraSmooth = 1.0f-expf(-GetFrameTime()*14.0f);
            thirdPersonCameraPosition.x += (safeCamera.x-thirdPersonCameraPosition.x)*cameraSmooth;
            thirdPersonCameraPosition.y += (safeCamera.y-thirdPersonCameraPosition.y)*cameraSmooth;
            thirdPersonCameraPosition.z += (safeCamera.z-thirdPersonCameraPosition.z)*cameraSmooth;
            camera.position = thirdPersonCameraPosition;

            // Небольшое плавное покачивание камеры в третьем лице при движении.
            // При остановке амплитуда мягко затухает, без резкого скачка.
            float bobTarget = (playerMoving && !mainMenuOpen && !settingsOpen &&
                               !editControls && !inventoryOpen) ? 1.0f : 0.0f;
            float bobBlendSpeed = 1.0f-expf(-GetFrameTime()*7.0f);
            thirdPersonBobBlend += (bobTarget-thirdPersonBobBlend)*bobBlendSpeed;
            if (bobTarget > 0.0f)
                thirdPersonBobPhase += GetFrameTime()*(sprintEnabled ? 12.0f : 8.5f);
            float bobAmplitude = thirdPersonBobBlend;
            camera.position.y += fabsf(sinf(thirdPersonBobPhase))*0.055f*bobAmplitude;
            camera.position.x += sinf(thirdPersonBobPhase*0.5f)*0.025f*bobAmplitude;

            // Камера смотрит вперёд по направлению прицела, а не прямо в игрока.
            camera.target = (Vector3){
                player.x+dirX*10.0f,
                player.y+1.4f+dirY*10.0f,
                player.z+dirZ*10.0f
            };
        }

        targetedBarrel = -1; targetedTree = -1; targetedOre = -1; targetedGasCrate = -1;
        nearbyWoodDrop = FindNearestWoodDrop(player, woodDrops, woodDropActive);
        nearbyOreDrop = FindNearestOreDrop(player, oreDrops, oreDropActive);
        Vector3 aimOrigin = {player.x, player.y+1.6f, player.z};
        Vector3 aimDirection = {dirX, dirY, dirZ};
        Ray centerAimRay = GetMouseRay((Vector2){screenWidth*0.5f, screenHeight*0.5f}, camera);
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

        // Ящики заправки выбираются наведением с близкого расстояния.
        float closestCrateDistance=9999.0f;
        const Vector3 gasCratePositions[4]={{-3.8f,0.48f,-20.8f},{0.0f,0.48f,-20.8f},{3.8f,0.48f,-20.8f},{0.0f,4.05f,-22.0f}};
        for(int c=0;c<4;c++){
            Vector3 center=gasCratePositions[c];
            float dx=center.x-aimOrigin.x,dy=center.y-aimOrigin.y,dz=center.z-aimOrigin.z;
            float distance=sqrtf(dx*dx+dy*dy+dz*dz);
            if(distance>3.0f||distance<0.1f)continue;
            RayCollision hit=GetRayCollisionSphere(centerAimRay,center,0.82f);
            if(hit.hit&&hit.distance<closestCrateDistance){closestCrateDistance=hit.distance;targetedGasCrate=c;}
        }

        // Дерево или руда выбираются только точным попаданием луча из центра экрана.
        float closestTreeDistance=9999.0f;
        for (int t=0; t<treeCount; t++)
        {
            if (treeChopped[t]) continue;
            Vector3 treeCenter = {treePositions[t].x, 1.45f, treePositions[t].z};
            float pdx = aimOrigin.x-treeCenter.x, pdy = aimOrigin.y-treeCenter.y, pdz = aimOrigin.z-treeCenter.z;
            float playerDistance = sqrtf(pdx*pdx+pdy*pdy+pdz*pdz);
            if (playerDistance > 3.2f) continue;
            RayCollision hit = GetRayCollisionSphere(centerAimRay, treeCenter, 0.95f);
            if (hit.hit && hit.distance < closestTreeDistance)
            {
                closestTreeDistance = hit.distance;
                targetedTree = t;
            }
        }
        float closestOreDistance=9999.0f;
        for (int o=0; o<MAX_ORE_NODES; o++)
        {
            if (!gOreActive[o]) continue;
            Vector3 oreCenter = {gOrePositions[o].x, 0.76f, gOrePositions[o].z};
            float pdx = aimOrigin.x-oreCenter.x, pdy = aimOrigin.y-oreCenter.y, pdz = aimOrigin.z-oreCenter.z;
            float playerDistance = sqrtf(pdx*pdx+pdy*pdy+pdz*pdz);
            if (playerDistance > 2.6f) continue;
            RayCollision hit = GetRayCollisionSphere(centerAimRay, oreCenter, 1.16f);
            if (hit.hit && hit.distance < closestOreDistance)
            {
                closestOreDistance = hit.distance;
                targetedOre = o;
            }
        }

        if (attackRequested && attackCooldown <= 0.0f &&
            !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && !gasCrateOpen)
        {
            attackCooldown = attackCooldownDuration;
            attackAnimation = 0.24f;
            int nearbyDoor = -1;
            float doorDistance = 3.2f;
            for (int i = 0; i < buildPieceCount; i++)
            {
                if (buildPieceTypes[i] != 2) continue;
                float dx = buildPieces[i].x-player.x;
                float dz = buildPieces[i].z-player.z;
                float distance = sqrtf(dx*dx+dz*dz);
                float dot = distance > 0.001f ?
                    (dx*sinf(playerAngle)-dz*cosf(playerAngle))/distance : 1.0f;
                if (distance < doorDistance && dot > 0.15f)
                { nearbyDoor = i; doorDistance = distance; }
            }
            if (targetedGasCrate >= 0)
            {
                openedGasCrate=targetedGasCrate; gasCrateOpen=true;
                joystickActive=false; lookTouchId=-1; attackAnimation=0.0f;
            }
            else if (nearbyDoor >= 0)
            {
                buildDoorOpen[nearbyDoor] = !buildDoorOpen[nearbyDoor];
                SaveBuildData(buildPieces, buildPieceTypes, buildDoorOpen,
                              buildPieceCount, itemCounts[4]);
                attackAnimation = 0.0f;
            }
            else if (targetedBarrel >= 0 && !barrelDestroyed[targetedBarrel])
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
                    SaveBuildData(buildPieces, buildPieceTypes, buildDoorOpen,
                                  buildPieceCount, itemCounts[4]);
                    targetedBarrel = -1;
                }
            }
            else if (targetedTree >= 0 && !treeChopped[targetedTree])
            {
                treeHealth[targetedTree]--;
                if(treeHealth[targetedTree]<=0){treeChopped[targetedTree]=true;for(int d=0;d<MAX_WOOD_DROPS;d++)if(!woodDropActive[d]){woodDrops[d]=treePositions[targetedTree];woodDropActive[d]=true;break;}}
            }
            else if (targetedOre >= 0 && gOreActive[targetedOre])
            {
                int type = targetedOre/4;
                // Кремний добывается только киркой ORF PIXCADE.
                if (type != 3 || (itemCounts[7] > 0 && hotbarItemIds[selectedHotbarSlot] == 7))
                {
                    oreHealth[targetedOre]--;
                    if (oreHealth[targetedOre] <= 0)
                    {
                        int dropSlot=-1;
                        for(int d=0;d<MAX_ORE_DROPS;d++)if(!oreDropActive[d]){dropSlot=d;break;}
                        if(dropSlot>=0)
                        {
                            oreDrops[dropSlot]=gOrePositions[targetedOre];
                            oreDropType[dropSlot]=type;
                            oreDropAmount[dropSlot]=(type==0)?30:((type==1)?10:((type==2)?25:20));
                            oreDropActive[dropSlot]=true;
                            gOreActive[targetedOre]=false;
                            oreRespawnAt[targetedOre]=GetTime()+60.0;
                        }
                    }
                }
            }
        }

        // Полноразмерный 3D-рендер: пониженное разрешение давало
        // сильное мерцание и рывки деталей земли при вращении камеры.
        float renderScale = 1.0f;

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

        // Большой остров с тремя климатическими поясами: снег, лес и песок.
        DrawPlane((Vector3){0, -0.22f, 0}, (Vector2){760, 760}, C(43, 112, 143, 255));
        for (int ix=-6; ix<=6; ix++)
        {
            for (int iz=-6; iz<=6; iz++)
            {
                float px = ix*40.0f;
                float pz = iz*40.0f;
                if (px*px+pz*pz > 270.0f*270.0f) continue;
                bool coast = (px*px+pz*pz > 225.0f*225.0f);
                Color biomeGround = (pz < -82.0f) ? C(205, 216, 216, 255) :
                                    ((pz > 82.0f) ? C(194, 169, 112, 255) :
                                                    C(73, 139, 76, 255));
                if (coast) biomeGround = (pz < -82.0f) ? C(176, 190, 190, 255) : C(151, 137, 102, 255);
                DrawPlane((Vector3){px, -0.08f, pz}, (Vector2){40.2f, 40.2f}, biomeGround);
            }
        }

        // Дороги сделаны тонкими объёмными полосами вместо наложенных
        // почти coplanar-плоскостей: это устраняет Z-fighting при вращении камеры.
        DrawCube((Vector3){0, -0.055f, 0}, 12.5f, 0.05f, 380.0f,
                 C(126, 111, 83, 255));
        DrawCube((Vector3){0, -0.044f, 0}, 9.2f, 0.05f, 380.0f,
                 C(157, 137, 101, 255));
        DrawCube((Vector3){0, -0.055f, 0}, 380.0f, 0.05f, 12.5f,
                 C(126, 111, 83, 255));
        DrawCube((Vector3){0, -0.033f, 0}, 380.0f, 0.05f, 9.2f,
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



        float chunkView = chunkDistance*50.0f;
        float chunkView2 = chunkView*chunkView;

        float stationDx=player.x,stationDz=-22.0f-player.z;
        if(stationDx*stationDx+stationDz*stationDz<chunkView2)
            DrawMiniGasStation(0.0f,-22.0f,graphicsQuality);

        if (buildMode && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen)
        {
            // Предпросмотр направлен туда же, куда смотрит камера, и совпадает с местом установки.
            float previewX, previewZ;
            int previewStoredType;
            GetBuildPlacement(player, cameraYaw, selectedBuildType,
                              buildPieces, buildPieceTypes, buildPieceCount,
                              &previewX, &previewZ, &previewStoredType);
            int previewCost=selectedBuildType==0?30:((selectedBuildType==1||selectedBuildType==3)?15:0);
            bool previewValid=selectedBuildType==2?itemCounts[6]>0:itemCounts[4]>=previewCost;
            if(previewStoredType<0)previewValid=false;
            if(fabsf(previewX-player.x)<2.0f&&fabsf(previewZ-player.z)<2.0f)previewValid=false;
            if(IsBlockedAt(previewX,previewZ,player.y,barrelPositions,barrelDestroyed))previewValid=false;
            for(int q=0;q<buildPieceCount;q++)if(fabsf(buildPieces[q].x-previewX)<0.01f&&fabsf(buildPieces[q].z-previewZ)<0.01f&&buildPieceTypes[q]==previewStoredType)previewValid=false;
            Color ghost=previewValid?C(90,230,140,115):C(240,75,65,115);
            float gy=selectedBuildType==0?0.14f:(selectedBuildType==1?1.25f:(selectedBuildType==3?2.55f:1.1f));
            float gw=(selectedBuildType==0||selectedBuildType==3)?2.5f:(selectedBuildType==1?(previewStoredType==4?0.20f:2.4f):1.35f);
            float gh=selectedBuildType==0?0.20f:(selectedBuildType==1?2.5f:(selectedBuildType==3?0.18f:2.2f));
            float gd=(selectedBuildType==0||selectedBuildType==3)?2.5f:0.20f;
            if(selectedBuildType==1 && previewStoredType==4) gd=2.4f;
            DrawCube((Vector3){previewX,gy,previewZ},gw,gh,gd,ghost);
            DrawCubeWires((Vector3){previewX,gy,previewZ},gw,gh,gd,previewValid?GREEN:RED);
        }

        for (int i = 0; i < buildPieceCount; i++)
        {
            Vector3 p = buildPieces[i];
            if (buildPieceTypes[i] == 0)
            {
                DrawCube((Vector3){p.x, 0.12f, p.z}, 2.4f, 0.20f, 2.4f, C(133, 94, 58, 255));
                DrawCube((Vector3){p.x, 0.235f, p.z}, 2.25f, 0.035f, 2.25f, C(166, 124, 77, 255));
            }
            else if (buildPieceTypes[i] == 1 || buildPieceTypes[i] == 4)
            {
                bool rotatedWall = buildPieceTypes[i] == 4;
                float wallW = rotatedWall ? 0.20f : 2.4f;
                float wallD = rotatedWall ? 2.4f : 0.20f;
                DrawCube((Vector3){p.x, 1.25f, p.z}, wallW, 2.5f, wallD, C(128, 91, 57, 255));
                DrawCube((Vector3){p.x+(rotatedWall ? -0.115f : 0.0f), 1.25f,
                                   p.z+(rotatedWall ? 0.0f : -0.115f)},
                         rotatedWall ? 0.025f : 2.15f, 2.25f,
                         rotatedWall ? 2.15f : 0.025f, C(157, 113, 70, 255));
            }
            else if (buildPieceTypes[i] == 3)
            {
                DrawCube((Vector3){p.x, 2.55f, p.z}, 2.55f, 0.18f, 2.55f, C(102, 70, 44, 255));
                DrawCube((Vector3){p.x, 2.66f, p.z}, 2.38f, 0.045f, 2.38f, C(150, 105, 65, 255));
            }
            else
            {
                float doorShift = buildDoorOpen[i] ? 0.95f : 0.0f;
                DrawCube((Vector3){p.x+doorShift, 1.1f, p.z}, 1.35f, 2.2f, 0.18f, C(103, 70, 43, 255));
                DrawCube((Vector3){p.x+doorShift, 1.1f, p.z-0.105f}, 1.15f, 1.98f, 0.025f, C(145, 99, 58, 255));
                DrawSphere((Vector3){p.x+doorShift+0.48f, 1.05f, p.z-0.16f}, 0.055f, C(214, 177, 91, 255));
            }
        }

        // Старые бочки удалены из мира.

        // Дополнительные скалы и пещерные конструкции убраны по просьбе игрока.

        // Все четыре вида руды появляются только в игровом мире, не на карте интерфейса.
        for(int ore=0;ore<MAX_ORE_NODES;ore++)
        {
            if(!gOreActive[ore])continue;
            float dx=gOrePositions[ore].x-player.x, dz=gOrePositions[ore].z-player.z;
            if(dx*dx+dz*dz<chunkView2)
                DrawOreNode(gOrePositions[ore].x,gOrePositions[ore].z,ore/4);
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

        // На низкой детализации ограничиваем лес ближней зоной.
        // Это заметно сокращает количество 3D-объектов на слабых телефонах.
        float detailTreeView = (detailLevel >= 2) ? 105.0f :
                               ((detailLevel == 1) ? 165.0f : 245.0f);
        float treeView = fminf(chunkView, detailTreeView);
        float treeView2 = treeView*treeView;

        for (int t = 0; t < treeCount; t++)
        {
            if (treeChopped[t]) continue;
            float tx = treePositions[t].x-player.x;
            float tz = treePositions[t].z-player.z;
            float distance2 = tx*tx + tz*tz;
            if (distance2 > treeView2) continue;

            int ix = (int)treePositions[t].x;
            int iz = (int)treePositions[t].z;
            float scale = 0.85f + (float)((ix*ix+iz*iz)%5)*0.07f;
            DrawTree(treePositions[t].x, treePositions[t].z, scale, graphicsQuality, treeBiome[t]);
        }

        for (int d = 0; d < MAX_ORE_DROPS; d++)
        {
            if (!oreDropActive[d]) continue;
            Vector3 p=oreDrops[d];
            float dx=p.x-player.x, dz=p.z-player.z;
            if(dx*dx+dz*dz>chunkView2)continue;
            DrawOreDrop(p,oreDropType[d]);
        }

        for (int d = 0; d < MAX_WOOD_DROPS; d++)
        {
            if (!woodDropActive[d]) continue;
            Vector3 p = woodDrops[d];
            float dx = p.x-player.x, dz = p.z-player.z;
            if (dx*dx + dz*dz > chunkView2) continue;
            DrawCylinderEx((Vector3){p.x-0.48f,0.16f,p.z},
                           (Vector3){p.x+0.48f,0.16f,p.z},
                           0.16f,0.16f,8,C(126,82,47,255));
            DrawCylinderEx((Vector3){p.x-0.48f,0.16f,p.z+0.22f},
                           (Vector3){p.x+0.48f,0.16f,p.z+0.22f},
                           0.13f,0.13f,8,C(151,103,61,255));
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

        // Дублируем предпросмотр в HUD: он остаётся заметным даже от первого лица,
        // когда руки персонажа могут закрывать 3D-модель на земле.
        if (buildMode && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen)
        {
            float px, pz;
            int hudStoredType;
            GetBuildPlacement(player, cameraYaw, selectedBuildType,
                              buildPieces, buildPieceTypes, buildPieceCount,
                              &px, &pz, &hudStoredType);
            float halfX = (selectedBuildType == 1) ? (hudStoredType == 4 ? 0.10f : 1.2f) :
                          ((selectedBuildType == 2) ? 0.68f : 1.25f);
            float halfZ = (selectedBuildType == 1) ? (hudStoredType == 4 ? 1.2f : 0.10f) :
                          ((selectedBuildType == 2) ? 0.10f : 1.25f);
            Vector3 corners[4] = {
                {px-halfX, 0.04f, pz-halfZ}, {px+halfX, 0.04f, pz-halfZ},
                {px+halfX, 0.04f, pz+halfZ}, {px-halfX, 0.04f, pz+halfZ}
            };
            Color markerColor = (selectedBuildType == 2 ? itemCounts[6] > 0 :
                itemCounts[4] >= (selectedBuildType == 0 ? 30 : 15)) ?
                C(80,255,145,255) : C(255,75,65,255);
            Vector2 screenCorners[4];
            for (int k=0; k<4; k++) screenCorners[k] = GetWorldToScreen(corners[k], camera);
            for (int k=0; k<4; k++)
                DrawLineEx(screenCorners[k], screenCorners[(k+1)%4], 5.0f, markerColor);
            DrawCircleLines(screenWidth/2, screenHeight/2, 13.0f, markerColor);
            DrawText("BUILD TARGET", screenWidth/2-58, screenHeight/2+22, 18, markerColor);
        }

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
                cameraButton.x+10, cameraButton.y+19, 17, RAYWHITE
            );
            DrawRectangleRounded(mapButton,0.30f,16,C(20,34,52,235));
            DrawRectangleRoundedLinesEx(mapButton,0.30f,16,2,C(130,205,255,220));
            DrawText("MAP",mapButton.x+mapButton.width/2-MeasureText("MAP",19)/2,mapButton.y+22,19,RAYWHITE);

            if (!settingsOpen && !editControls && !inventoryOpen)
            {
                DrawSurvivalButton(buildButton, buildMode ? "BUILD: ON" : "BUILD", 16, buildMode);
                if (buildMode)
                {
                    DrawSurvivalButton(buildFloorButton, "FOUND", 10, selectedBuildType == 0);
                    DrawSurvivalButton(buildWallButton, "WALL", 10, selectedBuildType == 1);
                    DrawSurvivalButton(buildRoofButton, "ROOF", 10, selectedBuildType == 3);
                    DrawSurvivalButton(buildDoorButton, "DOOR", 10, selectedBuildType == 2);
                    DrawSurvivalButton(buildPlaceButton, "PLACE", 15, true);
                    int buildCost = selectedBuildType == 0 ? 30 : ((selectedBuildType == 1 || selectedBuildType == 3) ? 15 : 0);
                    DrawText(TextFormat("WOOD: %d  |  %s", itemCounts[4], selectedBuildType == 2 ? TextFormat("DOORS: %d", itemCounts[6]) : TextFormat("COST: %d", buildCost)),
                        164, 278, 12, C(225, 235, 245, 255));
                }
            }

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

            if (nearbyOreDrop >= 0 && !settingsOpen && !editControls && !inventoryOpen)
                DrawSurvivalButton(takeButton, TextFormat("ВЗЯТЬ %s +%d",GetOreName(oreDropType[nearbyOreDrop]),oreDropAmount[nearbyOreDrop]), 12, true);
            else if (nearbyWoodDrop >= 0 && !settingsOpen && !editControls && !inventoryOpen)
                DrawSurvivalButton(takeButton, "ВЗЯТЬ ДЕРЕВО +15", 12, true);

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

            // Кнопка действия меняет подпись, когда прицел наведён на ящик.
            if (!settingsOpen && !editControls && !inventoryOpen && !gasCrateOpen)
            {
                unsigned char hitAlpha = (unsigned char)actionButtonAlpha[0];
                bool canOpenCrate = targetedGasCrate >= 0;
                Color hitColor = canOpenCrate ? C(35, 125, 82, hitAlpha) :
                    (attackCooldown <= 0.0f ? C(185, 75, 55, hitAlpha) : C(75, 83, 91, hitAlpha));
                const char *actionLabel = canOpenCrate ? "OPEN" : "HIT";
                int actionFont = canOpenCrate ? 16 : 20;
                DrawRectangleRounded(interactButton, 0.25f, 14, hitColor);
                DrawRectangleRoundedLinesEx(interactButton, 0.25f, 14, 2, C(230, 190, 160, hitAlpha));
                DrawText(actionLabel,
                    (int)(interactButton.x+interactButton.width/2-MeasureText(actionLabel,actionFont)/2),
                    (int)(interactButton.y+7), actionFont, C(255,255,255,hitAlpha));
                if (!canOpenCrate)
                {
                    float cooldownRatio=attackCooldown/attackCooldownDuration;
                    DrawRectangleRounded((Rectangle){interactButton.x+12,interactButton.y+34,
                        (interactButton.width-24)*(1.0f-cooldownRatio),7},0.5f,6,C(105,225,150,hitAlpha));
                    const char *cooldownLabel=attackCooldown<=0.0f?"READY":TextFormat("%.1f s",attackCooldown);
                    DrawText(cooldownLabel,(int)(interactButton.x+interactButton.width/2-
                        MeasureText(cooldownLabel,13)/2),(int)(interactButton.y+44),13,C(255,255,255,hitAlpha));
                }
            }

            if (targetedTree >= 0 && !treeChopped[targetedTree])
            {
                DrawRectangleRounded((Rectangle){screenWidth/2.0f-130.0f, 105, 260, 48}, 0.18f, 10, C(18, 31, 44, 225));
                DrawRectangleRoundedLinesEx((Rectangle){screenWidth/2.0f-130.0f, 105, 260, 48}, 0.18f, 10, 2, C(90, 165, 105, 240));
                DrawText(TextFormat("TREE  -  %d/3", treeHealth[targetedTree]), (int)(screenWidth/2.0f-105.0f), 119, 19, RAYWHITE);
                DrawText("HIT TO CHOP", (int)(screenWidth/2.0f+15.0f), 122, 13, C(175, 220, 175, 255));
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

            if (targetedOre >= 0 && gOreActive[targetedOre])
            {
                int oreType = targetedOre/4;
                int maxOreHp = (oreType == 2) ? 10 : ((oreType == 3) ? 30 : 15);
                const char *oreLabel = TextFormat("%s ORE", GetOreName(oreType));
                float hudX = screenWidth/2.0f-145.0f;
                Rectangle oreHud = {hudX, 105, 290, 105};
                DrawRectangleRounded(oreHud, 0.18f, 12, C(15, 27, 40, 238));
                DrawRectangleRoundedLinesEx(oreHud, 0.18f, 12, 2.2f, GetOreColor(oreType));
                DrawText(oreLabel, (int)hudX+16, 114, 19, RAYWHITE);
                DrawText(TextFormat("HP: %d / %d", oreHealth[targetedOre], maxOreHp),
                         (int)hudX+16, 142, 17, C(235, 241, 248, 255));
                if (oreType == 3 && hotbarItemIds[selectedHotbarSlot] != 7)
                    DrawText("EQUIP ORF PIXCADE IN SLOT 2", (int)hudX+16, 184, 13, C(255, 205, 115, 255));
                DrawRectangleRounded((Rectangle){hudX+16, 166, 258, 9}, 0.5f, 6, C(54, 63, 72, 255));
                float hpRatio = (float)oreHealth[targetedOre]/(float)maxOreHp;
                if (hpRatio < 0.0f) hpRatio = 0.0f;
                DrawRectangleRounded((Rectangle){hudX+16, 166, 258*hpRatio, 9},
                                     0.5f, 6, GetOreColor(oreType));
            }

            // Небольшой прицел в центре экрана — точка наведения для деревьев и руды.
            if (!settingsOpen && !editControls && !inventoryOpen && !mapOpen)
            {
                int crossX = screenWidth/2;
                int crossY = screenHeight/2;
                Color crossColor = (targetedTree >= 0 || targetedOre >= 0) ?
                    C(125, 245, 170, 245) : C(245, 250, 255, 220);
                DrawLineEx((Vector2){crossX-9, crossY}, (Vector2){crossX-3, crossY}, 2.0f, crossColor);
                DrawLineEx((Vector2){crossX+3, crossY}, (Vector2){crossX+9, crossY}, 2.0f, crossColor);
                DrawLineEx((Vector2){crossX, crossY-9}, (Vector2){crossX, crossY-3}, 2.0f, crossColor);
                DrawLineEx((Vector2){crossX, crossY+3}, (Vector2){crossX, crossY+9}, 2.0f, crossColor);
                DrawCircle(crossX, crossY, 1.8f, crossColor);
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
                Rectangle invPanel = {screenWidth*0.10f, screenHeight*0.12f, screenWidth*0.80f, screenHeight*0.76f};
                DrawRectangleRounded(invPanel, 0.025f, 8, C(30, 32, 30, 248));
                DrawRectangleRoundedLinesEx(invPanel, 0.025f, 8, 1.6f, C(116, 124, 104, 245));
                DrawText("INVENTORY", (int)invPanel.x+18, (int)invPanel.y+16, 22, RAYWHITE);
                DrawText("ITEMS / QUICK ACCESS", (int)invPanel.x+20, (int)invPanel.y+45, 11, C(145, 190, 220, 255));
                for (int item = 0; item < 12; item++)
                {
                    float gap = 8.0f;
                    float cellSize = fminf(54.0f, (invPanel.width-32.0f-gap*3.0f)/4.0f);
                    float gridWidth = cellSize*4.0f+gap*3.0f;
                    float gridX = invPanel.x+(invPanel.width-gridWidth)*0.5f;
                    Rectangle r = {gridX+(item%4)*(cellSize+gap),
                                   invPanel.y+72+(item/4)*58, cellSize, cellSize};
                    DrawRectangle((int)r.x+2,(int)r.y+3,(int)r.width,(int)r.height,C(0,0,0,105));
                    DrawRectangle(r.x,r.y,r.width,r.height,C(42,46,42,255));
                    DrawRectangle((int)r.x+2,(int)r.y+2,(int)r.width-4,(int)r.height-4,C(51,55,49,255));
                    DrawRectangleLinesEx(r,1.2f,C(105,116,99,235));
                    DrawRectangle(r.x,r.y,3,r.height,(item==2||item==3)?C(142,169,91,255):C(103,119,91,255));
                    int labelSize = (item == 7) ? 7 : 8;
                    DrawText(itemNames[item],(int)r.x+5,(int)r.y+7,labelSize,C(210,218,204,255));
                    DrawText(TextFormat("x%d",itemCounts[item]),
                        (int)(r.x+r.width-MeasureText(TextFormat("x%d",itemCounts[item]),10)-5),
                        (int)(r.y+r.height-14),10,C(190,211,164,255));
                }
                DrawSurvivalButton(inventoryButton, "BAG", 13, true);
                DrawText("CRAFTING", (int)invPanel.x+18, (int)invPanel.y+242,
                    12, C(145, 205, 240, 255));
                Rectangle craftAxeButton = {invPanel.x+16, invPanel.y+260, invPanel.width-32, 32};
                Rectangle craftMedButton = {invPanel.x+16, invPanel.y+296, invPanel.width-32, 32};
                Rectangle craftDoorButton = {invPanel.x+16, invPanel.y+332, invPanel.width-32, 32};
                bool canCraftAxe = itemCounts[4] >= 3 && itemCounts[5] >= 2 && itemCounts[0] < 99;
                bool canCraftMed = itemCounts[4] >= 2 && itemCounts[5] >= 1 && itemCounts[1] < 99;
                DrawRectangleRounded(craftAxeButton, 0.16f, 8,
                    canCraftAxe ? C(31, 91, 119, 255) : C(42, 51, 62, 230));
                DrawRectangleRoundedLinesEx(craftAxeButton, 0.16f, 8, 1.0f, C(100, 180, 220, 210));
                DrawText("CRAFT AXE  -  3 WOOD / 2 STONE",
                    (int)craftAxeButton.x+10, (int)craftAxeButton.y+10, 10, RAYWHITE);
                DrawRectangleRounded(craftMedButton, 0.16f, 8,
                    canCraftMed ? C(31, 91, 119, 255) : C(42, 51, 62, 230));
                DrawRectangleRoundedLinesEx(craftMedButton, 0.16f, 8, 1.0f, C(100, 180, 220, 210));
                DrawText("CRAFT MED  -  2 WOOD / 1 STONE",
                    (int)craftMedButton.x+10, (int)craftMedButton.y+10, 10, RAYWHITE);
                bool canCraftDoor = itemCounts[4] >= 10 && itemCounts[6] < 99;
                DrawRectangleRounded(craftDoorButton, 0.16f, 8, canCraftDoor ? C(31,91,119,255) : C(42,51,62,230));
                DrawRectangleRoundedLinesEx(craftDoorButton, 0.16f, 8, 1.0f, C(100,180,220,210));
                DrawText("CRAFT DOOR - 10 WOOD", (int)craftDoorButton.x+10, (int)craftDoorButton.y+10, 10, RAYWHITE);
                DrawText("TAP FOOD / WATER TO CONSUME",
                    (int)invPanel.x+18, (int)(invPanel.y+invPanel.height-26),
                    10, C(145, 190, 220, 255));
            }
        }

        if(gasCrateOpen&&openedGasCrate>=0){
            DrawRectangle(0,0,screenWidth,screenHeight,C(0,5,16,190));
            Rectangle cp={screenWidth*0.14f,screenHeight*0.30f,screenWidth*0.72f,screenHeight*0.40f};
            DrawRectangleRounded(cp,0.025f,8,C(25,28,27,250));
            DrawRectangleRoundedLinesEx(cp,0.025f,8,1.7f,C(135,145,119,255));
            DrawRectangle((int)cp.x+1,(int)cp.y+48,(int)cp.width-2,1,C(95,108,89,210));
            DrawText(openedGasCrate==3?"WEAPON CRATE":"STORAGE CRATE",(int)cp.x+15,(int)cp.y+14,18,RAYWHITE);
            Rectangle close={cp.x+cp.width-76,cp.y+9,66,29};
            DrawRectangleRounded(close,0.18f,8,C(78,48,43,255));
            DrawText("CLOSE",(int)close.x+11,(int)close.y+9,11,RAYWHITE);
            float gap=7.0f,cw=fminf((cp.width-32.0f-gap*3.0f)/4.0f,(cp.height-76.0f-gap)/2.0f),ch=cw;
            float gridX=cp.x+(cp.width-(cw*4.0f+gap*3.0f))*0.5f;
            for(int slot=0;slot<8;slot++){
                Rectangle cell={gridX+(slot%4)*(cw+gap),cp.y+52+(slot/4)*(ch+gap),cw,ch};
                DrawRectangle((int)cell.x+2,(int)cell.y+3,(int)cell.width,(int)cell.height,C(0,0,0,100));
                DrawRectangle(cell.x,cell.y,cell.width,cell.height,C(43,47,43,255));
                DrawRectangle((int)cell.x+2,(int)cell.y+2,(int)cell.width-4,(int)cell.height-4,C(51,55,49,255));
                DrawRectangleLinesEx(cell,1.2f,C(105,116,99,235));
                DrawRectangle(cell.x,cell.y,3,cell.height,C(112,145,76,255));
                int amount=gasCrateAmounts[openedGasCrate][slot],item=gasCrateItems[openedGasCrate][slot];
                if(amount>0){
                    DrawText(itemNames[item],(int)cell.x+6,(int)(cell.y+cell.height*0.39f),cell.width<100?9:12,C(225,229,218,255));
                    DrawText(TextFormat("x%d",amount),(int)(cell.x+cell.width-MeasureText(TextFormat("x%d",amount),11)-5),(int)(cell.y+cell.height-17),11,C(190,211,164,255));
                } else DrawText("EMPTY",(int)cell.x+6,(int)(cell.y+cell.height*0.48f),10,C(116,128,115,255));
            }
            DrawText("TAP A SLOT TO TAKE ITEMS",(int)cp.x+15,(int)(cp.y+cp.height-17),9,C(160,180,160,255));
        }

        if (mapOpen && !mainMenuOpen)
        {
            DrawRectangle(0,0,screenWidth,screenHeight,C(3,10,18,225));
            float ms=fminf(fminf(screenWidth*0.78f,screenHeight*0.70f),430.0f), mx=(screenWidth-ms)*0.5f, my=(screenHeight-ms)*0.5f;
            DrawRectangleRounded((Rectangle){mx-12,my-36,ms+24,ms+82},0.06f,12,C(15,29,34,250));
            DrawRectangleRoundedLinesEx((Rectangle){mx-12,my-36,ms+24,ms+82},0.06f,12,2,C(115,165,150,230));
            DrawText("ISLAND MAP",(int)mx,(int)my-27,18,RAYWHITE);
            float cx=mx+ms*0.5f,cy=my+ms*0.5f,rr=ms*0.46f;
            DrawCircle((int)cx,(int)cy,rr,C(48,91,83,255)); DrawCircleLines((int)cx,(int)cy,rr,C(185,190,157,255));
            DrawCircle((int)cx,(int)cy,rr*0.78f,C(75,111,75,255));
            DrawCircleSector((Vector2){cx+rr*0.55f,cy-rr*0.48f},rr*0.52f,210,350,24,C(184,195,190,230));
            DrawCircleSector((Vector2){cx-rr*0.55f,cy+rr*0.08f},rr*0.48f,70,260,24,C(139,126,91,220));
            for(int m=0;m<6;m++){float a=m*1.05f+0.4f,ax=cx+cosf(a)*rr*0.57f,ay=cy+sinf(a)*rr*0.57f;DrawCircle((int)ax,(int)ay,(int)(ms*0.035f),C(104,111,105,255));DrawCircle((int)(ax-3),(int)(ay-4),(int)(ms*0.018f),C(170,179,170,255));}
            for(int t=0;t<18;t++){float a=t*2.399f,r=rr*(0.18f+(float)(t%5)*0.105f);DrawCircle((int)(cx+cosf(a)*r),(int)(cy+sinf(a)*r),(int)(ms*0.012f),C(34,91,53,255));}
            float sc=rr/260.0f,px=cx+player.x*sc,py=cy+player.z*sc;
            DrawCircle((int)px,(int)py,8,C(245,245,230,255));DrawCircle((int)px,(int)py,4,C(55,145,255,255));DrawCircleLines((int)px,(int)py,11,RAYWHITE);DrawText("YOU",(int)px+12,(int)py-8,12,RAYWHITE);
            DrawText("Tap MAP to close",(int)(mx+ms-130),(int)(my+ms+12),10,C(170,190,195,255));
            DrawRectangleRounded(mapButton,0.30f,16,C(20,34,52,250));
            DrawRectangleRoundedLinesEx(mapButton,0.30f,16,2,C(130,205,255,255));
            DrawText("CLOSE MAP",mapButton.x+mapButton.width/2-MeasureText("CLOSE MAP",16)/2,mapButton.y+23,16,RAYWHITE);
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

            const char *qualityLabels[3] = {"POTATO", "DETAILED", "REALISM"};
            Rectangle qualityButtons[3] = {lowButton, mediumButton, highButton};
            for (int qi = 0; qi < 3; qi++)
            {
                int fontSize = 12;
                int textWidth = MeasureText(qualityLabels[qi], fontSize);
                DrawText(qualityLabels[qi],
                    (int)(qualityButtons[qi].x+qualityButtons[qi].width/2-textWidth/2),
                    (int)(qualityButtons[qi].y+16), fontSize, RAYWHITE);
            }

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

            DrawText("SIZE", (int)editSizeSlider.x, 99, 14, RAYWHITE);
            DrawText("OPACITY", (int)editAlphaSlider.x, 151, 14, RAYWHITE);

            DrawRectangleRounded(editSizeSlider, 0.5f, 8, C(35, 55, 72, 255));
            DrawRectangleRounded(editAlphaSlider, 0.5f, 8, C(35, 55, 72, 255));
            float sizeT = (actionButtonScale[selectedActionControl]-0.55f)/1.15f;
            float alphaT = (actionButtonAlpha[selectedActionControl]-60.0f)/195.0f;
            DrawRectangleRounded((Rectangle){editSizeSlider.x,editSizeSlider.y,editSizeSlider.width*sizeT,editSizeSlider.height},0.5f,8,C(65,175,235,255));
            DrawRectangleRounded((Rectangle){editAlphaSlider.x,editAlphaSlider.y,editAlphaSlider.width*alphaT,editAlphaSlider.height},0.5f,8,C(65,175,235,255));
            DrawCircleV((Vector2){editSizeSlider.x+editSizeSlider.width*sizeT,editSizeSlider.y+8},10,RAYWHITE);
            DrawCircleV((Vector2){editAlphaSlider.x+editAlphaSlider.width*alphaT,editAlphaSlider.y+8},10,RAYWHITE);

            DrawText(TextFormat("%d%%", (int)(actionButtonScale[selectedActionControl]*100.0f)),
                (int)(editSizeSlider.x+editSizeSlider.width+8), 116, 13, RAYWHITE);
            DrawText(TextFormat("%d%%", actionButtonAlpha[selectedActionControl]*100/255),
                (int)(editAlphaSlider.x+editAlphaSlider.width+8), 171, 13, RAYWHITE);

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
            DrawRectangleLinesEx(mapButton,3,C(80,200,255,255));

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

        if (loadingScreen)
        {
            DrawRectangleGradientV(0, 0, screenWidth, screenHeight,
                                   C(7, 15, 31, 255), C(18, 39, 65, 255));
            DrawRectangleLinesEx((Rectangle){24, 24, screenWidth-48, screenHeight-48},
                                 2.0f, C(85, 160, 215, 130));
            const char *loadTitle = "boltSurvival";
            int loadTitleSize = (int)fminf(48.0f, screenWidth*0.10f);
            DrawText(loadTitle, screenWidth/2-MeasureText(loadTitle, loadTitleSize)/2,
                     screenHeight/2-90, loadTitleSize, RAYWHITE);
            const char *loadLabel = "LOADING RESOURCES";
            DrawText(loadLabel, screenWidth/2-MeasureText(loadLabel, 20)/2,
                     screenHeight/2-20, 20, C(190, 220, 245, 255));
            Rectangle loadBar = {screenWidth*0.20f, screenHeight/2+25,
                                 screenWidth*0.60f, 14};
            DrawRectangleRounded(loadBar, 0.5f, 12, C(35, 55, 77, 255));
            DrawRectangleRounded((Rectangle){loadBar.x, loadBar.y,
                                 loadBar.width*loadingProgress, loadBar.height},
                                 0.5f, 12, C(55, 175, 245, 255));
            DrawRectangleRoundedLinesEx(loadBar, 0.5f, 12, 1.5f,
                                        C(130, 195, 235, 220));
            const char *percentText = TextFormat("%d%%", (int)(loadingProgress*100.0f));
            DrawText(percentText, screenWidth/2-MeasureText(percentText, 18)/2,
                     screenHeight/2+55, 18, RAYWHITE);
        }

        EndDrawing();
    }

    UnloadRenderTexture(sceneTexture);
    CloseWindow();

    return 0;
}

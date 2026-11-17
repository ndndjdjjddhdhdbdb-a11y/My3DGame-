#include "raymob.h"
#include "rlgl.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Шрифт интерфейса. На Android читаем TTF напрямую через stdio:
// LoadFontEx использует загрузчик ресурсов Raylib, который не обязан
// открывать абсолютные пути системных файлов.
static Font cyrillicFont;

static Font LoadAndroidCyrillicFont(int fontSize, int *codepoints, int codepointCount)
{
    const char *fontPaths[] = {
        "/system/fonts/Roboto-Regular.ttf",
        "/system/fonts/RobotoStatic-Regular.ttf",
        "/system/fonts/NotoSans-Regular.ttf"
    };

    for (int i = 0; i < (int)(sizeof(fontPaths)/sizeof(fontPaths[0])); i++)
    {
        FILE *file = fopen(fontPaths[i], "rb");
        if (file == NULL) continue;

        if (fseek(file, 0, SEEK_END) != 0)
        {
            fclose(file);
            continue;
        }

        long fileSize = ftell(file);
        if (fileSize <= 0 || fileSize > 16 * 1024 * 1024 ||
            fseek(file, 0, SEEK_SET) != 0)
        {
            fclose(file);
            continue;
        }

        unsigned char *fontBytes = (unsigned char *)malloc((size_t)fileSize);
        if (fontBytes == NULL)
        {
            fclose(file);
            continue;
        }

        size_t bytesRead = fread(fontBytes, 1, (size_t)fileSize, file);
        fclose(file);

        if (bytesRead == (size_t)fileSize)
        {
            Font loaded = LoadFontFromMemory(".ttf", fontBytes, (int)fileSize,
                                             fontSize, codepoints, codepointCount);
            free(fontBytes);
            if (loaded.texture.id != 0) return loaded;
        }
        else
        {
            free(fontBytes);
        }
    }

    return GetFontDefault();
}

static void DrawUiText(const char *text, int x, int y, int fontSize, Color color)
{
    DrawTextEx(cyrillicFont, text, (Vector2){(float)x, (float)y},
               (float)fontSize, 1.0f, color);
}
static void DrawUiTextBold(const char *text, int x, int y, int fontSize, Color color)
{
    // Небольшое смещение нескольких слоёв делает системный шрифт визуально жирнее.
    DrawTextEx(cyrillicFont,text,(Vector2){(float)x+0.55f,(float)y},(float)fontSize,1.0f,color);
    DrawTextEx(cyrillicFont,text,(Vector2){(float)x,(float)y+0.55f},(float)fontSize,1.0f,color);
    DrawTextEx(cyrillicFont,text,(Vector2){(float)x+0.55f,(float)y+0.55f},(float)fontSize,1.0f,color);
    DrawTextEx(cyrillicFont,text,(Vector2){(float)x,(float)y},(float)fontSize,1.0f,color);
}
static int MeasureUiText(const char *text, int fontSize)
{
    return (int)MeasureTextEx(cyrillicFont, text, (float)fontSize, 1.0f).x;
}
#define DrawText(text, x, y, size, color) DrawUiText(text, x, y, size, color)
#define MeasureText(text, size) MeasureUiText(text, size)

#define MAX_TOUCHES 10
#define MAX_BARRELS 14
#define MAX_BUILD_PIECES 64
#define MAX_TREES 420
#define MAX_WOOD_DROPS 64
#define MAX_ORE_NODES 32
#define MAX_ORE_DROPS 16
#define MAX_WILD_ANIMALS 12

// Центры биомов выбираются один раз при создании мира.
static Vector2 gBiomeCenters[3] = {{-270.0f,-230.0f},{285.0f,235.0f},{-290.0f,260.0f}};

static float BiomeBoundaryRadius(int biome, float angle)
{
    float phase=(float)biome*1.73f;
    return 78.0f + 10.0f*sinf(angle*2.0f+phase)
                  + 7.0f*sinf(angle*3.0f-phase*1.4f)
                  + 4.0f*cosf(angle*5.0f+phase*0.7f);
}

static int GetWorldBiome(float x, float z)
{
    for (int i=0;i<3;i++)
    {
        float dx=x-gBiomeCenters[i].x, dz=z-gBiomeCenters[i].y;
        float angle=atan2f(dz,dx);
        float radius=BiomeBoundaryRadius(i+1,angle);
        if (dx*dx+dz*dz < radius*radius) return i+1;
    }
    return 0;
}

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
    if (type == 0) return "ЖЕЛЕЗО";
    if (type == 1) return "ЛАЗУРИТ";
    if (type == 2) return "СЕРА";
    return "КРЕМНИЙ";
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

static int GetTreeHits(int biome)
{
    if (biome == 1) return 20; // снежное
    if (biome == 2) return 5;  // песочное
    return 15;                 // лесное
}

static int GetTreeWoodYield(int biome)
{
    if (biome == 1) return 55;
    if (biome == 2) return 15;
    return 40;
}

static const char *GetTreeTypeName(int biome)
{
    if (biome == 1) return "СНЕЖНОЕ";
    if (biome == 2) return "ПЕСОЧНОЕ";
    return "ЛЕСНОЕ";
}

static void DrawTree(float x, float z, float scale, int quality, int biome)
{
    // Биомы карты: 1 — снежный, 2 — песчаный, 3 и 0 — лесной.
    if (biome == 2)
    {
        // Пальма: изогнутый сегментированный ствол и длинные веерные листья.
        Color trunk = C(126, 87, 49, 255);
        Color leaf = C(49, 119, 57, 255);
        Color leafLight = C(91, 151, 64, 255);
        Vector3 base = {x, 0.08f*scale, z};
        Vector3 crown = {x+0.16f*scale, 3.05f*scale, z};
        DrawCylinderEx(base, crown, 0.25f*scale, 0.16f*scale, 8, trunk);
        for (int seg=0; seg<7; seg++)
        {
            float t=(seg+0.5f)/7.0f;
            Vector3 p={x+0.16f*t*scale, (0.35f+t*2.45f)*scale, z};
            DrawCylinderEx((Vector3){p.x-0.02f*scale,p.y-0.10f*scale,p.z},
                           (Vector3){p.x+0.02f*scale,p.y+0.10f*scale,p.z},
                           0.205f*(1.0f-t*0.48f)*scale,
                           0.19f*(1.0f-t*0.48f)*scale, 7, trunk);
        }
        for (int arm=0; arm<7; arm++)
        {
            float a=arm*0.8976f;
            Vector3 root=crown;
            Vector3 tip={crown.x+cosf(a)*1.65f*scale,
                         crown.y-0.28f*scale,
                         crown.z+sinf(a)*1.65f*scale};
            DrawCylinderEx(root,tip,0.075f*scale,0.025f*scale,5,leaf);
            for (int blade=0; blade<5; blade++)
            {
                float t=0.25f+blade*0.14f;
                Vector3 p={root.x+(tip.x-root.x)*t,
                           root.y+(tip.y-root.y)*t,
                           root.z+(tip.z-root.z)*t};
                float side=(blade%2==0)?1.0f:-1.0f;
                Vector3 q={p.x-cosf(a)*0.18f*scale-sinf(a)*side*0.48f*scale,
                           p.y-0.10f*scale,
                           p.z-sinf(a)*0.18f*scale+cosf(a)*side*0.48f*scale};
                DrawCylinderEx(p,q,0.045f*scale,0.008f*scale,4,
                               (blade%2)?leaf:leafLight);
            }
        }
        if (quality >= 1)
            for (int nut=0; nut<3; nut++)
            {
                float a=nut*2.094f;
                DrawSphere((Vector3){crown.x+cosf(a)*0.16f*scale,
                                     crown.y-0.18f*scale,
                                     crown.z+sinf(a)*0.16f*scale},
                           0.11f*scale,C(107,76,39,255));
            }
        return;
    }

    Color bark=C(101,68,43,255);
    DrawCylinder((Vector3){x,1.22f*scale,z},
                 0.18f*scale,0.24f*scale,2.44f*scale,7,bark);
    DrawCylinder((Vector3){x,0.16f*scale,z},
                 0.28f*scale,0.18f*scale,0.32f*scale,7,C(83,55,36,255));

    bool snowy=(biome==1);
    Color deep=snowy?C(38,91,66,255):C(24,91,42,255);
    Color mid=snowy?C(52,119,78,255):C(35,119,48,255);
    Color top=snowy?C(70,139,91,255):C(53,143,59,255);
    Color snow=C(238,246,249,255);

    // Ярусные ветви делают лесную и снежную ели узнаваемыми.
    for (int tier=0;tier<5;tier++)
    {
        float y=(0.95f+tier*0.48f)*scale;
        float radius=(1.12f-tier*0.16f)*scale;
        float height=(1.18f-tier*0.08f)*scale;
        Color foliage=(tier<2)?deep:((tier<4)?mid:top);
        DrawCylinder((Vector3){x,y+height*0.30f,z},
                     0.04f*scale,radius,height,8,foliage);
        if (snowy)
        {
            // Светлые снежные шапки на каждом ярусе.
            DrawCylinder((Vector3){x,y+height*0.48f,z},
                         0.0f,radius*0.88f,height*0.62f,8,snow);
            DrawCylinder((Vector3){x,y+height*0.62f,z},
                         0.0f,radius*0.72f,height*0.40f,8,C(250,252,253,255));
        }
    }
    if (quality >= 1)
    {
        for (int b=0;b<5;b++)
        {
            float a=b*1.2566f;
            Vector3 p={x+cosf(a)*0.62f*scale,1.55f*scale,
                       z+sinf(a)*0.62f*scale};
            DrawSphere(p,0.22f*scale,snowy?snow:mid);
        }
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
             w+0.22f*scale, 0.28f*scale,
             d+0.22f*scale, roof);

    DrawCube((Vector3){x, h+0.48f*scale, z},
             w*0.58f, 0.18f*scale,
             d+0.12f*scale, C(76, 72, 66, 255));

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
    // Узкая боковая стеклянная секция рядом с центральным входом.
    DrawCube((Vector3){x-3.18f,1.72f,z-9.16f},1.25f,2.65f,0.055f,C(78,151,169,185));
    DrawCube((Vector3){x-3.82f,1.72f,z-9.20f},0.08f,2.85f,0.10f,metal);
    DrawCube((Vector3){x-2.54f,1.72f,z-9.20f},0.08f,2.85f,0.10f,metal);
    DrawCube((Vector3){x-3.18f,3.15f,z-9.20f},1.35f,0.08f,0.10f,metal);
    DrawCube((Vector3){x-3.18f,0.29f,z-9.20f},1.35f,0.08f,0.10f,metal);

    // Две стеклянные створки центральной двери: левая почти закрыта,
    // правая полностью распахнута внутрь вдоль стены.
    DrawCube((Vector3){x-1.55f,1.55f,z-9.22f},1.05f,2.85f,0.075f,C(91,164,178,185));
    DrawCube((Vector3){x-2.08f,1.55f,z-9.27f},0.07f,2.95f,0.10f,metal);
    DrawCube((Vector3){x-1.02f,1.55f,z-9.27f},0.07f,2.95f,0.10f,metal);
    DrawCube((Vector3){x+2.18f,1.55f,z-8.50f},0.10f,2.85f,1.35f,C(91,164,178,185));
    DrawCube((Vector3){x+2.18f,1.55f,z-7.82f},0.12f,2.95f,0.08f,metal);
    DrawCube((Vector3){x+2.18f,1.55f,z-9.18f},0.12f,2.95f,0.08f,metal);

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

    // Обычные ящики стоят вдоль боковой стены, не перекрывая центральный проход.
    DrawGasCrate((Vector3){x-8.6f,0.48f,z+4.0f},false);
    DrawGasCrate((Vector3){x-8.6f,0.48f,z+6.0f},false);
    DrawGasCrate((Vector3){x+8.6f,0.48f,z+4.0f},false);
    // Оружейный ящик — отдельно на верхнем уровне.
    DrawGasCrate((Vector3){x,4.05f,z+0.5f},true);

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
        if(fabsf(x)<12.0f+radius && fabsf(z+31.0f)<0.24f+radius && fabsf(x)>4.0f) return true;
        // Стеклянная боковая секция и почти закрытая левая створка входа.
        if(fabsf(z+31.0f)<0.18f+radius &&
           (fabsf(x+3.18f)<0.68f+radius || fabsf(x+1.55f)<0.55f+radius)) return true;
        if(fabsf(z+22.0f)<9.0f+radius && fabsf(fabsf(x)-12.0f)<0.24f+radius &&
           !(fabsf(x-12.25f)<0.9f && z>-28.0f && z<-25.0f)) return true;
        // Перегородки трёх задних комнат с дверными проёмами.
        for(int side=-1;side<=1;side+=2)
        {
            float wallX=side*4.0f;
            if(fabsf(x-wallX)<0.24f+radius && z>-19.5f && z<-18.2f) return true;
            if(fabsf(x-wallX)<0.24f+radius && z>-16.0f && z<-13.5f) return true;
        }
        // Убрана старая невидимая перегородка за бензоколонками.
    }
    // Коллизии ящиков совпадают с новыми местами их моделей.
    if(playerY<1.35f)
    {
        const float crateX[3]={-8.6f,-8.6f,8.6f};
        const float crateZ[3]={-18.0f,-16.0f,-18.0f};
        for(int i=0;i<3;i++)
        {
            float dx=x-crateX[i], dz=z-crateZ[i];
            float crateRadius=0.78f+radius;
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

 // Сквозной горный проход: широкая ровная площадка внутри подходит для строительства.
static void DrawMountainPassage(float x, float z, float scale, bool snowy)
{
    Color stone = snowy ? C(151, 164, 164, 255) : C(91, 106, 91, 255);
    Color edge = snowy ? C(190, 201, 199, 255) : C(126, 137, 120, 255);
    // Боковые массивы и верх горы оставляют широкий проход насквозь.
    DrawCube((Vector3){x-5.0f*scale, 3.5f*scale, z+4.0f*scale},
             4.0f*scale, 7.0f*scale, 24.0f*scale, stone);
    DrawCube((Vector3){x+5.0f*scale, 3.5f*scale, z+4.0f*scale},
             4.0f*scale, 7.0f*scale, 24.0f*scale, stone);
    DrawCube((Vector3){x, 7.0f*scale, z+4.0f*scale},
             14.0f*scale, 2.0f*scale, 24.0f*scale, stone);
    // Каменные склоны снаружи подчёркивают форму горы.
    DrawSphere((Vector3){x-7.0f*scale, 2.0f*scale, z-3.0f*scale}, 4.5f*scale, edge);
    DrawSphere((Vector3){x+7.0f*scale, 2.0f*scale, z-3.0f*scale}, 4.5f*scale, edge);
    DrawSphere((Vector3){x-7.0f*scale, 2.0f*scale, z+11.0f*scale}, 4.5f*scale, stone);
    DrawSphere((Vector3){x+7.0f*scale, 2.0f*scale, z+11.0f*scale}, 4.5f*scale, stone);
}

// Большое сквозное отверстие: просторный тоннель с ровным полом для постройки дома.
static void DrawMountainHole(float x, float z, float scale, bool snowy)
{
    Color stone = snowy ? C(148, 160, 160, 255) : C(91, 105, 91, 255);
    Color edge = snowy ? C(190, 201, 199, 255) : C(125, 135, 119, 255);
    DrawCube((Vector3){x-6.5f*scale, 3.5f*scale, z+3.0f*scale},
             4.0f*scale, 7.0f*scale, 20.0f*scale, stone);
    DrawCube((Vector3){x+6.5f*scale, 3.5f*scale, z+3.0f*scale},
             4.0f*scale, 7.0f*scale, 20.0f*scale, stone);
    DrawCube((Vector3){x, 7.0f*scale, z+3.0f*scale},
             17.0f*scale, 2.0f*scale, 20.0f*scale, stone);
    // Наружные каменные склоны, не перекрывающие сквозной проём.
    DrawSphere((Vector3){x-8.0f*scale, 2.0f*scale, z-2.0f*scale}, 5.0f*scale, edge);
    DrawSphere((Vector3){x+8.0f*scale, 2.0f*scale, z-2.0f*scale}, 5.0f*scale, edge);
    DrawSphere((Vector3){x-8.0f*scale, 2.0f*scale, z+9.0f*scale}, 5.0f*scale, stone);
    DrawSphere((Vector3){x+8.0f*scale, 2.0f*scale, z+9.0f*scale}, 5.0f*scale, stone);
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

// Коллизия гор: внутри коридора можно пройти и строить, каменные боковины непроходимы.
static bool IsMountainBlockedAt(float x, float z, float radius)
{
    const Vector3 passage[2] = {{-118,0,-112}, {132,0,94}};
    const Vector3 holes[2] = {{108,0,-126}, {-142,0,78}};
    for (int i=0; i<2; i++)
    {
        float dx=x-passage[i].x, dz=z-passage[i].z;
        if (fabsf(dz) < 12.4f && fabsf(dx) > 2.55f-radius) return true;
        dx=x-holes[i].x; dz=z-holes[i].z;
        if (fabsf(dz) < 10.4f && fabsf(dx) > 4.05f-radius) return true;
    }
    return false;
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
    if (IsMountainBlockedAt(x,z,playerRadius)) return true;

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
    if (IsMountainBlockedAt(x,z,cameraRadius)) return true;

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

// Земля и дороги доступны для строительства. Запрет оставлен только
// на самой заправке и небольшом отступе вокруг её корпуса.
static bool IsBuildZoneForbidden(float x, float z, float halfX, float halfZ)
{
    const float stationHalfWidth = 15.5f;
    const float stationMinZ = -35.5f;
    const float stationMaxZ = -8.5f;
    if (x+halfX > -stationHalfWidth && x-halfX < stationHalfWidth &&
        z+halfZ > stationMinZ && z-halfZ < stationMaxZ) return true;
    return false;
}

// Проверяем не только центр, но и края постройки: нельзя ставить фундамент
// поверх стен, колонн, скал и других физических препятствий.
static bool IsBuildPlacementBlocked(float x, float z, int type, int storedType,
                                    float playerY, Vector3 *barrels,
                                    bool *barrelDestroyed)
{
    float hx=1.25f, hz=1.25f;
    if (type==1) { if(storedType==4) {hx=0.12f;hz=1.25f;} else {hx=1.25f;hz=0.12f;} }
    else if(type==2) {hx=0.72f;hz=0.12f;}
    if (IsBuildZoneForbidden(x,z,hx,hz)) return true;

    const float sample[9][2]={{0,0},{-1,-1},{1,-1},{1,1},{-1,1},
                                {0,-1},{1,0},{0,1},{-1,0}};
    for(int i=0;i<9;i++)
    {
        float sx=x+sample[i][0]*hx;
        float sz=z+sample[i][1]*hz;
        if(IsBlockedAt(sx,sz,playerY,barrels,barrelDestroyed)) return true;
    }
    return false;
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
    if (IsBuildPlacementBlocked(x,z,type,storedType,player.y,barrels,barrelDestroyed)) return false;
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

static void DrawRotatedCubeY(Vector3 center, float width, float height, float depth, float yaw, Color color)
{
    rlPushMatrix(); rlTranslatef(center.x,center.y,center.z);
    rlRotatef(-yaw*RAD2DEG,0.0f,1.0f,0.0f);
    DrawCube((Vector3){0,0,0},width,height,depth,color); rlPopMatrix();
}
static void DrawHeldAxe(Vector3 grip, Vector3 head, float yaw, float scale)
{
    // Чёрная рукоять с оранжевыми противоскользящими вставками.
    DrawCylinderEx(grip,head,0.038f*scale,0.030f*scale,10,C(31,34,34,255));
    Vector3 gripMid={grip.x+(head.x-grip.x)*0.30f,
                     grip.y+(head.y-grip.y)*0.30f,
                     grip.z+(head.z-grip.z)*0.30f};
    DrawCylinderEx(gripMid,
        (Vector3){gripMid.x+(head.x-grip.x)*0.10f,
                  gripMid.y+(head.y-grip.y)*0.10f,
                  gripMid.z+(head.z-grip.z)*0.10f},
        0.043f*scale,0.040f*scale,10,C(226,91,25,255));
    Vector3 neck={head.x-(head.x-grip.x)*0.10f,
                  head.y-(head.y-grip.y)*0.10f,
                  head.z-(head.z-grip.z)*0.10f};
    DrawCylinderEx(neck,head,0.052f*scale,0.045f*scale,10,C(47,50,49,255));
    // Широкое оранжевое лезвие и тёмная металлическая обойма.
    DrawRotatedCubeY((Vector3){head.x,head.y+0.015f*scale,head.z},
        0.46f*scale,0.25f*scale,0.13f*scale,yaw,C(224,91,25,255));
    DrawRotatedCubeY((Vector3){head.x,head.y-0.005f*scale,head.z-0.012f*scale},
        0.18f*scale,0.29f*scale,0.15f*scale,yaw,C(42,46,45,255));
    DrawRotatedCubeY((Vector3){head.x,head.y+0.105f*scale,head.z-0.018f*scale},
        0.43f*scale,0.045f*scale,0.135f*scale,yaw,C(247,126,43,255));
}
static void DrawHeldWaterBottle(Vector3 center, float scale)
{
    // Грязно-зелёная пластиковая бутылка с синей крышкой.
    DrawCylinder((Vector3){center.x,center.y,center.z},
                 0.065f*scale,0.082f*scale,0.27f*scale,12,C(91,111,67,245));
    DrawSphere((Vector3){center.x,center.y+0.105f*scale,center.z},
               0.060f*scale,C(105,124,76,245));
    DrawCylinder((Vector3){center.x,center.y+0.155f*scale,center.z},
                 0.034f*scale,0.040f*scale,0.075f*scale,10,C(88,109,68,255));
    DrawCylinder((Vector3){center.x,center.y+0.205f*scale,center.z},
                 0.043f*scale,0.043f*scale,0.055f*scale,10,C(35,105,194,255));
    DrawCube((Vector3){center.x-0.032f*scale,center.y-0.005f*scale,
                       center.z-0.080f*scale},
             0.018f*scale,0.17f*scale,0.008f*scale,C(174,184,130,125));
}
static void DrawHeldMeat(Vector3 center, float scale)
{
    // Неровный кусок жареного мяса с румяной корочкой.
    DrawSphere((Vector3){center.x,center.y,center.z},0.125f*scale,C(111,54,31,255));
    DrawSphere((Vector3){center.x-0.065f*scale,center.y+0.025f*scale,center.z},
               0.090f*scale,C(151,70,39,255));
    DrawSphere((Vector3){center.x+0.055f*scale,center.y+0.035f*scale,center.z+0.015f*scale},
               0.085f*scale,C(177,91,43,255));
    DrawSphere((Vector3){center.x,center.y+0.075f*scale,center.z-0.025f*scale},
               0.075f*scale,C(194,111,54,255));
    DrawSphere((Vector3){center.x-0.025f*scale,center.y+0.045f*scale,center.z-0.073f*scale},
               0.035f*scale,C(224,151,79,255));
}

static void DrawFirstPersonArms(Vector3 eye, float yaw,
                                float walkPhase, bool moving, float time,
                                float attackProgress, int heldItemId,
                                float useProgress, int useItem)
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
        float useLift = (side == 1 && (useItem == 2 || useItem == 3)) ?
            sinf(useProgress*3.1415926f)*0.34f : 0.0f;

        Vector3 shoulder = {
            eye.x + right.x*s*0.42f + forward.x*(0.18f+armSwing*0.25f),
            eye.y - 0.30f + armLift,
            eye.z + right.z*s*0.42f + forward.z*(0.18f+armSwing*0.25f)
        };

        Vector3 elbow = {
            eye.x + right.x*s*0.48f + forward.x*(0.48f+armSwing*0.65f),
            eye.y - 0.52f + armLift*0.8f + useLift*0.75f,
            eye.z + right.z*s*0.48f + forward.z*(0.48f+armSwing*0.65f)
        };

        float swingPhase = 1.0f - attackProgress;
        float swing = sinf(swingPhase*3.1415926f);
        float punch = (side == 1) ? swing*(heldItemId == 7 ? 0.48f : 0.34f) : 0.0f;
        Vector3 wrist = {
            eye.x + right.x*s*0.34f + forward.x*(0.78f+armSwing+punch),
            eye.y - 0.48f + armLift + punch*0.10f + useLift,
            eye.z + right.z*s*0.34f + forward.z*(0.78f+armSwing+punch)
        };

        Vector3 hand = {
            eye.x + right.x*s*0.30f + forward.x*(0.88f+armSwing+punch),
            eye.y - 0.45f + armLift + punch*0.10f + useLift,
            eye.z + right.z*s*0.30f + forward.z*(0.88f+armSwing+punch)
        };

        DrawCylinderEx(shoulder, elbow, 0.16f, 0.13f, 8, shirt);
        DrawSphere(elbow, 0.13f, shirt);
        DrawCylinderEx(elbow, wrist, 0.105f, 0.085f, 8, skin);
        DrawSphere(hand, 0.105f, skin);
    }

    // Предмет в правой руке следует за направлением камеры.
    // Основание предмета совпадает с центром правой кисти и повторяет её движение.
    float armSwing = moving ? sinf(walkPhase)*0.13f : sinf(time*1.6f)*0.012f;
    float armLift = moving ? cosf(walkPhase*2.0f)*0.025f : sinf(time*1.6f)*0.012f;
    float useLift = (useItem == 2 || useItem == 3) ?
        sinf(useProgress*3.1415926f)*0.34f : 0.0f;
    Vector3 toolBase = {eye.x+right.x*0.30f+forward.x*(0.88f+armSwing),
                        eye.y-0.45f+armLift+useLift,
                        eye.z+right.z*0.30f+forward.z*(0.88f+armSwing)};
    float toolSwing = sinf((1.0f-attackProgress)*3.1415926f);
    float swingStrength = heldItemId == 7 ? 0.62f : 0.48f;
    Vector3 toolTip = {toolBase.x+forward.x*(0.20f+toolSwing*swingStrength*0.35f),
                       toolBase.y+0.27f-toolSwing*swingStrength*0.52f,
                       toolBase.z+forward.z*(0.20f+toolSwing*swingStrength*0.25f)};
    if(useItem==2 || useItem==3) heldItemId=useItem;
    if(heldItemId==0) DrawHeldAxe(toolBase,toolTip,yaw,1.0f);
    else if(heldItemId==7)
    {
        DrawCylinderEx(toolBase,toolTip,0.045f,0.035f,8,C(111,75,45,255));
        DrawCylinderEx(toolTip,(Vector3){toolTip.x+forward.x*0.22f,toolTip.y+0.02f,toolTip.z+forward.z*0.22f},0.075f,0.055f,8,C(65,72,78,255));
    }
    else if(heldItemId==1)
    {
        DrawRotatedCubeY(toolTip,0.27f,0.19f,0.14f,yaw,C(190,55,55,255));
        DrawRotatedCubeY((Vector3){toolTip.x,toolTip.y+0.01f,toolTip.z-0.08f},0.07f,0.17f,0.025f,yaw,RAYWHITE);
        DrawRotatedCubeY((Vector3){toolTip.x,toolTip.y+0.01f,toolTip.z-0.08f},0.17f,0.07f,0.025f,yaw,RAYWHITE);
    }
    else if(heldItemId==2) DrawHeldMeat(toolTip,1.0f);
    else if(heldItemId==3) DrawHeldWaterBottle(toolTip,1.0f);
    else
    {
        Color itemColor=heldItemId==4?C(126,82,47,255):heldItemId==5?C(105,105,105,255):
            heldItemId==6?C(115,82,52,255):heldItemId==9?C(48,91,205,255):
            heldItemId==10?C(224,194,49,255):heldItemId==11?C(205,211,218,255):C(139,112,77,255);
        DrawRotatedCubeY(toolTip,0.18f,0.16f,0.17f,yaw,itemColor);
    }
}

static void DrawPlayer(Vector3 player, float angle,
                       float walkPhase, bool moving, float attackProgress, int heldItemId,
                       float useProgress, int useItem)
{
    float swing = moving ? sinf(walkPhase)*0.30f : sinf(walkPhase)*0.025f;
    float bob = moving ? fabsf(cosf(walkPhase))*0.035f : 0.0f;

    Color skin = C(211, 164, 125, 255);
    Color skinLight = C(231, 190, 151, 255);
    Color underwear = C(58, 64, 58, 255); // потёртые рабочие штаны
    Color underwearDark = C(35, 39, 36, 255);
    Color jacket = C(92, 83, 65, 255);
    Color jacketDark = C(55, 57, 49, 255);

    // Рабочие штаны и грубые ботинки в слегка состаренном стиле выжившего.
    Vector3 hipL = PlayerPoint(player, angle, -0.17f, 0.73f, 0.0f);
    Vector3 hipR = PlayerPoint(player, angle,  0.17f, 0.73f, 0.0f);
    Vector3 kneeL = PlayerPoint(player, angle, -0.18f, 0.39f, swing*0.55f);
    Vector3 kneeR = PlayerPoint(player, angle,  0.18f, 0.39f, -swing*0.55f);
    Vector3 ankleL = PlayerPoint(player, angle, -0.18f, 0.13f, swing);
    Vector3 ankleR = PlayerPoint(player, angle,  0.18f, 0.13f, -swing);

    DrawCylinderEx(hipL, kneeL, 0.145f, 0.115f, 10, underwear);
    DrawCylinderEx(kneeL, ankleL, 0.115f, 0.09f, 10, underwear);
    DrawCylinderEx(hipR, kneeR, 0.145f, 0.115f, 10, underwear);
    DrawCylinderEx(kneeR, ankleR, 0.115f, 0.09f, 10, underwear);
    DrawSphere(kneeL, 0.105f, underwear);
    DrawSphere(kneeR, 0.105f, underwear);

    // Простые шорты-бельё без ремней и карманов.
    DrawCube(PlayerPoint(player, angle, 0, 0.73f, 0),
             0.53f, 0.31f, 0.37f, underwear);
    DrawCube(PlayerPoint(player, angle, 0, 0.565f, -0.012f),
             0.49f, 0.035f, 0.375f, underwearDark);
    DrawCube(PlayerPoint(player, angle, -0.17f, 0.10f, -0.02f),0.27f,0.16f,0.42f,jacketDark);
    DrawCube(PlayerPoint(player, angle, 0.17f, 0.10f, -0.02f),0.27f,0.16f,0.42f,jacketDark);

    // Компактный походный рюкзак расположен за корпусом, чтобы не создавать
    // впечатление второго персонажа и не перекрывать силуэт головы.
    DrawCube(PlayerPoint(player, angle, 0, 1.16f, 0.46f),
             0.44f, 0.58f, 0.30f, C(57, 69, 54, 255));
    DrawCube(PlayerPoint(player, angle, 0, 1.16f, 0.63f),
             0.27f, 0.30f, 0.035f, C(78, 91, 67, 255));
    DrawCube(PlayerPoint(player, angle, -0.25f, 1.14f, 0.45f),
             0.10f, 0.31f, 0.22f, C(67, 80, 61, 255));
    DrawCube(PlayerPoint(player, angle, 0.25f, 1.14f, 0.45f),
             0.10f, 0.31f, 0.22f, C(67, 80, 61, 255));

    // Торс от третьего лица: отдельная цельная геометрия от талии
    // до плеч, чтобы корпус не терялся из-за тонкого цилиндра.
    DrawCylinderEx(
        PlayerPoint(player, angle, 0, 0.82f+bob, 0),
        PlayerPoint(player, angle, 0, 1.48f+bob, 0),
        0.245f,  0.315f, 12, jacket
    );
    DrawSphere(PlayerPoint(player, angle, 0, 1.20f+bob, 0),
               0.285f, jacket);
    DrawSphere(PlayerPoint(player, angle, 0, 1.49f+bob, 0),
               0.22f, jacket);

    // Две узкие лямки и нагрудный карман подчёркивают походную экипировку.
    Vector3 strapLTop = PlayerPoint(player, angle, -0.20f, 1.43f+bob, -0.245f);
    Vector3 strapLBottom = PlayerPoint(player, angle, -0.10f, 0.91f+bob, -0.275f);
    Vector3 strapRTop = PlayerPoint(player, angle, 0.20f, 1.43f+bob, -0.245f);
    Vector3 strapRBottom = PlayerPoint(player, angle, 0.10f, 0.91f+bob, -0.275f);
    DrawCylinderEx(strapLTop, strapLBottom, 0.032f, 0.026f, 6, C(171, 145, 99, 255));
    DrawCylinderEx(strapRTop, strapRBottom, 0.032f, 0.026f, 6, C(171, 145, 99, 255));
    DrawCube(PlayerPoint(player, angle, 0, 1.15f+bob, -0.302f),
             0.22f, 0.17f, 0.045f, C(67, 75, 56, 255));
    DrawCube(PlayerPoint(player, angle, 0, 1.25f+bob, -0.326f),
             0.08f, 0.025f, 0.012f, C(190, 160, 101, 255));

    DrawCylinderEx(
        PlayerPoint(player, angle, 0, 1.45f+bob, 0),
        PlayerPoint(player, angle, 0, 1.66f+bob, 0),
        0.12f, 0.105f, 10, skin
    );

    // Рукава куртки и открытые кисти.
    Vector3 shoulderL = PlayerPoint(player, angle, -0.36f, 1.42f+bob, 0);
    Vector3 elbowL = PlayerPoint(player, angle, -0.45f, 1.05f+bob, -swing*0.65f);
    Vector3 wristL = PlayerPoint(player, angle, -0.43f, 0.82f+bob, -swing*0.9f);
    Vector3 shoulderR = PlayerPoint(player, angle, 0.36f, 1.42f+bob, 0);
    Vector3 elbowR = PlayerPoint(player, angle, 0.45f, 1.05f+bob,
                                 swing*0.65f-attackProgress*0.24f);
    float useLift = (useItem == 2 || useItem == 3) ?
        sinf(useProgress*3.1415926f)*0.34f : 0.0f;
    Vector3 wristR = PlayerPoint(player, angle, 0.43f, 0.82f+bob+useLift,
                                 swing*0.9f-attackProgress*0.72f);

    DrawSphere(shoulderL, 0.145f, jacket);
    DrawSphere(shoulderR, 0.145f, jacket);
    DrawCylinderEx(shoulderL, elbowL, 0.115f, 0.09f, 10, jacket);
    DrawCylinderEx(shoulderR, elbowR, 0.115f, 0.09f, 10, jacket);
    DrawSphere(elbowL, 0.09f, jacketDark);
    DrawSphere(elbowR, 0.09f, jacketDark);
    DrawCylinderEx(elbowL, wristL, 0.085f, 0.068f, 10, jacket);
    DrawCylinderEx(elbowR, wristR, 0.085f, 0.068f, 10, jacket);
    DrawSphere(wristL, 0.075f, skinLight);
    DrawSphere(wristR, 0.075f, skinLight);

    // Предмет закреплён в кисти, а геометрия ориентирована по руке.
    Vector3 heldBase=wristR;
    Vector3 heldTip=PlayerPoint(player,angle,0.56f,1.13f+useLift,-0.38f);
    if(useItem==2 || useItem==3) heldItemId=useItem;
    if(heldItemId==0) DrawHeldAxe(heldBase,heldTip,angle,0.88f);
    else if(heldItemId==7)
    {
        DrawCylinderEx(heldBase,heldTip,0.045f,0.035f,8,C(111,75,45,255));
        DrawCylinderEx(heldTip,PlayerPoint(player,angle,0.56f,1.13f,-0.72f),0.07f,0.045f,8,C(65,72,78,255));
    }
    else if(heldItemId==1)
    {
        DrawRotatedCubeY(heldTip,0.25f,0.18f,0.14f,angle,C(190,55,55,255));
        DrawRotatedCubeY(PlayerPoint(player,angle,0.56f,1.14f,-0.47f),0.06f,0.16f,0.025f,angle,RAYWHITE);
    }
    else if(heldItemId==2) DrawHeldMeat(heldTip,0.88f);
    else if(heldItemId==3) DrawHeldWaterBottle(heldTip,0.88f);
    else
    {
        Color itemColor=heldItemId==4?C(126,82,47,255):heldItemId==5?C(105,105,105,255):
            heldItemId==6?C(115,82,52,255):heldItemId==9?C(48,91,205,255):
            heldItemId==10?C(224,194,49,255):heldItemId==11?C(205,211,218,255):C(139,112,77,255);
        DrawRotatedCubeY(heldTip,0.18f,0.16f,0.17f,angle,itemColor);
    }

    // Голова без кепки, с простыми волосами и лицом.
    DrawSphere(PlayerPoint(player, angle, 0, 1.86f+bob, 0),
               0.265f, skin);
    DrawSphere(PlayerPoint(player, angle, -0.265f, 1.86f+bob, 0),
               0.065f, skin);
    DrawSphere(PlayerPoint(player, angle, 0.265f, 1.86f+bob, 0),
               0.065f, skin);
    DrawSphere(PlayerPoint(player, angle, 0, 2.045f+bob, 0.035f),
               0.205f, C(65, 48, 35, 255));
    // Небольшие седые пряди и щетина — лицо выглядит немного старше.
    DrawSphere(PlayerPoint(player, angle, -0.13f, 2.13f+bob, -0.015f),0.085f,C(142,139,126,255));
    DrawSphere(PlayerPoint(player, angle, 0.13f, 2.13f+bob, -0.015f),0.085f,C(142,139,126,255));
    DrawSphere(PlayerPoint(player, angle, 0, 1.735f+bob, -0.235f),0.105f,C(91,75,62,255));

    DrawSphere(PlayerPoint(player, angle, -0.092f, 1.88f+bob, -0.245f),
               0.027f, C(37, 42, 37, 255));
    DrawSphere(PlayerPoint(player, angle, 0.092f, 1.88f+bob, -0.245f),
               0.027f, C(37, 42, 37, 255));
    DrawSphere(PlayerPoint(player, angle, 0, 1.81f+bob, -0.275f),
               0.045f, skinLight);
    DrawSphere(PlayerPoint(player, angle, -0.105f, 1.91f+bob, -0.255f),0.018f,C(102,82,68,255));
    DrawSphere(PlayerPoint(player, angle, 0.105f, 1.91f+bob, -0.255f),0.018f,C(102,82,68,255));
}
static void DrawRoadSegment(float x1, float z1, float x2, float z2)
{
    float cx=(x1+x2)*0.5f, cz=(z1+z2)*0.5f;
    float len=fabsf(x2-x1)+fabsf(z2-z1);
    bool horizontal=fabsf(x2-x1)>fabsf(z2-z1);
    float w=8.5f;
    DrawCube((Vector3){cx,-0.055f,cz},
             horizontal?len+w:w,0.055f,horizontal?w:len+w,
             C(111,99,78,255));
    DrawCube((Vector3){cx,-0.023f,cz},
             horizontal?len+w-1.2f: w-1.2f,0.035f,
             horizontal?w-1.2f:len+w-1.2f,
             C(151,132,99,255));
    // Две неглубокие колеи делают грунтовую дорогу менее плоской.
    Color tireTrack = C(91, 78, 60, 105);
    if (horizontal)
    {
        DrawCube((Vector3){cx, -0.006f, cz-1.65f},
                 len+w-1.5f, 0.006f, 0.20f, tireTrack);
        DrawCube((Vector3){cx, -0.006f, cz+1.65f},
                 len+w-1.5f, 0.006f, 0.20f, tireTrack);
    }
    else
    {
        DrawCube((Vector3){cx-1.65f, -0.006f, cz},
                 0.20f, 0.006f, len+w-1.5f, tireTrack);
        DrawCube((Vector3){cx+1.65f, -0.006f, cz},
                 0.20f, 0.006f, len+w-1.5f, tireTrack);
    }

    // Обочины из более тёмного грунта.
    if (horizontal)
    {
        DrawCube((Vector3){cx,-0.012f,cz-w*0.53f},len+w,0.035f,0.42f,C(92,88,72,255));
        DrawCube((Vector3){cx,-0.012f,cz+w*0.53f},len+w,0.035f,0.42f,C(92,88,72,255));
    }
    else
    {
        DrawCube((Vector3){cx-w*0.53f,-0.012f,cz},0.42f,0.035f,len+w,C(92,88,72,255));
        DrawCube((Vector3){cx+w*0.53f,-0.012f,cz},0.42f,0.035f,len+w,C(92,88,72,255));
    }
}

enum { ANIMAL_WANDER=0, ANIMAL_LOOK=1, ANIMAL_INSPECT=2, ANIMAL_CHASE=3, ANIMAL_GRAZE=4, ANIMAL_FLEE=5, ANIMAL_STARTLE=6, ANIMAL_SNORT=7, ANIMAL_COOLDOWN=8 };

static Sound gBoarAlertSound = {0};
static bool gAnimalAudioReady = false;

static void InitAnimalAudio(void)
{
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;

    // Короткий синтезированный низкий хриплый хрюк, без внешнего аудиофайла.
    const unsigned int sampleRate = 22050;
    const unsigned int frameCount = sampleRate*7/10;
    short *samples = (short *)malloc(sizeof(short)*frameCount);
    if (samples == NULL) return;

    unsigned int noise = 0x51A7u;
    for (unsigned int i=0; i<frameCount; i++)
    {
        float t=(float)i/(float)sampleRate;
        float pulse1=fmaxf(0.0f,1.0f-fabsf(t-0.16f)/0.16f);
        float pulse2=fmaxf(0.0f,1.0f-fabsf(t-0.48f)/0.18f);
        float envelope=fmaxf(pulse1,pulse2);
        float phase=6.2831853f*(82.0f*t-22.0f*t*t);
        noise=noise*1664525u+1013904223u;
        float grit=((float)((noise>>16)&0xFFFFu)/32767.5f)-1.0f;
        float growl=sinf(phase)+0.38f*sinf(phase*2.07f)+0.24f*grit;
        float value=growl*envelope*0.52f;
        if(value>1.0f)value=1.0f;
        if(value< -1.0f)value=-1.0f;
        samples[i]=(short)(value*32767.0f);
    }

    Wave wave={0};
    wave.frameCount=frameCount;
    wave.sampleRate=sampleRate;
    wave.sampleSize=16;
    wave.channels=1;
    wave.data=samples;
    gBoarAlertSound=LoadSoundFromWave(wave);
    free(samples);
    gAnimalAudioReady=(gBoarAlertSound.frameCount>0);
}

static bool IsAnimalBlockedAt(float x, float z, float playerY,
                              Vector3 animals[MAX_WILD_ANIMALS],
                              int animalTypes[MAX_WILD_ANIMALS],
                              int animalHealth[MAX_WILD_ANIMALS], int ignoreAnimal)
{
    if(playerY>1.8f)return false;
    for(int i=0;i<MAX_WILD_ANIMALS;i++)
    {
        if(i==ignoreAnimal || animalHealth[i]<=0)continue;
        float dx=x-animals[i].x, dz=z-animals[i].z;
        float radius=0.42f+(animalTypes[i]==0?0.52f:0.66f);
        if(dx*dx+dz*dz<radius*radius)return true;
    }
    return false;
}

static void DrawWildAnimal(Vector3 p, int type, float phase, float heading,
                           int state, float headYaw)
{
    bool deer=(type==0);
    bool moving=(state==ANIMAL_WANDER || state==ANIMAL_CHASE || state==ANIMAL_FLEE);
    float gait= moving ? sinf(phase)*((state==ANIMAL_CHASE || state==ANIMAL_FLEE)?0.34f:0.15f) : 0.0f;
    float bob=(deer && state==ANIMAL_STARTLE)?0.16f:(moving?fabsf(sinf(phase*2.0f))*0.045f:sinf(phase*0.45f)*0.012f);
    float headDown=(state==ANIMAL_GRAZE)?0.30f:0.0f;
    float headShift=sinf(headYaw)*0.20f;
    Color fur=deer?C(145,101,59,255):C(82,65,50,255);
    Color dark=deer?C(76,51,34,255):C(43,36,30,255);
    Color light=deer?C(195,165,125,255):C(125,93,69,255);

    rlPushMatrix();
    rlTranslatef(p.x,0.0f,p.z);
    rlScalef(0.78f,0.78f,0.78f);
    rlRotatef(-heading*57.2957795f,0.0f,1.0f,0.0f);

    DrawCylinder((Vector3){0,0.025f,0},0.72f,0.72f,0.035f,12,(Color){20,25,20,65});
    if(deer){
        DrawCubeV((Vector3){0,0.91f+bob,0.10f},(Vector3){0.78f,0.86f,1.44f},fur);
        DrawCubeV((Vector3){0,0.66f+bob,0.10f},(Vector3){0.56f,0.32f,0.98f},light);
        DrawCylinderEx((Vector3){headShift*0.35f,0.95f+bob, -0.38f},
                       (Vector3){headShift,1.43f+bob-headDown,-0.65f},0.19f,0.12f,9,fur);
        DrawSphere((Vector3){headShift,1.49f+bob-headDown,-0.69f},0.22f,fur);
        DrawSphere((Vector3){headShift,1.39f+bob-headDown,-0.89f},0.15f,dark);
        DrawCylinderEx((Vector3){headShift-0.16f,1.57f+bob-headDown,-0.84f},
                       (Vector3){headShift-0.16f,1.57f+bob-headDown,-0.52f},0.055f,0.035f,6,fur);
        DrawCylinderEx((Vector3){headShift+0.16f,1.57f+bob-headDown,-0.84f},
                       (Vector3){headShift+0.16f,1.57f+bob-headDown,-0.52f},0.055f,0.035f,6,fur);
        DrawSphere((Vector3){headShift,1.38f+bob-headDown,-1.00f},0.035f,dark);
        for(int side=-1;side<=1;side+=2){
            Vector3 base={headShift+side*0.11f,1.62f+bob-headDown,-0.69f};
            Vector3 fork={headShift+side*0.25f,1.82f+bob-headDown,-0.70f};
            DrawCylinderEx(base,fork,0.045f,0.030f,6,C(198,177,140,255));
            DrawCylinderEx(fork,(Vector3){headShift+side*0.43f,1.91f+bob-headDown,-0.70f},0.030f,0.016f,5,C(198,177,140,255));
            DrawCylinderEx((Vector3){headShift+side*0.21f,1.77f+bob-headDown,-0.70f},
                           (Vector3){headShift+side*0.19f,2.02f+bob-headDown,-0.70f},0.026f,0.012f,5,C(198,177,140,255));
        }
    }else{
        DrawCubeV((Vector3){0,0.73f+bob,0.12f},(Vector3){1.12f,0.90f,1.56f},fur);
        DrawCubeV((Vector3){0,0.53f+bob,0.10f},(Vector3){0.86f,0.32f,1.20f},light);
        DrawSphere((Vector3){headShift*0.5f,0.81f+bob-headDown,-0.48f},0.42f,fur);
        DrawSphere((Vector3){headShift,0.69f+bob-headDown,-0.77f},0.28f,fur);
        DrawSphere((Vector3){headShift,0.64f+bob-headDown,-0.99f},0.19f,C(120,82,68,255));
        for(int side=-1;side<=1;side+=2){
            DrawCylinderEx((Vector3){headShift+side*0.23f,0.94f+bob-headDown,-0.57f},
                           (Vector3){headShift+side*0.36f,1.17f+bob-headDown,-0.61f},0.13f,0.025f,6,dark);
            DrawSphere((Vector3){headShift+side*0.20f,0.72f+bob-headDown,-0.85f},0.035f,C(22,19,17,255));
            DrawCylinderEx((Vector3){side*0.18f,0.53f+bob, -0.88f},
                           (Vector3){side*0.23f,0.70f+bob,-0.94f},0.060f,0.014f,6,C(230,215,180,255));
        }
        DrawCylinderEx((Vector3){0,0.72f+bob,0.78f},
                       (Vector3){0,0.86f+bob,1.02f},0.055f,0.025f,6,dark);
    }

    for(int side=-1;side<=1;side+=2) for(int leg=0;leg<2;leg++){
        float lx=side*(deer?0.24f:0.36f);
        float swing=moving?gait*(leg==0?1.0f:-1.0f)*(side==1?1.0f:-1.0f):0.0f;
        float lz=(leg==0?-0.39f:0.43f)+swing;
        float top=deer?0.79f:0.60f, knee=deer?0.40f:0.34f;
        if(deer && state==ANIMAL_STARTLE && leg==0)
        {
            // Передние ноги подняты — олень привстаёт перед рывком.
            DrawCylinderEx((Vector3){lx,top+bob,lz},
                           (Vector3){lx+side*0.12f,0.98f+bob,lz-0.10f},
                           0.065f,0.050f,7,fur);
            DrawCylinderEx((Vector3){lx+side*0.12f,0.98f+bob,lz-0.10f},
                           (Vector3){lx+side*0.18f,0.86f+bob,lz-0.28f},
                           0.050f,0.035f,7,dark);
            DrawCube((Vector3){lx+side*0.18f,0.84f+bob,lz-0.29f},0.12f,0.08f,0.14f,C(35,30,26,255));
        }
        else
        {
            DrawCylinderEx((Vector3){lx,top+bob, lz},
                           (Vector3){lx+side*0.015f,knee+bob,lz+0.02f},
                           deer?0.065f:0.105f,deer?0.048f:0.075f,7,fur);
            DrawCylinderEx((Vector3){lx+side*0.015f,knee+bob,lz+0.02f},
                           (Vector3){lx,0.12f,lz+0.03f+swing},
                           deer?0.045f:0.075f,deer?0.035f:0.055f,7,dark);
            DrawCube((Vector3){lx,0.08f,lz+0.03f+swing},
                     deer?0.12f:0.17f,0.12f,0.16f,C(35,30,26,255));
        }
    }
    rlPopMatrix();
}

static void DrawBiomeAtmosphere(Vector3 player, int biome, float time, int quality)
{
    if(quality<1 || (biome!=1 && biome!=2)) return;
    // Лёгкие локальные частицы: снег в зимней зоне, песчаная пыль в пустыне.
    for(int i=0;i<14;i++)
    {
        float seed=(float)i*1.731f;
        float x=player.x+sinf(time*0.22f+seed)*7.0f+cosf(seed*2.0f)*3.0f;
        float z=player.z+cosf(time*0.18f+seed*1.3f)*7.0f+sinf(seed)*3.0f;
        float y=0.65f+fmodf((float)i*0.71f+time*(biome==1?0.75f:0.28f),4.5f);
        if(biome==1)
            DrawSphere((Vector3){x,y,z},0.035f,C(238,247,255,190));
        else
            DrawSphere((Vector3){x,y,z},0.045f,C(207,174,116,125));
    }
}

static void DrawCloud(float x, float y, float z, float s)
{
    Color cloud = C(245, 250, 255, 255);

    DrawSphere((Vector3){x, y, z}, s, cloud);
    DrawSphere((Vector3){x+s*0.85f, y+0.12f*s, z}, s*0.72f, cloud);
    DrawSphere((Vector3){x-s*0.8f, y+0.05f*s, z}, s*0.65f, cloud);
}

/* Получаем символы Android-клавиатуры как UTF-8.
   Стандартный GetLastSoftKeyChar() заменяет кириллицу на '?'. */
static void ReadSoftKeyboardUtf8(char *text, unsigned int size)
{
    int keyCode = GetLastSoftKeyCode();
    if (keyCode == 0 || size < 2) return;
    unsigned int len = (unsigned int)strlen(text);
    if (keyCode == 67) {
        if (len > 0) {
            len--;
            while (len > 0 && (((unsigned char)text[len] & 0xC0) == 0x80)) len--;
            text[len] = '\0';
        }
        ClearLastSoftKey();
        return;
    }
    if (keyCode == 66) { ClearLastSoftKey(); return; }
    int cp = GetLastSoftKeyUnicode();
    if (cp <= 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        ClearLastSoftKey();
        return;
    }
    unsigned char bytes[4];
    unsigned int count = 0;
    if (cp <= 0x7F) bytes[count++] = (unsigned char)cp;
    else if (cp <= 0x7FF) {
        bytes[count++] = (unsigned char)(0xC0 | (cp >> 6));
        bytes[count++] = (unsigned char)(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        bytes[count++] = (unsigned char)(0xE0 | (cp >> 12));
        bytes[count++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        bytes[count++] = (unsigned char)(0x80 | (cp & 0x3F));
    } else {
        bytes[count++] = (unsigned char)(0xF0 | (cp >> 18));
        bytes[count++] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
        bytes[count++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        bytes[count++] = (unsigned char)(0x80 | (cp & 0x3F));
    }
    if (len + count < size) {
        memcpy(text + len, bytes, count);
        text[len + count] = '\0';
    }
    ClearLastSoftKey();
}

static int GetInventorySlotAt(Vector2 pos, int screenWidth, int screenHeight)
{
    Rectangle panel={screenWidth*0.10f,screenHeight*0.12f,screenWidth*0.80f,screenHeight*0.76f};
    float gap=8.0f;
    float cell=fminf(68.0f,(panel.width-32.0f-gap*3.0f)/4.0f);
    float gridWidth=cell*4.0f+gap*3.0f;
    float gridX=panel.x+(panel.width-gridWidth)*0.5f;
    for(int slot=0;slot<12;slot++)
    {
        Rectangle r={gridX+(slot%4)*(cell+gap),panel.y+72+(slot/4)*74,cell,cell};
        if(CheckCollisionPointRec(pos,r)) return slot;
    }
    return -1;
}

static int GetHotbarSlotAt(Vector2 pos, Rectangle hotbarSlots[4])
{
    for(int slot=0;slot<4;slot++)
        if(CheckCollisionPointRec(pos,hotbarSlots[slot])) return slot;
    return -1;
}

int main(void)
{
    InitWindow(0, 0, "boltSurvival");

    // Стандартный шрифт Raylib не содержит кириллицу. Загружаем Roboto
    // Android с явным набором русских символов для сообщений интерфейса.
    int cyrillicCodepoints[191];
    int cpIndex = 0;
    for (int cp = 32; cp <= 126; cp++) cyrillicCodepoints[cpIndex++] = cp;
    for (int cp = 0x0400; cp <= 0x045F; cp++) cyrillicCodepoints[cpIndex++] = cp;
    cyrillicFont = LoadAndroidCyrillicFont(64, cyrillicCodepoints, cpIndex);
    if (cyrillicFont.texture.id != 0) SetTextureFilter(cyrillicFont.texture, TEXTURE_FILTER_BILINEAR);
    SetTargetFPS(60);
    InitAnimalAudio();

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
    Vector2 mapPanOffset = {0.0f, 0.0f};
    float mapZoom = 1.0f;
    int mapPanTouchId = -1;
    Vector2 mapPanPrevious = {0.0f, 0.0f};
    float mapPinchPreviousDistance = 0.0f;
    Vector2 mapPinchPreviousCenter = {0.0f, 0.0f};
    bool commandChatOpen = false, commandConsoleOpen = false, resourcePanelOpen = false;
    char commandInput[80] = {0};
    char commandOutput[180] = "Введите /console";
    bool buildMode = false;
    bool buildPlanHeld = false;
    // Own edge detection for Bluetooth mouse: Android Raylib can leave the
    // previous mouse-button state unchanged, which repeats held clicks.
    bool mouseLeftWasDown = false;
    bool mouseRightWasDown = false;
    float buildMessageTimer = 0.0f;
    int buildMessageType = 0; // 1 — запрет зоны, 2 — мешают объекты
    int selectedBuildType = 0; // 0 FOUNDATION, 1 WALL, 2 DOOR, 3 ROOF.
    int selectedHotbarSlot = 0;
    int inventoryOrder[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
    int inventoryDragItem = -1;
    int inventoryDragSourceSlot = -1;
    Vector2 inventoryDragPosition = {0,0};
    // 0 AXE, 1 MED, 2 FOOD, 3 WATER, 4 WOOD, 5 STONE, 6 DOOR, 7 ORF PIXCADE,
    // 8 IRON, 9 LAPIS, 10 SULFUR, 11 SILICON.
    const char *itemNames[12] = {"ТОПОР", "АПТЕЧКА", "ЕДА", "ВОДА", "ДЕРЕВО", "КАМЕНЬ", "ДВЕРЬ", "ОРУЖИЕ",
                                 "ЖЕЛЕЗО", "ЛАЗУРИТ", "СЕРА", "КРЕМНИЙ"};
    int itemCounts[12] = {1, 1, 3, 3, 0, 0, 0, 1, 0, 0, 0, 0};
    int giveSelectedItem = 0;
    int giveSelectedAmount = 1;
    int hotbarItemIds[4] = {0, 7, 2, 3}; // Предметы быстрого доступа можно менять перетаскиванием.

    // Главное меню.
    bool mainMenuOpen = true;
    bool loadingScreen = false;
    float loadingProgress = 0.0f;
    float menuIntroProgress = 0.0f;
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
    Rectangle chatButton = {screenWidth - 190, 240, 170, 64};

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
    float crateTakeCooldown = 0.0f;
    int crateGlowSlot = -1;
    int gasCrateItems[4][8] = {{2,3,4,5,8,9,10,11},{2,3,4,5,8,9,10,11},{2,3,4,5,8,9,10,11},{0,1,7,8,9,10,11,4}};
    int gasCrateAmounts[4][8] = {{3,2,18,12,4,2,3,4},{2,3,14,10,3,4,2,3},{3,2,20,8,5,3,2,4},{1,2,1,6,4,5,5,25}};
    Rectangle interactButton = {0};
    Rectangle sprintButton = {0};
    Rectangle inventoryButton = {0};
    Rectangle buildButton = {0};
    Rectangle buildFloorButton = {0}, buildWallButton = {0}, buildDoorButton = {0}, buildRoofButton = {0}, buildPlaceButton = {0};
    Rectangle takeButton = {0};
    Vector3 treePositions[MAX_TREES] = {{0}}, woodDrops[MAX_WOOD_DROPS] = {{0}};
    Vector3 wildAnimals[MAX_WILD_ANIMALS] = {{0}};
    int wildAnimalTypes[MAX_WILD_ANIMALS] = {0};
    int wildAnimalHealth[MAX_WILD_ANIMALS] = {0};
    int deerApproachCount[MAX_WILD_ANIMALS] = {0};
    bool deerWasNearPlayer[MAX_WILD_ANIMALS] = {false};
    bool deerProvoked[MAX_WILD_ANIMALS] = {false};
    int wildAnimalState[MAX_WILD_ANIMALS] = {0};
    float wildAnimalHeading[MAX_WILD_ANIMALS] = {0};
    float wildAnimalTargetHeading[MAX_WILD_ANIMALS] = {0};
    float wildAnimalTimer[MAX_WILD_ANIMALS] = {0};
    float wildAnimalLookTimer[MAX_WILD_ANIMALS] = {0};
    float wildAnimalGrazeTimer[MAX_WILD_ANIMALS] = {0};
    float wildAnimalHeadYaw[MAX_WILD_ANIMALS] = {0};
    double wildAnimalLastChase[MAX_WILD_ANIMALS];
    double wildAnimalChaseStarted[MAX_WILD_ANIMALS];
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
    Rectangle editSizeSlider, editAlphaSlider;
    bool attackRequested = false;
    float attackCooldown = 0.0f;
    const float attackCooldownDuration = 1.0f;
    float attackAnimation = 0.0f;
    float eatCooldown = 0.0f;
    bool eatRequested = false;
    Rectangle eatButton = {0};
    float consumeAnimationTime = 0.0f;
    int consumeAnimationType = -1;
    const float consumeAnimationDuration = 0.72f;

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
    // Три отдельных биома размещаются в случайных секторах карты один раз при старте мира.
    for(int bi=0;bi<3;bi++)
    {
        bool separated=false;
        for(int attempt=0;attempt<80 && !separated;attempt++)
        {
            float a=((float)rand()/(float)RAND_MAX)*6.2831853f;
            float radius=230.0f+((float)rand()/(float)RAND_MAX)*125.0f;
            Vector2 candidate={cosf(a)*radius,sinf(a)*radius};
            separated=true;
            for(int prev=0;prev<bi;prev++)
            {
                float dx=candidate.x-gBiomeCenters[prev].x;
                float dz=candidate.y-gBiomeCenters[prev].y;
                if(dx*dx+dz*dz<250.0f*250.0f) separated=false;
            }
            if(separated) gBiomeCenters[bi]=candidate;
        }
    }
    // Равномерно распределяем лес по большой карте с расстоянием между деревьями.
    for (int attempt=0; attempt<18000 && treeCount<MAX_TREES; attempt++)
    {
        float angle=((float)rand()/(float)RAND_MAX)*6.2831853f;
        float radius=28.0f+((float)rand()/(float)RAND_MAX)*500.0f;
        float tx=cosf(angle)*radius, tz=sinf(angle)*radius;
        if (!IsVillageClear(tx,tz)) continue;
        bool clear=true;
        for (int t=0;t<treeCount;t++)
        {
            float dx=tx-treePositions[t].x, dz=tz-treePositions[t].z;
            if (dx*dx+dz*dz<100.0f) { clear=false; break; }
        }
        if (!clear) continue;
        treePositions[treeCount]=(Vector3){tx,0,tz};
        treeBiome[treeCount]=GetWorldBiome(tx,tz);
        treeHealth[treeCount]=GetTreeHits(treeBiome[treeCount]);\n        treeCount++;
    }

    // Небольшие группы диких животных распределяются по свободным участкам карты.
    for(int animal=0;animal<MAX_WILD_ANIMALS;animal++)
    {
        bool placed=false;
        for(int attempt=0;attempt<120 && !placed;attempt++)
        {
            float a=((float)rand()/(float)RAND_MAX)*6.2831853f;
            float radius=animal<4?32.0f+((float)rand()/(float)RAND_MAX)*18.0f:65.0f+((float)rand()/(float)RAND_MAX)*145.0f;
            float ax=cosf(a)*radius, az=sinf(a)*radius;
            if(!IsVillageClear(ax,az)) continue;
            float sx=ax, sz=az+22.0f;
            if(sx*sx+sz*sz<42.0f*42.0f) continue;
            wildAnimals[animal]=(Vector3){ax,0,az};
            wildAnimalTypes[animal]=animal%2;
            placed=true;
        }
    }

    for(int animal=0;animal<MAX_WILD_ANIMALS;animal++)
    {
        wildAnimalState[animal]=ANIMAL_WANDER;
        wildAnimalHeading[animal]=((float)rand()/(float)RAND_MAX)*6.2831853f;
        wildAnimalTargetHeading[animal]=wildAnimalHeading[animal];
        wildAnimalTimer[animal]=2.0f+((float)rand()/(float)RAND_MAX)*4.0f;
        wildAnimalLookTimer[animal]=5.0f+((float)rand()/(float)RAND_MAX)*9.0f;
        wildAnimalGrazeTimer[animal]=20.0f+((float)rand()/(float)RAND_MAX)*10.0f;
        wildAnimalHeadYaw[animal]=0.0f;
        wildAnimalLastChase[animal]=-1000.0;
        wildAnimalHealth[animal]=(wildAnimalTypes[animal]==0)?50:100;
        wildAnimalChaseStarted[animal]=0.0;
    }

    // По четыре залежи каждого вида. Руды размещаются вне деревни и дорог.
    for (int ore = 0; ore < MAX_ORE_NODES; ore++)
    {
        int type = ore/8;
        bool placed = false;
        for (int attempt = 0; attempt < 250 && !placed; attempt++)
        {
            float angle = ((float)rand()/(float)RAND_MAX)*6.2831853f;
            float radius = 35.0f + ((float)rand()/(float)RAND_MAX)*470.0f;
            float x = cosf(angle)*radius, z = sinf(angle)*radius;
            if (!IsVillageClear(x,z)) continue;
            bool clear = true;
            for (int t=0; t<treeCount; t++)
            {
                float dx=x-treePositions[t].x, dz=z-treePositions[t].z;
                if (dx*dx+dz*dz < 25.0f) { clear=false; break; }
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
                if (dx*dx+dz*dz < 100.0f) clear=false;
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
        attackRequested = false;
        eatRequested = false;
        if (loadingScreen)
        {
            loadingProgress += frameTime/2.4f;
            if (loadingProgress >= 1.0f) { loadingProgress = 1.0f; loadingScreen = false; }
        }
        if(commandChatOpen || commandConsoleOpen)
        {
            // Android передаёт одно событие после отпускания клавиши;
            // ReadSoftKeyboardUtf8 сразу очищает его, поэтому символ не дублируется.
            ReadSoftKeyboardUtf8(commandInput,sizeof(commandInput));
            if (IsKeyPressed(KEY_ENTER))
            {
                HideSoftKeyboard();
                if (commandChatOpen)
                {
                    if(strcmp(commandInput,"/console")==0)
                    {
                        commandChatOpen=false;
                        commandConsoleOpen=true;
                        strcpy(commandOutput,"Консоль открыта: /give или /update");
                    }
                    else strcpy(commandOutput,"Введите /console");
                }
                else if (commandConsoleOpen)
                {
                    if(strcmp(commandInput,"/give")==0)
                    {
                        commandConsoleOpen=false;
                        resourcePanelOpen=true;
                        strcpy(commandOutput,"Окно выдачи предметов открыто");
                    }
                    else if(strcmp(commandInput,"/update")==0)
                        strcpy(commandOutput,"git pull --ff-only origin master &&\n./gradlew assembleDebug");
                    else strcpy(commandOutput,"Команды: /give или /update");
                }
                commandInput[0]='\0';
            }
        }
        else if (!mainMenuOpen && !loadingScreen && !settingsOpen && !editControls && !inventoryOpen &&
                 !mapOpen && !gasCrateOpen && !resourcePanelOpen)
        {
            if (IsKeyPressed(KEY_ONE)) selectedHotbarSlot = 0;
            if (IsKeyPressed(KEY_TWO)) selectedHotbarSlot = 1;
            if (IsKeyPressed(KEY_THREE)) selectedHotbarSlot = 2;
            if (IsKeyPressed(KEY_FOUR)) selectedHotbarSlot = 3;
            if (IsKeyPressed(KEY_TAB)) { inventoryOpen = !inventoryOpen; joystickActive = false; lookTouchId = -1; }
            if (IsKeyPressed(KEY_V)) firstPerson = !firstPerson;
            // X is a hold-to-open Building Plan key only; it must not
            // permanently switch the build mode on.
            buildPlanHeld = IsKeyDown(KEY_X);
            if (IsKeyPressed(KEY_E)) attackRequested = true;
        }
        if (attackCooldown > 0.0f) attackCooldown -= frameTime;
        if (eatCooldown > 0.0f) eatCooldown -= frameTime;
        if (eatCooldown < 0.0f) eatCooldown = 0.0f;
        if (crateTakeCooldown > 0.0f) { crateTakeCooldown -= frameTime; if (crateTakeCooldown <= 0.0f) { crateTakeCooldown = 0.0f; crateGlowSlot = -1; } }
        if (attackCooldown < 0.0f) attackCooldown = 0.0f;
        if (attackAnimation > 0.0f) attackAnimation -= frameTime;
        if (attackAnimation < 0.0f) attackAnimation = 0.0f;
        if (consumeAnimationTime > 0.0f) {
            consumeAnimationTime -= frameTime;
            if (consumeAnimationTime <= 0.0f) {
                consumeAnimationTime = 0.0f;
                consumeAnimationType = -1;
            }
        }

        if (!mainMenuOpen && !settingsOpen && !editControls)
        {
            foodLevel -= foodDrainRate*frameTime;
            waterLevel -= waterDrainRate*frameTime;
            if (foodLevel < 0.0f) foodLevel = 0.0f;
            if (waterLevel < 0.0f) waterLevel = 0.0f;
        }
        // Возрождение каждой залежи через 60 секунд в новой случайной точке.
        for (int ore=0; ore<MAX_ORE_NODES; ore++)
        {
            if (gOreActive[ore] || GetTime() < oreRespawnAt[ore]) continue;
            bool placed = false;
            int type = ore/8;
            for (int attempt=0; attempt<250 && !placed; attempt++)
            {
                float angle=((float)rand()/(float)RAND_MAX)*6.2831853f;
                float radius=35.0f+((float)rand()/(float)RAND_MAX)*470.0f;
                float x=cosf(angle)*radius, z=sinf(angle)*radius;
                if (!IsVillageClear(x,z)) continue;
                bool clear=true;
                for(int t=0;t<treeCount;t++){float dx=x-treePositions[t].x,dz=z-treePositions[t].z;if(dx*dx+dz*dz<25.0f){clear=false;break;}}
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
            14.0f, 14.0f, 135.0f, 48.0f
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
        eatButton = interactButton;
        sprintButton = (Rectangle){actionButtonCenters[1].x-runW/2, actionButtonCenters[1].y-runH/2, runW, runH};
        inventoryButton = (Rectangle){actionButtonCenters[2].x-bagW/2, actionButtonCenters[2].y-bagH/2, bagW, bagH};

        editSizeSlider = (Rectangle){screenWidth*0.18f, 68.0f, screenWidth*0.64f, 16.0f};
        editAlphaSlider = (Rectangle){screenWidth*0.18f, 126.0f, screenWidth*0.64f, 16.0f};
        float slotSize = fminf(66.0f, (screenWidth-125.0f)/4.0f);
        float slotGap = 6.0f;
        float hotbarWidth = slotSize*4.0f + slotGap*3.0f;
        float hotbarX = (screenWidth-hotbarWidth)/2.0f;
        for (int slot = 0; slot < 4; slot++)
            hotbarSlots[slot] = (Rectangle){hotbarX+slot*(slotSize+slotGap),
                                           screenHeight-88.0f, slotSize, 66.0f};
        buildButton = (Rectangle){14, 170, 142, 42};
        float buildChoiceW = fminf(82.0f, (screenWidth-50.0f)/4.0f);
        buildFloorButton = (Rectangle){10, 220, buildChoiceW, 38};
        buildWallButton = (Rectangle){16+buildChoiceW, 220, buildChoiceW, 38};
        buildRoofButton = (Rectangle){22+buildChoiceW*2, 220, buildChoiceW, 38};
        buildDoorButton = (Rectangle){28+buildChoiceW*3, 220, buildChoiceW, 38};
        buildPlaceButton = (Rectangle){14, 266, 142, 42};
        takeButton = (Rectangle){screenWidth/2.0f-76.0f, screenHeight-150.0f, 152.0f, 46.0f};
        chatButton = (Rectangle){screenWidth-190.0f, 240.0f, 170.0f, 64.0f};

        bool mouseLeftDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        bool mouseRightDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        bool mouseLeftPressed = mouseLeftDown && !mouseLeftWasDown;
        bool mouseRightPressed = mouseRightDown && !mouseRightWasDown;
        mouseLeftWasDown = mouseLeftDown;
        mouseRightWasDown = mouseRightDown;

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

            if (resourcePanelOpen)
            {
                Rectangle rp={screenWidth*0.08f,screenHeight*0.10f,screenWidth*0.84f,screenHeight*0.78f};
                Rectangle closeRp={rp.x+rp.width-82,rp.y+10,68,34};
                float rowH=(rp.height-158.0f)/12.0f;
                float controlsY=rp.y+rp.height-94.0f;
                Rectangle minusTen={rp.x+10,controlsY,42,32};
                Rectangle minusOne={rp.x+56,controlsY,36,32};
                Rectangle plusOne={rp.x+rp.width-132,controlsY,36,32};
                Rectangle plusTen={rp.x+rp.width-90,controlsY,42,32};
                Rectangle giveButton={rp.x+14,controlsY+38,rp.width-28,38};
                if(CheckCollisionPointRec(pos,closeRp)) resourcePanelOpen=false;
                else if(CheckCollisionPointRec(pos,minusTen)) giveSelectedAmount=fmaxf(1,giveSelectedAmount-10);
                else if(CheckCollisionPointRec(pos,minusOne)) giveSelectedAmount=fmaxf(1,giveSelectedAmount-1);
                else if(CheckCollisionPointRec(pos,plusOne)) giveSelectedAmount=fminf(9999,giveSelectedAmount+1);
                else if(CheckCollisionPointRec(pos,plusTen)) giveSelectedAmount=fminf(9999,giveSelectedAmount+10);
                else if(CheckCollisionPointRec(pos,giveButton))
                {
                    itemCounts[giveSelectedItem]+=giveSelectedAmount;
                    if(itemCounts[giveSelectedItem]>9999)itemCounts[giveSelectedItem]=9999;
                    SaveBuildData(buildPieces,buildPieceTypes,buildDoorOpen,buildPieceCount,itemCounts[4]);
                }
                else for(int item=0;item<12;item++)
                {
                    float ry=rp.y+54+item*rowH;
                    Rectangle row={rp.x+10,ry,rp.width-20,rowH-2};
                    if(CheckCollisionPointRec(pos,row)){giveSelectedItem=item;break;}
                }
            }
            else if(commandChatOpen || commandConsoleOpen)
            {
                Rectangle cp={screenWidth*0.08f,screenHeight*0.10f,screenWidth*0.84f,190};
                Rectangle inputBox={cp.x+14,cp.y+58,cp.width-28,42};
                Rectangle sendButton={cp.x+cp.width-128,cp.y+116,114,42};
                Rectangle closeCmd={cp.x+14,cp.y+116,100,42};
                if(CheckCollisionPointRec(pos,closeCmd)){commandChatOpen=false;commandConsoleOpen=false;HideSoftKeyboard();}
                else if(CheckCollisionPointRec(pos,inputBox)) ShowSoftKeyboard();
                else if(CheckCollisionPointRec(pos,sendButton))
                {
                    HideSoftKeyboard();
                    if(commandChatOpen)
                    {
                        if(strcmp(commandInput,"/console")==0){commandChatOpen=false;commandConsoleOpen=true;strcpy(commandOutput,"Консоль открыта: /give или /update");}
                        else strcpy(commandOutput,"Введите /console");
                    }
                    else if(commandConsoleOpen)
                    {
                        if(strcmp(commandInput,"/give")==0){commandConsoleOpen=false;resourcePanelOpen=true;strcpy(commandOutput,"Панель ресурсов открыта");}
                        else if(strcmp(commandInput,"/update")==0)strcpy(commandOutput,"git pull --ff-only origin master &&\n./gradlew assembleDebug");
                        else strcpy(commandOutput,"Команды: /give или /update");
                    }
                    commandInput[0]='\0';
                }
            }
            else if (editControls)
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
                else if (CheckCollisionPointRec(pos, editSizeSlider))
                {
                    dragTarget = 8; dragTouchId = id;
                    float t = (pos.x-editSizeSlider.x)/editSizeSlider.width;
                    actionButtonScale[selectedActionControl] = 0.10f+fmaxf(0.0f,fminf(1.0f,t))*2.90f;
                }
                else if (CheckCollisionPointRec(pos, editAlphaSlider))
                {
                    dragTarget = 9; dragTouchId = id;
                    float t = (pos.x-editAlphaSlider.x)/editAlphaSlider.width;
                    actionButtonAlpha[selectedActionControl] = (int)(fmaxf(0.0f,fminf(1.0f,t))*255.0f);
                }
                else if (CheckCollisionPointRec(pos, interactButton))
                {
                    selectedActionControl = 0;
                    dragTarget = 4;
                    dragTouchId = id;
                    dragOffset = (Vector2){actionButtonCenters[0].x-pos.x, actionButtonCenters[0].y-pos.y};
                }
                else if (CheckCollisionPointRec(pos, sprintButton))
                {
                    selectedActionControl = 1;
                    dragTarget = 5;
                    dragTouchId = id;
                    dragOffset = (Vector2){actionButtonCenters[1].x-pos.x, actionButtonCenters[1].y-pos.y};
                }
                else if (CheckCollisionPointRec(pos, inventoryButton))
                {
                    selectedActionControl = 2;
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
                        loadingScreen = true;
                        loadingProgress = 0.0f;
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
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos,chatButton))
            {
                commandChatOpen=true;commandConsoleOpen=false;commandInput[0]='\0';
                strcpy(commandOutput,"Введите /console");ShowSoftKeyboard();
            }
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
                    if (!placed)
                    {
                        float bx,bz; int bt;
                        GetBuildPlacement(player,cameraYaw,selectedBuildType,
                            buildPieces,buildPieceTypes,buildPieceCount,&bx,&bz,&bt);
                        float bhx=(selectedBuildType==1 && bt==4)?0.12f:1.25f;
                        float bhz=(selectedBuildType==1 && bt!=4)?0.12f:1.25f;
                        bool zone=IsBuildZoneForbidden(bx,bz,bhx,bhz);
                        buildMessageType=zone?1:((selectedBuildType==0)?2:0);
                        buildMessageTimer=buildMessageType?2.8f:0.0f;
                    }
                    else { buildMessageType=0; buildMessageTimer=0.0f; }
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
                float crateSize=fminf(screenHeight*0.62f,screenWidth*0.30f);
                float invSize=fminf(screenHeight*0.48f,screenWidth*0.25f);
                Rectangle cratePanel={screenWidth- crateSize-18.0f,screenHeight-crateSize-62.0f,crateSize,crateSize};
                Rectangle invPanel={(screenWidth-invSize)*0.5f,screenHeight-invSize-50.0f,invSize,invSize};
                Rectangle close={cratePanel.x+cratePanel.width-48.0f,cratePanel.y+4.0f,42.0f,40.0f};
                Rectangle closeHit={close.x-10.0f,close.y-8.0f,close.width+20.0f,close.height+16.0f};
                float crateContentY=cratePanel.y+50.0f;
                float crateContentH=cratePanel.height-58.0f;
                if(CheckCollisionPointRec(pos,closeHit)){gasCrateOpen=false;openedGasCrate=-1;crateGlowSlot=-1;crateTakeCooldown=0.0f;}
                else if(openedGasCrate>=0 && crateTakeCooldown<=0.0f){
                    int cols=4, rows=2;
                    float gap=4.0f;
                    float cellW=(cratePanel.width-16.0f-gap*(cols-1))/(float)cols;
                    float cellH=(crateContentH-gap*(rows-1))/(float)rows;
                    for(int slot=0;slot<8;slot++){
                        int col=slot%cols, row=slot/cols;
                        Rectangle cell={cratePanel.x+8.0f+col*(cellW+gap),crateContentY+row*(cellH+gap),cellW,cellH};
                        if(CheckCollisionPointRec(pos,cell)&&gasCrateAmounts[openedGasCrate][slot]>0){
                            int item=gasCrateItems[openedGasCrate][slot],room=99-itemCounts[item];
                            int take=gasCrateAmounts[openedGasCrate][slot]<room?gasCrateAmounts[openedGasCrate][slot]:room;
                            if(take>0){
                                itemCounts[item]+=take;
                                gasCrateAmounts[openedGasCrate][slot]-=take;
                                crateGlowSlot=slot;
                                crateTakeCooldown=0.75f;
                            }
                            break;
                        }
                    }
                }
            }
            else if (inventoryOpen)
            {
                // Еду и воду можно использовать касанием карточки в рюкзаке.
                Rectangle invPanel = {screenWidth*0.10f, screenHeight*0.12f,
                                      screenWidth*0.80f, screenHeight*0.76f};
                Rectangle craftAxeButton = {invPanel.x+16, invPanel.y+320, invPanel.width-32, 32};
                Rectangle craftMedButton = {invPanel.x+16, invPanel.y+356, invPanel.width-32, 32};
                Rectangle craftDoorButton = {invPanel.x+16, invPanel.y+392, invPanel.width-32, 32};
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
                int dragSlot=GetInventorySlotAt(pos,screenWidth,screenHeight);
                int hotbarDragSlot=GetHotbarSlotAt(pos,hotbarSlots);
                if(dragSlot>=0 && inventoryDragItem<0)
                {
                    inventoryDragItem=inventoryOrder[dragSlot];
                    inventoryDragSourceSlot=dragSlot;
                    inventoryDragPosition=pos;
                }
                else if(hotbarDragSlot>=0 && inventoryDragItem<0)
                {
                    inventoryDragItem=hotbarItemIds[hotbarDragSlot];
                    inventoryDragSourceSlot=12+hotbarDragSlot;
                    inventoryDragPosition=pos;
                }
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, takeButton) && (nearbyOreDrop = FindNearestOreDrop(player, oreDrops, oreDropActive)) >= 0)
            { int item=8+oreDropType[nearbyOreDrop]; itemCounts[item]+=oreDropAmount[nearbyOreDrop]; if(itemCounts[item]>99)itemCounts[item]=99; oreDropActive[nearbyOreDrop]=false; }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen && CheckCollisionPointRec(pos, takeButton) && (nearbyWoodDrop = FindNearestWoodDrop(player, woodDrops, woodDropActive)) >= 0)
            { itemCounts[4] += 15; if(itemCounts[4]>99)itemCounts[4]=99; woodDropActive[nearbyWoodDrop]=false; SaveBuildData(buildPieces,buildPieceTypes,buildDoorOpen,buildPieceCount,itemCounts[4]); }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen &&
                     CheckCollisionPointRec(pos, eatButton) && hotbarItemIds[selectedHotbarSlot] == 2)
            {
                eatRequested = true;
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
                    else if (joystickTouchId != id && pos.x >= screenWidth*0.52f && pos.y > screenHeight*0.12f)
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
                else if (joystickTouchId != id && pos.x > screenWidth*0.52f && pos.y > screenHeight*0.12f)
                {
                    lookTouchId = id;
                    previousLookPosition = pos;
                }
            }
        }

        if (touchCount == 0 &&
            mouseLeftPressed)
        {
            Vector2 pos = GetMousePosition();

            if (buildPlanHeld && buildMode && !mainMenuOpen && !settingsOpen && !inventoryOpen && !mapOpen)
            {
                if (CheckCollisionPointRec(pos, buildFloorButton)) selectedBuildType = 0;
                else if (CheckCollisionPointRec(pos, buildWallButton)) selectedBuildType = 1;
                else if (CheckCollisionPointRec(pos, buildRoofButton)) selectedBuildType = 3;
                else if (CheckCollisionPointRec(pos, buildDoorButton)) selectedBuildType = 2;
            }
            else if (mainMenuOpen && !settingsOpen && !editControls && !loadingScreen)
            {
                if (menuPage == 0)
                {
                    if (CheckCollisionPointRec(pos, menuPlayButton))
                    {
                        mainMenuOpen = false;
                        loadingScreen = true;
                        loadingProgress = 0.0f;
                    }
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
            else if (commandChatOpen || commandConsoleOpen)
            {
                Rectangle cp={screenWidth*0.08f,screenHeight*0.10f,screenWidth*0.84f,190};
                Rectangle inputBox={cp.x+14,cp.y+58,cp.width-28,42};
                Rectangle sendButton={cp.x+cp.width-128,cp.y+116,114,42};
                Rectangle closeCmd={cp.x+14,cp.y+116,100,42};
                if(CheckCollisionPointRec(pos,closeCmd))
                {
                    commandChatOpen=false;
                    commandConsoleOpen=false;
                    HideSoftKeyboard();
                }
                else if(CheckCollisionPointRec(pos,inputBox)) ShowSoftKeyboard();
                else if(CheckCollisionPointRec(pos,sendButton))
                {
                    HideSoftKeyboard();
                    if(commandChatOpen)
                    {
                        if(strcmp(commandInput,"/console")==0)
                        {
                            commandChatOpen=false;
                            commandConsoleOpen=true;
                            strcpy(commandOutput,"Консоль открыта: /give или /update");
                        }
                        else strcpy(commandOutput,"Введите /console");
                    }
                    else if(commandConsoleOpen)
                    {
                        if(strcmp(commandInput,"/give")==0)
                        {
                            commandConsoleOpen=false;
                            resourcePanelOpen=true;
                            strcpy(commandOutput,"Панель ресурсов открыта");
                        }
                        else if(strcmp(commandInput,"/update")==0)
                            strcpy(commandOutput,"git pull --ff-only origin master &&\n./gradlew assembleDebug");
                        else strcpy(commandOutput,"Команды: /give или /update");
                    }
                    commandInput[0]='\0';
                }
            }
            else if (!mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen &&
                     CheckCollisionPointRec(pos, chatButton))
            {
                commandChatOpen=true;
                commandConsoleOpen=false;
                commandInput[0]='\0';
                strcpy(commandOutput,"Введите /console");
                ShowSoftKeyboard();
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
                int dragSlot=GetInventorySlotAt(pos,screenWidth,screenHeight);
                int hotbarDragSlot=GetHotbarSlotAt(pos,hotbarSlots);
                if(dragSlot>=0 && inventoryDragItem<0)
                {
                    inventoryDragItem=inventoryOrder[dragSlot];
                    inventoryDragSourceSlot=dragSlot;
                    inventoryDragPosition=pos;
                }
                else if(hotbarDragSlot>=0 && inventoryDragItem<0)
                {
                    inventoryDragItem=hotbarItemIds[hotbarDragSlot];
                    inventoryDragSourceSlot=12+hotbarDragSlot;
                    inventoryDragPosition=pos;
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
                bool clickedHotbar = false;
                for (int slot = 0; slot < 4; slot++)
                {
                    if (CheckCollisionPointRec(pos, hotbarSlots[slot]))
                    {
                        selectedHotbarSlot = slot;
                        clickedHotbar = true;
                        break;
                    }
                }
                // Щелчок по игровому миру — удар/взаимодействие.
                if (!clickedHotbar) attackRequested = true;
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
                        actionButtonScale[selectedActionControl] = 0.10f+t*2.90f;
                    }
                    else if (dragTarget == 9)
                    {
                        float t = fmaxf(0.0f, fminf(1.0f, (pos.x-editAlphaSlider.x)/editAlphaSlider.width));
                        actionButtonAlpha[selectedActionControl] = (int)(t*255.0f);
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
                    if (!IsBlockedAt(nextX, player.z,player.y,barrelPositions,barrelDestroyed) &&
                        !IsAnimalBlockedAt(nextX,player.z,player.y,wildAnimals,wildAnimalTypes,wildAnimalHealth,-1))
                        player.x = nextX;
                    if (!IsBlockedAt(player.x, nextZ,player.y,barrelPositions,barrelDestroyed) &&
                        !IsAnimalBlockedAt(player.x,nextZ,player.y,wildAnimals,wildAnimalTypes,wildAnimalHealth,-1))
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

        // Управление с физической клавиатуры: WASD или стрелки.
        // Работает одновременно с сенсорным управлением, не перехватывая ввод чата.
        if (!joystickActive && !mainMenuOpen && !settingsOpen && !editControls &&
            !inventoryOpen && !mapOpen && !gasCrateOpen && !commandChatOpen &&
            !commandConsoleOpen && !resourcePanelOpen)
        {
            float moveX = (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT) ? 1.0f : 0.0f) -
                          (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT) ? 1.0f : 0.0f);
            float moveForward = (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) ? 1.0f : 0.0f) -
                                (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) ? 1.0f : 0.0f);
            float moveLength = sqrtf(moveX*moveX + moveForward*moveForward);

            if (moveLength > 0.01f)
            {
                if (moveLength > 1.0f) { moveX /= moveLength; moveForward /= moveLength; }
                float worldMoveX = cosf(cameraYaw)*moveX + sinf(cameraYaw)*moveForward;
                float worldMoveZ = sinf(cameraYaw)*moveX - cosf(cameraYaw)*moveForward;
                float survivalSpeed = playerSpeed;
                if (foodLevel < 20.0f || waterLevel < 20.0f) survivalSpeed *= 0.72f;
                if (sprintEnabled || IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                    survivalSpeed *= 1.80f;

                float nextX = player.x + worldMoveX*survivalSpeed;
                float nextZ = player.z + worldMoveZ*survivalSpeed;
                bool ladderZone = fabsf(player.x-12.25f)<0.78f &&
                                  player.z>-27.35f && player.z<-25.65f;
                if (ladderZone &&
                    ((player.y<3.72f && moveForward>0.12f) ||
                     (player.y>0.0f && moveForward< -0.12f)))
                {
                    player.y += moveForward*survivalSpeed*1.15f;
                    if (player.y<0.0f) player.y=0.0f;
                    if (player.y>3.72f) player.y=3.72f;
                    nextX=12.25f;
                    nextZ=-26.5f;
                }
                else if (player.y>0.0f)
                {
                    bool roofSurface = fabsf(player.x)<11.55f &&
                                       player.z>-30.75f && player.z<-13.25f &&
                                       player.y>=3.35f;
                    bool ladderTop = fabsf(player.x-12.25f)<0.95f &&
                                     player.z>-27.35f && player.z<-25.65f &&
                                     player.y>=3.35f;
                    if (!(roofSurface || ladderTop))
                    {
                        player.y -= frameTime*4.0f;
                        if (player.y<0.0f) player.y=0.0f;
                    }
                }

                if (!IsBlockedAt(nextX, player.z, player.y, barrelPositions, barrelDestroyed) &&
                    !IsAnimalBlockedAt(nextX,player.z,player.y,wildAnimals,wildAnimalTypes,wildAnimalHealth,-1))
                    player.x = nextX;
                if (!IsBlockedAt(player.x, nextZ, player.y, barrelPositions, barrelDestroyed) &&
                    !IsAnimalBlockedAt(player.x,nextZ,player.y,wildAnimals,wildAnimalTypes,wildAnimalHealth,-1))
                    player.z = nextZ;

                playerMoving = true;
                walkPhase += 0.22f;
                float targetAngle = atan2f(worldMoveX, -worldMoveZ);
                float angleDiff = targetAngle-playerAngle;
                while (angleDiff > 3.14159265f) angleDiff -= 6.28318530f;
                while (angleDiff < -3.14159265f) angleDiff += 6.28318530f;
                playerAngle += angleDiff*0.22f;
            }
        }

        // Дикая жизнь: блуждание, осмотр, проверка цели, погоня и выпас.
        if(!mainMenuOpen && !loadingScreen)
        {
            double animalNow=GetTime();
            for(int animal=0;animal<MAX_WILD_ANIMALS;animal++)
            {
                Vector3 *a=&wildAnimals[animal];
                float dx=player.x-a->x, dz=player.z-a->z;
                float distance=sqrtf(dx*dx+dz*dz);
                float vision=(wildAnimalTypes[animal]==0)?17.0f:21.0f;
                float sightHeading=wildAnimalHeading[animal]+wildAnimalHeadYaw[animal];
                float forwardX=sinf(sightHeading);
                float forwardZ=-cosf(sightHeading);
                float sightDot=(distance>0.001f)?(dx*forwardX+dz*forwardZ)/distance:1.0f;
                bool seesPlayer=(distance<vision && distance>0.001f &&
                                 sightDot>cosf(0.62f) && player.y<2.8f);
                bool boarIgnoringPlayer=(wildAnimalTypes[animal]!=0 &&
                    animalNow-wildAnimalLastChase[animal]<30.0);

                wildAnimalTimer[animal]-=frameTime;
                wildAnimalLookTimer[animal]-=frameTime;
                wildAnimalGrazeTimer[animal]-=frameTime;

                // При каждом новом приближении олень убегает.
                // После четвёртого отдельного подхода он начинает атаковать.
                if(wildAnimalTypes[animal]==0 && !deerProvoked[animal] &&
                   wildAnimalState[animal]!=ANIMAL_FLEE && wildAnimalState[animal]!=ANIMAL_STARTLE)
                {
                    if(distance<4.0f && !deerWasNearPlayer[animal])
                    {
                        deerWasNearPlayer[animal]=true;
                        deerApproachCount[animal]++;
                        if(deerApproachCount[animal]>=4)
                        {
                            deerProvoked[animal]=true;
                            wildAnimalState[animal]=ANIMAL_CHASE;
                            wildAnimalChaseStarted[animal]=animalNow;
                            wildAnimalHeadYaw[animal]=0.0f;
                            if(gAnimalAudioReady)PlaySound(gBoarAlertSound);
                        }
                        else
                        {
                            wildAnimalState[animal]=ANIMAL_FLEE;
                            wildAnimalChaseStarted[animal]=animalNow;
                            wildAnimalTargetHeading[animal]=atan2f(-dx,dz);
                        }
                    }
                    else if(distance>6.0f)
                    {
                        deerWasNearPlayer[animal]=false;
                    }
                }

                if(wildAnimalState[animal]==ANIMAL_FLEE)
                {
                    dx=player.x-a->x; dz=player.z-a->z;
                    distance=sqrtf(dx*dx+dz*dz);
                    // Олень убегает до 15 секунд или пока игрок не отдалится.
                    if(distance>12.0f || animalNow-wildAnimalChaseStarted[animal]>15.0)
                    {
                        wildAnimalState[animal]=ANIMAL_WANDER;
                        wildAnimalLookTimer[animal]=5.0f+((float)rand()/(float)RAND_MAX)*9.0f;
                        wildAnimalGrazeTimer[animal]=25.0f+((float)rand()/(float)RAND_MAX)*10.0f;
                        wildAnimalTimer[animal]=2.0f;
                    }
                    else if(distance>0.001f)
                    {
                        wildAnimalTargetHeading[animal]=atan2f(-dx,dz);
                    }
                }
                else if(wildAnimalState[animal]==ANIMAL_CHASE)
                {
                    if(wildAnimalTypes[animal]==0)
                    {
                        if(distance>28.0f || animalNow-wildAnimalChaseStarted[animal]>90.0)
                        {
                            deerProvoked[animal]=false;
                            wildAnimalState[animal]=ANIMAL_WANDER;
                            wildAnimalTimer[animal]=2.0f;
                            wildAnimalLookTimer[animal]=6.0f;
                        }
                    }
                    else if(distance>29.0f || animalNow-wildAnimalChaseStarted[animal]>12.0)
                    {
                        // Кабан прекращает погоню: замирает, фыркает,
                        // затем отходит от игрока и 30 секунд не нападает.
                        wildAnimalState[animal]=ANIMAL_SNORT;
                        wildAnimalTimer[animal]=0.9f;
                        // Во время фырканья кабан стоит и смотрит прямо на игрока.
                        wildAnimalTargetHeading[animal]=atan2f(dx,-dz);
                        if(gAnimalAudioReady)PlaySound(gBoarAlertSound);
                        wildAnimalLastChase[animal]=animalNow;
                        wildAnimalGrazeTimer[animal]=60.0f+((float)rand()/(float)RAND_MAX)*60.0f;
                        wildAnimalLookTimer[animal]=4.0f+((float)rand()/(float)RAND_MAX)*7.0f;
                    }
                }
                else if(wildAnimalState[animal]==ANIMAL_SNORT)
                {
                    // Во время фырканья кабан полностью стоит на месте.
                    if(wildAnimalTimer[animal]<=0.0f)
                    {
                        // После фырканья кабан возвращается к обычному блужданию.
                        // Игрока он будет игнорировать до истечения 30 секунд
                        // с момента окончания погони.
                        wildAnimalState[animal]=ANIMAL_WANDER;
                        wildAnimalTimer[animal]=2.0f;
                        wildAnimalLastChase[animal]=animalNow;
                    }
                }
                else if(wildAnimalState[animal]==ANIMAL_INSPECT)
                {
                    wildAnimalTargetHeading[animal]=atan2f(dx,-dz);
                    wildAnimalHeadYaw[animal]=0.0f;
                    if(wildAnimalTimer[animal]<=0.0f)
                    {
                        wildAnimalState[animal]=ANIMAL_CHASE;
                        wildAnimalChaseStarted[animal]=animalNow;
                    }
                }
                else if(wildAnimalState[animal]==ANIMAL_LOOK)
                {
                    wildAnimalHeadYaw[animal]=sinf((float)animalNow*3.4f+animal)*0.62f;
                    if(seesPlayer && !boarIgnoringPlayer)
                    {
                        wildAnimalState[animal]=ANIMAL_INSPECT;
                        wildAnimalTimer[animal]=1.15f;
                        wildAnimalTargetHeading[animal]=atan2f(dx,-dz);
                        wildAnimalHeadYaw[animal]=0.0f;
                        if(gAnimalAudioReady)PlaySound(gBoarAlertSound);
                    }
                    else if(wildAnimalTimer[animal]<=0.0f)
                    {
                        wildAnimalState[animal]=ANIMAL_WANDER;
                        wildAnimalHeadYaw[animal]=0.0f;
                        wildAnimalLookTimer[animal]=5.0f+((float)rand()/(float)RAND_MAX)*10.0f;
                    }
                }
                else if(wildAnimalState[animal]==ANIMAL_GRAZE)
                {
                    wildAnimalHeadYaw[animal]=sinf((float)animalNow*1.8f+animal)*0.06f;
                    if(wildAnimalTimer[animal]<=0.0f)
                    {
                        wildAnimalState[animal]=ANIMAL_WANDER;
                        wildAnimalHeadYaw[animal]=0.0f;
                        wildAnimalGrazeTimer[animal]=(animalNow-wildAnimalLastChase[animal]<600.0)?
                            60.0f+((float)rand()/(float)RAND_MAX)*60.0f:30.0f;
                        wildAnimalLookTimer[animal]=5.0f+((float)rand()/(float)RAND_MAX)*10.0f;
                        wildAnimalTimer[animal]=2.0f+((float)rand()/(float)RAND_MAX)*3.0f;
                    }
                }
                else if(wildAnimalState[animal]!=ANIMAL_STARTLE)
                {
                    if(seesPlayer && !boarIgnoringPlayer)
                    {
                        wildAnimalState[animal]=ANIMAL_CHASE;
                        wildAnimalChaseStarted[animal]=animalNow;
                        if(gAnimalAudioReady)PlaySound(gBoarAlertSound);
                    }
                    else if(wildAnimalLookTimer[animal]<=0.0f)
                    {
                        wildAnimalState[animal]=ANIMAL_LOOK;
                        wildAnimalTimer[animal]=1.6f;
                    }
                    else if(wildAnimalGrazeTimer[animal]<=0.0f)
                    {
                        wildAnimalState[animal]=ANIMAL_GRAZE;
                        wildAnimalTimer[animal]=3.0f+((float)rand()/(float)RAND_MAX)*2.0f;
                        wildAnimalHeadYaw[animal]=0.0f;
                    }
                    else if(wildAnimalTimer[animal]<=0.0f)
                    {
                        wildAnimalTargetHeading[animal]=((float)rand()/(float)RAND_MAX)*6.2831853f;
                        wildAnimalTimer[animal]=2.0f+((float)rand()/(float)RAND_MAX)*4.0f;
                    }
                }

                if(wildAnimalState[animal]==ANIMAL_CHASE)
                {
                    dx=player.x-a->x; dz=player.z-a->z;
                    distance=sqrtf(dx*dx+dz*dz);
                    if(distance>2.20f)
                    {
                        if(wildAnimalTypes[animal]!=0)
                        {
                            // Кабан слегка меняет линию атаки, стараясь обойти игрока.
                            float side=(animal%2==0)?1.0f:-1.0f;
                            float sway=sinf((float)animalNow*1.7f+animal*2.1f)*0.48f;
                            float steerX=dx-side*dz*sway;
                            float steerZ=dz+side*dx*sway;
                            wildAnimalTargetHeading[animal]=atan2f(steerX,-steerZ);
                        }
                        else wildAnimalTargetHeading[animal]=atan2f(dx,-dz);
                    }
                }
                else if(wildAnimalState[animal]==ANIMAL_FLEE)
                {
                    dx=player.x-a->x; dz=player.z-a->z;
                    if(dx*dx+dz*dz>0.001f)
                        wildAnimalTargetHeading[animal]=atan2f(-dx,dz);
                }

                // Животные стараются не идти прямо друг в друга.
                // Олень обходит соседа дугой, кабан при тесном сближении
                // предпочитает остановиться, а не толкаться и застревать.
                float nearestAnimalDist2=999999.0f;
                float avoidX=0.0f, avoidZ=0.0f;
                for(int other=0;other<MAX_WILD_ANIMALS;other++)
                {
                    if(other==animal)continue;
                    float ox=a->x-wildAnimals[other].x;
                    float oz=a->z-wildAnimals[other].z;
                    float d2=ox*ox+oz*oz;
                    if(d2<2.6f*2.6f && d2>0.0001f)
                    {
                        float d=sqrtf(d2);
                        float strength=(2.6f-d)/2.6f;
                        avoidX+=(ox/d)*strength;
                        avoidZ+=(oz/d)*strength;
                        if(d2<nearestAnimalDist2)nearestAnimalDist2=d2;
                    }
                }
                bool isDeer=(wildAnimalTypes[animal]==0);
                if(nearestAnimalDist2<999998.0f && (avoidX*avoidX+avoidZ*avoidZ)>0.0001f)
                {
                    if(isDeer)
                    {
                        float desiredX=avoidX, desiredZ=avoidZ;
                        // Небольшой боковой компонент помогает именно обходить,
                        // а не пятиться назад от другого животного.
                        float side=(animal%2==0)?1.0f:-1.0f;
                        float steerX=desiredX-side*avoidZ*0.65f;
                        float steerZ=desiredZ+side*avoidX*0.65f;
                        wildAnimalTargetHeading[animal]=atan2f(steerX,-steerZ);
                    }
                }

                // Плавный поворот с ограничением угловой скорости.
                float turnDiff=atan2f(sinf(wildAnimalTargetHeading[animal]-wildAnimalHeading[animal]),
                                      cosf(wildAnimalTargetHeading[animal]-wildAnimalHeading[animal]));
                float maxTurn=(wildAnimalTypes[animal]==0?1.65f:1.35f)*frameTime;
                if(turnDiff>maxTurn)turnDiff=maxTurn;
                if(turnDiff< -maxTurn)turnDiff=-maxTurn;
                wildAnimalHeading[animal]+=turnDiff;

                float animalSpeed=(wildAnimalState[animal]==ANIMAL_CHASE)?3.5f:
                                  (wildAnimalState[animal]==ANIMAL_FLEE?8.5f:
                                  (wildAnimalState[animal]==ANIMAL_WANDER?0.72f:0.0f));
                if(!isDeer && nearestAnimalDist2<1.30f*1.30f)
                    animalSpeed=0.0f;
                if(animalSpeed>0.0f)
                {
                    float moveX=sinf(wildAnimalHeading[animal])*animalSpeed*frameTime;
                    float moveZ=-cosf(wildAnimalHeading[animal])*animalSpeed*frameTime;
                    float nextX=a->x+moveX, nextZ=a->z+moveZ;
                    float px=nextX-player.x, pz=nextZ-player.z;
                    bool tooClose=(wildAnimalState[animal]!=ANIMAL_FLEE && px*px+pz*pz<1.55f*1.55f);
                    bool blocked=(nextX*nextX+nextZ*nextZ>258.0f*258.0f) ||
                                 IsAnimalBlockedAt(nextX,nextZ,0.0f,wildAnimals,wildAnimalTypes,wildAnimalHealth,animal) ||
                                 IsGasStationBlocked(nextX,nextZ,0.65f,0.0f) ||
                                 IsMountainBlockedAt(nextX,nextZ,0.65f);
                    for(int t=0;t<treeCount && !blocked;t++)
                    {
                        if(treeChopped[t])continue;
                        float tx=nextX-treePositions[t].x, tz=nextZ-treePositions[t].z;
                        if(tx*tx+tz*tz<1.15f*1.15f)blocked=true;
                    }
                    if(!tooClose && !blocked)
                    {
                        a->x=nextX;
                        a->z=nextZ;
                    }
                    else
                    {
                        // Если животное упёрлось в препятствие, выбирает
                        // обходную траекторию вместо попытки идти в стену.
                        float escapeTurn=(animal%2==0)?1.35f:-1.35f;
                        wildAnimalTargetHeading[animal]=wildAnimalHeading[animal]+escapeTurn;
                        if(wildAnimalState[animal]==ANIMAL_CHASE &&
                           wildAnimalTypes[animal]!=0 && distance>2.2f)
                            wildAnimalState[animal]=ANIMAL_WANDER;
                    }
                }
            }
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
            lookTouchId != -1 && lookTouchId != joystickTouchId)
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

        // ПК: левая кнопка — удар/взаимодействие, правая — установка постройки.
        Vector2 desktopMousePos = GetMousePosition();
        bool mouseOnHud = CheckCollisionPointRec(desktopMousePos, settingsButton) ||
            CheckCollisionPointRec(desktopMousePos, cameraButton) ||
            CheckCollisionPointRec(desktopMousePos, mapButton) ||
            CheckCollisionPointRec(desktopMousePos, buildButton) ||
            CheckCollisionPointRec(desktopMousePos, sprintButton) ||
            CheckCollisionPointRec(desktopMousePos, inventoryButton) ||
            CheckCollisionPointRec(desktopMousePos, interactButton) ||
            CheckCollisionPointRec(desktopMousePos, chatButton) ||
            CheckCollisionPointRec(desktopMousePos, takeButton);
        for (int hs = 0; hs < 4; hs++)
            if (CheckCollisionPointRec(desktopMousePos, hotbarSlots[hs])) mouseOnHud = true;

        if (touchCount == 0 && !mainMenuOpen && !settingsOpen && !editControls &&
            !inventoryOpen && !mapOpen && !gasCrateOpen &&
            !commandChatOpen && !commandConsoleOpen && !resourcePanelOpen &&
            !buildPlanHeld)
        {
            if (mouseLeftPressed && hotbarItemIds[selectedHotbarSlot] == 2 &&
                CheckCollisionPointRec(desktopMousePos, eatButton)) eatRequested = true;
            else if (mouseLeftPressed && !mouseOnHud)
                attackRequested = true;

            if (mouseRightPressed && buildMode)
            {
                if (selectedBuildType != 2 || itemCounts[6] > 0)
                {
                    bool placed = PlaceBuildPiece(player, cameraYaw, selectedBuildType,
                        buildPieces, buildPieceTypes, &buildPieceCount,
                        &itemCounts[4], barrelPositions, barrelDestroyed);
                    if (placed && selectedBuildType == 2) itemCounts[6]--;
                    if (!placed)
                    {
                        float bx, bz; int bt;
                        GetBuildPlacement(player, cameraYaw, selectedBuildType,
                            buildPieces, buildPieceTypes, buildPieceCount, &bx, &bz, &bt);
                        float bhx = (selectedBuildType == 1 && bt == 4) ? 0.12f : 1.25f;
                        float bhz = (selectedBuildType == 1 && bt != 4) ? 0.12f : 1.25f;
                        buildMessageType = IsBuildZoneForbidden(bx, bz, bhx, bhz) ? 1 : 2;
                        buildMessageTimer = 2.2f;
                    }
                    SaveBuildData(buildPieces, buildPieceTypes, buildDoorOpen,
                                  buildPieceCount, itemCounts[4]);
                }
            }
        }

        // В игровом режиме курсор захвачен, а движение мыши вращает камеру.
        // При удержании X курсор освобождается для выбора элемента Building Plan.
        bool showBuildCursor = buildPlanHeld && buildMode &&
            !mainMenuOpen && !settingsOpen && !inventoryOpen && !mapOpen;
        // Android raylib does not implement real pointer capture. Hide/show the
        // cursor without recentering it, otherwise mouse delta becomes zero.
        if (showBuildCursor || mainMenuOpen || settingsOpen || editControls ||
            inventoryOpen || mapOpen || commandChatOpen || commandConsoleOpen ||
            resourcePanelOpen) ShowCursor();
        else HideCursor();

        if (touchCount==0 && !mainMenuOpen && !settingsOpen && !editControls && !inventoryOpen &&
            !mapOpen && !commandChatOpen && !commandConsoleOpen &&
            !showBuildCursor)
        {
            Vector2 mouseDelta = GetMouseDelta();
            float mouseSensitivity = lookSensitivity*0.004f;
            cameraYaw += mouseDelta.x*mouseSensitivity;
            cameraPitch -= mouseDelta.y*mouseSensitivity;
            if (cameraPitch > 1.2f) cameraPitch = 1.2f;
            if (cameraPitch < -1.2f) cameraPitch = -1.2f;
        }

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
        const Vector3 gasCratePositions[4]={{-8.6f,0.48f,-18.0f},{-8.6f,0.48f,-16.0f},{8.6f,0.48f,-18.0f},{0.0f,4.05f,-21.5f}};
        for(int c=0;c<4;c++){
            Vector3 center=gasCratePositions[c];
            float dx=center.x-aimOrigin.x,dy=center.y-aimOrigin.y,dz=center.z-aimOrigin.z;
            float distance=sqrtf(dx*dx+dy*dy+dz*dz);
            if(distance>2.8f||distance<0.1f)continue;
            // Попадание луча проверяется по небольшому объёму именно самого ящика.
            RayCollision hit=GetRayCollisionSphere(centerAimRay,center,0.58f);
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
        int targetedAnimal=-1;
        float closestAnimalDistance=9999.0f;
        for(int animal=0;animal<MAX_WILD_ANIMALS;animal++)
        {
            if(wildAnimalHealth[animal]<=0)continue;
            Vector3 animalCenter={wildAnimals[animal].x,0.82f,wildAnimals[animal].z};
            float dx=animalCenter.x-aimOrigin.x,dy=animalCenter.y-aimOrigin.y,dz=animalCenter.z-aimOrigin.z;
            float distance=sqrtf(dx*dx+dy*dy+dz*dz);
            if(distance>2.5f||distance<0.1f)continue;
            RayCollision hit=GetRayCollisionSphere(centerAimRay,animalCenter,0.78f);
            if(hit.hit && hit.distance<closestAnimalDistance)
            {
                closestAnimalDistance=hit.distance;
                targetedAnimal=animal;
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

        if (eatRequested && eatCooldown <= 0.0f && hotbarItemIds[selectedHotbarSlot] == 2 && itemCounts[2] > 0 &&
            !mainMenuOpen && !settingsOpen && !inventoryOpen && !gasCrateOpen)
        {
            itemCounts[2]--;
            foodLevel = fminf(100.0f, foodLevel + 25.0f);
            consumeAnimationType = 2;
            consumeAnimationTime = consumeAnimationDuration;
            eatCooldown = 1.0f;
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
                openedGasCrate=targetedGasCrate; gasCrateOpen=true; crateTakeCooldown=0.0f; crateGlowSlot=-1;
                joystickActive=false; lookTouchId=-1; attackAnimation=0.0f;
            }
            else if (nearbyDoor >= 0)
            {
                buildDoorOpen[nearbyDoor] = !buildDoorOpen[nearbyDoor];
                SaveBuildData(buildPieces, buildPieceTypes, buildDoorOpen,
                              buildPieceCount, itemCounts[4]);
                attackAnimation = 0.0f;
            }
            else if(targetedAnimal>=0 && wildAnimalHealth[targetedAnimal]>0)
            {
                int damage=(rand()%100==0)?10:5;
                wildAnimalHealth[targetedAnimal]-=damage;
                if(wildAnimalHealth[targetedAnimal]<0)wildAnimalHealth[targetedAnimal]=0;
                if(wildAnimalTypes[targetedAnimal]==0)
                {
                    // Удар по оленю заставляет его сразу спасаться бегством.
                    deerProvoked[targetedAnimal]=false;
                    deerApproachCount[targetedAnimal]=0;
                    deerWasNearPlayer[targetedAnimal]=true;
                    wildAnimalState[targetedAnimal]=ANIMAL_FLEE;
                    wildAnimalChaseStarted[targetedAnimal]=GetTime();
                    wildAnimalTargetHeading[targetedAnimal]=atan2f(
                        wildAnimals[targetedAnimal].x-player.x,
                        player.z-wildAnimals[targetedAnimal].z);
                }
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
            else if (targetedTree >= 0 && !treeChopped[targetedTree] && hotbarItemIds[selectedHotbarSlot] == 0)
            {
                treeHealth[targetedTree]--;
                if (treeHealth[targetedTree] <= 0)
                {
                    treeHealth[targetedTree] = 0;
                    treeChopped[targetedTree] = true;
                    int woodYield = GetTreeWoodYield(treeBiome[targetedTree]);
                    itemCounts[4] = (itemCounts[4] + woodYield > 99) ? 99 : itemCounts[4] + woodYield;
                    SaveBuildData(buildPieces, buildPieceTypes, buildDoorOpen, buildPieceCount, itemCounts[4]);
                }
            }
            else if (targetedOre >= 0 && gOreActive[targetedOre])
            {
                int type = targetedOre/8;
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

        // Перетаскивание предмета завершается, когда палец отпущен.
        if(inventoryDragItem>=0)
        {
            int liveTouches=GetTouchPointCount();
            bool pointerHeld=(liveTouches>0)||IsMouseButtonDown(MOUSE_BUTTON_LEFT);
            if(pointerHeld)
                inventoryDragPosition=(liveTouches>0)?GetTouchPosition(0):GetMousePosition();
            else
            {
                int targetSlot=GetInventorySlotAt(inventoryDragPosition,screenWidth,screenHeight);
                int targetHotbar=GetHotbarSlotAt(inventoryDragPosition,hotbarSlots);
                if(inventoryDragSourceSlot>=0)
                {
                    if(targetSlot>=0)
                    {
                        if(inventoryDragSourceSlot<12)
                        {
                            if(targetSlot!=inventoryDragSourceSlot)
                            {
                                int tmp=inventoryOrder[targetSlot];
                                inventoryOrder[targetSlot]=inventoryOrder[inventoryDragSourceSlot];
                                inventoryOrder[inventoryDragSourceSlot]=tmp;
                            }
                            else if(inventoryDragItem==2 && itemCounts[2]>0)
                            {
                                foodLevel=fminf(100.0f,foodLevel+25.0f);
                                itemCounts[2]--;
                                consumeAnimationType=2;
                                consumeAnimationTime=consumeAnimationDuration;
                            }
                            else if(inventoryDragItem==3 && itemCounts[3]>0)
                            {
                                waterLevel=fminf(100.0f,waterLevel+25.0f);
                                itemCounts[3]--;
                                consumeAnimationType=3;
                                consumeAnimationTime=consumeAnimationDuration;
                            }
                        }
                        else
                        {
                            int sourceHotbar=inventoryDragSourceSlot-12;
                            int tmp=inventoryOrder[targetSlot];
                            inventoryOrder[targetSlot]=hotbarItemIds[sourceHotbar];
                            hotbarItemIds[sourceHotbar]=tmp;
                        }
                    }
                    else if(targetHotbar>=0)
                    {
                        if(inventoryDragSourceSlot<12)
                        {
                            int tmp=hotbarItemIds[targetHotbar];
                            hotbarItemIds[targetHotbar]=inventoryOrder[inventoryDragSourceSlot];
                            inventoryOrder[inventoryDragSourceSlot]=tmp;
                        }
                        else
                        {
                            int sourceHotbar=inventoryDragSourceSlot-12;
                            int tmp=hotbarItemIds[targetHotbar];
                            hotbarItemIds[targetHotbar]=hotbarItemIds[sourceHotbar];
                            hotbarItemIds[sourceHotbar]=tmp;
                        }
                    }
                }
                inventoryDragItem=-1;
                inventoryDragSourceSlot=-1;
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
        DrawPlane((Vector3){0, -0.22f, 0}, (Vector2){1200, 1200}, C(43, 112, 143, 255));
        float terrainView=fminf(500.0f,fmaxf(220.0f,chunkDistance*25.0f));
        float terrainView2=terrainView*terrainView;
        for (int ix=-14; ix<=14; ix++)
        {
            for (int iz=-14; iz<=14; iz++)
            {
                float px = ix*40.0f;
                float pz = iz*40.0f;
                float tdx=px-player.x, tdz=pz-player.z;
                if(tdx*tdx+tdz*tdz>terrainView2) continue;
                if (px*px+pz*pz > 540.0f*540.0f) continue;
                bool coast = (px*px+pz*pz > 490.0f*490.0f);
                int groundBiome=GetWorldBiome(px,pz);
                Color biomeGround = groundBiome==1?C(205,216,220,255):
                                    groundBiome==2?C(207,177,116,255):
                                    groundBiome==3?C(55,111,58,255):C(73,139,76,255);
                if(coast) biomeGround=groundBiome==1?C(176,190,190,255):
                                      groundBiome==2?C(166,145,105,255):C(151,137,102,255);
                DrawPlane((Vector3){px, -0.08f, pz}, (Vector2){40.2f, 40.2f}, biomeGround);
            }
        }

        DrawBiomeAtmosphere(player,GetWorldBiome(player.x,player.z),(float)GetTime(),graphicsQuality);

        // Две извилистые дороги с разными поворотами вместо крестообразной сетки.
        const Vector3 routeA[]={{-205,0,-150},{-145,0,-150},{-145,0,-92},{-70,0,-92},{-70,0,-35},{5,0,-35},{5,0,35},{82,0,35},{82,0,112},{150,0,112},{150,0,178}};
        const Vector3 routeB[]={{-205,0,118},{-145,0,118},{-145,0,55},{-88,0,55},{-88,0,5},{-25,0,5},{-25,0,-35}};
        for(int ri=0;ri<(int)(sizeof(routeA)/sizeof(routeA[0]))-1;ri++)
            DrawRoadSegment(routeA[ri].x,routeA[ri].z,routeA[ri+1].x,routeA[ri+1].z);
        for(int ri=0;ri<(int)(sizeof(routeB)/sizeof(routeB[0]))-1;ri++)
            DrawRoadSegment(routeB[ri].x,routeB[ri].z,routeB[ri+1].x,routeB[ri+1].z);

        // Небольшие естественные пятна травы вокруг игрока.
        if (graphicsQuality >= 1)
        {
            int grassStep = (detailLevel == 0) ? 9 : 13;
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

        // Два типа гор добавлены в мир и отрисовываются по дистанции чанков.
        const Vector3 passageMountains[2] = {{-118,0,-112}, {132,0,94}};
        const Vector3 holeMountains[2] = {{108,0,-126}, {-142,0,78}};
        for (int mi=0; mi<2; mi++)
        {
            float dx=passageMountains[mi].x-player.x;
            float dz=passageMountains[mi].z-player.z;
            if (dx*dx+dz*dz < chunkView2)
                DrawMountainPassage(passageMountains[mi].x, passageMountains[mi].z,
                                    1.0f, mi==1);
            dx=holeMountains[mi].x-player.x;
            dz=holeMountains[mi].z-player.z;
            if (dx*dx+dz*dz < chunkView2)
                DrawMountainHole(holeMountains[mi].x, holeMountains[mi].z,
                                 1.0f, mi==0);
        }

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
            if(IsBuildPlacementBlocked(previewX,previewZ,selectedBuildType,previewStoredType,player.y,barrelPositions,barrelDestroyed))previewValid=false;
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
                DrawOreNode(gOrePositions[ore].x,gOrePositions[ore].z,ore/8);
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
        float detailTreeView = (detailLevel >= 2) ? 155.0f :
                               ((detailLevel == 1) ? 215.0f : 260.0f);
        float treeView = fminf(chunkView, detailTreeView);
        float treeView2 = treeView*treeView;

        for(int animal=0;animal<MAX_WILD_ANIMALS;animal++)
        {
            if(wildAnimalHealth[animal]<=0)continue;
            float adx=wildAnimals[animal].x-player.x, adz=wildAnimals[animal].z-player.z;
            if(adx*adx+adz*adz>chunkView2) continue;
            DrawWildAnimal(wildAnimals[animal],wildAnimalTypes[animal],
                           (float)GetTime()*((wildAnimalState[animal]==ANIMAL_CHASE || wildAnimalState[animal]==ANIMAL_FLEE)?10.0f:6.0f)+animal*1.7f,
                           wildAnimalHeading[animal],wildAnimalState[animal],
                           wildAnimalHeadYaw[animal]);
        }

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
                attackAnimation > 0.0f ? attackAnimation/0.24f : 0.0f,
                hotbarItemIds[selectedHotbarSlot],
                consumeAnimationTime > 0.0f ?
                    1.0f-consumeAnimationTime/consumeAnimationDuration : 0.0f,
                consumeAnimationType
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
                attackAnimation > 0.0f ? attackAnimation/0.24f : 0.0f,
                hotbarItemIds[selectedHotbarSlot],
                consumeAnimationTime > 0.0f ?
                    1.0f-consumeAnimationTime/consumeAnimationDuration : 0.0f,
                consumeAnimationType
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
            bool hudHasResources = selectedBuildType == 2 ? itemCounts[6] > 0 :
                itemCounts[4] >= (selectedBuildType == 0 ? 30 : 15);
            bool hudBlocked = hudStoredType < 0 ||
                IsBuildPlacementBlocked(px,pz,selectedBuildType,hudStoredType,player.y,
                                        barrelPositions,barrelDestroyed);
            Color markerColor = (hudHasResources && !hudBlocked) ?
                C(80,255,145,255) : C(255,75,65,255);
            Vector2 screenCorners[4];
            for (int k=0; k<4; k++) screenCorners[k] = GetWorldToScreen(corners[k], camera);
            for (int k=0; k<4; k++)
                DrawLineEx(screenCorners[k], screenCorners[(k+1)%4], 5.0f, markerColor);
            DrawCircleLines(screenWidth/2, screenHeight/2, 13.0f, markerColor);
            const char *buildTargetLabel = "МЕСТО СТРОИТЕЛЬСТВА";
            DrawText(buildTargetLabel, screenWidth/2-MeasureText(buildTargetLabel,18)/2,
                     screenHeight/2+22, 18, markerColor);
        }

        if(buildMessageTimer>0.0f) buildMessageTimer-=GetFrameTime();
        // Предупреждение при приближении к заправке; после отхода исчезает.
        bool nearGasStation = fabsf(player.x) < 25.0f && player.z > -56.0f && player.z < 13.0f;
        if (nearGasStation && !mainMenuOpen && !settingsOpen && !inventoryOpen)
        {
            const char *msg = "СТРОИТЕЛЬСТВО ЗАПРЕЩЕНО";
            float tw=MeasureTextEx(cyrillicFont,msg,20,1.0f).x;
            DrawRectangleRounded((Rectangle){screenWidth/2-tw/2-14,82,tw+28,36},
                                 0.25f,8,C(75,25,20,225));
            DrawTextEx(cyrillicFont,msg,(Vector2){screenWidth/2-tw/2,91},
                       20,1.0f,C(255,210,190,255));
        }
        if(buildMessageTimer>0.0f && !mainMenuOpen)
        {
            const char *msg = buildMessageType==1 ? "СТРОИТЕЛЬСТВО ЗАПРЕЩЕНО" :
                              "ФУНДАМЕНТ НЕЛЬЗЯ РАЗМЕСТИТЬ: МЕШАЮТ ОБЪЕКТЫ";
            float tw=MeasureTextEx(cyrillicFont,msg,16,1.0f).x;
            DrawRectangleRounded((Rectangle){screenWidth/2-tw/2-12,124,tw+24,32},
                                 0.22f,8,C(60,25,20,220));
            DrawTextEx(cyrillicFont,msg,(Vector2){screenWidth/2-tw/2,132},
                       16,1.0f,C(255,220,205,255));
        }

        // Игровой интерфейс скрывается в главном меню.
        if (!mainMenuOpen)
        {
            // Компактная панель выживания в стиле военного HUD.
            DrawRectangleRounded(
                (Rectangle){14, 72, 244, 88},
                0.16f, 14, C(8, 18, 34, 228)
            );
            DrawRectangleRoundedLinesEx(
                (Rectangle){14, 72, 244, 88},
                0.16f, 14, 1.5f, C(105, 176, 220, 220)
            );
            DrawText("СОСТОЯНИЕ", 27, 78, 11,
                     C(155, 205, 238, 255));
            DrawText("ЕДА", 28, 98, 12, RAYWHITE);
            DrawRectangleRounded(
                (Rectangle){82, 88, 112, 9},
                0.5f, 6, C(38, 55, 74, 235)
            );
            DrawRectangleRounded(
                (Rectangle){82, 88, 112*(foodLevel/100.0f), 9},
                0.5f, 6, C(215, 160, 75, 255)
            );
            DrawText(TextFormat("%d%%", (int)foodLevel),
                     203, 85, 12, C(245, 220, 170, 255));

            DrawText("ВОДА", 28, 124, 12, RAYWHITE);
            DrawRectangleRounded(
                (Rectangle){82, 126, 112, 9},
                0.5f, 6, C(38, 55, 74, 235)
            );
            DrawRectangleRounded(
                (Rectangle){82, 126, 112*(waterLevel/100.0f), 9},
                0.5f, 6, C(75, 170, 235, 255)
            );
            DrawText(TextFormat("%d%%", (int)waterLevel),
                     203, 123, 12, C(170, 220, 250, 255));

            DrawRectangleRounded(
                settingsButton, 0.30f, 16,
                C(20, 34, 52, 235)
            );

            DrawRectangleRoundedLinesEx(
                settingsButton, 0.30f, 16, 2,
                C(130, 205, 255, 240)
            );

            DrawText(
                "НАСТРОЙКИ",
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
                firstPerson ? "ВИД: 1-Е ЛИЦО" : "ВИД: 3-Е ЛИЦО",
                cameraButton.x+10, cameraButton.y+19, 17, RAYWHITE
            );
            DrawRectangleRounded(mapButton,0.30f,16,C(20,34,52,235));
            DrawRectangleRoundedLinesEx(mapButton,0.30f,16,2,C(130,205,255,220));
            DrawText("КАРТА",mapButton.x+mapButton.width/2-MeasureText("КАРТА",19)/2,mapButton.y+22,19,RAYWHITE);
            if(!settingsOpen && !inventoryOpen && !gasCrateOpen) DrawSurvivalButton(chatButton,"ЧАТ",18,false);

            if (!settingsOpen && !editControls && !inventoryOpen)
            {
                DrawSurvivalButton(buildButton, buildMode ? "СТРОИТЬ: ВКЛ." : "СТРОИТЬ", 16, buildMode);
                if (buildMode)
                {
                    bool showTouchBuildControls = false;
#if defined(PLATFORM_ANDROID)
                    showTouchBuildControls = true;
#endif
                    if (buildPlanHeld || showTouchBuildControls)
                    {
                        DrawRectangleRounded((Rectangle){8, 218, 356, 94}, 0.12f, 12, C(8, 19, 32, 240));
                        DrawSurvivalButton(buildFloorButton, "ФУНДАМЕНТ", 10, selectedBuildType == 0);
                        DrawSurvivalButton(buildWallButton, "СТЕНА", 10, selectedBuildType == 1);
                        DrawSurvivalButton(buildRoofButton, "ПОТОЛОК", 10, selectedBuildType == 3);
                        DrawSurvivalButton(buildDoorButton, "ДВЕРЬ", 10, selectedBuildType == 2);
                        DrawSurvivalButton(buildPlaceButton, "ПОСТАВИТЬ", 13, false);
#if defined(PLATFORM_ANDROID)
                        DrawText("ВЫБЕРИ ДЕТАЛЬ И НАЖМИ «ПОСТАВИТЬ»", 12, 311, 10, RAYWHITE);
#else
                        DrawText("УДЕРЖИВАЙТЕ X • ВЫБЕРИТЕ ПОСТРОЙКУ", 12, 311, 10, RAYWHITE);
#endif
                    }
                    else
                    {
                        DrawText("ЗАЖМИТЕ X — ВЫБОР ПОСТРОЙКИ", 12, 307, 10, C(180, 215, 240, 255));
                    }
                    int buildCost = selectedBuildType == 0 ? 30 : ((selectedBuildType == 1 || selectedBuildType == 3) ? 15 : 0);
                    DrawText(TextFormat("ДЕРЕВО: %d  |  %s  | ПКМ — ПОСТАВИТЬ", itemCounts[4], selectedBuildType == 2 ? TextFormat("ДВЕРИ: %d", itemCounts[6]) : TextFormat("ЦЕНА: %d", buildCost)),
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

            // Компактный HUD выживания в верхнем левом углу.
            if (!settingsOpen && !editControls)
            {
                // Нижняя панель быстрого доступа — остаётся видимой поверх инвентаря.
                Rectangle hotbarPanel = {hotbarSlots[0].x-9, hotbarSlots[0].y-7,
                    hotbarSlots[3].x+hotbarSlots[3].width-hotbarSlots[0].x+18, 80};
                DrawRectangleRounded(hotbarPanel, 0.12f, 10, C(6, 14, 28, 218));
                DrawRectangleRoundedLinesEx(hotbarPanel, 0.12f, 10, 1.2f, C(100, 165, 211, 215));
                for (int slot = 0; slot < 4; slot++)
                {
                    Rectangle r = hotbarSlots[slot];
                    bool activeSlot = slot == selectedHotbarSlot;
                    DrawRectangleRounded((Rectangle){r.x+2, r.y+3, r.width, r.height}, 0.13f, 8, C(0,0,0,125));
                    DrawRectangleRounded(r, 0.13f, 8, activeSlot ? C(29, 76, 113, 252) : C(16, 31, 49, 245));
                    DrawRectangleRoundedLinesEx(r, 0.13f, 8, activeSlot ? 2.4f : 1.0f,
                        activeSlot ? C(145, 216, 250, 255) : C(83, 132, 170, 225));
                    DrawText(TextFormat("%d", slot+1), (int)r.x+5, (int)r.y+4, 10,
                        activeSlot ? C(210, 240, 255, 255) : C(150, 184, 210, 255));
                    const char *slotName = itemNames[hotbarItemIds[slot]];
                    int nw = MeasureText(slotName, 10);
                    DrawText(slotName, (int)(r.x+r.width/2-nw/2), (int)r.y+26, 10, RAYWHITE);
                    DrawText(TextFormat("x%d", itemCounts[hotbarItemIds[slot]]),
                        (int)(r.x+r.width-23), (int)(r.y+r.height-14), 10, C(195, 220, 238, 255));
                }
                const char *selectedName = itemNames[hotbarItemIds[selectedHotbarSlot]];
                int selectedNameWidth = MeasureText(selectedName, 12);
                DrawText(selectedName, (int)(screenWidth/2.0f-selectedNameWidth/2.0f),
                    (int)hotbarPanel.y-16, 12, C(160, 220, 255, 250));
                if (!inventoryOpen)
                {
                    DrawSurvivalButtonAlpha(inventoryButton, "СУМКА", 13, false, (unsigned char)actionButtonAlpha[2]);
                    DrawSurvivalButtonAlpha(sprintButton, sprintEnabled ? "БЕГ: ВКЛ." : "БЕГ", 18, sprintEnabled, (unsigned char)actionButtonAlpha[1]);
                }
            }

            // Кнопка действия меняет подпись, когда прицел наведён на ящик.
            if (!settingsOpen && !editControls && !inventoryOpen && !gasCrateOpen)
            {
                unsigned char hitAlpha = (unsigned char)actionButtonAlpha[0];
                bool canOpenCrate = targetedGasCrate >= 0;
                Color hitColor = canOpenCrate ? C(35, 125, 82, hitAlpha) :
                    (attackCooldown <= 0.0f ? C(185, 75, 55, hitAlpha) : C(75, 83, 91, hitAlpha));
                const char *actionLabel = canOpenCrate ? "ОТКРЫТЬ" : "УДАР";
                int actionFont = canOpenCrate ? 16 : 20;
                bool holdingFood = hotbarItemIds[selectedHotbarSlot] == 2;
                const char *visibleActionLabel = holdingFood ? "ЕСТЬ" : actionLabel;
                int visibleActionFont = holdingFood ? 18 : actionFont;
                if (holdingFood) hitColor = eatCooldown <= 0.0f && itemCounts[2] > 0 ? C(45,135,75,hitAlpha) : C(75,83,91,hitAlpha);
                DrawRectangleRounded(interactButton, 0.25f, 14, hitColor);
                DrawRectangleRoundedLinesEx(interactButton, 0.25f, 14, 2, C(230, 190, 160, hitAlpha));
                DrawText(visibleActionLabel,
                    (int)(interactButton.x+interactButton.width/2-MeasureText(visibleActionLabel,visibleActionFont)/2),
                    (int)(interactButton.y+7), visibleActionFont, C(255,255,255,hitAlpha));
                if (holdingFood)
                {
                    const char *eatLabel = eatCooldown <= 0.0f ? (itemCounts[2] > 0 ? "ГОТОВО" : "НЕТ ЕДЫ") : TextFormat("%.1f с",eatCooldown);
                    DrawText(eatLabel,(int)(interactButton.x+interactButton.width/2-MeasureText(eatLabel,13)/2),
                        (int)(interactButton.y+34),13,C(255,255,255,hitAlpha));
                }
                else if (!canOpenCrate)
                {
                    float cooldownRatio=attackCooldown/attackCooldownDuration;
                    DrawRectangleRounded((Rectangle){interactButton.x+12,interactButton.y+34,
                        (interactButton.width-24)*(1.0f-cooldownRatio),7},0.5f,6,C(105,225,150,hitAlpha));
                    const char *cooldownLabel=attackCooldown<=0.0f?"ГОТОВО":TextFormat("%.1f с",attackCooldown);
                    DrawText(cooldownLabel,(int)(interactButton.x+interactButton.width/2-
                        MeasureText(cooldownLabel,13)/2),(int)(interactButton.y+44),13,C(255,255,255,hitAlpha));
                }
            }

            if (targetedTree >= 0 && !treeChopped[targetedTree])
            {
                DrawRectangleRounded((Rectangle){screenWidth/2.0f-130.0f, 105, 260, 48}, 0.18f, 10, C(18, 31, 44, 225));
                DrawRectangleRoundedLinesEx((Rectangle){screenWidth/2.0f-130.0f, 105, 260, 48}, 0.18f, 10, 2, C(90, 165, 105, 240));
                int totalHits = GetTreeHits(treeBiome[targetedTree]);
                DrawText(TextFormat("%s — %d/%d", GetTreeTypeName(treeBiome[targetedTree]),
                                    treeHealth[targetedTree], totalHits),
                         (int)(screenWidth/2.0f-119.0f), 112, 16, RAYWHITE);
                DrawText(TextFormat("ДЕРЕВА: %d", GetTreeWoodYield(treeBiome[targetedTree])),
                         (int)(screenWidth/2.0f-119.0f), 135, 13, C(175, 220, 175, 255));
            }

            if(targetedAnimal>=0 && wildAnimalHealth[targetedAnimal]>0)
            {
                float hudX=screenWidth/2.0f-130.0f;
                Rectangle animalHud={hudX,105,260,58};
                DrawRectangleRounded(animalHud,0.22f,12,C(18,31,44,225));
                DrawRectangleRoundedLinesEx(animalHud,0.22f,12,2,
                    wildAnimalTypes[targetedAnimal]==0?C(150,195,220,240):C(190,145,90,240));
                DrawText(wildAnimalTypes[targetedAnimal]==0?"ОЛЕНЬ":"КАБАН",
                    (int)hudX+18,113,18,RAYWHITE);
                DrawText(TextFormat("ЗДОРОВЬЕ: %d/%d",wildAnimalHealth[targetedAnimal],
                    wildAnimalTypes[targetedAnimal]==0?50:100),(int)hudX+18,137,16,
                    C(220,235,240,255));
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
                    "БОЧКА С ЛУТОМ", (int)hudX+18, 116, 19, RAYWHITE
                );
                DrawText(
                    TextFormat("ЗДОРОВЬЕ: %d/50", barrelHealth[targetedBarrel]),
                    (int)hudX+18, 145, 18,
                    C(255, 218, 145, 255)
                );

                DrawText(
                    "НАВЕДИТЕСЬ НА БОЧКУ",
                    (int)hudX+18, 166, 14, C(190, 215, 230, 255)
                );
            }

            if (targetedOre >= 0 && gOreActive[targetedOre])
            {
                int oreType = targetedOre/8;
                int maxOreHp = (oreType == 2) ? 10 : ((oreType == 3) ? 30 : 15);
                const char *oreLabel = TextFormat("%s: РУДА", GetOreName(oreType));
                float hudX = screenWidth/2.0f-145.0f;
                Rectangle oreHud = {hudX, 105, 290, 105};
                DrawRectangleRounded(oreHud, 0.18f, 12, C(15, 27, 40, 238));
                DrawRectangleRoundedLinesEx(oreHud, 0.18f, 12, 2.2f, GetOreColor(oreType));
                DrawText(oreLabel, (int)hudX+16, 114, 19, RAYWHITE);
                DrawText(TextFormat("ЗДОРОВЬЕ: %d / %d", oreHealth[targetedOre], maxOreHp),
                         (int)hudX+16, 142, 17, C(235, 241, 248, 255));
                if (oreType == 3 && hotbarItemIds[selectedHotbarSlot] != 7)
                    DrawText("ОРУЖИЕ НАХОДИТСЯ В ЯЧЕЙКЕ 2", (int)hudX+16, 184, 13, C(255, 205, 115, 255));
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
            DrawSurvivalButton(menuReturnButton, "МЕНЮ", 19, false);

            if(commandChatOpen || commandConsoleOpen)
            {
                DrawRectangle(0,0,screenWidth,screenHeight,C(0,4,12,190));
                Rectangle cp={screenWidth*0.08f,screenHeight*0.10f,screenWidth*0.84f,190};
                DrawRectangleRounded(cp,0.08f,10,C(18,29,42,250));
                DrawRectangleRoundedLinesEx(cp,0.08f,10,2,C(105,170,210,255));
                DrawTextEx(cyrillicFont,commandChatOpen?"ЧАТ КОМАНД":"КОНСОЛЬ",(Vector2){cp.x+14,cp.y+12},20,1,RAYWHITE);
                Rectangle inputBox={cp.x+14,cp.y+58,cp.width-28,42};
                DrawRectangleRounded(inputBox,0.12f,8,C(7,14,23,255));
                DrawRectangleRoundedLinesEx(inputBox,0.12f,8,1,C(90,145,180,255));
                DrawTextEx(cyrillicFont,commandInput[0]?commandInput:"Введите команду...",(Vector2){inputBox.x+10,inputBox.y+12},16,1,RAYWHITE);
                DrawSurvivalButton((Rectangle){cp.x+14,cp.y+116,100,42},"ЗАКРЫТЬ",13,false);
                DrawSurvivalButton((Rectangle){cp.x+cp.width-128,cp.y+116,114,42},"ОТПРАВИТЬ",13,true);
                DrawTextEx(cyrillicFont,commandOutput,(Vector2){cp.x+14,cp.y+160},11,1,C(170,210,235,255));
            }
            if(resourcePanelOpen)
            {
                DrawRectangle(0,0,screenWidth,screenHeight,C(0,4,12,205));
                Rectangle rp={screenWidth*0.08f,screenHeight*0.10f,screenWidth*0.84f,screenHeight*0.78f};
                DrawRectangleRounded(rp,0.06f,10,C(12, 27, 45, 252));
                DrawRectangleRoundedLinesEx(rp,0.06f,10,2,C(105, 174, 218, 255));
                DrawTextEx(cyrillicFont,"ВЫДАЧА ПРЕДМЕТОВ",(Vector2){rp.x+14,rp.y+12},20,1,RAYWHITE);
                DrawSurvivalButton((Rectangle){rp.x+rp.width-82,rp.y+10,68,34},"X",16,false);
                float rowH=(rp.height-158.0f)/12.0f;
                for(int item=0;item<12;item++)
                {
                    float ry=rp.y+54+item*rowH;
                    Rectangle row={rp.x+10,ry,rp.width-20,rowH-2};
                    bool selected=(giveSelectedItem==item);
                    DrawRectangleRounded(row,0.12f,6,selected?C(30, 83, 123, 255):C(19, 39, 60, 230));
                    if(selected) DrawRectangleRoundedLinesEx(row,0.12f,6,2,C(135, 205, 245, 255));
                    DrawTextEx(cyrillicFont,itemNames[item],(Vector2){rp.x+18,ry+5},14,1,RAYWHITE);
                    DrawText(TextFormat("В ИНВЕНТАРЕ: %d",itemCounts[item]),
                        (int)(rp.x+rp.width-150),(int)ry+6,11,C(180, 220, 245, 255));
                }
                float controlsY=rp.y+rp.height-94.0f;
                DrawSurvivalButton((Rectangle){rp.x+10,controlsY,42,32},"-10",11,false);
                DrawSurvivalButton((Rectangle){rp.x+56,controlsY,36,32},"-1",11,false);
                DrawTextEx(cyrillicFont,TextFormat("%d шт.",giveSelectedAmount),
                    (Vector2){rp.x+100,controlsY+7},14,1,RAYWHITE);
                DrawSurvivalButton((Rectangle){rp.x+rp.width-132,controlsY,36,32},"+1",11,true);
                DrawSurvivalButton((Rectangle){rp.x+rp.width-90,controlsY,42,32},"+10",11,true);
                DrawSurvivalButton((Rectangle){rp.x+14,controlsY+38,rp.width-28,38},
                    TextFormat("ВЫДАТЬ: %s",itemNames[giveSelectedItem]),14,true);
            }
            if (inventoryOpen)
            {
                DrawRectangle(0, 0, screenWidth, screenHeight, C(0, 5, 16, 125));
                Rectangle invPanel = {screenWidth*0.10f, screenHeight*0.12f, screenWidth*0.80f, screenHeight*0.76f};
                DrawRectangleRounded(invPanel, 0.025f, 8, C(10, 23, 41, 250));
                DrawRectangleRoundedLinesEx(invPanel, 0.025f, 8, 1.6f, C(105, 177, 220, 245));
                DrawUiTextBold("ИНВЕНТАРЬ", (int)invPanel.x+18, (int)invPanel.y+13, 26, RAYWHITE);
                DrawUiTextBold("ПРЕДМЕТЫ / БЫСТРЫЙ ДОСТУП", (int)invPanel.x+20, (int)invPanel.y+47, 14, C(165, 205, 232, 255));
                for (int slot = 0; slot < 12; slot++)
                {
                    int item=inventoryOrder[slot];
                    float gap = 8.0f;
                    float cellSize = fminf(68.0f, (invPanel.width-32.0f-gap*3.0f)/4.0f);
                    float gridWidth = cellSize*4.0f+gap*3.0f;
                    float gridX = invPanel.x+(invPanel.width-gridWidth)*0.5f;
                    Rectangle r = {gridX+(slot%4)*(cellSize+gap),
                                   invPanel.y+72+(slot/4)*74, cellSize, cellSize};
                    DrawRectangle((int)r.x+2,(int)r.y+3,(int)r.width,(int)r.height,C(0,0,0,105));
                    DrawRectangle(r.x,r.y,r.width,r.height,C(18, 37, 57, 255));
                    DrawRectangle((int)r.x+2,(int)r.y+2,(int)r.width-4,(int)r.height-4,C(27, 50, 74, 255));
                    DrawRectangleLinesEx(r,1.2f,inventoryDragItem==item?C(145, 216, 250, 255):C(88, 145, 190, 240));
                    DrawRectangle(r.x,r.y,3,r.height,(item==2||item==3)?C(82, 178, 225, 255):C(76, 126, 166, 255));
                    int labelSize = (item == 7) ? 9 : 10;
                    DrawUiTextBold(itemNames[item],(int)r.x+5,(int)r.y+7,labelSize,C(226, 240, 250, 255));
                    DrawUiTextBold(TextFormat("x%d",itemCounts[item]),
                        (int)(r.x+r.width-MeasureText(TextFormat("x%d",itemCounts[item]),12)-5),
                        (int)(r.y+r.height-16),12,C(175, 220, 245, 255));
                }
                if(inventoryDragItem>=0)
                {
                    Rectangle dragCard={inventoryDragPosition.x-42,inventoryDragPosition.y-25,84,50};
                    DrawRectangleRounded(dragCard,0.12f,6,C(13, 34, 55, 245));
                    DrawRectangleRoundedLinesEx(dragCard,0.12f,6,2,C(145, 216, 250, 255));
                    DrawUiTextBold(itemNames[inventoryDragItem],(int)dragCard.x+5,(int)dragCard.y+7,9,RAYWHITE);
                    DrawUiTextBold(TextFormat("x%d",itemCounts[inventoryDragItem]),(int)dragCard.x+5,(int)dragCard.y+27,10,C(190, 225, 245, 255));
                }
                DrawSurvivalButton(inventoryButton, "СУМКА", 13, true);
                DrawUiTextBold("СОЗДАНИЕ", (int)invPanel.x+18, (int)invPanel.y+300,
                    16, C(165, 215, 245, 255));
                Rectangle craftAxeButton = {invPanel.x+16, invPanel.y+320, invPanel.width-32, 32};
                Rectangle craftMedButton = {invPanel.x+16, invPanel.y+356, invPanel.width-32, 32};
                Rectangle craftDoorButton = {invPanel.x+16, invPanel.y+392, invPanel.width-32, 32};
                bool canCraftAxe = itemCounts[4] >= 3 && itemCounts[5] >= 2 && itemCounts[0] < 99;
                bool canCraftMed = itemCounts[4] >= 2 && itemCounts[5] >= 1 && itemCounts[1] < 99;
                DrawRectangleRounded(craftAxeButton, 0.16f, 8,
                    canCraftAxe ? C(31, 91, 119, 255) : C(42, 51, 62, 230));
                DrawRectangleRoundedLinesEx(craftAxeButton, 0.16f, 8, 1.0f, C(100, 180, 220, 210));
                DrawText("СОЗДАТЬ ТОПОР — 3 ДЕРЕВА / 2 КАМНЯ",
                    (int)craftAxeButton.x+10, (int)craftAxeButton.y+8, 12, RAYWHITE);
                DrawRectangleRounded(craftMedButton, 0.16f, 8,
                    canCraftMed ? C(31, 91, 119, 255) : C(42, 51, 62, 230));
                DrawRectangleRoundedLinesEx(craftMedButton, 0.16f, 8, 1.0f, C(100, 180, 220, 210));
                DrawText("СОЗДАТЬ АПТЕЧКУ — 2 ДЕРЕВА / 1 КАМЕНЬ",
                    (int)craftMedButton.x+10, (int)craftMedButton.y+8, 12, RAYWHITE);
                bool canCraftDoor = itemCounts[4] >= 10 && itemCounts[6] < 99;
                DrawRectangleRounded(craftDoorButton, 0.16f, 8, canCraftDoor ? C(31,91,119,255) : C(42,51,62,230));
                DrawRectangleRoundedLinesEx(craftDoorButton, 0.16f, 8, 1.0f, C(100,180,220,210));
                DrawText("СОЗДАТЬ ДВЕРЬ — 10 ДЕРЕВА", (int)craftDoorButton.x+10, (int)craftDoorButton.y+8, 12, RAYWHITE);
                DrawText("НАЖМИТЕ НА ЕДУ ИЛИ ВОДУ, ЧТОБЫ ИСПОЛЬЗОВАТЬ",
                    (int)invPanel.x+18, (int)(invPanel.y+invPanel.height-28),
                    13, C(165, 205, 232, 255));
            }
        }

        // Повторно рисуем хотбар поверх окна инвентаря, чтобы затемнение его не закрывало.
        if (inventoryOpen)
        {
            Rectangle hotbarPanel = {hotbarSlots[0].x-9, hotbarSlots[0].y-7,
                hotbarSlots[3].x+hotbarSlots[3].width-hotbarSlots[0].x+18, 80};
            DrawRectangleRounded(hotbarPanel, 0.12f, 10, C(6, 14, 28, 238));
            DrawRectangleRoundedLinesEx(hotbarPanel, 0.12f, 10, 1.4f, C(115, 185, 230, 245));
            for (int slot = 0; slot < 4; slot++)
            {
                Rectangle r = hotbarSlots[slot];
                bool activeSlot = slot == selectedHotbarSlot;
                DrawRectangleRounded(r, 0.13f, 8, activeSlot ? C(29, 76, 113, 255) : C(16, 31, 49, 250));
                DrawRectangleRoundedLinesEx(r, 0.13f, 8, activeSlot ? 2.4f : 1.2f,
                    activeSlot ? C(155, 225, 255, 255) : C(100, 160, 200, 240));
                DrawText(TextFormat("%d", slot+1), (int)r.x+5, (int)r.y+4, 10, RAYWHITE);
                const char *slotName = itemNames[hotbarItemIds[slot]];
                int nw = MeasureText(slotName, 10);
                DrawText(slotName, (int)(r.x+r.width/2-nw/2), (int)r.y+26, 10, RAYWHITE);
                DrawText(TextFormat("x%d", itemCounts[hotbarItemIds[slot]]),
                    (int)(r.x+r.width-25), (int)(r.y+r.height-15), 10, C(195, 220, 238, 255));
            }
        }

        if(gasCrateOpen&&openedGasCrate>=0){
            DrawRectangle(0,0,screenWidth,screenHeight,C(0,5,16,155));
            float crateSize=fminf(screenHeight*0.62f,screenWidth*0.30f);
            float invSize=fminf(screenHeight*0.48f,screenWidth*0.25f);
            Rectangle cratePanel={screenWidth-crateSize-18.0f,screenHeight-crateSize-62.0f,crateSize,crateSize};
            Rectangle invPanel={(screenWidth-invSize)*0.5f,screenHeight-invSize-50.0f,invSize,invSize};
            DrawRectangleRounded(cratePanel,0.035f,8,C(10, 23, 41, 250));
            DrawRectangleRoundedLinesEx(cratePanel,0.035f,8,1.8f,C(105, 177, 220, 245));
            int crateTitleSize=(int)fmaxf(11.0f,fminf(15.0f,cratePanel.width*0.045f));
            DrawUiTextBold(openedGasCrate==3?"ОРУЖЕЙНЫЙ ЯЩИК":"ЯЩИК С РЕСУРСАМИ",
                (int)cratePanel.x+9,(int)cratePanel.y+13,crateTitleSize,C(230,236,220,255));
            Rectangle close={cratePanel.x+cratePanel.width-48.0f,cratePanel.y+4.0f,42.0f,40.0f};
            DrawRectangleRounded(close,0.18f,8,C(100,49,43,255));
            DrawRectangleRoundedLinesEx(close,0.18f,8,1.5f,C(190,125,110,255));
            DrawUiTextBold("X",(int)close.x+14,(int)close.y+7,22,RAYWHITE);

            DrawRectangleRounded(invPanel,0.035f,8,C(13, 29, 48, 250));
            DrawRectangleRoundedLinesEx(invPanel,0.035f,8,1.8f,C(91, 158, 204, 255));
            DrawUiTextBold("ИНВЕНТАРЬ",(int)invPanel.x+7,(int)invPanel.y+8,13,C(210,230,238,255));

            // Почти квадратный ящик: 4 столбца на 2 ряда.
            int cols=4, rows=2;
            float gap=4.0f;
            float contentY=cratePanel.y+50.0f, contentH=cratePanel.height-58.0f;
            float cellW=(cratePanel.width-16.0f-gap*(cols-1))/(float)cols;
            float cellH=(contentH-gap*(rows-1))/(float)rows;
            for(int slot=0;slot<8;slot++){
                int col=slot%cols,row=slot/cols;
                Rectangle cell={cratePanel.x+8.0f+col*(cellW+gap),contentY+row*(cellH+gap),cellW,cellH};
                int amount=gasCrateAmounts[openedGasCrate][slot];
                int item=gasCrateItems[openedGasCrate][slot];
                DrawRectangle((int)cell.x+2,(int)cell.y+3,(int)cell.width,(int)cell.height,C(0,0,0,100));
                DrawRectangleRec(cell,C(20, 39, 59, 255));
                DrawRectangleLinesEx(cell,1.0f,C(86, 139, 182, 235));
                if(amount>0){
                    int fs=cell.width<55?9:11;
                    DrawUiTextBold(itemNames[item],(int)cell.x+4,(int)(cell.y+cell.height*0.27f),fs,C(235,238,226,255));
                    DrawUiTextBold(TextFormat("x%d",amount),(int)(cell.x+cell.width-MeasureText(TextFormat("x%d",amount),11)-4),(int)(cell.y+cell.height-15),11,C(205,225,174,255));
                } else DrawUiTextBold("ПУСТО",(int)cell.x+4,(int)(cell.y+cell.height*0.36f),9,C(145,155,143,255));
                if(crateGlowSlot==slot && crateTakeCooldown>0.0f){
                    float pulse=0.5f+0.5f*sinf((0.75f-crateTakeCooldown)*18.0f);
                    unsigned char alpha=(unsigned char)(115.0f+125.0f*pulse);
                    DrawRectangleRec(cell,C(105,225,112,(unsigned char)(alpha*0.16f)));
                    DrawRectangleLinesEx(cell,2.0f,C(130,255,135,alpha));
                }
            }

            // Квадратное окно рюкзака: 3 столбца на 4 ряда.
            int invCols=3, invRows=4;
            float invGap=4.0f;
            float invContentY=invPanel.y+34.0f, invContentH=invPanel.height-41.0f;
            float invCellW=(invPanel.width-14.0f-invGap*(invCols-1))/(float)invCols;
            float invCellH=(invContentH-invGap*(invRows-1))/(float)invRows;
            for(int item=0;item<12;item++){
                int col=item%invCols,row=item/invCols;
                Rectangle cell={invPanel.x+7.0f+col*(invCellW+invGap),invContentY+row*(invCellH+invGap),invCellW,invCellH};
                DrawRectangle((int)cell.x+2,(int)cell.y+2,(int)cell.width,(int)cell.height,C(0,0,0,90));
                DrawRectangleRec(cell,C(23, 44, 65, 255));
                DrawRectangleLinesEx(cell,1.0f,C(80, 132, 174, 235));
                DrawRectangle(cell.x,cell.y,2,cell.height,(item==2||item==3)?C(142,169,91,255):C(73, 133, 180, 255));
                int fs=cell.width<48?6:8;
                DrawText(itemNames[item],(int)cell.x+3,(int)(cell.y+cell.height*0.28f),fs,C(224, 238, 248, 255));
                DrawText(TextFormat("x%d",itemCounts[item]),
                    (int)(cell.x+cell.width-MeasureText(TextFormat("x%d",itemCounts[item]),8)-3),
                    (int)(cell.y+cell.height-11),8,C(174, 218, 242, 255));
            }
            if(crateTakeCooldown>0.0f){
                DrawText("ЗАБИРАЕТЕ",(int)cratePanel.x+7,(int)(cratePanel.y+cratePanel.height-13),8,C(155,235,155,255));
                DrawRectangleRounded((Rectangle){cratePanel.x+48,cratePanel.y+cratePanel.height-11,cratePanel.width-56,4},0.5f,6,C(55,67,55,255));
                DrawRectangleRounded((Rectangle){cratePanel.x+48,cratePanel.y+cratePanel.height-11,(cratePanel.width-56)*(crateTakeCooldown/0.75f),4},0.5f,6,C(125,225,125,255));
            } else {
                DrawText("НАЖМИТЕ, ЧТОБЫ ВЗЯТЬ",(int)cratePanel.x+7,(int)(cratePanel.y+cratePanel.height-13),8,C(160,180,160,255));
            }
        }

        if (mapOpen && !mainMenuOpen)
        {
            DrawRectangle(0,0,screenWidth,screenHeight,C(3,10,18,225));
            float ms=fminf(fminf(screenWidth*0.78f,screenHeight*0.70f),430.0f), mx=(screenWidth-ms)*0.5f, my=(screenHeight-ms)*0.5f;
            Rectangle mapPanel={mx-12,my-36,ms+24,ms+82};
            DrawRectangleRounded(mapPanel,0.06f,12,C(12, 27, 45, 250));
            DrawRectangleRoundedLinesEx(mapPanel,0.06f,12,2,C(105, 174, 218, 235));
            DrawText("КАРТА ОСТРОВА",(int)mx,(int)my-27,22,RAYWHITE);

            // Круглая карта и всё её содержимое образуют единый подвижный слой.
            float baseRadius=ms*0.40f;
            float maxZoomX=(mapPanel.width*0.5f-3.0f)/baseRadius;
            float maxZoomY=(mapPanel.height*0.5f-3.0f)/baseRadius;
            float maxMapZoom=fminf(maxZoomX,maxZoomY);
            mapZoom=fmaxf(0.65f,fminf(maxMapZoom,mapZoom));
            float rr=baseRadius*mapZoom;
            float cx=mx+ms*0.5f+mapPanOffset.x;
            float cy=my+ms*0.5f+mapPanOffset.y;
            float sc=rr/540.0f;

            // Перетаскивание перемещает весь круг; жест двумя пальцами
            // одновременно перемещает и масштабирует весь слой карты.
            if(touchCount>=2)
            {
                Vector2 a=GetTouchPosition(0), b=GetTouchPosition(1);
                Vector2 center={(a.x+b.x)*0.5f,(a.y+b.y)*0.5f};
                float dx=a.x-b.x,dy=a.y-b.y;
                float distance=sqrtf(dx*dx+dy*dy);
                if(mapPinchPreviousDistance>1.0f && distance>1.0f)
                {
                    mapZoom*=distance/mapPinchPreviousDistance;
                    mapZoom=fmaxf(0.65f,fminf(maxMapZoom,mapZoom));
                    mapPanOffset.x+=(center.x-mapPinchPreviousCenter.x);
                    mapPanOffset.y+=(center.y-mapPinchPreviousCenter.y);
                }
                mapPinchPreviousDistance=distance;
                mapPinchPreviousCenter=center;
                mapPanTouchId=-1;
            }
            else if(touchCount==1)
            {
                int id=GetTouchPointId(0);
                Vector2 p=GetTouchPosition(0);
                float dx=p.x-cx,dy=p.y-cy;
                if(dx*dx+dy*dy<=rr*rr && CheckCollisionPointRec(p,mapPanel))
                {
                    if(mapPanTouchId==id)
                    {
                        mapPanOffset.x+=(p.x-mapPanPrevious.x);
                        mapPanOffset.y+=(p.y-mapPanPrevious.y);
                    }
                    mapPanPrevious=p;
                    mapPanTouchId=id;
                }
                else mapPanTouchId=-1;
                mapPinchPreviousDistance=0.0f;
            }
            else { mapPanTouchId=-1; mapPinchPreviousDistance=0.0f; }

            // Не даём круглой карте выйти за неподвижную прямоугольную панель.
            rr=baseRadius*mapZoom;
            float minCx=mapPanel.x+rr, maxCx=mapPanel.x+mapPanel.width-rr;
            float minCy=mapPanel.y+rr, maxCy=mapPanel.y+mapPanel.height-rr;
            cx=fmaxf(minCx,fminf(maxCx,mx+ms*0.5f+mapPanOffset.x));
            cy=fmaxf(minCy,fminf(maxCy,my+ms*0.5f+mapPanOffset.y));
            mapPanOffset.x=cx-(mx+ms*0.5f);
            mapPanOffset.y=cy-(my+ms*0.5f);
            sc=rr/260.0f;

            // Вся карта рисуется и обрезается только границами неподвижной панели.
            BeginScissorMode((int)mapPanel.x,(int)mapPanel.y,
                             (int)mapPanel.width,(int)mapPanel.height);
            DrawCircle((int)cx,(int)cy,(int)rr,C(48,91,83,255));
            DrawCircle((int)cx,(int)cy,(int)(rr*0.91f),C(75,111,75,255));

            const Color biomeMapColors[3]={ {205,220,229,255},{211,177,116,255},{42,103,53,255} };
            const char *biomeMapNames[3]={"ЗИМА","ПЕСОК","ЛЕС"};
            int biomeFont=(int)fmaxf(8.0f,13.0f*mapZoom);
            for(int bi=0;bi<3;bi++)
            {
                float bx=cx+gBiomeCenters[bi].x*sc, by=cy+gBiomeCenters[bi].y*sc;
                Vector2 boundary[65];
                boundary[0]=(Vector2){bx,by};
                for(int edge=0;edge<=63;edge++)
                {
                    float angle=(float)edge*6.2831853f/64.0f;
                    float radius=BiomeBoundaryRadius(bi+1,angle)*sc;
                    boundary[edge+1]=(Vector2){bx+cosf(angle)*radius,by+sinf(angle)*radius};
                }
                for(int edge=0;edge<64;edge++)
                    DrawTriangle(boundary[0],boundary[edge+1],boundary[edge+2],biomeMapColors[bi]);
                DrawText(biomeMapNames[bi],(int)bx-MeasureText(biomeMapNames[bi],biomeFont)/2,
                         (int)by-biomeFont/2,biomeFont,C(25,35,30,255));
            }
            for(int m=0;m<6;m++)
            {
                float angle=m*1.05f+0.4f;
                float ax=cx+cosf(angle)*rr*0.57f,ay=cy+sinf(angle)*rr*0.57f;
                DrawCircle((int)ax,(int)ay,(int)(ms*0.035f*mapZoom),C(104,111,105,255));
                DrawCircle((int)(ax-3*mapZoom),(int)(ay-4*mapZoom),
                           (int)(ms*0.018f*mapZoom),C(170,179,170,255));
            }
            for(int t=0;t<18;t++)
            {
                float angle=t*2.399f,r=rr*(0.18f+(float)(t%5)*0.105f);
                DrawCircle((int)(cx+cosf(angle)*r),(int)(cy+sinf(angle)*r),
                           (int)(ms*0.012f*mapZoom),C(34,91,53,255));
            }

            float px=cx+player.x*sc,py=cy+player.z*sc;
            float gasX=cx,gasY=cy-22.0f*sc;
            float markerScale=mapZoom;
            DrawRectangle((int)(gasX-7*markerScale),(int)(gasY-6*markerScale),
                          (int)(14*markerScale),(int)(12*markerScale),C(185,65,48,255));
            DrawRectangleLines((int)(gasX-7*markerScale),(int)(gasY-6*markerScale),
                               (int)(14*markerScale),(int)(12*markerScale),RAYWHITE);
            int markerFont=(int)fmaxf(8.0f,13.0f*mapZoom);
            DrawText("ЗАПРАВКА",(int)(gasX+10*markerScale),(int)(gasY-8*markerScale),
                     markerFont,RAYWHITE);
            DrawCircle((int)px,(int)py,(int)(8*markerScale),C(245,245,230,255));
            DrawCircle((int)px,(int)py,(int)(4*markerScale),C(55,145,255,255));
            DrawCircleLines((int)px,(int)py,(int)(11*markerScale),RAYWHITE);
            DrawText("ВЫ",(int)(px+12*markerScale),(int)(py-8*markerScale),
                     (int)fmaxf(8.0f,12.0f*mapZoom),RAYWHITE);
            // Маскируем всё, что выходит за круг карты.
            for(int row=(int)(cy-rr);row<=(int)(cy+rr);row+=2)
            {
                float dy=(float)row-cy;
                float halfWidth=sqrtf(fmaxf(0.0f,rr*rr-dy*dy));
                float left=cx-halfWidth, right=cx+halfWidth;
                if(left>mapPanel.x) DrawRectangle((int)mapPanel.x,row,(int)(left-mapPanel.x+1),2,C(10,20,32,255));
                float panelRight=mapPanel.x+mapPanel.width;
                if(right<panelRight) DrawRectangle((int)right,row,(int)(panelRight-right+1),2,C(10,20,32,255));
            }
            DrawCircleLines((int)cx,(int)cy,(int)rr,C(185,190,157,255));
            EndScissorMode();

            DrawText("ПЕРЕМЕЩЕНИЕ: 1 ПАЛЕЦ  |  МАСШТАБ: 2 ПАЛЬЦА",
                     (int)mx,(int)(my+ms+12),12,C(205,220,225,255));
            DrawRectangleRounded(mapButton,0.30f,16,C(20,34,52,250));
            DrawRectangleRoundedLinesEx(mapButton,0.30f,16,2,C(130,205,255,255));
            DrawText("ЗАКРЫТЬ КАРТУ",mapButton.x+mapButton.width/2-MeasureText("ЗАКРЫТЬ КАРТУ",16)/2,mapButton.y+23,16,RAYWHITE);
        }

        // Главное меню boltSurvival.
        if (mainMenuOpen)
        {
            menuIntroProgress=fminf(1.0f,menuIntroProgress+frameTime*0.75f);
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

            const char *subtitle = "ВЫЖИВАЙТЕ — ИССЛЕДУЙТЕ — СТРОЙТЕ";
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
                const char *labels[4] = {"ИГРАТЬ", "СЕРВЕРЫ", "МАГАЗИН", "НАСТРОЙКИ"};
                for (int i = 0; i < 4; i++)
                    DrawSurvivalButton(buttons[i], labels[i], 18, i == 0);
            }
            else
            {
                const char *pageTitle =
                    (menuPage == 1) ? "СЕРВЕРЫ" : "МАГАЗИН";

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
                        "СЕРВЕР №1",
                        (int)serverCard.x+18,
                        (int)serverCard.y+14, 20, RAYWHITE
                    );
                    DrawCircle(
                        (int)(serverCard.x+serverCard.width-26),
                        (int)serverCard.y+25, 6,
                        C(80, 220, 135, 255)
                    );
                    DrawText(
                        "В СЕТИ",
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
                    const char *message = "СКОРО";
                    int messageWidth = MeasureText(message, 18);
                    DrawText(
                        message,
                        screenWidth/2-messageWidth/2,
                        menuStartY+75,
                        18,
                        C(165, 190, 210, 255)
                    );
                }
            
                DrawSurvivalButton(menuBackButton, "НАЗАД", 19, false);
            }

            DrawText(
                "АЛЬФА-ВЕРСИЯ",
                18,
                screenHeight-35,
                14,
                C(130, 160, 180, 255)
            );
            float fadeT=menuIntroProgress*menuIntroProgress*(3.0f-2.0f*menuIntroProgress);
            unsigned char introShade=(unsigned char)(255.0f*(1.0f-fadeT));
            if(introShade>0) DrawRectangle(0,0,screenWidth,screenHeight,C(0,0,0,introShade));
        }

        // Меню настроек.
        if (settingsOpen)
        {
            DrawRectangle(
                0, 0, screenWidth, screenHeight,
                C(0, 0, 0, 42)
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
                "НАСТРОЙКИ ИГРЫ",
                panelX+24, panelY+18,
                25, RAYWHITE
            );

            DrawText(
                "Управление и графика",
                panelX+26, panelY+47,
                15, C(155, 177, 198, 255)
            );

            DrawRectangleRounded(
                moveButtonsButton, 0.25f, 14,
                C(24, 56, 83, 255)
            );

            DrawText(
                "ПЕРЕМЕСТИТЬ КНОПКИ",
                moveButtonsButton.x+18,
                moveButtonsButton.y+17,
                19, RAYWHITE
            );

            DrawRectangleRounded(
                sizeButton, 0.25f, 14,
                C(42, 65, 91, 255)
            );

            DrawText(
                "РАЗМЕР ДЖОЙСТИКА",
                sizeButton.x+18,
                sizeButton.y+17,
                19, RAYWHITE
            );

            DrawText(
                "КАЧЕСТВО ГРАФИКИ",
                panelX+25, panelY+166,
                16, C(205, 220, 238, 255)
            );

            Color lowColor =
                graphicsQuality == 0 ?
                C(38, 103, 148, 255) :
                C(42, 65, 91, 255);

            Color medColor =
                graphicsQuality == 1 ?
                C(35, 112, 164, 255) :
                C(42, 65, 91, 255);

            Color highColor =
                graphicsQuality == 2 ?
                C(35, 112, 164, 255) :
                C(42, 65, 91, 255);

            DrawRectangleRounded(lowButton, 0.22f, 12, lowColor);
            DrawRectangleRounded(mediumButton, 0.22f, 12, medColor);
            DrawRectangleRounded(highButton, 0.22f, 12, highColor);

            const char *qualityLabels[3] = {"НИЗКОЕ", "СРЕДНЕЕ", "ВЫСОКОЕ"};
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
                "ЛИМИТ КАДРОВ/С",
                panelX+25, panelY+240,
                16, RAYWHITE
            );

            for (int fi = 0; fi < 5; fi++)
            {
                Color fpsColor =
                    (targetFps == fpsValues[fi]) ?
                    C(35, 112, 164, 255) :
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
                "ДЕТАЛИ МИРА",
                panelX+25, panelY+320,
                16, RAYWHITE
            );

            Color moreColor =
                detailLevel == 0 ?
                C(35, 112, 164, 255) :
                C(42, 65, 91, 255);

            Color someColor =
                detailLevel == 1 ?
                C(35, 112, 164, 255) :
                C(42, 65, 91, 255);

            Color fewColor =
                detailLevel == 2 ?
                C(35, 112, 164, 255) :
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
                "БОЛЬШЕ",
                detailMoreButton.x+detailMoreButton.width/2-25,
                detailMoreButton.y+13,
                15, RAYWHITE
            );

            DrawText(
                "СРЕДНЕ",
                detailSomeButton.x+detailSomeButton.width/2-24,
                detailSomeButton.y+13,
                15, RAYWHITE
            );

            DrawText(
                "МЕНЬШЕ",
                detailFewButton.x+detailFewButton.width/2-20,
                detailFewButton.y+13,
                15, RAYWHITE
            );

            DrawText(
                "РАЗРЕШЕНИЕ РЕНДЕРА",
                panelX+25, panelY+400,
                16, RAYWHITE
            );

            Color fullColor =
                resolutionLevel == 0 ?
                C(35, 112, 164, 255) :
                C(42, 65, 91, 255);

            Color midResColor =
                resolutionLevel == 1 ?
                C(35, 112, 164, 255) :
                C(42, 65, 91, 255);

            Color lowResColor =
                resolutionLevel == 2 ?
                C(35, 112, 164, 255) :
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
                TextFormat("ДАЛЬНОСТЬ ЗАГРУЗКИ: %d / 20", chunkDistance),
                chunkButton.x+15,
                chunkButton.y+11,
                17, RAYWHITE
            );

            DrawText(
                "ЧУВСТВИТЕЛЬНОСТЬ КАМЕРЫ",
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
                "СБРОСИТЬ",
                resetButton.x+resetButton.width/2-34,
                resetButton.y+15,
                18, RAYWHITE
            );

            DrawRectangleRounded(
                closeButton, 0.25f, 14,
                C(35, 133, 218, 255)
            );

            DrawText(
                "ЗАКРЫТЬ",
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
                "НАСТРОЙКА КНОПОК",
                screenWidth/2-145, 12,
                20, RAYWHITE
            );

            DrawText("НАЖМИ НА КНОПКУ, ЧТОБЫ ВЫБРАТЬ ЕЁ",
                screenWidth/2-170, 39, 13, C(205,220,235,255));
            DrawText("РАЗМЕР (10–300%)", (int)editSizeSlider.x, 51, 13, RAYWHITE);
            DrawText("ПРОЗРАЧНОСТЬ", (int)editAlphaSlider.x, 108, 13, RAYWHITE);

            DrawRectangleRounded(editSizeSlider, 0.5f, 8, C(35, 55, 72, 255));
            DrawRectangleRounded(editAlphaSlider, 0.5f, 8, C(35, 55, 72, 255));
            float sizeT = fmaxf(0.0f,fminf(1.0f,(actionButtonScale[selectedActionControl]-0.10f)/2.90f));
            float alphaT = actionButtonAlpha[selectedActionControl]/255.0f;
            DrawRectangleRounded((Rectangle){editSizeSlider.x,editSizeSlider.y,editSizeSlider.width*sizeT,editSizeSlider.height},0.5f,8,C(65,175,235,255));
            DrawRectangleRounded((Rectangle){editAlphaSlider.x,editAlphaSlider.y,editAlphaSlider.width*alphaT,editAlphaSlider.height},0.5f,8,C(65,175,235,255));
            DrawCircleV((Vector2){editSizeSlider.x+editSizeSlider.width*sizeT,editSizeSlider.y+8},10,RAYWHITE);
            DrawCircleV((Vector2){editAlphaSlider.x+editAlphaSlider.width*alphaT,editAlphaSlider.y+8},10,RAYWHITE);

            DrawText(TextFormat("%d%%", (int)(actionButtonScale[selectedActionControl]*100.0f)),
                (int)(editSizeSlider.x+editSizeSlider.width+8), 65, 13, RAYWHITE);
            DrawText(TextFormat("%d%%", actionButtonAlpha[selectedActionControl]*100/255),
                (int)(editAlphaSlider.x+editAlphaSlider.width+8), 123, 13, RAYWHITE);

            // Показываем сами кнопки в режиме редактирования, чтобы было
            // понятно, какую именно область пользователь перетаскивает.
            DrawRectangleRounded(interactButton, 0.25f, 14,
                C(185, 75, 55, (unsigned char)actionButtonAlpha[0]));
            DrawText("УДАР",
                (int)(interactButton.x+interactButton.width/2-MeasureText("УДАР", 20)/2),
                (int)(interactButton.y+interactButton.height/2-10), 20,
                C(255, 255, 255, (unsigned char)actionButtonAlpha[0]));
            DrawSurvivalButtonAlpha(sprintButton, "БЕГ", 18, false,
                (unsigned char)actionButtonAlpha[1]);
            DrawSurvivalButtonAlpha(inventoryButton, "СУМКА", 13, false,
                (unsigned char)actionButtonAlpha[2]);

            DrawRectangleLinesEx(interactButton, selectedActionControl==0?4:2,
                selectedActionControl==0?C(255,235,110,255):C(255,210,120,180));
            DrawRectangleLinesEx(sprintButton, selectedActionControl==1?4:2,
                selectedActionControl==1?C(255,235,110,255):C(255,210,120,180));
            DrawRectangleLinesEx(inventoryButton, selectedActionControl==2?4:2,
                selectedActionControl==2?C(255,235,110,255):C(255,210,120,180));

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
                "ГОТОВО",
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
            const char *loadLabel = "ЗАГРУЗКА МИРА";
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
    UnloadFont(cyrillicFont);
    if(gAnimalAudioReady)UnloadSound(gBoarAlertSound);
    if(IsAudioDeviceReady())CloseAudioDevice();
    CloseWindow();

    return 0;
}

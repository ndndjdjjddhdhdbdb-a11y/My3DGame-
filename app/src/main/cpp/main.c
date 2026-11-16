#include "raymob.h"
#include <math.h>
#include <stdlib.h>

#define MAX_TOUCHES 10

static Color C(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return (Color){r, g, b, a};
}

// Увеличенная невидимая область нажатия кнопки настроек.
// Это облегчает нажатие на маленьких экранах.
static bool IsSettingsButtonHit(Vector2 point, Rectangle button)
{
    // Широкая область слева и сверху. Снизу не заходит на кнопку смены камеры.
    Rectangle hitArea = {
        button.x - 65,
        button.y - 32,
        button.width + 130,
        button.height + 32
    };
    return CheckCollisionPointRec(point, hitArea);
}

static void DrawTree(float x, float z, float scale)
{
    DrawCylinder((Vector3){x, 0.85f*scale, z},
                 0.18f*scale, 0.23f*scale, 1.7f*scale,
                 8, C(112, 75, 45, 255));
    DrawSphere((Vector3){x, 2.0f*scale, z},
               0.95f*scale, C(44, 132, 69, 255));
    DrawSphere((Vector3){x-0.48f*scale, 1.85f*scale, z+0.08f*scale},
               0.62f*scale, C(57, 153, 77, 255));
    DrawSphere((Vector3){x+0.48f*scale, 1.88f*scale, z-0.08f*scale},
               0.62f*scale, C(35, 117, 61, 255));
}


// Небольшой деревянный домик в стиле заброшенного поселения.
static void DrawRustHouse(float x, float z, float scale, int variant)
{
    float w = 5.2f*scale;
    float h = 3.0f*scale;
    float d = 4.6f*scale;
    Color wall = (variant % 3 == 0) ? C(112, 91, 69, 255) :
                 (variant % 3 == 1) ? C(132, 105, 76, 255) :
                                      C(91, 99, 91, 255);
    Color roof = (variant % 2 == 0) ? C(91, 83, 73, 255) : C(105, 78, 62, 255);

    DrawCube((Vector3){x, h*0.5f, z}, w, h, d, wall);
    // Крыша и два ската — простая низкополигональная форма.
    DrawCube((Vector3){x, h+0.18f*scale, z}, w+0.55f*scale,
             0.38f*scale, d+0.55f*scale, roof);
    DrawCube((Vector3){x, h+0.48f*scale, z}, w*0.72f,
             0.28f*scale, d+0.25f*scale, C(76, 72, 66, 255));

    // Дверь и небольшие окна на фасаде.
    DrawCube((Vector3){x, 1.05f*scale, z-d*0.505f},
             0.85f*scale, 2.1f*scale, 0.10f*scale,
             C(58, 48, 39, 255));
    DrawCube((Vector3){x-w*0.30f, 1.85f*scale, z-d*0.515f},
             0.78f*scale, 0.72f*scale, 0.12f*scale,
             C(72, 112, 122, 255));
    DrawCube((Vector3){x+w*0.30f, 1.85f*scale, z-d*0.515f},
             0.78f*scale, 0.72f*scale, 0.12f*scale,
             C(72, 112, 122, 255));
    // Тёмные доски и короткая труба добавляют вид старого домика.
    DrawCube((Vector3){x-w*0.18f, 1.5f*scale, z+d*0.515f},
             0.12f*scale, 2.8f*scale, 0.10f*scale,
             C(65, 54, 43, 255));
    DrawCube((Vector3){x+w*0.18f, 1.5f*scale, z+d*0.515f},
             0.12f*scale, 2.8f*scale, 0.10f*scale,
             C(65, 54, 43, 255));
    DrawCube((Vector3){x+w*0.30f, h+0.75f*scale, z},
             0.42f*scale, 1.2f*scale, 0.42f*scale,
             C(69, 65, 59, 255));
}

// Проверка, чтобы деревья не появлялись на дорогах и вплотную к домам.
static bool IsVillageClear(float x, float z)
{
    if (fabsf(x) < 7.0f || fabsf(z) < 7.0f) return false;
    if (fabsf(z + 120.0f) < 5.0f || fabsf(z - 120.0f) < 5.0f) return false;

    // Посёлок располагается вдоль центральной дороги.
    for (int i = -3; i <= 3; i++)
    {
        float hz = i*55.0f;
        if (fabsf(z-hz) < 12.0f && (fabsf(x-18.0f) < 13.0f ||
                                    fabsf(x+18.0f) < 13.0f))
            return false;
    }
    return true;
}

static void DrawRock(float x, float y, float z, float size)
{
    DrawSphere((Vector3){x, y, z}, size, C(112, 121, 119, 255));
    DrawSphere((Vector3){x-size*0.42f, y+size*0.2f, z-size*0.1f},
               size*0.52f, C(145, 151, 145, 255));
}


// Поворачивает точку персонажа относительно его центра.
static Vector3 PlayerPoint(Vector3 player, float angle, float x, float y, float z)
{
    float c = cosf(angle);
    float sn = sinf(angle);
    return (Vector3){player.x + x*c - z*sn,
                     player.y + y,
                     player.z + x*sn + z*c};
}

// Более детализированный персонаж с простой анимацией ходьбы.
// Руки, видимые снизу экрана в режиме от первого лица.
static void DrawFirstPersonArms(Vector3 eye, float yaw, float walkPhase, bool moving, float time)
{
    Vector3 forward = {sinf(yaw), 0.0f, -cosf(yaw)};
    Vector3 right = {cosf(yaw), 0.0f, sinf(yaw)};

    Color shirt = C(40, 112, 205, 255);
    Color skin = C(225, 183, 143, 255);

    for (int side = -1; side <= 1; side += 2)
    {
        float s = (float)side;

        // Руки двигаются попеременно при ходьбе и слегка покачиваются на месте.
        float armSwing = moving ? sinf(walkPhase)*s*0.13f : sinf(time*1.6f)*0.012f*s;
        float armLift = moving ? cosf(walkPhase*2.0f)*0.025f : sinf(time*1.6f)*0.012f;
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
        Vector3 wrist = {
            eye.x + right.x*s*0.34f + forward.x*(0.78f+armSwing),
            eye.y - 0.48f + armLift,
            eye.z + right.z*s*0.34f + forward.z*(0.78f+armSwing)
        };
        Vector3 hand = {
            eye.x + right.x*s*0.30f + forward.x*(0.88f+armSwing),
            eye.y - 0.45f + armLift,
            eye.z + right.z*s*0.30f + forward.z*(0.88f+armSwing)
        };

        DrawCylinderEx(shoulder, elbow, 0.16f, 0.13f, 8, shirt);
        DrawSphere(elbow, 0.13f, shirt);
        DrawCylinderEx(elbow, wrist, 0.105f, 0.085f, 8, skin);
        DrawSphere(hand, 0.105f, skin);
    }
}

static void DrawPlayer(Vector3 player, float angle, float walkPhase, bool moving)
{
    float swing = moving ? sinf(walkPhase) * 0.34f : 0.0f;
    Color skin = C(225, 183, 143, 255);
    Color shirt = C(40, 112, 205, 255);
    Color pants = C(48, 55, 82, 255);
    Color shoes = C(35, 38, 43, 255);

    // Ноги и обувь.
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

    // Корпус и шея.
    DrawCylinder(PlayerPoint(player, angle, 0, 1.05f, 0),
                 0.31f, 0.36f, 0.78f, 12, shirt);
    DrawCylinder(PlayerPoint(player, angle, 0, 1.48f, 0),
                 0.11f, 0.11f, 0.18f, 8, skin);

    // Руки двигаются навстречу ногам.
    Vector3 shoulderL = PlayerPoint(player, angle, -0.39f, 1.35f, 0);
    Vector3 elbowL = PlayerPoint(player, angle, -0.48f, 0.98f, -swing*0.65f);
    Vector3 handL = PlayerPoint(player, angle, -0.47f, 0.76f, -swing*0.9f);
    Vector3 shoulderR = PlayerPoint(player, angle, 0.39f, 1.35f, 0);
    Vector3 elbowR = PlayerPoint(player, angle, 0.48f, 0.98f, swing*0.65f);
    Vector3 handR = PlayerPoint(player, angle, 0.47f, 0.76f, swing*0.9f);
    DrawCylinderEx(shoulderL, elbowL, 0.12f, 0.095f, 8, shirt);
    DrawCylinderEx(elbowL, handL, 0.09f, 0.075f, 8, skin);
    DrawCylinderEx(shoulderR, elbowR, 0.12f, 0.095f, 8, shirt);
    DrawCylinderEx(elbowR, handR, 0.09f, 0.075f, 8, skin);
    DrawSphere(handL, 0.085f, skin);
    DrawSphere(handR, 0.085f, skin);

    // Голова, волосы, лицо и глаза (лицо направлено вперёд, к -Z).
    DrawSphere(PlayerPoint(player, angle, 0, 1.78f, 0), 0.29f, skin);
    DrawSphere(PlayerPoint(player, angle, 0, 1.94f, 0.015f),
               0.255f, C(65, 43, 30, 255));
    DrawSphere(PlayerPoint(player, angle, 0, 1.82f, -0.17f),
               0.22f, skin);
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
    InitWindow(0, 0, "My 3D World");
    SetTargetFPS(60);

    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    Vector3 player = {0, 0, 0};
    float playerAngle = 0.0f;  // направление, куда смотрит персонаж
    float cameraYaw = 0.0f;    // направление камеры
    float cameraPitch = 0.0f;
    float lookSensitivity = 3.0f;
    float playerSpeed = 0.08f;
    float walkPhase = 0.0f;
    bool playerMoving = false;

    bool firstPerson = false;
    bool settingsOpen = false;
    bool editControls = false;
    // 0 = low, 1 = medium, 2 = high
    int graphicsQuality = 1;
    // 0 = more details, 1 = some details removed, 2 = fewer details
    int detailLevel = 1;
    // 0 = full resolution, 1 = 85%, 2 = 70%
    int resolutionLevel = 0;
    SetTargetFPS(45);

    int renderWidth = (int)(screenWidth * 1.0f);
    int renderHeight = (int)(screenHeight * 1.0f);
    if (renderWidth < 320) renderWidth = 320;
    if (renderHeight < 240) renderHeight = 240;
    RenderTexture2D sceneTexture = LoadRenderTexture(renderWidth, renderHeight);
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
    float panelHeight = 540;
    float panelX = (screenWidth - panelWidth)/2;
    float panelY = (screenHeight - panelHeight)/2;

    Rectangle panel, moveButtonsButton, sizeButton, slider;
    Rectangle lowButton, mediumButton, highButton;
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

    for (int i = 0; i < MAX_TOUCHES; i++) previousTouchIds[i] = -1;

    while (!WindowShouldClose())
    {
        screenWidth = GetScreenWidth();
        screenHeight = GetScreenHeight();

        panelWidth = fminf(540, screenWidth - 30);
        panelHeight = fminf(540, screenHeight - 20);
        panelY = (screenWidth - panelWidth)/2;
        panelY = (screenHeight - panelHeight)/2;

        // Keep the menu controls on-screen, even after changing orientation.
        if (settingsButton.x + settingsButton.width > screenWidth) settingsButton.x = screenWidth-settingsButton.width-12;
        if (settingsButton.x < 8) settingsButton.x = 8;
        if (settingsButton.y < 8) settingsButton.y = 8;
        if (settingsButton.y + settingsButton.height > screenHeight) settingsButton.y = screenHeight-settingsButton.height-8;
        if (cameraButton.x + cameraButton.width > screenWidth) cameraButton.x = screenWidth-cameraButton.width-12;
        if (cameraButton.x < 8) cameraButton.x = 8;
        if (cameraButton.y < 8) cameraButton.y = 8;
        if (cameraButton.y + cameraButton.height > screenHeight) cameraButton.y = screenHeight-cameraButton.height-8;

        panel = (Rectangle){panelX, panelY, panelWidth, panelHeight};
        moveButtonsButton = (Rectangle){panelX+20, panelY+75, panelWidth-40, 44};
        sizeButton = (Rectangle){panelX+20, panelY+128, panelWidth-40, 44};
        float qualityGap = 8;
        float qualityW = (panelWidth-40-qualityGap*2)/3;
        lowButton = (Rectangle){panelX+20, panelY+184, qualityW, 46};
        mediumButton = (Rectangle){panelX+20+qualityW+qualityGap, panelY+184, qualityW, 46};
        highButton = (Rectangle){panelX+20+(qualityW+qualityGap)*2, panelY+184, qualityW, 46};
        detailMoreButton = (Rectangle){panelX+20, panelY+260, qualityW, 42};
        detailSomeButton = (Rectangle){panelX+20+qualityW+qualityGap, panelY+260, qualityW, 42};
        detailFewButton = (Rectangle){panelX+20+(qualityW+qualityGap)*2, panelY+260, qualityW, 42};
        resolutionFullButton = (Rectangle){panelX+20, panelY+336, qualityW, 42};
        resolution85Button = (Rectangle){panelX+20+qualityW+qualityGap, panelY+336, qualityW, 42};
        resolution70Button = (Rectangle){panelX+20+(qualityW+qualityGap)*2, panelY+336, qualityW, 42};
        slider = (Rectangle){panelX+40, panelY+440, panelWidth-80, 12};
        resetButton = (Rectangle){panelX+20, panelY+478, (panelWidth-48)/2, 46};
        closeButton = (Rectangle){panelX+28+(panelWidth-48)/2, panelY+478,
                                  (panelWidth-48)/2, 46};
        doneButton = (Rectangle){screenWidth/2-105, screenHeight-100, 210, 62};

        int touchCount = GetTouchPointCount();
        if (touchCount > MAX_TOUCHES) touchCount = MAX_TOUCHES;

        int currentTouchIds[MAX_TOUCHES];
        for (int i = 0; i < MAX_TOUCHES; i++) currentTouchIds[i] = -1;

        for (int i = 0; i < touchCount; i++)
        {
            int id = GetTouchPointId(i);
            Vector2 pos = GetTouchPosition(i);
            currentTouchIds[i] = id;

            bool newTouch = true;
            for (int j = 0; j < MAX_TOUCHES; j++)
            {
                if (previousTouchIds[j] == id) { newTouch = false; break; }
            }
            if (!newTouch) continue;

            if (editControls)
            {
                if (CheckCollisionPointRec(pos, doneButton))
                {
                    editControls = false;
                    dragTarget = 0;
                    dragTouchId = -1;
                }
                else
                {
                    float dx = pos.x - joystickPosition.x;
                    float dy = pos.y - joystickPosition.y;
                    if (sqrtf(dx*dx + dy*dy) < joystickRadius + 45)
                    {
                        dragTarget = 1;
                        dragTouchId = id;
                        dragOffset = (Vector2){joystickPosition.x-pos.x,
                                               joystickPosition.y-pos.y};
                    }
                    else if (IsSettingsButtonHit(pos, settingsButton))
                    {
                        dragTarget = 2; dragTouchId = id;
                        dragOffset = (Vector2){settingsButton.x-pos.x,
                                               settingsButton.y-pos.y};
                    }
                    else if (CheckCollisionPointRec(pos, cameraButton))
                    {
                        dragTarget = 3; dragTouchId = id;
                        dragOffset = (Vector2){cameraButton.x-pos.x,
                                               cameraButton.y-pos.y};
                    }
                }
            }
            else if (IsSettingsButtonHit(pos, settingsButton))
            {
                settingsOpen = !settingsOpen;
                joystickActive = false;
                lookTouchId = -1;
            }
            else if (CheckCollisionPointRec(pos, cameraButton))
            {
                firstPerson = !firstPerson;
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
                    if (joystickRadius > 180) joystickRadius = 90;
                    knobRadius = joystickRadius*0.42f;
                }
                else if (CheckCollisionPointRec(pos, lowButton))
                {
                    graphicsQuality = 0;
                    SetTargetFPS(30);
                }
                else if (CheckCollisionPointRec(pos, mediumButton))
                {
                    graphicsQuality = 1;
                    SetTargetFPS(45);
                }
                else if (CheckCollisionPointRec(pos, highButton))
                {
                    graphicsQuality = 2;
                    SetTargetFPS(60);
                }
                else if (CheckCollisionPointRec(pos, detailMoreButton)) detailLevel = 0;
                else if (CheckCollisionPointRec(pos, detailSomeButton)) detailLevel = 1;
                else if (CheckCollisionPointRec(pos, detailFewButton)) detailLevel = 2;
                else if (CheckCollisionPointRec(pos, resolutionFullButton)) resolutionLevel = 0;
                else if (CheckCollisionPointRec(pos, resolution85Button)) resolutionLevel = 1;
                else if (CheckCollisionPointRec(pos, resolution70Button)) resolutionLevel = 2;
                else if (CheckCollisionPointRec(pos, slider))
                {
                    draggingSlider = true;
                    sliderTouchId = id;
                }
                else if (CheckCollisionPointRec(pos, resetButton))
                {
                    joystickRadius = 120;
                    knobRadius = 50;
                    joystickPosition = (Vector2){150, screenHeight-160};
                    joystickKnob = joystickPosition;
                    settingsButton = (Rectangle){screenWidth-190, 18, 170, 64};
                    cameraButton = (Rectangle){screenWidth-190, 92, 170, 64};
                    lookSensitivity = 3.0f;
                    graphicsQuality = 1; detailLevel = 1; resolutionLevel = 0;
                    SetTargetFPS(45);
                }
                else if (CheckCollisionPointRec(pos, closeButton))
                {
                    settingsOpen = false;
                }
            }
            else
            {
                if (pos.x < screenWidth*0.48f && pos.y > screenHeight*0.25f)
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

        // Дополнительная обработка мыши: на некоторых устройствах Android
        // касание передаётся Raylib как нажатие левой кнопки мыши.
        if (touchCount == 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            Vector2 pos = GetMousePosition();
            if (!editControls && IsSettingsButtonHit(pos, settingsButton))
            {
                settingsOpen = !settingsOpen;
                joystickActive = false;
                lookTouchId = -1;
            }
            else if (!settingsOpen && !editControls && CheckCollisionPointRec(pos, cameraButton))
            {
                firstPerson = !firstPerson;
            }
            else if (settingsOpen)
            {
                if (CheckCollisionPointRec(pos, moveButtonsButton))
                { settingsOpen = false; editControls = true; }
                else if (CheckCollisionPointRec(pos, sizeButton))
                { joystickRadius += 15; if (joystickRadius > 180) joystickRadius = 90; knobRadius = joystickRadius*0.42f; }
                else if (CheckCollisionPointRec(pos, lowButton))
                { graphicsQuality = 0; SetTargetFPS(30); }
                else if (CheckCollisionPointRec(pos, mediumButton))
                { graphicsQuality = 1; SetTargetFPS(45); }
                else if (CheckCollisionPointRec(pos, highButton))
                { graphicsQuality = 2; SetTargetFPS(60); }
                else if (CheckCollisionPointRec(pos, detailMoreButton)) detailLevel=0;
                else if (CheckCollisionPointRec(pos, detailSomeButton)) detailLevel=1;
                else if (CheckCollisionPointRec(pos, detailFewButton)) detailLevel=2;
                else if (CheckCollisionPointRec(pos, resolutionFullButton)) resolutionLevel=0;
                else if (CheckCollisionPointRec(pos, resolution85Button)) resolutionLevel=1;
                else if (CheckCollisionPointRec(pos, resolution70Button)) resolutionLevel=2;
                else if (CheckCollisionPointRec(pos, slider))
                { float v = (pos.x-slider.x)/slider.width; if(v<0)v=0; if(v>1)v=1; lookSensitivity=0.1f+v*9.9f; }
                else if (CheckCollisionPointRec(pos, resetButton))
                {
                    joystickRadius=120; knobRadius=50;
                    joystickPosition=(Vector2){150,screenHeight-160}; joystickKnob=joystickPosition;
                    settingsButton=(Rectangle){screenWidth-190,18,170,64};
                    cameraButton=(Rectangle){screenWidth-190,92,170,64};
                    lookSensitivity=3.0f; graphicsQuality=1; detailLevel=1; resolutionLevel=0; SetTargetFPS(45);
                }
                else if (CheckCollisionPointRec(pos, closeButton)) settingsOpen=false;
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
                    if (nx < joystickRadius+8) nx = joystickRadius+8;
                    if (nx > screenWidth-joystickRadius-8) nx = screenWidth-joystickRadius-8;
                    if (ny < joystickRadius+8) ny = joystickRadius+8;
                    if (ny > screenHeight-joystickRadius-8) ny = screenHeight-joystickRadius-8;
                    if (dragTarget == 1)
                    {
                        joystickPosition = (Vector2){nx, ny};
                        joystickKnob = joystickPosition;
                    }
                    else if (dragTarget == 2)
                    {
                        settingsButton.x = fmaxf(8, fminf(nx, screenWidth-settingsButton.width-8));
                        settingsButton.y = fmaxf(8, fminf(ny, screenHeight-settingsButton.height-8));
                    }
                    else if (dragTarget == 3)
                    {
                        cameraButton.x = fmaxf(8, fminf(nx, screenWidth-cameraButton.width-8));
                        cameraButton.y = fmaxf(8, fminf(ny, screenHeight-cameraButton.height-8));
                    }
                    found = true;
                    break;
                }
            }
            if (!found) { dragTarget = 0; dragTouchId = -1; }
        }

        playerMoving = false;
        if (joystickActive)
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
                    joystickKnob = (Vector2){joystickPosition.x+dx,
                                             joystickPosition.y+dy};

                    float moveX = dx/joystickRadius;
                    float moveForward = -dy/joystickRadius;
                    if (fabsf(moveX) + fabsf(moveForward) > 0.12f)
                    {
                        playerMoving = true;
                        walkPhase += 0.22f;
                    }
                    float worldMoveX = cosf(cameraYaw)*moveX +
                                       sinf(cameraYaw)*moveForward;
                    float worldMoveZ = sinf(cameraYaw)*moveX -
                                       cosf(cameraYaw)*moveForward;

                    player.x += worldMoveX*playerSpeed;
                    player.z += worldMoveZ*playerSpeed;

                    // Плавно разворачиваем персонажа туда, куда он движется.
                    if (playerMoving)
                    {
                        float targetAngle = atan2f(worldMoveX, -worldMoveZ);
                        float angleDiff = targetAngle - playerAngle;
                        while (angleDiff > 3.14159265f) angleDiff -= 6.28318530f;
                        while (angleDiff < -3.14159265f) angleDiff += 6.28318530f;
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
                    lookSensitivity = 0.1f + value*9.9f;
                    found = true;
                    break;
                }
            }
            if (!found) { draggingSlider = false; sliderTouchId = -1; }
        }

        if (!settingsOpen && !editControls && lookTouchId != -1)
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
            if (!found) lookTouchId = -1;
        }

        if (settingsOpen || editControls) lookTouchId = -1;
        for (int i = 0; i < MAX_TOUCHES; i++) previousTouchIds[i] = currentTouchIds[i];

        float dirX = sinf(cameraYaw)*cosf(cameraPitch);
        float dirY = sinf(cameraPitch);
        float dirZ = -cosf(cameraYaw)*cosf(cameraPitch);

        if (firstPerson)
        {
            camera.position = (Vector3){player.x, player.y+1.6f, player.z};
            camera.target = (Vector3){player.x+dirX*10,
                                      player.y+1.6f+dirY*10,
                                      player.z+dirZ*10};
        }
        else
        {
            camera.position = (Vector3){player.x-dirX*7,
                                        player.y+3.0f-dirY*7,
                                        player.z-dirZ*7};
            camera.target = (Vector3){player.x, player.y+1.4f, player.z};
        }

        float renderScale = (resolutionLevel == 0) ? 1.0f : ((resolutionLevel == 1) ? 0.85f : 0.70f);
        int wantedWidth = (int)(screenWidth * renderScale);
        int wantedHeight = (int)(screenHeight * renderScale);
        if (wantedWidth < 320) wantedWidth = 320;
        if (wantedHeight < 240) wantedHeight = 240;
        if (wantedWidth != renderWidth || wantedHeight != renderHeight)
        {
            UnloadRenderTexture(sceneTexture);
            renderWidth = wantedWidth; renderHeight = wantedHeight;
            sceneTexture = LoadRenderTexture(renderWidth, renderHeight);
            SetTextureFilter(sceneTexture.texture, TEXTURE_FILTER_BILINEAR);
        }

        BeginTextureMode(sceneTexture);
        ClearBackground(C(135, 194, 235, 255));
        BeginMode3D(camera);

        // Большой открытый мир: поле 520 на 520 метров.
        DrawPlane((Vector3){0, -0.08f, 0}, (Vector2){520, 520},
                  C(83, 158, 86, 255));

        // Пыльные дороги с тёмными обочинами, как в survival-играх.
        DrawPlane((Vector3){0, -0.052f, 0}, (Vector2){9.5f, 520},
                  C(116, 105, 84, 255));
        DrawPlane((Vector3){0, -0.048f, 0}, (Vector2){7.0f, 520},
                  C(164, 145, 111, 255));
        DrawPlane((Vector3){0, -0.052f, 0}, (Vector2){520, 9.5f},
                  C(116, 105, 84, 255));
        DrawPlane((Vector3){0, -0.048f, 0}, (Vector2){520, 7.0f},
                  C(164, 145, 111, 255));

        // Две дополнительные поперечные дороги соединяют окраины.
        DrawPlane((Vector3){0, -0.052f, -120}, (Vector2){520, 7.5f},
                  C(116, 105, 84, 255));
        DrawPlane((Vector3){0, -0.048f, -120}, (Vector2){520, 5.2f},
                  C(164, 145, 111, 255));
        DrawPlane((Vector3){0, -0.052f, 120}, (Vector2){520, 7.5f},
                  C(116, 105, 84, 255));
        DrawPlane((Vector3){0, -0.048f, 120}, (Vector2){520, 5.2f},
                  C(164, 145, 111, 255));

        // Небольшие домики вдоль центральной дороги.
        int houseStep = (graphicsQuality == 0 || detailLevel >= 2) ? 2 : 1;
        for (int i = -3; i <= 3; i += houseStep)
        {
            float hz = i*55.0f;
            float sc = 0.82f + (float)((i*i+3)%3)*0.08f;
            DrawRustHouse(-18.0f, hz, sc, i+4);
            if (graphicsQuality >= 1)
                DrawRustHouse(18.0f, hz+22.0f, sc*0.92f, i+7);
        }

        // Несколько дальних домиков у боковых дорог.
        if (graphicsQuality >= 1 && detailLevel == 0)
        {
            DrawRustHouse(-42.0f, -120.0f, 0.78f, 2);
            DrawRustHouse(48.0f, 120.0f, 0.84f, 1);
            DrawRustHouse(42.0f, -120.0f, 0.80f, 0);
        }

        // Дальний холмистый горизонт
        for (int i = -5; i <= 5; i++)
        {
            DrawSphere((Vector3){i*48.0f, -8.0f, -265.0f},
                       34.0f, C(101, 174, 105, 255));
            DrawSphere((Vector3){i*48.0f, -8.0f, 265.0f},
                       34.0f, C(91, 160, 94, 255));
            DrawSphere((Vector3){-265.0f, -8.0f, i*48.0f},
                       34.0f, C(101, 174, 105, 255));
            DrawSphere((Vector3){265.0f, -8.0f, i*48.0f},
                       34.0f, C(91, 160, 94, 255));
        }

        // Солнце и облака
        DrawSphere((Vector3){-28, 28, -45}, 5.0f, C(255, 226, 125, 255));
        if (graphicsQuality >= 1)
        {
            DrawCloud(-15, 18, -35, 2.4f);
            DrawCloud(15, 22, -48, 2.8f);
        }
        if (graphicsQuality >= 2) DrawCloud(32, 17, 12, 2.1f);

        // Деревья вокруг поляны
        int treeStep = (graphicsQuality == 0) ? 28 : ((graphicsQuality == 1) ? 21 : 16);
        treeStep += detailLevel * 3;
        int treeLimit = 245;
        for (int x = -treeLimit; x <= treeLimit; x += treeStep)
        {
            for (int z = -treeLimit; z <= treeLimit; z += treeStep)
            {
                if (!IsVillageClear((float)x, (float)z)) continue;
                float scale = 0.85f + (float)((x*x+z*z)%5)*0.07f;
                DrawTree((float)x, (float)z, scale);
            }
        }

        // Камни и небольшие кусты
        if (graphicsQuality >= 1)
        {
            DrawRock(-5, 0.35f, -8, 0.65f);
            DrawRock(7, 0.28f, 5, 0.48f);
            DrawSphere((Vector3){-7, 0.45f, 3}, 0.65f, C(48, 137, 67, 255));
            if (detailLevel < 2)
                DrawSphere((Vector3){9, 0.42f, -7}, 0.58f, C(58, 147, 72, 255));
        }
        if (graphicsQuality >= 2) DrawRock(-10, 0.25f, 9, 0.42f);

        // Цветы по краям дорожки
        if (graphicsQuality >= 1)
        {
            int flowerStep = (graphicsQuality == 1) ? 6 : 3;
            flowerStep += detailLevel * 3;
            for (int i = -12; i <= 12; i += flowerStep)
            {
                DrawCylinder((Vector3){-3.6f, 0.18f, i*2.0f},
                             0.035f, 0.035f, 0.35f, 5, C(49, 120, 54, 255));
                DrawSphere((Vector3){-3.6f, 0.38f, i*2.0f},
                           0.13f, C(255, 220, 95, 255));
                DrawCylinder((Vector3){3.6f, 0.18f, i*2.0f},
                             0.035f, 0.035f, 0.35f, 5, C(49, 120, 54, 255));
                DrawSphere((Vector3){3.6f, 0.38f, i*2.0f},
                           0.13f, C(245, 150, 175, 255));
            }
        }

        // Новый персонаж с руками, ногами, одеждой и анимацией ходьбы.
        if (!firstPerson)
            DrawPlayer(player, playerAngle, walkPhase, playerMoving);
        else
            DrawFirstPersonArms(camera.position, cameraYaw, walkPhase, playerMoving, (float)GetTime());

        EndMode3D();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(sceneTexture.texture,
                       (Rectangle){0, 0, (float)sceneTexture.texture.width, -(float)sceneTexture.texture.height},
                       (Rectangle){0, 0, (float)screenWidth, (float)screenHeight},
                       (Vector2){0, 0}, 0.0f, WHITE);

        // Небольшая верхняя информационная панель
        DrawRectangleRounded((Rectangle){18, 16, 210, 48}, 0.35f, 12,
                             C(20, 35, 48, 165));
        DrawCircle(42, 40, 7, C(103, 230, 135, 255));
        DrawText("GREEN VALLEY", 58, 29, 19, RAYWHITE);

        // Кнопки в стиле полупрозрачного стекла
        DrawRectangleRounded(settingsButton, 0.30f, 16, C(20, 34, 52, 235));
        DrawRectangleRoundedLinesEx(settingsButton, 0.30f, 16, 2,
                                    C(130, 205, 255, 240));
        DrawText("SETTINGS", settingsButton.x+18, settingsButton.y+22,
                 19, RAYWHITE);

        DrawRectangleRounded(cameraButton, 0.30f, 16, C(20, 34, 52, 235));
        DrawRectangleRoundedLinesEx(cameraButton, 0.30f, 16, 2,
                                    C(130, 205, 255, 220));
        DrawText(firstPerson ? "FIRST PERSON" : "THIRD PERSON",
                 cameraButton.x+10, cameraButton.y+19, 17, RAYWHITE);

        // Джойстик с внешним кольцом
        DrawCircleV(joystickPosition, joystickRadius+5, C(10, 25, 35, 65));
        DrawCircleV(joystickPosition, joystickRadius, C(20, 35, 48, 105));
        DrawCircleLines((int)joystickPosition.x, (int)joystickPosition.y,
                        joystickRadius-4, C(220, 240, 255, 145));
        DrawCircleV(joystickKnob, knobRadius+5, C(10, 20, 30, 90));
        DrawCircleV(joystickKnob, knobRadius, C(220, 239, 255, 205));
        DrawCircleLines((int)joystickKnob.x, (int)joystickKnob.y,
                        knobRadius-3, C(255, 255, 255, 230));

        // Панель настроек
        if (settingsOpen)
        {
            DrawRectangle(0, 0, screenWidth, screenHeight, C(0, 0, 0, 155));
            DrawRectangleRounded(panel, 0.08f, 20, C(19, 29, 45, 250));
            DrawRectangleRoundedLinesEx(panel, 0.08f, 20, 2,
                                        C(100, 175, 225, 230));

            DrawText("GAME SETTINGS", panelX+24, panelY+18, 25, RAYWHITE);
            DrawText("Controls and graphics", panelX+26, panelY+47,
                     15, C(155, 177, 198, 255));

            DrawRectangleRounded(moveButtonsButton, 0.25f, 14,
                                 C(42, 65, 91, 255));
            DrawText("MOVE BUTTONS", moveButtonsButton.x+18,
                     moveButtonsButton.y+17, 19, RAYWHITE);

            DrawRectangleRounded(sizeButton, 0.25f, 14,
                                 C(42, 65, 91, 255));
            DrawText("JOYSTICK SIZE", sizeButton.x+18,
                     sizeButton.y+17, 19, RAYWHITE);

            DrawText("GRAPHICS QUALITY", panelX+25, panelY+166,
                     16, C(205, 220, 238, 255));
            Color lowColor = graphicsQuality == 0 ? C(42, 150, 100, 255) : C(42, 65, 91, 255);
            Color medColor = graphicsQuality == 1 ? C(42, 150, 100, 255) : C(42, 65, 91, 255);
            Color highColor = graphicsQuality == 2 ? C(42, 150, 100, 255) : C(42, 65, 91, 255);
            DrawRectangleRounded(lowButton, 0.22f, 12, lowColor);
            DrawRectangleRounded(mediumButton, 0.22f, 12, medColor);
            DrawRectangleRounded(highButton, 0.22f, 12, highColor);
            DrawText("LOW", lowButton.x+lowButton.width/2-19, lowButton.y+14, 16, RAYWHITE);
            DrawText("MEDIUM", mediumButton.x+mediumButton.width/2-34, mediumButton.y+14, 16, RAYWHITE);
            DrawText("HIGH", highButton.x+highButton.width/2-19, highButton.y+14, 16, RAYWHITE);

            DrawText("WORLD DETAILS", panelX+25, panelY+240, 16, RAYWHITE);
            Color moreColor = detailLevel == 0 ? C(42,150,100,255) : C(42,65,91,255);
            Color someColor = detailLevel == 1 ? C(42,150,100,255) : C(42,65,91,255);
            Color fewColor = detailLevel == 2 ? C(42,150,100,255) : C(42,65,91,255);
            DrawRectangleRounded(detailMoreButton, 0.22f, 12, moreColor);
            DrawRectangleRounded(detailSomeButton, 0.22f, 12, someColor);
            DrawRectangleRounded(detailFewButton, 0.22f, 12, fewColor);
            DrawText("MORE", detailMoreButton.x+detailMoreButton.width/2-25, detailMoreButton.y+13, 15, RAYWHITE);
            DrawText("SOME", detailSomeButton.x+detailSomeButton.width/2-24, detailSomeButton.y+13, 15, RAYWHITE);
            DrawText("LESS", detailFewButton.x+detailFewButton.width/2-20, detailFewButton.y+13, 15, RAYWHITE);

            DrawText("RENDER RESOLUTION", panelX+25, panelY+316, 16, RAYWHITE);
            Color fullColor = resolutionLevel == 0 ? C(42,150,100,255) : C(42,65,91,255);
            Color midResColor = resolutionLevel == 1 ? C(42,150,100,255) : C(42,65,91,255);
            Color lowResColor = resolutionLevel == 2 ? C(42,150,100,255) : C(42,65,91,255);
            DrawRectangleRounded(resolutionFullButton, 0.22f, 12, fullColor);
            DrawRectangleRounded(resolution85Button, 0.22f, 12, midResColor);
            DrawRectangleRounded(resolution70Button, 0.22f, 12, lowResColor);
            DrawText("100%", resolutionFullButton.x+resolutionFullButton.width/2-22, resolutionFullButton.y+13, 15, RAYWHITE);
            DrawText("85%", resolution85Button.x+resolution85Button.width/2-17, resolution85Button.y+13, 15, RAYWHITE);
            DrawText("70%", resolution70Button.x+resolution70Button.width/2-17, resolution70Button.y+13, 15, RAYWHITE);

            DrawText("CAMERA SENSITIVITY", panelX+25, panelY+402,
                     16, RAYWHITE);
            DrawText(TextFormat("%.1f", lookSensitivity),
                     panelX+panelWidth-70, panelY+402, 20,
                     C(95, 195, 255, 255));

            float sliderValue = (lookSensitivity-0.1f)/9.9f;
            DrawRectangleRounded((Rectangle){slider.x, slider.y-5,
                               slider.width, 22}, 0.5f, 12,
                               C(53, 69, 91, 255));
            DrawRectangleRounded((Rectangle){slider.x, slider.y-5,
                               slider.width*sliderValue, 22}, 0.5f, 12,
                               C(52, 164, 238, 255));
            DrawCircle((int)(slider.x+slider.width*sliderValue),
                       (int)(slider.y+6), 15, RAYWHITE);
            DrawText("0.1", slider.x, slider.y+18, 14, C(160, 180, 200, 255));
            DrawText("10", slider.x+slider.width-20, slider.y+18,
                     14, C(160, 180, 200, 255));

            DrawRectangleRounded(resetButton, 0.25f, 14,
                                 C(57, 75, 98, 255));
            DrawText("RESET", resetButton.x+resetButton.width/2-34,
                     resetButton.y+15, 18, RAYWHITE);

            DrawRectangleRounded(closeButton, 0.25f, 14,
                                 C(35, 133, 218, 255));
            DrawText("CLOSE", closeButton.x+closeButton.width/2-34,
                     closeButton.y+15, 18, RAYWHITE);
        }

        if (editControls)
        {
            DrawRectangle(0, 0, screenWidth, screenHeight, C(0, 0, 0, 125));
            DrawText("MOVE YOUR CONTROLS", screenWidth/2-145, 30,
                     22, RAYWHITE);
            DrawRectangleLinesEx(settingsButton, 3, C(80, 200, 255, 255));
            DrawRectangleLinesEx(cameraButton, 3, C(80, 200, 255, 255));
            DrawCircleLines((int)joystickPosition.x,
                            (int)joystickPosition.y,
                            joystickRadius+5, C(80, 200, 255, 255));
            DrawRectangleRounded(doneButton, 0.25f, 14,
                                 C(35, 133, 218, 255));
            DrawText("DONE", doneButton.x+74, doneButton.y+20,
                     22, RAYWHITE);
        }

        EndDrawing();
    }

    UnloadRenderTexture(sceneTexture);
    CloseWindow();
    return 0;
}

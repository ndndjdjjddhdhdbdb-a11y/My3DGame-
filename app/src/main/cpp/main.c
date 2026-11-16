#include "raymob.h"
#include <math.h>
#include <stdlib.h>

#define MAX_TOUCHES 10

static Color C(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return (Color){r, g, b, a};
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

static void DrawRock(float x, float y, float z, float size)
{
    DrawSphere((Vector3){x, y, z}, size, C(112, 121, 119, 255));
    DrawSphere((Vector3){x-size*0.42f, y+size*0.2f, z-size*0.1f},
               size*0.52f, C(145, 151, 145, 255));
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
    float playerAngle = 0.0f;
    float cameraPitch = 0.0f;
    float lookSensitivity = 3.0f;
    float playerSpeed = 0.08f;

    bool firstPerson = false;
    bool settingsOpen = false;
    bool editControls = false;

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

    Rectangle settingsButton = {screenWidth - 170, 20, 150, 58};
    Rectangle cameraButton = {screenWidth - 170, 90, 150, 58};

    float panelWidth = fminf(540, screenWidth - 30);
    float panelHeight = 455;
    float panelX = (screenWidth - panelWidth)/2;
    float panelY = (screenHeight - panelHeight)/2;

    Rectangle panel, moveButtonsButton, sizeButton, slider;
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
        panelX = (screenWidth - panelWidth)/2;
        panelY = (screenHeight - panelHeight)/2;

        panel = (Rectangle){panelX, panelY, panelWidth, panelHeight};
        moveButtonsButton = (Rectangle){panelX+25, panelY+100, panelWidth-50, 52};
        sizeButton = (Rectangle){panelX+25, panelY+165, panelWidth-50, 52};
        slider = (Rectangle){panelX+45, panelY+275, panelWidth-90, 12};
        resetButton = (Rectangle){panelX+25, panelY+365, (panelWidth-65)/2, 55};
        closeButton = (Rectangle){panelX+40+(panelWidth-65)/2, panelY+365,
                                  (panelWidth-65)/2, 55};
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
                    else if (CheckCollisionPointRec(pos, settingsButton))
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
            else if (CheckCollisionPointRec(pos, settingsButton))
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
                    settingsButton = (Rectangle){screenWidth-170, 20, 150, 58};
                    cameraButton = (Rectangle){screenWidth-170, 90, 150, 58};
                    lookSensitivity = 3.0f;
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
                        joystickPosition = (Vector2){nx, ny};
                        joystickKnob = joystickPosition;
                    }
                    else if (dragTarget == 2)
                    {
                        settingsButton.x = nx; settingsButton.y = ny;
                    }
                    else if (dragTarget == 3)
                    {
                        cameraButton.x = nx; cameraButton.y = ny;
                    }
                    found = true;
                    break;
                }
            }
            if (!found) { dragTarget = 0; dragTouchId = -1; }
        }

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
                    player.x += (cosf(playerAngle)*moveX +
                                 sinf(playerAngle)*moveForward)*playerSpeed;
                    player.z += (sinf(playerAngle)*moveX -
                                 cosf(playerAngle)*moveForward)*playerSpeed;
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
                    playerAngle += dx*sensitivity;
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

        float dirX = sinf(playerAngle)*cosf(cameraPitch);
        float dirY = sinf(cameraPitch);
        float dirZ = -cosf(playerAngle)*cosf(cameraPitch);

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

        BeginDrawing();
        ClearBackground(C(135, 194, 235, 255));

        BeginMode3D(camera);

        // Большая зелёная поляна и светлая дорожка
        DrawPlane((Vector3){0, -0.08f, 0}, (Vector2){220, 220},
                  C(83, 158, 86, 255));
        DrawPlane((Vector3){0, -0.045f, 0}, (Vector2){5, 220},
                  C(190, 174, 128, 255));
        DrawPlane((Vector3){0, -0.04f, 0}, (Vector2){220, 5},
                  C(190, 174, 128, 255));

        // Дальний холмистый горизонт
        for (int i = -3; i <= 3; i++)
        {
            DrawSphere((Vector3){i*24.0f, -2.5f, -52.0f},
                       18.0f, C(101, 174, 105, 255));
            DrawSphere((Vector3){i*24.0f, -2.8f, 52.0f},
                       19.0f, C(91, 160, 94, 255));
        }

        // Солнце и облака
        DrawSphere((Vector3){-28, 28, -45}, 5.0f, C(255, 226, 125, 255));
        DrawCloud(-15, 18, -35, 2.4f);
        DrawCloud(15, 22, -48, 2.8f);
        DrawCloud(32, 17, 12, 2.1f);

        // Деревья вокруг поляны
        for (int x = -32; x <= 32; x += 8)
        {
            for (int z = -32; z <= 32; z += 8)
            {
                if ((abs(x) < 9 && abs(z) < 9) ||
                    (abs(x) < 5 && abs(z) < 25) ||
                    (abs(z) < 5 && abs(x) < 25)) continue;
                float scale = 0.85f + (float)((x*x+z*z)%5)*0.07f;
                DrawTree((float)x, (float)z, scale);
            }
        }

        // Камни и небольшие кусты
        DrawRock(-5, 0.35f, -8, 0.65f);
        DrawRock(7, 0.28f, 5, 0.48f);
        DrawRock(-10, 0.25f, 9, 0.42f);
        DrawSphere((Vector3){-7, 0.45f, 3}, 0.65f, C(48, 137, 67, 255));
        DrawSphere((Vector3){9, 0.42f, -7}, 0.58f, C(58, 147, 72, 255));

        // Цветы по краям дорожки
        for (int i = -12; i <= 12; i += 3)
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

        // Персонаж
        if (!firstPerson)
        {
            DrawCylinder((Vector3){player.x, player.y+0.68f, player.z},
                         0.40f, 0.40f, 1.35f, 16, C(49, 112, 205, 255));
            DrawSphere((Vector3){player.x, player.y+1.58f, player.z},
                       0.36f, C(232, 194, 153, 255));
            DrawSphere((Vector3){player.x-0.42f, player.y+0.78f, player.z},
                       0.17f, C(232, 194, 153, 255));
            DrawSphere((Vector3){player.x+0.42f, player.y+0.78f, player.z},
                       0.17f, C(232, 194, 153, 255));
        }

        EndMode3D();

        // Небольшая верхняя информационная панель
        DrawRectangleRounded((Rectangle){18, 16, 210, 48}, 0.35f, 12,
                             C(20, 35, 48, 165));
        DrawCircle(42, 40, 7, C(103, 230, 135, 255));
        DrawText("GREEN VALLEY", 58, 29, 19, RAYWHITE);

        // Кнопки в стиле полупрозрачного стекла
        DrawRectangleRounded(settingsButton, 0.35f, 16, C(20, 34, 52, 215));
        DrawRectangleRoundedLinesEx(settingsButton, 0.35f, 16, 2,
                                    C(130, 205, 255, 220));
        DrawText("SETTINGS", settingsButton.x+23, settingsButton.y+19,
                 19, RAYWHITE);

        DrawRectangleRounded(cameraButton, 0.35f, 16, C(20, 34, 52, 215));
        DrawRectangleRoundedLinesEx(cameraButton, 0.35f, 16, 2,
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

            DrawText("GAME SETTINGS", panelX+28, panelY+25, 29, RAYWHITE);
            DrawText("Personalize your controls", panelX+30, panelY+61,
                     16, C(155, 177, 198, 255));

            DrawRectangleRounded(moveButtonsButton, 0.25f, 14,
                                 C(42, 65, 91, 255));
            DrawText("MOVE BUTTONS", moveButtonsButton.x+18,
                     moveButtonsButton.y+17, 19, RAYWHITE);

            DrawRectangleRounded(sizeButton, 0.25f, 14,
                                 C(42, 65, 91, 255));
            DrawText("JOYSTICK SIZE", sizeButton.x+18,
                     sizeButton.y+17, 19, RAYWHITE);

            DrawText("CAMERA SENSITIVITY", panelX+30, panelY+235,
                     18, RAYWHITE);
            DrawText(TextFormat("%.1f", lookSensitivity),
                     panelX+panelWidth-82, panelY+235, 22,
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
            DrawText("0.1", slider.x, slider.y+28, 16, C(160, 180, 200, 255));
            DrawText("10", slider.x+slider.width-20, slider.y+28,
                     16, C(160, 180, 200, 255));

            DrawRectangleRounded(resetButton, 0.25f, 14,
                                 C(57, 75, 98, 255));
            DrawText("RESET", resetButton.x+resetButton.width/2-34,
                     resetButton.y+18, 19, RAYWHITE);

            DrawRectangleRounded(closeButton, 0.25f, 14,
                                 C(35, 133, 218, 255));
            DrawText("CLOSE", closeButton.x+closeButton.width/2-34,
                     closeButton.y+18, 19, RAYWHITE);
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

    CloseWindow();
    return 0;
}

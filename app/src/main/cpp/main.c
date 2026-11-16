#include "raymob.h"
#include <math.h>

#define MAX_TOUCHES 10

int main(void)
{
    InitWindow(0, 0, "My 3D Game");
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

    // Джойстик
    float joystickRadius = 120.0f;
    float knobRadius = 50.0f;

    Vector2 joystickPosition = {
        150,
        screenHeight - 160
    };

    Vector2 joystickKnob = joystickPosition;

    bool joystickActive = false;
    int joystickTouchId = -1;

    // Кнопки
    Rectangle settingsButton = {
        screenWidth - 160, 20, 140, 55
    };

    Rectangle cameraButton = {
        screenWidth - 160, 85, 140, 55
    };

    // Панель настроек
    float panelWidth = fminf(520, screenWidth - 30);
    float panelHeight = 440;

    float panelX = (screenWidth - panelWidth) / 2;
    float panelY = (screenHeight - panelHeight) / 2;

    Rectangle panel;
    Rectangle moveButtonsButton;
    Rectangle sizeButton;
    Rectangle slider;
    Rectangle resetButton;
    Rectangle closeButton;
    Rectangle doneButton;

    // Перемещение интерфейса
    int dragTarget = 0;
    int dragTouchId = -1;

    Vector2 dragOffset = {0, 0};

    // Поворот камеры
    int lookTouchId = -1;
    Vector2 previousLookPosition = {0, 0};

    // Ползунок
    bool draggingSlider = false;
    int sliderTouchId = -1;

    int previousTouchIds[MAX_TOUCHES];

    for (int i = 0; i < MAX_TOUCHES; i++)
        previousTouchIds[i] = -1;

    while (!WindowShouldClose())
    {
        screenWidth = GetScreenWidth();
        screenHeight = GetScreenHeight();

        // Обновление размеров кнопок
        panelWidth = fminf(520, screenWidth - 30);
        panelX = (screenWidth - panelWidth) / 2;
        panelY = (screenHeight - panelHeight) / 2;

        panel = (Rectangle){
            panelX, panelY, panelWidth, panelHeight
        };

        moveButtonsButton = (Rectangle){
            panelX + 25, panelY + 90,
            panelWidth - 50, 50
        };

        sizeButton = (Rectangle){
            panelX + 25, panelY + 155,
            panelWidth - 50, 50
        };

        slider = (Rectangle){
            panelX + 45, panelY + 255,
            panelWidth - 90, 12
        };

        resetButton = (Rectangle){
            panelX + 25, panelY + 335,
            (panelWidth - 65) / 2, 55
        };

        closeButton = (Rectangle){
            panelX + 40 + (panelWidth - 65) / 2,
            panelY + 335,
            (panelWidth - 65) / 2, 55
        };

        doneButton = (Rectangle){
            screenWidth / 2 - 100,
            screenHeight - 100,
            200, 60
        };

        // Количество касаний
        int touchCount = GetTouchPointCount();

        if (touchCount > MAX_TOUCHES)
            touchCount = MAX_TOUCHES;

        int currentTouchIds[MAX_TOUCHES];

        for (int i = 0; i < MAX_TOUCHES; i++)
            currentTouchIds[i] = -1;

        // Обработка новых касаний
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

            if (!newTouch)
                continue;

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

                    if (sqrtf(dx * dx + dy * dy) < joystickRadius + 40)
                    {
                        dragTarget = 1;
                        dragTouchId = id;

                        dragOffset = (Vector2){
                            joystickPosition.x - pos.x,
                            joystickPosition.y - pos.y
                        };
                    }
                    else if (CheckCollisionPointRec(pos, settingsButton))
                    {
                        dragTarget = 2;
                        dragTouchId = id;

                        dragOffset = (Vector2){
                            settingsButton.x - pos.x,
                            settingsButton.y - pos.y
                        };
                    }
                    else if (CheckCollisionPointRec(pos, cameraButton))
                    {
                        dragTarget = 3;
                        dragTouchId = id;

                        dragOffset = (Vector2){
                            cameraButton.x - pos.x,
                            cameraButton.y - pos.y
                        };
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

                    if (joystickRadius > 180)
                        joystickRadius = 90;

                    knobRadius = joystickRadius * 0.42f;
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

                    joystickPosition = (Vector2){
                        150, screenHeight - 160
                    };

                    settingsButton = (Rectangle){
                        screenWidth - 160, 20, 140, 55
                    };

                    cameraButton = (Rectangle){
                        screenWidth - 160, 85, 140, 55
                    };

                    lookSensitivity = 3.0f;
                }
                else if (CheckCollisionPointRec(pos, closeButton))
                {
                    settingsOpen = false;
                }
            }
            else
            {
                if (pos.x < screenWidth * 0.48f &&
                    pos.y > screenHeight * 0.25f)
                {
                    float dx = pos.x - joystickPosition.x;
                    float dy = pos.y - joystickPosition.y;

                    if (sqrtf(dx * dx + dy * dy) <
                        joystickRadius + 100)
                    {
                        joystickActive = true;
                        joystickTouchId = id;
                    }
                }
                else if (pos.x > screenWidth * 0.48f &&
                         pos.y > 155)
                {
                    lookTouchId = id;
                    previousLookPosition = pos;
                }
            }
        }

        // Перемещение кнопок
        if (editControls && dragTarget != 0)
        {
            bool found = false;

            for (int i = 0; i < touchCount; i++)
            {
                if (GetTouchPointId(i) == dragTouchId)
                {
                    Vector2 pos = GetTouchPosition(i);

                    float newX = pos.x + dragOffset.x;
                    float newY = pos.y + dragOffset.y;

                    if (dragTarget == 1)
                    {
                        joystickPosition = (Vector2){
                            newX, newY
                        };

                        joystickKnob = joystickPosition;
                    }
                    else if (dragTarget == 2)
                    {
                        settingsButton.x = newX;
                        settingsButton.y = newY;
                    }
                    else if (dragTarget == 3)
                    {
                        cameraButton.x = newX;
                        cameraButton.y = newY;
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

        // Работа джойстика
        if (joystickActive)
        {
            bool found = false;

            for (int i = 0; i < touchCount; i++)
            {
                if (GetTouchPointId(i) == joystickTouchId)
                {
                    Vector2 pos = GetTouchPosition(i);

                    float dx = pos.x - joystickPosition.x;
                    float dy = pos.y - joystickPosition.y;

                    float length = sqrtf(dx * dx + dy * dy);

                    if (length > joystickRadius)
                    {
                        dx = dx / length * joystickRadius;
                        dy = dy / length * joystickRadius;
                    }

                    joystickKnob = (Vector2){
                        joystickPosition.x + dx,
                        joystickPosition.y + dy
                    };

                    float moveX = dx / joystickRadius;

                    // ВАЖНО: исправлено направление движения.
                    // Палец вверх теперь двигает персонажа вперед.
                    float moveForward = -dy / joystickRadius;

                    player.x += (
                        cosf(playerAngle) * moveX +
                        sinf(playerAngle) * moveForward
                    ) * playerSpeed;

                    player.z += (
                        sinf(playerAngle) * moveX -
                        cosf(playerAngle) * moveForward
                    ) * playerSpeed;

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

        // Ползунок чувствительности
        if (draggingSlider)
        {
            bool found = false;

            for (int i = 0; i < touchCount; i++)
            {
                if (GetTouchPointId(i) == sliderTouchId)
                {
                    Vector2 pos = GetTouchPosition(i);

                    float value =
                        (pos.x - slider.x) / slider.width;

                    if (value < 0) value = 0;
                    if (value > 1) value = 1;

                    lookSensitivity = 0.1f + value * 9.9f;

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

        // Поворот камеры
        if (!settingsOpen && !editControls &&
            lookTouchId != -1)
        {
            bool found = false;

            for (int i = 0; i < touchCount; i++)
            {
                if (GetTouchPointId(i) == lookTouchId)
                {
                    Vector2 pos = GetTouchPosition(i);

                    float dx = pos.x - previousLookPosition.x;
                    float dy = pos.y - previousLookPosition.y;

                    float sensitivity = lookSensitivity * 0.004f;

                    playerAngle += dx * sensitivity;
                    cameraPitch -= dy * sensitivity;

                    if (cameraPitch > 1.2f)
                        cameraPitch = 1.2f;

                    if (cameraPitch < -1.2f)
                        cameraPitch = -1.2f;

                    previousLookPosition = pos;

                    found = true;
                    break;
                }
            }

            if (!found)
                lookTouchId = -1;
        }

        if (settingsOpen || editControls)
            lookTouchId = -1;

        // Запоминаем касания
        for (int i = 0; i < MAX_TOUCHES; i++)
            previousTouchIds[i] = currentTouchIds[i];

        // Камера
        float dirX = sinf(playerAngle) * cosf(cameraPitch);
        float dirY = sinf(cameraPitch);
        float dirZ = -cosf(playerAngle) * cosf(cameraPitch);

        if (firstPerson)
        {
            camera.position = (Vector3){
                player.x,
                player.y + 1.6f,
                player.z
            };

            camera.target = (Vector3){
                player.x + dirX * 10,
                player.y + 1.6f + dirY * 10,
                player.z + dirZ * 10
            };
        }
        else
        {
            camera.position = (Vector3){
                player.x - dirX * 7,
                player.y + 3.0f - dirY * 7,
                player.z - dirZ * 7
            };

            camera.target = (Vector3){
                player.x,
                player.y + 1.4f,
                player.z
            };
        }

        // Рисование
        BeginDrawing();

        ClearBackground((Color){135, 190, 235, 255});

        BeginMode3D(camera);

        DrawPlane(
            (Vector3){0, -0.05f, 0},
            (Vector2){200, 200},
            (Color){75, 145, 75, 255}
        );

        DrawGrid(40, 1.0f);

        // Деревья
        for (int x = -20; x <= 20; x += 8)
        {
            for (int z = -20; z <= 20; z += 8)
            {
                if (x == 0 && z == 0)
                    continue;

                DrawCylinder(
                    (Vector3){x, 1, z},
                    0.25f, 0.25f, 2,
                    8, BROWN
                );

                DrawSphere(
                    (Vector3){x, 2.8f, z},
                    1.2f, DARKGREEN
                );
            }
        }

        if (!firstPerson)
        {
            DrawCylinder(
                (Vector3){
                    player.x, player.y + 0.65f, player.z
                },
                0.4f, 0.4f, 1.3f,
                12, BLUE
            );

            DrawSphere(
                (Vector3){
                    player.x, player.y + 1.55f, player.z
                },
                0.35f, BEIGE
            );
        }

        EndMode3D();

        // Джойстик
        DrawCircleV(
            joystickPosition,
            joystickRadius,
            (Color){20, 20, 20, 100}
        );

        DrawCircleV(
            joystickKnob,
            knobRadius,
            (Color){220, 220, 220, 180}
        );

        // Кнопка настроек
        DrawRectangleRounded(
            settingsButton, 0.25f, 12,
            (Color){25, 30, 42, 230}
        );

        DrawText(
            "SETTINGS",
            settingsButton.x + 20,
            settingsButton.y + 18,
            20, RAYWHITE
        );

        // Кнопка вида камеры
        DrawRectangleRounded(
            cameraButton, 0.25f, 12,
            (Color){25, 30, 42, 230}
        );

        DrawText(
            firstPerson ? "1 PERSON" : "3 PERSON",
            cameraButton.x + 20,
            cameraButton.y + 18,
            20, RAYWHITE
        );

        // Меню настроек
        if (settingsOpen)
        {
            DrawRectangle(
                0, 0, screenWidth, screenHeight,
                (Color){0, 0, 0, 170}
            );

            DrawRectangleRounded(
                panel, 0.08f, 16,
                (Color){24, 29, 42, 255}
            );

            DrawText(
                "SETTINGS",
                panelX + 30, panelY + 25,
                30, RAYWHITE
            );

            DrawText(
                "Customize your game",
                panelX + 30, panelY + 60,
                16, GRAY
            );

            DrawRectangleRounded(
                moveButtonsButton, 0.25f, 12,
                (Color){45, 55, 75, 255}
            );

            DrawText(
                "MOVE BUTTONS",
                moveButtonsButton.x + 18,
                moveButtonsButton.y + 16,
                19, RAYWHITE
            );

            DrawRectangleRounded(
                sizeButton, 0.25f, 12,
                (Color){45, 55, 75, 255}
            );

            DrawText(
                "JOYSTICK SIZE",
                sizeButton.x + 18,
                sizeButton.y + 16,
                19, RAYWHITE
            );

            DrawText(
                "CAMERA SENSITIVITY",
                panelX + 30, panelY + 215,
                18, RAYWHITE
            );

            DrawText(
                TextFormat("%.1f", lookSensitivity),
                panelX + panelWidth - 85,
                panelY + 215,
                22, SKYBLUE
            );

            float sliderValue =
                (lookSensitivity - 0.1f) / 9.9f;

            DrawRectangleRounded(
                (Rectangle){
                    slider.x, slider.y - 5,
                    slider.width, 22
                },
                0.5f, 10,
                (Color){55, 65, 85, 255}
            );

            DrawRectangleRounded(
                (Rectangle){
                    slider.x, slider.y - 5,
                    slider.width * sliderValue, 22
                },
                0.5f, 10, SKYBLUE
            );

            DrawCircle(
                slider.x + slider.width * sliderValue,
                slider.y + 6,
                15, RAYWHITE
            );

            DrawText(
                "0.1",
                slider.x, slider.y + 28,
                16, GRAY
            );

            DrawText(
                "10",
                slider.x + slider.width - 20,
                slider.y + 28,
                16, GRAY
            );

            DrawRectangleRounded(
                resetButton, 0.25f, 12,
                (Color){45, 55, 75, 255}
            );

            DrawText(
                "RESET",
                resetButton.x + resetButton.width / 2 - 35,
                resetButton.y + 18,
                19, RAYWHITE
            );

            DrawRectangleRounded(
                closeButton, 0.25f, 12,
                (Color){35, 125, 220, 255}
            );

            DrawText(
                "CLOSE",
                closeButton.x + closeButton.width / 2 - 35,
                closeButton.y + 18,
                19, RAYWHITE
            );
        }

        // Режим перемещения кнопок
        if (editControls)
        {
            DrawRectangle(
                0, 0, screenWidth, screenHeight,
                (Color){0, 0, 0, 130}
            );

            DrawText(
                "MOVE YOUR CONTROLS",
                screenWidth / 2 - 145,
                30, 22, RAYWHITE
            );

            DrawRectangleLinesEx(
                settingsButton, 3, SKYBLUE
            );

            DrawRectangleLinesEx(
                cameraButton, 3, SKYBLUE
            );

            DrawCircleLines(
                joystickPosition.x,
                joystickPosition.y,
                joystickRadius + 5,
                SKYBLUE
            );

            DrawRectangleRounded(
                doneButton, 0.25f, 12,
                (Color){35, 125, 220, 255}
            );

            DrawText(
                "DONE",
                doneButton.x + 65,
                doneButton.y + 19,
                22, RAYWHITE
            );
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}

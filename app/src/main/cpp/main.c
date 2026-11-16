#include "raymob.h"
#include <math.h>

int main(void)
{
    InitWindow(0, 0, "My 3D Game");
    SetTargetFPS(60);

    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    Vector3 player = { 0.0f, 0.0f, 0.0f };

    float playerAngle = 0.0f;
    float cameraPitch = 0.0f;

    // Чувствительность камеры
    float lookSensitivity = 0.004f;

    Camera3D camera = { 0 };
    camera.up = (Vector3){ 0, 1, 0 };
    camera.fovy = 65.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    bool firstPerson = false;

    // Джойстик
    float joystickRadius = 120.0f;
    float knobRadius = 50.0f;

    Vector2 joystickPosition = {
        150.0f,
        screenHeight - 160.0f
    };

    Vector2 joystickKnob = { 0, 0 };

    bool joystickActive = false;
    int joystickTouchId = -1;

    // Настройки
    bool settingsOpen = false;
    bool editJoystick = false;
    bool draggingJoystick = false;

    bool pointerWasDown = false;

    // Поворот камеры
    int lookTouchId = -1;
    Vector2 previousLookPosition = { 0, 0 };

    float playerSpeed = 0.08f;

    while (!WindowShouldClose())
    {
        screenWidth = GetScreenWidth();
        screenHeight = GetScreenHeight();

        // Кнопки интерфейса
        Rectangle settingsButton = {
            screenWidth - 160, 30, 130, 60
        };

        Rectangle cameraButton = {
            screenWidth - 160, 105, 130, 60
        };

        Rectangle moveButton = {
            screenWidth / 2 - 140,
            screenHeight / 2 - 150,
            280, 45
        };

        Rectangle sizeButton = {
            screenWidth / 2 - 140,
            screenHeight / 2 - 98,
            280, 45
        };

        Rectangle sensitivityDownButton = {
            screenWidth / 2 - 140,
            screenHeight / 2 - 46,
            280, 45
        };

        Rectangle sensitivityUpButton = {
            screenWidth / 2 - 140,
            screenHeight / 2 + 6,
            280, 45
        };

        Rectangle resetButton = {
            screenWidth / 2 - 140,
            screenHeight / 2 + 58,
            280, 45
        };

        Rectangle closeButton = {
            screenWidth / 2 - 140,
            screenHeight / 2 + 110,
            280, 45
        };

        Rectangle doneButton = {
            screenWidth / 2 - 100,
            screenHeight - 100,
            200, 55
        };

        bool touching = GetTouchPointCount() > 0;

        Vector2 pointer = { 0, 0 };

        if (touching)
        {
            pointer = GetTouchPosition(0);
        }

        bool pointerPressed = touching && !pointerWasDown;

        // Обработка нажатий на кнопки
        if (pointerPressed)
        {
            if (editJoystick)
            {
                if (CheckCollisionPointRec(pointer, doneButton))
                {
                    editJoystick = false;
                    draggingJoystick = false;
                }
                else
                {
                    float dx = pointer.x - joystickPosition.x;
                    float dy = pointer.y - joystickPosition.y;

                    if (sqrtf(dx * dx + dy * dy) <
                        joystickRadius + 60.0f)
                    {
                        draggingJoystick = true;
                    }
                }
            }
            else if (CheckCollisionPointRec(pointer, cameraButton))
            {
                firstPerson = !firstPerson;
            }
            else if (CheckCollisionPointRec(pointer, settingsButton))
            {
                settingsOpen = !settingsOpen;

                joystickActive = false;
                joystickTouchId = -1;
            }
            else if (settingsOpen)
            {
                if (CheckCollisionPointRec(pointer, moveButton))
                {
                    editJoystick = true;
                }
                else if (CheckCollisionPointRec(pointer, sizeButton))
                {
                    joystickRadius += 20.0f;
                    knobRadius += 7.0f;

                    if (joystickRadius > 200.0f)
                    {
                        joystickRadius = 80.0f;
                        knobRadius = 35.0f;
                    }
                }
                else if (CheckCollisionPointRec(
                    pointer, sensitivityDownButton))
                {
                    lookSensitivity -= 0.001f;

                    if (lookSensitivity < 0.001f)
                        lookSensitivity = 0.001f;
                }
                else if (CheckCollisionPointRec(
                    pointer, sensitivityUpButton))
                {
                    lookSensitivity += 0.001f;

                    if (lookSensitivity > 0.015f)
                        lookSensitivity = 0.015f;
                }
                else if (CheckCollisionPointRec(pointer, resetButton))
                {
                    joystickRadius = 120.0f;
                    knobRadius = 50.0f;

                    joystickPosition.x = 150.0f;
                    joystickPosition.y = screenHeight - 160.0f;

                    lookSensitivity = 0.004f;
                }
                else if (CheckCollisionPointRec(pointer, closeButton))
                {
                    settingsOpen = false;
                }
            }
            else
            {
                float dx = pointer.x - joystickPosition.x;
                float dy = pointer.y - joystickPosition.y;

                if (sqrtf(dx * dx + dy * dy) <
                    joystickRadius + 80.0f)
                {
                    joystickActive = true;
                    joystickTouchId = GetTouchPointId(0);
                }
            }
        }

        // Перемещение джойстика в режиме настройки
        if (editJoystick && touching && draggingJoystick)
        {
            joystickPosition.x = pointer.x;
            joystickPosition.y = pointer.y;
        }

        if (!touching)
        {
            draggingJoystick = false;
        }

        // Поиск пальца, управляющего джойстиком
        bool joystickFound = false;

        if (joystickActive && !settingsOpen && !editJoystick)
        {
            for (int i = 0; i < GetTouchPointCount(); i++)
            {
                if (GetTouchPointId(i) == joystickTouchId)
                {
                    Vector2 touchPosition = GetTouchPosition(i);

                    float dx = touchPosition.x - joystickPosition.x;
                    float dy = touchPosition.y - joystickPosition.y;

                    float distance = sqrtf(dx * dx + dy * dy);

                    if (distance > joystickRadius)
                    {
                        dx = dx / distance * joystickRadius;
                        dy = dy / distance * joystickRadius;
                    }

                    joystickKnob.x = dx;
                    joystickKnob.y = dy;

                    float forward = -dy / joystickRadius;
                    float sideways = dx / joystickRadius;

                    player.x += (
                        sideways * cosf(playerAngle) +
                        forward * sinf(playerAngle)
                    ) * playerSpeed;

                    player.z += (
                        -sideways * sinf(playerAngle) -
                        forward * cosf(playerAngle)
                    ) * playerSpeed;

                    joystickFound = true;
                    break;
                }
            }

            if (!joystickFound)
            {
                joystickActive = false;
                joystickTouchId = -1;
                joystickKnob = (Vector2){ 0, 0 };
            }
        }

        // Поворот камеры правой стороной экрана
        bool lookFound = false;

        if (!settingsOpen && !editJoystick)
        {
            // Продолжаем движение уже выбранного пальца
            if (lookTouchId != -1)
            {
                for (int i = 0; i < GetTouchPointCount(); i++)
                {
                    if (GetTouchPointId(i) == lookTouchId)
                    {
                        Vector2 currentPosition =
                            GetTouchPosition(i);

                        float dx = currentPosition.x -
                                   previousLookPosition.x;

                        float dy = currentPosition.y -
                                   previousLookPosition.y;

                        playerAngle += dx * lookSensitivity;
                        cameraPitch -= dy * lookSensitivity;

                        previousLookPosition = currentPosition;

                        lookFound = true;
                        break;
                    }
                }
            }

            // Если палец отпущен, ищем новый справа
            if (!lookFound)
            {
                lookTouchId = -1;

                for (int i = 0; i < GetTouchPointCount(); i++)
                {
                    Vector2 pos = GetTouchPosition(i);

                    if (pos.x > screenWidth * 0.5f &&
                        pos.y > 180.0f)
                    {
                        lookTouchId = GetTouchPointId(i);
                        previousLookPosition = pos;
                        lookFound = true;
                        break;
                    }
                }
            }
        }
        else
        {
            lookTouchId = -1;
        }

        // Ограничение наклона камеры
        if (cameraPitch > 1.2f)
            cameraPitch = 1.2f;

        if (cameraPitch < -1.2f)
            cameraPitch = -1.2f;

        pointerWasDown = touching;

        // Направление камеры
        float dirX = sinf(playerAngle) * cosf(cameraPitch);
        float dirY = sinf(cameraPitch);
        float dirZ = -cosf(playerAngle) * cosf(cameraPitch);

        // Камера от первого лица
        if (firstPerson)
        {
            camera.position = (Vector3){
                player.x,
                1.7f,
                player.z
            };

            camera.target = (Vector3){
                player.x + dirX,
                1.7f + dirY,
                player.z + dirZ
            };
        }
        else
        {
            camera.position = (Vector3){
                player.x - dirX * 7.0f,
                3.0f - dirY * 7.0f,
                player.z - dirZ * 7.0f
            };

            camera.target = (Vector3){
                player.x,
                1.4f,
                player.z
            };
        }

        // Рисование
        BeginDrawing();

        ClearBackground(SKYBLUE);

        BeginMode3D(camera);

        DrawPlane(
            (Vector3){ 0, 0, 0 },
            (Vector2){ 100, 100 },
            GREEN
        );

        DrawGrid(40, 1.0f);

        // Игрок
        if (!firstPerson)
        {
            DrawCylinder(
                (Vector3){ player.x, 0.9f, player.z },
                0.35f, 0.35f, 1.3f,
                12, BLUE
            );

            DrawSphere(
                (Vector3){ player.x, 1.8f, player.z },
                0.35f, BEIGE
            );
        }

        // Деревья
        for (int i = -5; i <= 5; i++)
        {
            DrawCylinder(
                (Vector3){ i * 3.0f, 1.0f, -6.0f },
                0.2f, 0.2f, 2.0f,
                8, BROWN
            );

            DrawSphere(
                (Vector3){ i * 3.0f, 2.5f, -6.0f },
                1.0f, DARKGREEN
            );
        }

        EndMode3D();

        // Кнопка настроек
        DrawRectangleRounded(
            settingsButton, 0.2f, 8, DARKGRAY
        );

        DrawText(
            "SETTINGS",
            settingsButton.x + 10,
            settingsButton.y + 20,
            20, WHITE
        );

        // Переключение камеры
        DrawRectangleRounded(
            cameraButton, 0.2f, 8, DARKBLUE
        );

        DrawText(
            firstPerson ? "1 PERSON" : "3 PERSON",
            cameraButton.x + 10,
            cameraButton.y + 20,
            20, WHITE
        );

        // Джойстик
        if (!settingsOpen || editJoystick)
        {
            DrawCircleV(
                joystickPosition,
                joystickRadius,
                Fade(DARKGRAY, 0.65f)
            );

            DrawCircleV(
                (Vector2){
                    joystickPosition.x + joystickKnob.x,
                    joystickPosition.y + joystickKnob.y
                },
                knobRadius,
                LIGHTGRAY
            );
        }

        // Меню настроек
        if (settingsOpen && !editJoystick)
        {
            DrawRectangle(
                screenWidth / 2 - 165,
                screenHeight / 2 - 200,
                330, 410,
                Fade(BLACK, 0.88f)
            );

            DrawText(
                "SETTINGS",
                screenWidth / 2 - 75,
                screenHeight / 2 - 185,
                28, WHITE
            );

            DrawRectangleRounded(moveButton, 0.2f, 8, BLUE);

            DrawText(
                "MOVE JOYSTICK",
                moveButton.x + 65,
                moveButton.y + 12,
                20, WHITE
            );

            DrawRectangleRounded(sizeButton, 0.2f, 8, DARKGREEN);

            DrawText(
                "CHANGE SIZE",
                sizeButton.x + 75,
                sizeButton.y + 12,
                20, WHITE
            );

            DrawRectangleRounded(
                sensitivityDownButton, 0.2f, 8, DARKBLUE
            );

            DrawText(
                "SENSITIVITY -",
                sensitivityDownButton.x + 75,
                sensitivityDownButton.y + 12,
                20, WHITE
            );

            DrawRectangleRounded(
                sensitivityUpButton, 0.2f, 8, DARKBLUE
            );

            DrawText(
                "SENSITIVITY +",
                sensitivityUpButton.x + 75,
                sensitivityUpButton.y + 12,
                20, WHITE
            );

            DrawText(
                TextFormat("VALUE: %.3f", lookSensitivity),
                screenWidth / 2 - 65,
                screenHeight / 2 + 55,
                18, WHITE
            );

            DrawRectangleRounded(resetButton, 0.2f, 8, ORANGE);

            DrawText(
                "RESET",
                resetButton.x + 115,
                resetButton.y + 12,
                20, WHITE
            );

            DrawRectangleRounded(closeButton, 0.2f, 8, RED);

            DrawText(
                "CLOSE",
                closeButton.x + 115,
                closeButton.y + 12,
                20, WHITE
            );
        }

        // Окно перемещения джойстика
        if (editJoystick)
        {
            DrawRectangle(
                0, 0,
                screenWidth,
                screenHeight,
                Fade(BLACK, 0.55f)
            );

            DrawRectangleRounded(
                (Rectangle){
                    screenWidth / 2 - 200,
                    screenHeight / 2 - 130,
                    400, 280
                },
                0.15f, 8,
                Fade(DARKGRAY, 0.95f)
            );

            DrawText(
                "MOVE JOYSTICK",
                screenWidth / 2 - 130,
                screenHeight / 2 - 110,
                25, WHITE
            );

            DrawText(
                "DRAG THE JOYSTICK",
                screenWidth / 2 - 125,
                screenHeight / 2 - 65,
                20, LIGHTGRAY
            );

            DrawRectangleRounded(
                doneButton, 0.2f, 8, DARKGREEN
            );

            DrawText(
                "DONE",
                doneButton.x + 70,
                doneButton.y + 17,
                22, WHITE
            );
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}

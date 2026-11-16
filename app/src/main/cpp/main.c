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

    Camera3D camera = { 0 };
    camera.up = (Vector3){ 0, 1, 0 };
    camera.fovy = 65.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    bool firstPerson = false;

    float joystickRadius = 120.0f;
    float knobRadius = 50.0f;

    Vector2 joystickPosition = {
        150.0f, screenHeight - 160.0f
    };

    Vector2 joystickKnob = { 0, 0 };

    bool joystickActive = false;
    bool settingsOpen = false;
    bool editJoystick = false;
    bool draggingJoystick = false;
    bool pointerWasDown = false;

    float playerSpeed = 0.08f;

    while (!WindowShouldClose())
    {
        screenWidth = GetScreenWidth();
        screenHeight = GetScreenHeight();

        Rectangle settingsButton = {
            screenWidth - 160, 30, 130, 60
        };

        Rectangle cameraButton = {
            screenWidth - 160, 105, 130, 60
        };

        Rectangle moveButton = {
            screenWidth / 2 - 150,
            screenHeight / 2 - 100,
            300, 55
        };

        Rectangle sizeButton = {
            screenWidth / 2 - 150,
            screenHeight / 2 - 30,
            300, 55
        };

        Rectangle resetButton = {
            screenWidth / 2 - 150,
            screenHeight / 2 + 40,
            300, 55
        };

        Rectangle closeButton = {
            screenWidth / 2 - 150,
            screenHeight / 2 + 110,
            300, 55
        };

        Rectangle doneButton = {
            screenWidth / 2 - 100,
            screenHeight / 2 + 100,
            200, 55
        };

        bool touching = GetTouchPointCount() > 0;
        Vector2 pointer = { 0, 0 };

        if (touching)
            pointer = GetTouchPosition(0);

        bool pointerPressed = touching && !pointerWasDown;

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

                    if (sqrtf(dx * dx + dy * dy) < joystickRadius + 50)
                        draggingJoystick = true;
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
            }
            else if (settingsOpen)
            {
                if (CheckCollisionPointRec(pointer, moveButton))
                {
                    editJoystick = true;
                }
                else if (CheckCollisionPointRec(pointer, sizeButton))
                {
                    joystickRadius += 20;
                    knobRadius += 7;

                    if (joystickRadius > 200)
                    {
                        joystickRadius = 80;
                        knobRadius = 35;
                    }
                }
                else if (CheckCollisionPointRec(pointer, resetButton))
                {
                    joystickRadius = 120;
                    knobRadius = 50;

                    joystickPosition.x = 150;
                    joystickPosition.y = screenHeight - 160;
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

                if (sqrtf(dx * dx + dy * dy) < joystickRadius + 80)
                    joystickActive = true;
            }
        }

        if (editJoystick && touching && draggingJoystick)
        {
            joystickPosition.x = pointer.x;
            joystickPosition.y = pointer.y;
        }

        if (!touching)
        {
            draggingJoystick = false;

            if (!editJoystick)
            {
                joystickActive = false;
                joystickKnob = (Vector2){ 0, 0 };
            }
        }

        if (touching && joystickActive &&
            !settingsOpen && !editJoystick)
        {
            float dx = pointer.x - joystickPosition.x;
            float dy = pointer.y - joystickPosition.y;

            float distance = sqrtf(dx * dx + dy * dy);

            if (distance > joystickRadius)
            {
                dx = dx / distance * joystickRadius;
                dy = dy / distance * joystickRadius;
            }

            joystickKnob.x = dx;
            joystickKnob.y = dy;

            float forward = dy / joystickRadius;
            float sideways = dx / joystickRadius;

            player.x += (sideways * cosf(playerAngle) +
                         forward * sinf(playerAngle)) * playerSpeed;

            player.z += (forward * cosf(playerAngle) -
                         sideways * sinf(playerAngle)) * playerSpeed;
        }

        pointerWasDown = touching;

        // Камера от первого или третьего лица
        if (firstPerson)
        {
            camera.position = (Vector3){
                player.x,
                1.7f,
                player.z
            };

            camera.target = (Vector3){
                player.x + sinf(playerAngle),
                1.7f,
                player.z - cosf(playerAngle)
            };
        }
        else
        {
            camera.position = (Vector3){
                player.x - sinf(playerAngle) * 7.0f,
                4.0f,
                player.z + cosf(playerAngle) * 7.0f
            };

            camera.target = (Vector3){
                player.x,
                1.0f,
                player.z
            };
        }

        BeginDrawing();

        ClearBackground(SKYBLUE);

        BeginMode3D(camera);

        DrawPlane(
            (Vector3){ 0, 0, 0 },
            (Vector2){ 100, 100 },
            GREEN
        );

        DrawGrid(40, 1.0f);

        // Игрок виден только от третьего лица
        if (!firstPerson)
        {
            DrawCylinder(
                (Vector3){ player.x, 0.9f, player.z },
                0.35f, 0.35f, 1.3f, 12, BLUE
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
                0.2f, 0.2f, 2.0f, 8, BROWN
            );

            DrawSphere(
                (Vector3){ i * 3.0f, 2.5f, -6.0f },
                1.0f, DARKGREEN
            );
        }

        EndMode3D();

        // Настройки
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
                screenWidth / 2 - 180,
                screenHeight / 2 - 180,
                360, 370,
                Fade(BLACK, 0.85f)
            );

            DrawText(
                "SETTINGS",
                screenWidth / 2 - 75,
                screenHeight / 2 - 160,
                30, WHITE
            );

            DrawRectangleRounded(moveButton, 0.2f, 8, BLUE);
            DrawText("MOVE JOYSTICK",
                moveButton.x + 65, moveButton.y + 17, 22, WHITE);

            DrawRectangleRounded(sizeButton, 0.2f, 8, DARKGREEN);
            DrawText("CHANGE SIZE",
                sizeButton.x + 70, sizeButton.y + 17, 22, WHITE);

            DrawRectangleRounded(resetButton, 0.2f, 8, ORANGE);
            DrawText("RESET",
                resetButton.x + 115, resetButton.y + 17, 22, WHITE);

            DrawRectangleRounded(closeButton, 0.2f, 8, RED);
            DrawText("CLOSE",
                closeButton.x + 115, closeButton.y + 17, 22, WHITE);
        }

        // Окно перемещения джойстика
        if (editJoystick)
        {
            DrawRectangle(
                0, 0, screenWidth, screenHeight,
                Fade(BLACK, 0.55f)
            );

            DrawRectangleRounded(
                (Rectangle){
                    screenWidth / 2 - 200,
                    screenHeight / 2 - 130,
                    400, 280
                },
                0.15f, 8, Fade(DARKGRAY, 0.95f)
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

            DrawRectangleRounded(doneButton, 0.2f, 8, DARKGREEN);

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

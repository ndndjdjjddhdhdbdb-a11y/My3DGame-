#include "raymob.h"
#include <math.h>

int main(void)
{
    InitWindow(0, 0, "My 3D Game");
    SetTargetFPS(60);

    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    Vector3 player = { 0.0f, 0.0f, 0.0f };

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 5.0f, 8.0f };
    camera.target = player;
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float joystickRadius = 120.0f;
    float knobRadius = 50.0f;

    Vector2 joystickPosition = {
        150.0f,
        screenHeight - 160.0f
    };

    Vector2 joystickKnob = { 0.0f, 0.0f };

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
        {
            pointer = GetTouchPosition(0);
        }

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

                    if (sqrtf(dx * dx + dy * dy) < joystickRadius + 50.0f)
                    {
                        draggingJoystick = true;
                    }
                }
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
                    joystickRadius += 20.0f;
                    knobRadius += 7.0f;

                    if (joystickRadius > 200.0f)
                    {
                        joystickRadius = 80.0f;
                        knobRadius = 35.0f;
                    }
                }
                else if (CheckCollisionPointRec(pointer, resetButton))
                {
                    joystickRadius = 120.0f;
                    knobRadius = 50.0f;

                    joystickPosition.x = 150.0f;
                    joystickPosition.y = screenHeight - 160.0f;
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

                if (sqrtf(dx * dx + dy * dy) < joystickRadius + 80.0f)
                {
                    joystickActive = true;
                }
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

            player.x += dx / joystickRadius * playerSpeed;
            player.z += dy / joystickRadius * playerSpeed;
        }

        pointerWasDown = touching;

        camera.target = (Vector3){
            player.x, 1.0f, player.z
        };

        camera.position = (Vector3){
            player.x,
            5.0f,
            player.z + 8.0f
        };

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
        DrawCylinder(
            (Vector3){ player.x, 0.9f, player.z },
            0.35f, 0.35f, 1.3f, 12, BLUE
        );

        DrawSphere(
            (Vector3){ player.x, 1.8f, player.z },
            0.35f, BEIGE
        );

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

        // Кнопка настроек
        DrawRectangleRounded(
            settingsButton, 0.2f, 8, DARKGRAY
        );

        DrawText(
            "SETTINGS",
            settingsButton.x + 10,
            settingsButton.y + 20,
            20,
            WHITE
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

        // Обычные настройки
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
            DrawText(
                "MOVE JOYSTICK",
                moveButton.x + 65,
                moveButton.y + 17,
                22, WHITE
            );

            DrawRectangleRounded(sizeButton, 0.2f, 8, DARKGREEN);
            DrawText(
                "CHANGE SIZE",
                sizeButton.x + 70,
                sizeButton.y + 17,
                22, WHITE
            );

            DrawRectangleRounded(resetButton, 0.2f, 8, ORANGE);
            DrawText(
                "RESET",
                resetButton.x + 115,
                resetButton.y + 17,
                22, WHITE
            );

            DrawRectangleRounded(closeButton, 0.2f, 8, RED);
            DrawText(
                "CLOSE",
                closeButton.x + 115,
                closeButton.y + 17,
                22, WHITE
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
                doneButton,
                0.2f, 8, DARKGREEN
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

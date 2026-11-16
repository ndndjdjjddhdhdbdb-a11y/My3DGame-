#include "raymob.h"
#include <math.h>

int main(void)
{
    InitWindow(0, 0, "My 3D World");
    SetTargetFPS(60);

    Camera3D camera = { 0 };

    camera.position = (Vector3){ 8, 8, 8 };
    camera.target = (Vector3){ 0, 1, 0 };
    camera.up = (Vector3){ 0, 1, 0 };
    camera.fovy = 45;
    camera.projection = CAMERA_PERSPECTIVE;

    Vector3 player = { 0, 1, 0 };

    // Настройки джойстика
    Vector2 joystickCenter = { 170, 0 };
    Vector2 joystickKnob = { 0, 0 };

    float joystickRadius = 160;

    // Меню
    bool settingsOpen = false;
    bool editJoystick = false;
    bool joystickActive = false;
    bool editDragging = false;

    bool pointerWasDown = false;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        // Начальное положение джойстика
        if (joystickCenter.y == 0)
        {
            joystickCenter.y = screenHeight - 170;
        }

        // Получение касания
        bool pointerDown = false;
        Vector2 pointer = { 0, 0 };

        if (GetTouchPointCount() > 0)
        {
            pointerDown = true;
            pointer = GetTouchPosition(0);
        }
        else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            pointerDown = true;
            pointer = GetMousePosition();
        }

        bool pointerPressed = pointerDown && !pointerWasDown;
        bool pointerReleased = !pointerDown && pointerWasDown;

        pointerWasDown = pointerDown;

        // Кнопка настроек
        Rectangle settingsButton = {
            screenWidth - 115,
            20,
            95,
            65
        };

        // Кнопки меню
        Rectangle moveButton = {
            screenWidth / 2 - 170,
            screenHeight / 2 - 70,
            340,
            65
        };

        Rectangle sizeButton = {
            screenWidth / 2 - 170,
            screenHeight / 2 + 10,
            340,
            65
        };

        Rectangle resetButton = {
            screenWidth / 2 - 170,
            screenHeight / 2 + 90,
            340,
            65
        };

        // Обработка нажатий
        if (pointerPressed)
        {
            if (CheckCollisionPointRec(pointer, settingsButton))
            {
                settingsOpen = !settingsOpen;
                editJoystick = false;
                joystickActive = false;
                editDragging = false;
                joystickKnob = (Vector2){ 0, 0 };
            }
            else if (settingsOpen)
            {
                if (CheckCollisionPointRec(pointer, moveButton))
                {
                    editJoystick = !editJoystick;
                    joystickActive = false;
                    joystickKnob = (Vector2){ 0, 0 };
                }
                else if (CheckCollisionPointRec(pointer, sizeButton))
                {
                    joystickRadius += 20;

                    if (joystickRadius > 240)
                        joystickRadius = 80;
                }
                else if (CheckCollisionPointRec(pointer, resetButton))
                {
                    joystickRadius = 160;

                    joystickCenter.x = 170;
                    joystickCenter.y = screenHeight - 170;

                    editJoystick = false;
                }
            }
            else
            {
                float dx = pointer.x - joystickCenter.x;
                float dy = pointer.y - joystickCenter.y;

                float distance = sqrtf(dx * dx + dy * dy);

                if (editJoystick)
                {
                    editDragging = true;
                }
                else if (distance < joystickRadius * 2)
                {
                    joystickActive = true;
                }
            }
        }

        // Перемещение джойстика в режиме настроек
        if (pointerDown && editJoystick && editDragging)
        {
            joystickCenter.x = pointer.x;
            joystickCenter.y = pointer.y;

            if (joystickCenter.x < joystickRadius + 10)
                joystickCenter.x = joystickRadius + 10;

            if (joystickCenter.x > screenWidth - joystickRadius - 10)
                joystickCenter.x = screenWidth - joystickRadius - 10;

            if (joystickCenter.y < joystickRadius + 10)
                joystickCenter.y = joystickRadius + 10;

            if (joystickCenter.y > screenHeight - joystickRadius - 10)
                joystickCenter.y = screenHeight - joystickRadius - 10;
        }

        // Управление персонажем
        if (pointerDown && joystickActive && !settingsOpen)
        {
            float dx = pointer.x - joystickCenter.x;
            float dy = pointer.y - joystickCenter.y;

            float length = sqrtf(dx * dx + dy * dy);

            if (length > joystickRadius)
            {
                dx *= joystickRadius / length;
                dy *= joystickRadius / length;
            }

            joystickKnob = (Vector2){ dx, dy };
        }

        // Отпускание пальца
        if (pointerReleased)
        {
            joystickActive = false;
            editDragging = false;

            joystickKnob = (Vector2){ 0, 0 };
        }

        // Если открыты настройки, персонаж не двигается
        if (settingsOpen)
        {
            joystickActive = false;
            joystickKnob = (Vector2){ 0, 0 };
        }

        // Движение игрока
        if (!settingsOpen && !editJoystick)
        {
            player.x += joystickKnob.x / joystickRadius * 5.0f * dt;
            player.z += joystickKnob.y / joystickRadius * 5.0f * dt;
        }

        // Камера следует за игроком
        camera.position = (Vector3){
            player.x + 8,
            player.y + 7,
            player.z + 8
        };

        camera.target = (Vector3){
            player.x,
            player.y,
            player.z
        };

        // Рисование
        BeginDrawing();

        ClearBackground(SKYBLUE);

        BeginMode3D(camera);

        DrawPlane(
            (Vector3){ 0, 0, 0 },
            (Vector2){ 100, 100 },
            GREEN
        );

        DrawGrid(50, 1);

        // Игрок
        DrawCube(player, 1, 2, 1, BLUE);

        DrawSphere(
            (Vector3){ player.x, 2.3f, player.z },
            0.5f,
            BEIGE
        );

        // Деревья
        for (int x = -15; x <= 15; x += 5)
        {
            for (int z = -15; z <= 15; z += 5)
            {
                if (x == 0 && z == 0)
                    continue;

                DrawCylinder(
                    (Vector3){ x, 1, z },
                    0.3f,
                    0.3f,
                    2,
                    8,
                    BROWN
                );

                DrawSphere(
                    (Vector3){ x, 3, z },
                    1.5f,
                    DARKGREEN
                );
            }
        }

        EndMode3D();

        // Название игры
        DrawText("MY 3D WORLD", 20, 20, 25, WHITE);

        // Кнопка настроек
        DrawRectangleRounded(
            settingsButton,
            0.3f,
            8,
            DARKGRAY
        );

        DrawText(
            "SETTINGS",
            settingsButton.x + 8,
            settingsButton.y + 23,
            19,
            WHITE
        );

        // Джойстик
        DrawCircleV(
            joystickCenter,
            joystickRadius,
            Fade(DARKGRAY, 0.65f)
        );

        DrawCircleV(
            (Vector2){
                joystickCenter.x + joystickKnob.x,
                joystickCenter.y + joystickKnob.y
            },
            joystickRadius * 0.4f,
            WHITE
        );

        // Режим перемещения
        if (editJoystick)
        {
            DrawText(
                "MOVE JOYSTICK",
                20,
                65,
                22,
                YELLOW
            );
        }

        // Меню настроек
        if (settingsOpen)
        {
            DrawRectangle(
                0,
                0,
                screenWidth,
                screenHeight,
                Fade(BLACK, 0.7f)
            );

            DrawText(
                "SETTINGS",
                screenWidth / 2 - 95,
                screenHeight / 2 - 160,
                35,
                WHITE
            );

            // Перемещение
            DrawRectangleRounded(
                moveButton,
                0.2f,
                8,
                editJoystick ? DARKGREEN : DARKGRAY
            );

            DrawText(
                editJoystick ? "FINISH MOVING" : "MOVE JOYSTICK",
                moveButton.x + 35,
                moveButton.y + 20,
                25,
                WHITE
            );

            // Размер
            DrawRectangleRounded(
                sizeButton,
                0.2f,
                8,
                DARKGRAY
            );

            DrawText(
                "CHANGE SIZE",
                sizeButton.x + 65,
                sizeButton.y + 20,
                25,
                WHITE
            );

            // Сброс
            DrawRectangleRounded(
                resetButton,
                0.2f,
                8,
                MAROON
            );

            DrawText(
                "RESET SETTINGS",
                resetButton.x + 55,
                resetButton.y + 20,
                25,
                WHITE
            );
        }

        EndDrawing(DrawRectangleRounded(
    closeButton,
    0.2f,
    8,
    RED
);

DrawText(
    "CLOSE",
    closeButton.x + 115,
    closeButton.y + 20,
    25,
    WHITE
););
    }

    CloseWindow();

    return 0;
}

#include "raymob.h"
#include <math.h>

#define MAX_TOUCHES 10

int main(void)
{
    InitWindow(0, 0, "My 3D Game");
    SetTargetFPS(60);

    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    Vector3 player = { 0.0f, 0.0f, 0.0f };

    float playerAngle = 0.0f;
    float cameraPitch = 0.0f;

    // Чувствительность камеры: от 0.1 до 10
    float lookSensitivity = 3.0f;

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

    Vector2 joystickKnob = joystickPosition;

    bool joystickActive = false;
    int joystickTouchId = -1;

    // Настройки
    bool settingsOpen = false;
    bool editJoystick = false;
    bool draggingJoystick = false;
    bool draggingSensitivity = false;

    int sensitivityTouchId = -1;
    int lookTouchId = -1;

    Vector2 previousLookPosition = { 0, 0 };

    float playerSpeed = 0.08f;

    // Кнопки интерфейса
    Rectangle settingsButton = {
        screenWidth - 160, 20, 140, 55
    };

    Rectangle cameraButton = {
        screenWidth - 160, 85, 140, 55
    };

    // Панель настроек
    float panelWidth = fminf(520.0f, screenWidth - 30.0f);
    float panelHeight = 440.0f;

    float panelX = (screenWidth - panelWidth) / 2.0f;
    float panelY = (screenHeight - panelHeight) / 2.0f;

    Rectangle panel = {
        panelX, panelY, panelWidth, panelHeight
    };

    Rectangle moveButton = {
        panelX + 25, panelY + 90,
        panelWidth - 50, 50
    };

    Rectangle sizeButton = {
        panelX + 25, panelY + 155,
        panelWidth - 50, 50
    };

    Rectangle sensitivitySlider = {
        panelX + 45, panelY + 255,
        panelWidth - 90, 12
    };

    Rectangle resetButton = {
        panelX + 25, panelY + 335,
        (panelWidth - 65) / 2, 55
    };

    Rectangle closeButton = {
        panelX + 40 + (panelWidth - 65) / 2,
        panelY + 335,
        (panelWidth - 65) / 2, 55
    };

    Rectangle doneButton = {
        screenWidth / 2.0f - 100,
        screenHeight - 100,
        200, 60
    };

    // Запоминаем предыдущие касания
    int previousTouchIds[MAX_TOUCHES];

    for (int i = 0; i < MAX_TOUCHES; i++)
        previousTouchIds[i] = -1;

    while (!WindowShouldClose())
    {
        screenWidth = GetScreenWidth();
        screenHeight = GetScreenHeight();

        // Обновляем размеры интерфейса
        settingsButton = (Rectangle){
            screenWidth - 160, 20, 140, 55
        };

        cameraButton = (Rectangle){
            screenWidth - 160, 85, 140, 55
        };

        panelWidth = fminf(520.0f, screenWidth - 30.0f);
        panelX = (screenWidth - panelWidth) / 2.0f;
        panelY = (screenHeight - panelHeight) / 2.0f;

        panel = (Rectangle){
            panelX, panelY, panelWidth, panelHeight
        };

        moveButton = (Rectangle){
            panelX + 25, panelY + 90,
            panelWidth - 50, 50
        };

        sizeButton = (Rectangle){
            panelX + 25, panelY + 155,
            panelWidth - 50, 50
        };

        sensitivitySlider = (Rectangle){
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
            screenWidth / 2.0f - 100,
            screenHeight - 100,
            200, 60
        };

        int touchCount = GetTouchPointCount();

        if (touchCount > MAX_TOUCHES)
            touchCount = MAX_TOUCHES;

        int currentTouchIds[MAX_TOUCHES];

        for (int i = 0; i < MAX_TOUCHES; i++)
            currentTouchIds[i] = -1;

        // Обрабатываем новые касания
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

            if (editJoystick)
            {
                if (CheckCollisionPointRec(pos, doneButton))
                {
                    editJoystick = false;
                    draggingJoystick = false;
                }
                else
                {
                    float dx = pos.x - joystickPosition.x;
                    float dy = pos.y - joystickPosition.y;

                    if (sqrtf(dx * dx + dy * dy) < 150)
                    {
                        draggingJoystick = true;
                        joystickTouchId = id;
                    }
                }
            }
            else if (CheckCollisionPointRec(pos, settingsButton))
            {
                settingsOpen = !settingsOpen;
                joystickActive = false;
                joystickTouchId = -1;
                lookTouchId = -1;
            }
            else if (CheckCollisionPointRec(pos, cameraButton))
            {
                firstPerson = !firstPerson;
            }
            else if (settingsOpen)
            {
                if (CheckCollisionPointRec(pos, moveButton))
                {
                    editJoystick = true;
                    settingsOpen = false;
                }
                else if (CheckCollisionPointRec(pos, sizeButton))
                {
                    joystickRadius += 15.0f;

                    if (joystickRadius > 180.0f)
                        joystickRadius = 90.0f;

                    knobRadius = joystickRadius * 0.42f;
                }
                else if (CheckCollisionPointRec(pos, sensitivitySlider))
                {
                    draggingSensitivity = true;
                    sensitivityTouchId = id;
                }
                else if (CheckCollisionPointRec(pos, resetButton))
                {
                    joystickRadius = 120.0f;
                    knobRadius = 50.0f;

                    joystickPosition = (Vector2){
                        150.0f,
                        screenHeight - 160.0f
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
                // Левая часть экрана — джойстик
                if (pos.x < screenWidth * 0.48f &&
                    pos.y > screenHeight * 0.25f)
                {
                    float dx = pos.x - joystickPosition.x;
                    float dy = pos.y - joystickPosition.y;

                    if (sqrtf(dx * dx + dy * dy) < joystickRadius + 100)
                    {
                        joystickActive = true;
                        joystickTouchId = id;
                    }
                }
                // Правая часть — поворот камеры
                else if (pos.x > screenWidth * 0.48f &&
                         pos.y > 155)
                {
                    lookTouchId = id;
                    previousLookPosition = pos;
                }
            }
        }

        // Ищем положение пальца джойстика
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
                    float moveY = dy / joystickRadius;

                    player.x += (
                        cosf(playerAngle) * moveX +
                        sinf(playerAngle) * moveY
                    ) * playerSpeed;

                    player.z += (
                        sinf(playerAngle) * moveX -
                        cosf(playerAngle) * moveY
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

        // Перемещение джойстика в режиме настройки
        if (draggingJoystick)
        {
            bool found = false;

            for (int i = 0; i < touchCount; i++)
            {
                if (GetTouchPointId(i) == joystickTouchId)
                {
                    joystickPosition = GetTouchPosition(i);
                    joystickKnob = joystickPosition;
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                draggingJoystick = false;
                joystickTouchId = -1;
            }
        }

        // Работа ползунка чувствительности
        if (draggingSensitivity)
        {
            bool found = false;

            for (int i = 0; i < touchCount; i++)
            {
                if (GetTouchPointId(i) == sensitivityTouchId)
                {
                    Vector2 pos = GetTouchPosition(i);

                    float value =
                        (pos.x - sensitivitySlider.x) /
                        sensitivitySlider.width;

                    if (value < 0.0f) value = 0.0f;
                    if (value > 1.0f) value = 1.0f;

                    lookSensitivity = 0.1f + value * 9.9f;

                    found = true;
                    break;
                }
            }

            if (!found)
            {
                draggingSensitivity = false;
                sensitivityTouchId = -1;
            }
        }

        // Поворот камеры
        if (!settingsOpen && !editJoystick &&
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

        if (settingsOpen || editJoystick)
            lookTouchId = -1;

        // Сохраняем касания для следующего кадра
        for (int i = 0; i < MAX_TOUCHES; i++)
            previousTouchIds[i] = currentTouchIds[i];

        // Направление камеры
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

        ClearBackground((Color){ 135, 190, 235, 255 });

        BeginMode3D(camera);

        // Земля
        DrawPlane(
            (Vector3){ 0, -0.05f, 0 },
            (Vector2){ 200, 200 },
            (Color){ 75, 145, 75, 255 }
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
                    (Vector3){ x, 1.0f, z },
                    0.25f, 0.25f, 2.0f,
                    8, BROWN
                );

                DrawSphere(
                    (Vector3){ x, 2.8f, z },
                    1.2f, DARKGREEN
                );
            }
        }

        // Игрок в третьем лице
        if (!firstPerson)
        {
            DrawCylinder(
                (Vector3){ player.x, player.y + 0.65f, player.z },
                0.4f, 0.4f, 1.3f,
                12, BLUE
            );

            DrawSphere(
                (Vector3){ player.x, player.y + 1.55f, player.z },
                0.35f, BEIGE
            );
        }

        EndMode3D();

        // Джойстик
        DrawCircleV(
            joystickPosition,
            joystickRadius,
            (Color){ 20, 20, 20, 100 }
        );

        DrawCircleV(
            joystickKnob,
            knobRadius,
            (Color){ 220, 220, 220, 180 }
        );

        // Кнопки интерфейса
        DrawRectangleRounded(
            settingsButton, 0.25f, 12,
            (Color){ 25, 30, 42, 220 }
        );

        DrawText(
            "SETTINGS",
            settingsButton.x + 25,
            settingsButton.y + 18,
            20, RAYWHITE
        );

        DrawRectangleRounded(
            cameraButton, 0.25f, 12,
            (Color){ 25, 30, 42, 220 }
        );

        DrawText(
            firstPerson ? "1 PERSON" : "3 PERSON",
            cameraButton.x + 20,
            cameraButton.y + 18,
            20, RAYWHITE
        );

        // Современное меню настроек
        if (settingsOpen)
        {
            DrawRectangle(
                0, 0, screenWidth, screenHeight,
                (Color){ 0, 0, 0, 170 }
            );

            // Основная панель
            DrawRectangleRounded(
                panel, 0.08f, 16,
                (Color){ 24, 29, 42, 255 }
            );

            // Заголовок
            DrawText(
                "SETTINGS",
                panelX + 30,
                panelY + 25,
                30, RAYWHITE
            );

            DrawText(
                "Customize your game",
                panelX + 30,
                panelY + 60,
                16, GRAY
            );

            // Перемещение джойстика
            DrawRectangleRounded(
                moveButton, 0.25f, 12,
                (Color){ 45, 55, 75, 255 }
            );

            DrawText(
                "MOVE JOYSTICK",
                moveButton.x + 18,
                moveButton.y + 16,
                19, RAYWHITE
            );

            // Размер джойстика
            DrawRectangleRounded(
                sizeButton, 0.25f, 12,
                (Color){ 45, 55, 75, 255 }
            );

            DrawText(
                "JOYSTICK SIZE",
                sizeButton.x + 18,
                sizeButton.y + 16,
                19, RAYWHITE
            );

            // Чувствительность
            DrawText(
                "CAMERA SENSITIVITY",
                panelX + 30,
                panelY + 215,
                18, RAYWHITE
            );

            DrawText(
                TextFormat("%.1f", lookSensitivity),
                panelX + panelWidth - 85,
                panelY + 215,
                22, SKYBLUE
            );

            // Фон ползунка
            DrawRectangleRounded(
                (Rectangle){
                    sensitivitySlider.x,
                    sensitivitySlider.y - 5,
                    sensitivitySlider.width,
                    22
                },
                0.5f, 10,
                (Color){ 55, 65, 85, 255 }
            );

            // Заполненная часть ползунка
            float sliderValue =
                (lookSensitivity - 0.1f) / 9.9f;

            DrawRectangleRounded(
                (Rectangle){
                    sensitivitySlider.x,
                    sensitivitySlider.y - 5,
                    sensitivitySlider.width * sliderValue,
                    22
                },
                0.5f, 10,
                SKYBLUE
            );

            // Круглый бегунок
            DrawCircle(
                sensitivitySlider.x +
                sensitivitySlider.width * sliderValue,
                sensitivitySlider.y + 6,
                15,
                RAYWHITE
            );

            DrawText(
                "0.1",
                sensitivitySlider.x,
                sensitivitySlider.y + 28,
                16, GRAY
            );

            DrawText(
                "10",
                sensitivitySlider.x +
                sensitivitySlider.width - 20,
                sensitivitySlider.y + 28,
                16, GRAY
            );

            // Сброс
            DrawRectangleRounded(
                resetButton, 0.25f, 12,
                (Color){ 45, 55, 75, 255 }
            );

            DrawText(
                "RESET",
                resetButton.x + resetButton.width / 2 - 35,
                resetButton.y + 18,
                19, RAYWHITE
            );

            // Закрыть
            DrawRectangleRounded(
                closeButton, 0.25f, 12,
                (Color){ 35, 125, 220, 255 }
            );

            DrawText(
                "CLOSE",
                closeButton.x + closeButton.width / 2 - 35,
                closeButton.y + 18,
                19, RAYWHITE
            );
        }

        // Редактирование положения джойстика
        if (editJoystick)
        {
            DrawRectangle(
                0, 0, screenWidth, screenHeight,
                (Color){ 0, 0, 0, 130 }
            );

            DrawRectangleRounded(
                doneButton, 0.25f, 12,
                (Color){ 35, 125, 220, 255 }
            );

            DrawText(
                "DONE",
                doneButton.x + 65,
                doneButton.y + 19,
                22, RAYWHITE
            );

            DrawText(
                "DRAG JOYSTICK",
                screenWidth / 2 - 100,
                40,
                22, RAYWHITE
            );
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}

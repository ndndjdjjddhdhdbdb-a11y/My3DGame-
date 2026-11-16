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

    Vector2 joystickCenter = { 90, 0 };
    Vector2 joystickKnob = { 0, 0 };
    float joystickRadius = 160;
    float joystickRadius = 80;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        Vector2 touch = GetTouchPosition(0);

        float screenHeight = GetScreenHeight();
        joystickCenter.y = screenHeight - 90;

        bool moving = false;

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
            GetTouchPointCount() > 0)
        {
            if (touch.x < 220 && touch.y > screenHeight - 220)
            {
                float dx = touch.x - joystickCenter.x;
                float dy = touch.y - joystickCenter.y;

                float length = sqrtf(dx*dx + dy*dy);

                if (length > joystickRadius)
                {
                    dx *= joystickRadius / length;
                    dy *= joystickRadius / length;
                }

                joystickKnob = (Vector2){ dx, dy };
                moving = true;
            }
        }

        if (!moving)
        {
            joystickKnob = (Vector2){ 0, 0 };
        }

        player.x += joystickKnob.x / joystickRadius * 5.0f * dt;
        player.z += joystickKnob.y / joystickRadius * 5.0f * dt;

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

        BeginDrawing();
        ClearBackground(SKYBLUE);

        BeginMode3D(camera);

        DrawPlane((Vector3){ 0, 0, 0 },
                  (Vector2){ 100, 100 }, GREEN);

        DrawGrid(50, 1);

        DrawCube(player, 1, 2, 1, BLUE);
        DrawSphere((Vector3){ player.x, 2.3f, player.z },
                   0.5f, BEIGE);

        for (int x = -15; x <= 15; x += 5)
        {
            for (int z = -15; z <= 15; z += 5)
            {
                if (x == 0 && z == 0) continue;

                DrawCylinder((Vector3){ x, 1, z },
                             0.3f, 0.3f, 2, 8, BROWN);

                DrawSphere((Vector3){ x, 3, z },
                           1.5f, DARKGREEN);
            }
        }

        EndMode3D();

        // Джойстик
        DrawCircleV(joystickCenter, joystickRadius, Fade(DARKGRAY, 0.6f));

        DrawCircleV(
            (Vector2){
                joystickCenter.x + joystickKnob.x,
                joystickCenter.y + joystickKnob.y
            },
            70,
            WHITE
        );

        DrawText("MY 3D WORLD", 20, 20, 25, WHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}

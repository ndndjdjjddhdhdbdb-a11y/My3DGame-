#include "raymob.h"

int main(void)
{
    InitWindow(0, 0, "My 3D World");
    SetTargetFPS(60);

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 8.0f, 8.0f, 8.0f };
    camera.target = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Vector3 playerPosition = { 0.0f, 1.0f, 0.0f };

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(SKYBLUE);

        BeginMode3D(camera);

        DrawPlane((Vector3){ 0, 0, 0 },
                  (Vector2){ 50, 50 }, GREEN);

        DrawGrid(50, 1.0f);

        DrawCube(playerPosition, 1.0f, 2.0f, 1.0f, BLUE);

        DrawSphere(
            (Vector3){ playerPosition.x, 2.3f, playerPosition.z },
            0.5f, BEIGE
        );

        for (int x = -15; x <= 15; x += 5)
        {
            for (int z = -15; z <= 15; z += 5)
            {
                if (x == 0 && z == 0) continue;

                DrawCylinder(
                    (Vector3){ x, 1.0f, z },
                    0.3f, 0.3f, 2.0f, 8, BROWN
                );

                DrawSphere(
                    (Vector3){ x, 3.0f, z },
                    1.5f, DARKGREEN
                );
            }
        }

        EndMode3D();

        DrawText("MY 3D WORLD", 20, 20, 30, WHITE);
        DrawText("Welcome!", 20, 60, 20, WHITE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}

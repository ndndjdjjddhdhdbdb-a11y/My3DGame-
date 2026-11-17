#include "Game.h"

#include "raylib.h"

void Game::Run()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "Bolt Survival - C++17 Prototype");

    if (!IsWindowReady())
        return;

    SetTargetFPS(60);
    DisableCursor();

    while (!WindowShouldClose())
    {
        const float deltaTime = GetFrameTime();
        player_.Update(deltaTime);

        BeginDrawing();
        ClearBackground(Color{135, 190, 225, 255});

        BeginMode3D(player_.GetCamera());
        world_.Draw();
        EndMode3D();

        DrawInterface();
        EndDrawing();
    }

    EnableCursor();
    CloseWindow();
}

void Game::DrawInterface() const
{
    const int centerX = GetScreenWidth() / 2;
    const int centerY = GetScreenHeight() / 2;

    // Небольшой прицел в центре экрана.
    DrawLine(centerX - 7, centerY, centerX + 7, centerY, WHITE);
    DrawLine(centerX, centerY - 7, centerX, centerY + 7, WHITE);

    DrawRectangle(14, 14, 210, 38, Fade(BLACK, 0.45f));
    DrawText("BOLT SURVIVAL", 25, 23, 20, RAYWHITE);

    DrawRectangle(14, GetScreenHeight() - 42, 440, 28,
                  Fade(BLACK, 0.45f));
    DrawText("WASD - движение | SHIFT - бег | SPACE - прыжок",
             23, GetScreenHeight() - 35, 16, RAYWHITE);
}

#include "World.h"

#include "raylib.h"

void World::Draw() const
{
    // Большая плоская поверхность земли.
    DrawCube({0.0f, -0.25f, 0.0f}, 120.0f, 0.5f, 120.0f,
             Color{82, 112, 65, 255});

    // Простые ориентиры на карте. В следующих этапах это будут
    // полноценные интерактивные ресурсы.
    for (int i = 0; i < 9; ++i)
    {
        const float x = static_cast<float>((i % 3) * 12 - 12);
        const float z = static_cast<float>((i / 3) * -12 - 10);

        DrawCylinder({x, 1.2f, z}, 0.35f, 0.25f, 2.4f, 8,
                     Color{105, 73, 45, 255});
        DrawSphere({x, 3.0f, z}, 1.5f,
                   Color{42, 105, 48, 255});
    }

    // Камни-ориентиры.
    DrawSphere({7.0f, 0.8f, -7.0f}, 1.0f, GRAY);
    DrawSphere({9.0f, 0.55f, -7.5f}, 0.7f, DARKGRAY);
    DrawSphere({-8.0f, 0.65f, -19.0f}, 0.85f, LIGHTGRAY);

    // Несколько дальних холмов.
    DrawSphere({-28.0f, -3.0f, -42.0f}, 12.0f,
               Color{95, 119, 77, 255});
    DrawSphere({26.0f, -4.0f, -48.0f}, 15.0f,
               Color{103, 126, 82, 255});
}

#pragma once

#include "raylib.h"

class Player
{
public:
    Player();

    // Обновляет камеру, движение и прыжок игрока.
    void Update(float deltaTime);

    // Возвращает настоящую камеру с учётом поворота мышью.
    const Camera3D& GetCamera() const;

private:
    Camera3D camera_{};

    Vector3 position_{0.0f, 1.8f, 0.0f};
    float verticalVelocity_ = 0.0f;
    float yaw_ = -1.5707963f;
    float pitch_ = 0.0f;

    const float walkSpeed_ = 5.0f;
    const float runMultiplier_ = 1.6f;
    const float jumpSpeed_ = 7.0f;
    const float gravity_ = 20.0f;
    const float eyeHeight_ = 1.8f;

    bool onGround_ = true;
};

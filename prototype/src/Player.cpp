#include "Player.h"

#include <cmath>

namespace
{
    Vector3 NormalizeHorizontal(Vector3 value)
    {
        const float length = std::sqrt(value.x * value.x + value.z * value.z);

        if (length > 0.0001f)
        {
            value.x /= length;
            value.z /= length;
        }

        return value;
    }
}

Player::Player()
{
    camera_.position = position_;
    camera_.target = {0.0f, eyeHeight_, -1.0f};
    camera_.up = {0.0f, 1.0f, 0.0f};
    camera_.fovy = 75.0f;
    camera_.projection = CAMERA_PERSPECTIVE;

    // Скрываем курсор, чтобы мышь управляла обзором.
    DisableCursor();
}

void Player::Update(float deltaTime)
{
    // Ограничиваем большой скачок времени после сворачивания окна.
    if (deltaTime > 0.05f)
        deltaTime = 0.05f;

    // Raylib самостоятельно обрабатывает мышь и поворот FPS-камеры.
    UpdateCamera(&camera_, CAMERA_FIRST_PERSON);

    // Направление взгляда на плоскости земли.
    Vector3 forward = NormalizeHorizontal({
        camera_.target.x - camera_.position.x,
        0.0f,
        camera_.target.z - camera_.position.z
    });

    const Vector3 right = {-forward.z, 0.0f, forward.x};
    Vector3 movement = {0.0f, 0.0f, 0.0f};

    if (IsKeyDown(KEY_W)) { movement.x += forward.x; movement.z += forward.z; }
    if (IsKeyDown(KEY_S)) { movement.x -= forward.x; movement.z -= forward.z; }
    if (IsKeyDown(KEY_D)) { movement.x += right.x;   movement.z += right.z; }
    if (IsKeyDown(KEY_A)) { movement.x -= right.x;   movement.z -= right.z; }

    // Нормализация не позволяет двигаться быстрее по диагонали.
    const float moveLength = std::sqrt(
        movement.x * movement.x + movement.z * movement.z
    );

    if (moveLength > 0.0001f)
    {
        movement.x /= moveLength;
        movement.z /= moveLength;
    }

    float speed = walkSpeed_;
    if (IsKeyDown(KEY_LEFT_SHIFT))
        speed *= runMultiplier_;

    position_.x += movement.x * speed * deltaTime;
    position_.z += movement.z * speed * deltaTime;

    // Прыжок и гравитация.
    if (IsKeyPressed(KEY_SPACE) && onGround_)
    {
        verticalVelocity_ = jumpSpeed_;
        onGround_ = false;
    }

    verticalVelocity_ -= gravity_ * deltaTime;
    position_.y += verticalVelocity_ * deltaTime;

    if (position_.y <= eyeHeight_)
    {
        position_.y = eyeHeight_;
        verticalVelocity_ = 0.0f;
        onGround_ = true;
    }

    // Сохраняем направление камеры и перемещаем её вместе с игроком.
    const Vector3 viewOffset = {
        camera_.target.x - camera_.position.x,
        camera_.target.y - camera_.position.y,
        camera_.target.z - camera_.position.z
    };

    camera_.position = position_;
    camera_.target = {
        position_.x + viewOffset.x,
        position_.y + viewOffset.y,
        position_.z + viewOffset.z
    };
}

const Camera3D& Player::GetCamera() const
{
    return camera_;
}

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

    // Управляем обзором мышью самостоятельно, чтобы UpdateCamera
    // не дублировал WASD-перемещение, которое реализовано ниже.
    const Vector2 mouseDelta = GetMouseDelta();
    const float mouseSensitivity = 0.0025f;

    yaw_ += mouseDelta.x * mouseSensitivity;
    pitch_ -= mouseDelta.y * mouseSensitivity;

    if (pitch_ > 1.50f) pitch_ = 1.50f;
    if (pitch_ < -1.50f) pitch_ = -1.50f;

    const Vector3 lookDirection = {
        std::cos(pitch_) * std::cos(yaw_),
        std::sin(pitch_),
        std::cos(pitch_) * std::sin(yaw_)
    };

    // Направление движения по земле не зависит от наклона камеры вверх/вниз.
    Vector3 forward = NormalizeHorizontal(lookDirection);

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

    // Камера следует за игроком и смотрит в рассчитанном направлении.
    camera_.position = position_;
    camera_.target = {
        position_.x + lookDirection.x,
        position_.y + lookDirection.y,
        position_.z + lookDirection.z
    };
}

const Camera3D& Player::GetCamera() const
{
    return camera_;
}

#include "Camera.h"

#include <algorithm>
#include <cmath>

#include "Config.h"

void Camera::reset(const Vec3& playerPos)
{
    focus_ = playerPos;
    yaw_ = 0.0f;
    pitch_ = Config::CAMERA_START_PITCH;
    layout();
}

void Camera::orbit(float mouseDx, float mouseDy)
{
    yaw_ = std::remainder(yaw_ + mouseDx * Config::CAMERA_MOUSE_SENSITIVITY, Config::FULL_TURN);
    pitch_ = std::clamp(pitch_ + mouseDy * Config::CAMERA_MOUSE_SENSITIVITY,
                       Config::CAMERA_MIN_PITCH, Config::CAMERA_MAX_PITCH);
}

Vec3 Camera::worldDirection(float right, float forward) const
{
    const float s = std::sin(yaw_);
    const float c = std::cos(yaw_);
    return {right * c + forward * s, 0.0f, forward * c - right * s};
}

void Camera::layout()
{
    const Vec3 forward = worldDirection(0.0f, 1.0f);
    const float distance = Config::CAMERA_DISTANCE * std::cos(pitch_);
    const float focusY = focus_.y + Config::CAMERA_LOOK_Y;
    pos_ = {focus_.x - forward.x * distance,
            std::max(focusY + Config::CAMERA_DISTANCE * std::sin(pitch_),
                     Config::SEA_Y + Config::CAMERA_MIN_ABOVE_SEA),
            focus_.z - forward.z * distance};
    target_ = {focus_.x + forward.x * Config::CAMERA_LOOK_AHEAD, focusY,
               focus_.z + forward.z * Config::CAMERA_LOOK_AHEAD};
}

void Camera::update(float dt, const Vec3& playerPos)
{
    // Chỉ làm mượt điểm bám để camera luôn xoay đúng quanh phù thủy.
    const float k = 1.0f - std::exp(-Config::CAMERA_FOLLOW_RATE * dt);
    focus_.x += (playerPos.x - focus_.x) * k;
    focus_.y += (playerPos.y - focus_.y) * k;
    focus_.z += (playerPos.z - focus_.z) * k;
    layout();
}

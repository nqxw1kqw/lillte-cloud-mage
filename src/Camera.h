#pragma once

#include "Player.h"

// Camera góc nhìn thứ ba: bám vị trí nhân vật, quay riêng theo chuột.
class Camera
{
public:
    void reset(const Vec3& playerPos);
    void orbit(float mouseDx, float mouseDy);
    void update(float dt, const Vec3& playerPos);
    Vec3 worldDirection(float right, float forward) const;

    const Vec3& position() const { return pos_; }
    const Vec3& target() const { return target_; }

private:
    void layout();

    Vec3 focus_{};
    Vec3 pos_{};
    Vec3 target_{};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
};

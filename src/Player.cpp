#include "Player.h"

#include <algorithm>
#include <cmath>

#include "Cloud.h"
#include "Config.h"

namespace
{
    void translate(Vec3& pos, const Vec3& shift)
    {
        pos.x += shift.x;
        pos.y += shift.y;
        pos.z += shift.z;
    }

    Vec3 takeoffVelocity(float yaw, float speed)
    {
        return {speed * std::sin(yaw), Config::JUMP_VY, speed * std::cos(yaw)};
    }

    void stepFlight(float dt, const Vec3& direction, int bounces, float launchSpeed, Vec3& pos, Vec3& vel)
    {
        vel.y -= Config::GRAVITY * dt;
        const float length = std::hypot(direction.x, direction.z);
        if (bounces == 0 && length > 0.0f)
        {
            // Phím chỉ chỉnh hướng trên không. Tốc độ ngang bị giới hạn bởi tốc độ bật nhảy.
            const float speed = launchSpeed / length;
            const float k = std::min(1.0f, Config::AIR_STEER_RATE * dt);
            vel.x += (direction.x * speed - vel.x) * k;
            vel.z += (direction.z * speed - vel.z) * k;
        }
        pos.x += vel.x * dt;
        pos.y += vel.y * dt;
        pos.z += vel.z * dt;
    }
}

void Player::reset(float top)
{
    pos_ = {0.0f, top, 0.0f};
    vel_ = {};
    rotX_ = 0.0f;
    rotZ_ = 0.0f;
    yaw_ = 0.0f;
    launchYaw_ = 0.0f;
    launchSpeed_ = 0.0f;
    runupDistance_ = 0.0f;
    runTime_ = 0.0f;
    groundSpeed_ = 0.0f;
    jumpBuffer_ = 0.0f;
    bufferedDirection_ = {};
    bounces_ = 0;
    jumpsUsed_ = 0;
    sprintHeld_ = false;
    supportId_ = 0;
    grounded_ = true;
    atSea_ = false;
    setMotion(Motion::Idle);
}

void Player::walk(float dt, const Vec3& direction, float turn, const CloudField& clouds)
{
    if (!grounded_ || motion_ == Motion::Jump)
    {
        return;
    }

    const float length = std::hypot(direction.x, direction.z);
    if (length == 0.0f)
    {
        runTime_ = 0.0f;
        groundSpeed_ = 0.0f;
        yaw_ = std::remainder(yaw_ + turn * Config::GROUND_TURN_SPEED * dt, Config::FULL_TURN);
        return;
    }

    yaw_ = std::atan2(direction.x, direction.z);
    const float maxSpeed = sprintHeld_ ? Config::SPRINT_MOVE_SPEED : Config::GROUND_MOVE_SPEED;
    groundSpeed_ = std::min(maxSpeed, groundSpeed_ + Config::GROUND_ACCELERATION * dt);
    const float distance = groundSpeed_ * dt;
    float fraction = 1.0f;
    // Chia nhỏ bước ở sườn dốc để đi tiếp được, không nhảy cóc độ cao hoặc kẹt tại chỗ.
    for (int attempt=0;attempt<Config::GROUND_FOOTING_SUBSTEPS;++attempt)
    {
        const float step = distance * fraction / length;
        const float x = pos_.x + direction.x * step, z = pos_.z + direction.z * step;
        float y=0;
        if (clouds.supportAt(supportId_,x,z,y) && std::abs(y-pos_.y)<=Config::GROUND_STEP_HEIGHT)
        {
            pos_={x,y,z};
            runTime_+=distance*fraction/Config::GROUND_MOVE_SPEED;
            runupDistance_=std::min(Config::JUMP_RUNUP_DISTANCE,runupDistance_+distance*fraction);
            return;
        }
        fraction*=.5f;
    }
    runTime_=0.0f;
}

float Player::takeoffSpeed() const
{
    // Giữ đà khi dừng ở mép để chỉnh góc camera trước khi nhảy.
    const float ratio = runupDistance_ / Config::JUMP_RUNUP_DISTANCE;
    const float maxSpeed = sprintHeld_ ? Config::JUMP_SPRINT_SPEED : Config::JUMP_RUN_SPEED;
    return Config::JUMP_STAND_SPEED + (maxSpeed - Config::JUMP_STAND_SPEED) * ratio;
}

float Player::jumpCharge() const
{
    return runupDistance_ / Config::JUMP_RUNUP_DISTANCE;
}

bool Player::canDoubleJump() const
{
    return !grounded_ && jumpsUsed_ < Config::MAX_PLAYER_JUMPS && !atSea_;
}

void Player::jump(const Vec3& direction, bool sprint)
{
    sprintHeld_ = sprint;
    if (atSea_ || (grounded_ && motion_ == Motion::Jump))
    {
        return;
    }
    if (!grounded_)
    {
        if (jumpsUsed_ < Config::MAX_PLAYER_JUMPS)
        {
            ++jumpsUsed_;
            bounces_ = 0;
            vel_.y = Config::DOUBLE_JUMP_VY;
            launchSpeed_ = std::max(launchSpeed_, Config::JUMP_STAND_SPEED);
            const float length = std::hypot(direction.x, direction.z);
            if (length > 0.0f)
            {
                yaw_ = std::atan2(direction.x, direction.z);
                vel_.x = direction.x * launchSpeed_ / length;
                vel_.z = direction.z * launchSpeed_ / length;
            }
            jumpBuffer_ = 0.0f;
            setMotion(Motion::Fly);
            return;
        }
        // Nhớ một lần bấm sát lúc đáp. Giữ phím không tạo thêm yêu cầu nhảy.
        jumpBuffer_ = Config::JUMP_BUFFER_TIME;
        bufferedDirection_ = direction;
        return;
    }
    jumpBuffer_ = 0.0f;
    if (direction.x != 0.0f || direction.z != 0.0f)
    {
        yaw_ = std::atan2(direction.x, direction.z);
    }
    launchYaw_ = yaw_;
    jumpsUsed_ = 1;
    launchSpeed_ = takeoffSpeed();
    runTime_ = 0.0f;
    groundSpeed_ = 0.0f;
    bounces_ = 0;
    setMotion(Motion::Jump);
}

void Player::land(const Cloud& c, const Vec3& point, float impact, int& score)
{
    pos_ = point;
    score = std::max(score, c.id);
    supportId_ = c.id;
    if (impact > Config::BOUNCE_IMPACT_MIN && bounces_ < Config::MAX_BOUNCES)
    {
        // Giữ đúng X/Z của điểm chạm để rơi lại trên cùng mặt mây.
        vel_ = {0.0f, impact * Config::BOUNCE_FACTOR, 0.0f};
        ++bounces_;
        setMotion(Motion::Bounce);
    }
    else
    {
        grounded_ = true;
        jumpsUsed_ = 0;
        runupDistance_ = 0.0f;
        groundSpeed_ = 0.0f;
        vel_ = {};
        bounces_ = 0;
        setMotion(Motion::Land);
    }
}

void Player::setMotion(Motion motion)
{
    motion_ = motion;
    motionTime_ = 0.0f;
}

void Player::updateMotion(float dt)
{
    motionTime_ += dt;
    if ((motion_ == Motion::Jump && motionTime_ < Config::WITCH_JUMP_TIME) ||
        (motion_ == Motion::Bounce && motionTime_ < Config::WITCH_BOUNCE_TIME) ||
        (motion_ == Motion::Land && motionTime_ < Config::WITCH_LAND_TIME))
    {
        return;
    }
    const Motion next = grounded_ ? Motion::Idle : (vel_.y >= 0.0f ? Motion::Fly : Motion::Fall);
    if (motion_ != next)
    {
        setMotion(next);
    }
}

void Player::update(float dt, const Vec3& direction, const CloudField& clouds, bool collide, int& score, float turn, bool sprint)
{
    sprintHeld_ = sprint;
    if (atSea_)
    {
        return;
    }
    const double fromTime = clouds.previousTime();
    const double toTime = clouds.time();
    if (grounded_)
    {
        translate(pos_, clouds.displacement(supportId_, fromTime, toTime));
        if (jumpBuffer_ > 0.0f)
        {
            jump(bufferedDirection_, sprintHeld_);
        }
        walk(dt, direction, turn, clouds);
        updateMotion(dt);
        if (motion_ == Motion::Jump && motionTime_ >= Config::JUMP_WINDUP)
        {
            grounded_ = false;
            vel_ = takeoffVelocity(launchYaw_, launchSpeed_);
        }
        rotX_ = 0.0f;
        rotZ_ = 0.0f;
        return;
    }

    updateMotion(dt);
    const Vec3 from = pos_;
    const float previousVy = vel_.y;
    stepFlight(dt, direction, bounces_, launchSpeed_, pos_, vel_);
    // Cú nảy đi theo bệ đang trôi, nên luôn quay lại cùng điểm trên bề mặt.
    const Vec3 carry = bounces_ > 0 ? clouds.displacement(supportId_, fromTime, toTime) : Vec3{};
    translate(pos_, carry);
    if (bounces_ == 0 && (vel_.x != 0.0f || vel_.z != 0.0f))
    {
        yaw_ = std::atan2(vel_.x, vel_.z);
    }

    if (collide)
    {
        CloudContact contact;
        if (const Cloud* c = clouds.findLanding(from, pos_, fromTime, toTime, contact))
        {
            const float impact = -previousVy + Config::GRAVITY * dt * contact.fraction +
                (contact.platformShift.y - carry.y) / dt;
            land(*c, contact.pos, impact, score);
        }
    }

    jumpBuffer_ = std::max(0.0f, jumpBuffer_ - dt);

    rotX_ = grounded_ ? 0.0f : std::clamp(-vel_.y * Config::TILT_FACTOR, -Config::TILT_MAX, Config::TILT_MAX);
    const float bank = direction.x * std::cos(yaw_) - direction.z * std::sin(yaw_);
    rotZ_ = grounded_ || bounces_ > 0 ? 0.0f : std::clamp(-0.3f * bank, -Config::TILT_MAX, Config::TILT_MAX);
}

bool Player::isBelowSea() const
{
    return pos_.y <= Config::SEA_Y;
}

bool Player::predictLanding(const Vec3& direction, const CloudField& clouds, LandingMarker& marker) const
{
    if (atSea_)
    {
        return false;
    }
    // Khi còn đứng, dự đoán theo hướng mặt hiện tại để người chơi căn trước khi nhảy.
    float heading = motion_ == Motion::Jump ? launchYaw_ : yaw_;
    if (grounded_ && motion_ != Motion::Jump && (direction.x != 0.0f || direction.z != 0.0f))
    {
        heading = std::atan2(direction.x, direction.z);
    }
    Vec3 pos = pos_;
    const float speed = grounded_ && motion_ != Motion::Jump ? takeoffSpeed() : launchSpeed_;
    Vec3 vel = grounded_ ? takeoffVelocity(heading, speed) : vel_;
    const double now = clouds.time();
    double time = now;
    constexpr float dt = static_cast<float>(Config::PHYSICS_DT);
    if (grounded_)
    {
        float elapsed = motion_ == Motion::Jump ? motionTime_ : 0.0f;
        while (elapsed < Config::JUMP_WINDUP)
        {
            time += dt;
            elapsed += dt;
        }
        translate(pos, clouds.displacement(supportId_, now, time));
    }

    for (int step = 0; step < Config::LANDING_PREDICT_MAX_STEPS; ++step)
    {
        const Vec3 from = pos;
        const double nextTime = time + dt;
        stepFlight(dt, direction, bounces_, speed, pos, vel);
        if (bounces_ > 0)
        {
            translate(pos, clouds.displacement(supportId_, time, nextTime));
        }

        CloudContact contact;
        if (const Cloud* cloud = clouds.findLanding(from, pos, time, nextTime, contact))
        {
            // Dự đoán điểm chạm tương lai, nhưng vẽ trên mặt mây ở vị trí hiện tại.
            marker.pos = contact.pos;
            translate(marker.pos, clouds.displacement(cloud->id, nextTime, now));
            marker.slopeX = contact.slopeX;
            marker.slopeZ = contact.slopeZ;
            marker.cloudId = cloud->id;
            return true;
        }
        if (pos.y <= Config::SEA_Y)
        {
            return false;
        }
        time = nextTime;
    }
    return false;
}

void Player::stopAtSea(float surfaceY)
{
    pos_.y = surfaceY;
    vel_ = {};
    atSea_ = true;
    jumpBuffer_ = 0.0f;
}

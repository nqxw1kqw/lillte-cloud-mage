#pragma once

#include <array>
#include <vector>

#include "Config.h"
#include "Player.h"

struct CloudContact
{
    Vec3 pos{};
    float fraction = 1.0f;
    float slopeX = 0.0f;
    float slopeZ = 0.0f;
    Vec3 platformShift{};
};

// Lưới mặt trên dùng chung cho tất cả mây vẽ bằng cùng một model.
class CloudSurface
{
public:
    void addTriangle(const Vec3& a, const Vec3& b, const Vec3& c);
    bool cross(const Vec3& from, const Vec3& to, CloudContact& contact) const;
    bool roofHeight(float x, float z, float& height) const;

private:
    struct Triangle
    {
        Vec3 a;
        Vec3 b;
        Vec3 c;
        float determinant;
        float slopeX;
        float slopeZ;

        float heightAt(float x, float z) const;
        bool contains(float x, float z) const;
    };

    bool isExposed(const Vec3& point) const;

    std::vector<Triangle> triangles_;
};

struct Cloud
{
    int id = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    size_t model = 0;
    bool movable = false;
    Vec3 windFrom{};
    Vec3 windTarget{};
    double windStarted = 0.0;
    Vec3 windOffsetAt(double time) const;
    Vec3 positionAt(double time) const;
};

class CloudField
{
public:
    void reset();
    void extend(float playerZ);
    void advance(float dt);
    double time() const { return time_; }
    double previousTime() const { return previousTime_; }
    Vec3 displacement(int cloudId, double from, double to) const;
    int aimCloud(const Vec3& eye, const Vec3& look, const Vec3& player, int supportId) const;
    void pullCloud(int cloudId, const Vec3& destination);
    void releaseCloud(int cloudId);
    Vec3 cloudPosition(int id) const;

    CloudSurface& surface(size_t model) { return surfaces_[model]; }
    const CloudSurface& surface(size_t model) const { return surfaces_[model]; }
    float startHeight() const;
    bool supportAt(int cloudId, float x, float z, float& height) const;
    int count() const { return count_; }
    // i = 0 là mây cũ nhất.
    const Cloud& at(int i) const { return pool_[static_cast<size_t>((head_ + i) % Config::CLOUD_POOL_SIZE)]; }
    const Cloud& first() const { return at(0); }
    const Cloud* findLanding(const Vec3& from, const Vec3& to, double fromTime, double toTime,
                             CloudContact& contact) const;

private:
    const Cloud* find(int id) const;
    void addCloud(float x, float y, float z);

    std::array<Cloud, Config::CLOUD_POOL_SIZE> pool_{};
    std::array<CloudSurface, Config::CLOUD_PLATFORM_COUNT> surfaces_{};
    int head_ = 0;
    int count_ = 0;
    int nextId_ = 0;
    double time_ = 0.0;
    double previousTime_ = 0.0;
};

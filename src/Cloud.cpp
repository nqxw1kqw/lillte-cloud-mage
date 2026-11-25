#include "Cloud.h"

#include <algorithm>
#include <cmath>

void CloudSurface::addTriangle(const Vec3& a, const Vec3& b, const Vec3& c)
{
    const float bx = b.x - a.x;
    const float bz = b.z - a.z;
    const float cx = c.x - a.x;
    const float cz = c.z - a.z;
    const float determinant = bx * cz - cx * bz;
    // Các model dùng thứ tự đỉnh DxLib. Tích có hướng theo X/Z dương là mặt hướng lên.
    if (determinant < Config::CLOUD_CONTACT_EPSILON)
    {
        return;
    }

    const float by = b.y - a.y;
    const float cy = c.y - a.y;
    const float slopeX = (by * cz - cy * bz) / determinant;
    const float slopeZ = (bx * cy - cx * by) / determinant;
    triangles_.push_back({a, b, c, determinant, slopeX, slopeZ});
}

float CloudSurface::Triangle::heightAt(float x, float z) const
{
    return a.y + slopeX * (x - a.x) + slopeZ * (z - a.z);
}

bool CloudSurface::Triangle::contains(float x, float z) const
{
    constexpr float eps = Config::CLOUD_CONTACT_EPSILON;
    const float dx = x - a.x;
    const float dz = z - a.z;
    const float u = (dx * (c.z - a.z) - dz * (c.x - a.x)) / determinant;
    const float v = ((b.x - a.x) * dz - (b.z - a.z) * dx) / determinant;
    return u >= -eps && v >= -eps && u + v <= 1.0f + eps;
}

bool CloudSurface::roofHeight(float x, float z, float& height) const
{
    bool found = false;
    for (const Triangle& t : triangles_)
    {
        if (t.contains(x, z))
        {
            const float y = t.heightAt(x, z);
            if (!found || y > height)
            {
                height = y;
                found = true;
            }
        }
    }
    return found;
}

bool CloudSurface::isExposed(const Vec3& point) const
{
    // Mặt dốc không cho đứng vẫn có thể che một mặt thoải bên dưới.
    for (const Triangle& t : triangles_)
    {
        if (t.contains(point.x, point.z) &&
            t.heightAt(point.x, point.z) > point.y + Config::CLOUD_CONTACT_EPSILON)
        {
            return false;
        }
    }
    return true;
}

bool CloudSurface::cross(const Vec3& from, const Vec3& to, CloudContact& contact) const
{
    bool hit = false;
    constexpr float eps = Config::CLOUD_CONTACT_EPSILON;
    for (const Triangle& t : triangles_)
    {
        if (t.slopeX * t.slopeX + t.slopeZ * t.slopeZ > Config::CLOUD_MAX_LANDING_SLOPE_SQUARED)
        {
            continue;
        }

        const float fromGap = from.y - t.heightAt(from.x, from.z);
        const float toGap = to.y - t.heightAt(to.x, to.z);
        // Chỉ đáp từ phía trên. Sai số nhỏ giữ cú nảy thẳng đứng trên cùng một tam giác.
        if (fromGap < -eps || toGap > eps || fromGap <= toGap)
        {
            continue;
        }

        const float fraction = std::clamp(fromGap / (fromGap - toGap), 0.0f, 1.0f);
        if (fraction > contact.fraction)
        {
            continue;
        }
        const float x = from.x + (to.x - from.x) * fraction;
        const float z = from.z + (to.z - from.z) * fraction;
        const Vec3 point{x, t.heightAt(x, z), z};
        if (t.contains(x, z) && isExposed(point))
        {
            contact = {point, fraction, t.slopeX, t.slopeZ};
            hit = true;
        }
    }
    return hit;
}

Vec3 Cloud::windOffsetAt(double time) const
{
    const float remaining = static_cast<float>(std::exp(-Config::WIND_RESPONSE * std::max(0.0, time - windStarted)));
    return {windTarget.x + (windFrom.x - windTarget.x) * remaining,
            0.0f, windTarget.z + (windFrom.z - windTarget.z) * remaining};
}

Vec3 Cloud::positionAt(double time) const
{
    const Vec3 offset = windOffsetAt(time);
    const double phase = id * static_cast<double>(Config::CLOUD_DRIFT_PHASE);
    auto wave = [time](float range, float rate, double offset) {
        return range * static_cast<float>(std::sin(time * rate + offset));
    };
    return {x + offset.x + wave(Config::CLOUD_DRIFT_X, Config::CLOUD_DRIFT_RATE_X, phase),
            y + wave(Config::CLOUD_DRIFT_Y, Config::CLOUD_DRIFT_RATE_Y, phase * 1.4),
            z + offset.z + wave(Config::CLOUD_DRIFT_Z, Config::CLOUD_DRIFT_RATE_Z, phase * 0.7)};
}

void CloudField::reset()
{
    head_ = 0;
    count_ = 0;
    nextId_ = 0;
    time_ = 0.0;
    previousTime_ = 0.0;
    for (const auto& entry : Config::WIND_LESSON)
    {
        addCloud(entry.x, entry.y, entry.z);
    }
}

Vec3 CloudField::cloudPosition(int id) const
{
    const Cloud* cloud = find(id);
    return cloud != nullptr ? cloud->positionAt(time_) : Vec3{};
}

int CloudField::aimCloud(const Vec3& eye, const Vec3& look, const Vec3& player, int supportId) const
{
    const Vec3 ray{look.x - eye.x, look.y - eye.y, look.z - eye.z};
    const float length = std::sqrt(ray.x * ray.x + ray.y * ray.y + ray.z * ray.z);
    if (length == 0.0f)
    {
        return -1;
    }
    int selected = -1;
    float nearest = Config::WIND_RANGE;
    for (int i = 0; i < count_; ++i)
    {
        const Cloud& cloud = at(i);
        if (!cloud.movable || cloud.id != supportId + 1)
        {
            continue;
        }
        const Vec3 p = cloud.positionAt(time_);
        const float dx = p.x - player.x;
        const float dy = p.y - player.y;
        const float dz = p.z - player.z;
        if (dx * dx + dy * dy + dz * dz > Config::WIND_RANGE * Config::WIND_RANGE)
        {
            continue;
        }
        const Vec3 delta{p.x - eye.x, p.y - eye.y, p.z - eye.z};
        const float depth = (delta.x * ray.x + delta.y * ray.y + delta.z * ray.z) / length;
        const float distanceSquared = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z - depth * depth;
        if (depth > 0.0f && depth < nearest && distanceSquared <= Config::WIND_AIM_RADIUS * Config::WIND_AIM_RADIUS)
        {
            nearest = depth;
            selected = cloud.id;
        }
    }
    return selected;
}

void CloudField::pullCloud(int cloudId, const Vec3& destination)
{
    const int index = cloudId - first().id;
    if (index < 0 || index >= count_ || !at(index).movable)
    {
        return;
    }
    Cloud& cloud = pool_[static_cast<size_t>((head_ + index) % Config::CLOUD_POOL_SIZE)];
    const Vec3 current = cloud.windOffsetAt(time_);
    Vec3 target{destination.x - cloud.x, 0.0f, destination.z - cloud.z};
    const float radius = std::hypot(target.x, target.z);
    if (radius > Config::WIND_MAX_OFFSET)
    {
        target.x *= Config::WIND_MAX_OFFSET / radius;
        target.z *= Config::WIND_MAX_OFFSET / radius;
    }
    cloud.windFrom = current;
    cloud.windTarget = target;
    cloud.windStarted = time_;
}

void CloudField::releaseCloud(int cloudId)
{
    const int index = cloudId - first().id;
    if (index < 0 || index >= count_)
    {
        return;
    }
    Cloud& cloud = pool_[static_cast<size_t>((head_ + index) % Config::CLOUD_POOL_SIZE)];
    cloud.windFrom = cloud.windOffsetAt(time_);
    cloud.windTarget = cloud.windFrom;
    cloud.windStarted = time_;
}

void CloudField::advance(float dt)
{
    previousTime_ = time_;
    time_ += dt;
}

const Cloud* CloudField::find(int id) const
{
    const int index = id - first().id;
    return index >= 0 && index < count_ ? &at(index) : nullptr;
}

Vec3 CloudField::displacement(int cloudId, double from, double to) const
{
    const Cloud* cloud = find(cloudId);
    if (cloud == nullptr)
    {
        return {};
    }
    const Vec3 before = cloud->positionAt(from);
    const Vec3 after = cloud->positionAt(to);
    return {after.x - before.x, after.y - before.y, after.z - before.z};
}

float CloudField::startHeight() const
{
    float height = Config::CLOUD_TOP_OFFSET;
    supportAt(first().id, 0.0f, 0.0f, height);
    return height;
}

bool CloudField::supportAt(int cloudId, float x, float z, float& height) const
{
    const Cloud* cloud = find(cloudId);
    if (cloud == nullptr)
    {
        return false;
    }

    const Vec3 p = cloud->positionAt(time_);
    const float localX = x - p.x;
    const float localZ = z - p.z;
    if (std::abs(localX) > Config::CLOUD_HALF_W || std::abs(localZ) > Config::CLOUD_HALF_D)
    {
        return false;
    }

    CloudContact hit;
    if (!surfaces_[cloud->model].cross({localX, Config::CLOUD_TOP_OFFSET, localZ},
                                      {localX, Config::CLOUD_BOTTOM_OFFSET, localZ}, hit))
    {
        return false;
    }

    height = p.y + hit.pos.y;
    return true;
}

const Cloud* CloudField::findLanding(const Vec3& from, const Vec3& to, double fromTime, double toTime,
                                    CloudContact& contact) const
{
    const Cloud* hit = nullptr;
    float closest = 1.0f;
    for (int i = 0; i < count_; ++i)
    {
        const Cloud& c = at(i);
        // Loại bệ ở xa bằng bao ngoài của cả quỹ đạo trước khi tính vị trí hai đầu bước.
        constexpr float halfW = Config::CLOUD_HALF_W + Config::CLOUD_DRIFT_X + Config::WIND_MAX_OFFSET;
        constexpr float halfD = Config::CLOUD_HALF_D + Config::CLOUD_DRIFT_Z + Config::WIND_MAX_OFFSET;
        if (std::min(from.x, to.x) > c.x + halfW || std::max(from.x, to.x) < c.x - halfW ||
            std::min(from.z, to.z) > c.z + halfD || std::max(from.z, to.z) < c.z - halfD ||
            std::min(from.y, to.y) > c.y + Config::CLOUD_TOP_OFFSET + Config::CLOUD_DRIFT_Y ||
            std::max(from.y, to.y) < c.y + Config::CLOUD_BOTTOM_OFFSET - Config::CLOUD_DRIFT_Y)
        {
            continue;
        }

        const Vec3 start = c.positionAt(fromTime);
        const Vec3 end = c.positionAt(toTime);
        const Vec3 localFrom{from.x - start.x, from.y - start.y, from.z - start.z};
        const Vec3 localTo{to.x - end.x, to.y - end.y, to.z - end.z};
        if (localFrom.y <= localTo.y)
        {
            continue;
        }
        CloudContact localContact;
        localContact.fraction = closest;
        if (surfaces_[c.model].cross(localFrom, localTo, localContact))
        {
            closest = localContact.fraction;
            contact = localContact;
            contact.pos = {contact.pos.x + end.x, contact.pos.y + end.y, contact.pos.z + end.z};
            contact.platformShift = {end.x - start.x, end.y - start.y, end.z - start.z};
            hit = &c;
        }
    }
    return hit;
}

void CloudField::addCloud(float x, float y, float z)
{
    if (count_ == Config::CLOUD_POOL_SIZE)
    {
        head_ = (head_ + 1) % Config::CLOUD_POOL_SIZE;
        --count_;
    }

    Cloud& c = pool_[static_cast<size_t>((head_ + count_) % Config::CLOUD_POOL_SIZE)];
    ++count_;

    c = Cloud{};
    c.id = nextId_++;
    c.x = x;
    c.y = y;
    c.z = z;
    const auto& entry = Config::WIND_LESSON[static_cast<size_t>(c.id)];
    c.model = entry.model;
    c.movable = entry.movable;
}

void CloudField::extend(float playerZ)
{
    while (count_ > 1 && first().z < playerZ - Config::DESPAWN_BEHIND)
    {
        head_ = (head_ + 1) % Config::CLOUD_POOL_SIZE;
        --count_;
    }
}

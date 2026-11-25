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

Vec3 Cloud::positionAt(double time) const
{
    const double phase = id * static_cast<double>(Config::CLOUD_DRIFT_PHASE);
    auto wave = [time](float range, float rate, double offset) {
        return range * static_cast<float>(std::sin(time * rate + offset));
    };
    return {x + wave(Config::CLOUD_DRIFT_X, Config::CLOUD_DRIFT_RATE_X, phase),
            y + wave(Config::CLOUD_DRIFT_Y, Config::CLOUD_DRIFT_RATE_Y, phase * 1.4),
            z + wave(Config::CLOUD_DRIFT_Z, Config::CLOUD_DRIFT_RATE_Z, phase * 0.7)};
}

CloudField::CloudField()
    : rng_(std::random_device{}())
{
}

float CloudField::rnd(float a, float b)
{
    std::uniform_real_distribution<float> dist(a, b);
    return dist(rng_);
}

void CloudField::reset()
{
    head_ = 0;
    count_ = 0;
    nextId_ = 0;
    time_ = 0.0;
    previousTime_ = 0.0;
    addCloud(0.0f, 0.0f, 0.0f);
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
        constexpr float halfW = Config::CLOUD_HALF_W + Config::CLOUD_DRIFT_X;
        constexpr float halfD = Config::CLOUD_HALF_D + Config::CLOUD_DRIFT_Z;
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

    c.id = nextId_++;
    c.x = x;
    c.y = y;
    c.z = z;
    if (c.id == 0)
    {
        c.model = Config::CLOUD_START_MODEL;
    }
    else
    {
        // Luân phiên đủ năm hình mây ngay trong đoạn leo đầu tiên.
        c.model = static_cast<size_t>(c.id) % Config::CLOUD_PLATFORM_COUNT;
    }
}

void CloudField::extend(float playerZ)
{
    while (last().z < playerZ + Config::SPAWN_AHEAD)
    {
        const Cloud& p = last();
        const float nx = std::clamp(p.x + rnd(-Config::CLOUD_DX_RANGE, Config::CLOUD_DX_RANGE),
                                    -Config::CLOUD_X_LIMIT, Config::CLOUD_X_LIMIT);
        const float tier = nextId_ % Config::CLOUDS_PER_TIER == 0 ? Config::CLOUD_TIER_BOOST : 0.0f;
        const float ny = p.y + rnd(Config::CLOUD_DY_MIN, Config::CLOUD_DY_MAX) + tier;
        const float nz = p.z + rnd(Config::CLOUD_GAP_MIN, Config::CLOUD_GAP_MAX);
        addCloud(nx, ny, nz);
    }

    while (count_ > 1 && first().z < playerZ - Config::DESPAWN_BEHIND)
    {
        head_ = (head_ + 1) % Config::CLOUD_POOL_SIZE;
        --count_;
    }
}

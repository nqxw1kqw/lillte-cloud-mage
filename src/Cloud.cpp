#include "Cloud.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

void CloudSurface::addTriangle(const Vec3& a, const Vec3& b, const Vec3& c)
{
    footprint_.push_back(a);
    footprint_.push_back(b);
    footprint_.push_back(c);
    baked_=false;
    const float bx=b.x-a.x,bz=b.z-a.z,cx=c.x-a.x,cz=c.z-a.z;
    const float determinant=bx*cz-cx*bz;
    if (determinant<=Config::CLOUD_CONTACT_EPSILON) return;
    const float by=b.y-a.y,cy=c.y-a.y;
    roof_.push_back({a,b,c,determinant,(by*cz-cy*bz)/determinant,(bx*cy-cx*by)/determinant,
                    std::min({a.x,b.x,c.x}),std::max({a.x,b.x,c.x}),
                    std::min({a.z,b.z,c.z}),std::max({a.z,b.z,c.z})});
}

void CloudSurface::buildFootprint()
{
    auto turn = [](const Vec3& a, const Vec3& b, const Vec3& c) {
        return (b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x);
    };
    std::sort(footprint_.begin(), footprint_.end(), [](const Vec3& a, const Vec3& b) {
        return a.x < b.x || (a.x == b.x && a.z < b.z);
    });
    footprint_.erase(std::unique(footprint_.begin(), footprint_.end(), [](const Vec3& a, const Vec3& b) {
        return a.x == b.x && a.z == b.z;
    }), footprint_.end());
    if (footprint_.size() < 3) return;
    std::vector<Vec3> hull;
    hull.reserve(footprint_.size()*2);
    for (const Vec3& p : footprint_) {
        while (hull.size() >= 2 && turn(hull[hull.size()-2], hull.back(), p) <= 0) hull.pop_back();
        hull.push_back(p);
    }
    const size_t lower = hull.size();
    for (size_t i = footprint_.size()-1; i-- > 0;) {
        const Vec3& p = footprint_[i];
        while (hull.size() > lower && turn(hull[hull.size()-2], hull.back(), p) <= 0) hull.pop_back();
        hull.push_back(p);
    }
    hull.pop_back();
    footprint_ = std::move(hull);
}

bool CloudSurface::contains(float x, float z) const
{
    if (footprint_.size() < 3) return false;
    for (size_t i=0; i<footprint_.size(); ++i) {
        const Vec3& a=footprint_[i]; const Vec3& b=footprint_[(i+1)%footprint_.size()];
        if ((b.x-a.x)*(z-a.z)-(b.z-a.z)*(x-a.x) < -Config::CLOUD_CONTACT_EPSILON) return false;
    }
    return true;
}

bool CloudSurface::roofHeight(float x, float z, float& height) const
{
    if (!contains(x,z)) return false;
    height = sampledHeight(x,z);
    return true;
}

bool CloudSurface::meshHeight(float x, float z, float& height) const
{
    bool found=false;
    for (const auto& t : roof_)
    {
        constexpr float eps=Config::CLOUD_CONTACT_EPSILON;
        if (x<t.minX-eps || x>t.maxX+eps || z<t.minZ-eps || z>t.maxZ+eps) continue;
        const float dx=x-t.a.x,dz=z-t.a.z;
        const float u=(dx*(t.c.z-t.a.z)-dz*(t.c.x-t.a.x))/t.determinant;
        const float v=((t.b.x-t.a.x)*dz-(t.b.z-t.a.z)*dx)/t.determinant;
        if (u>=-Config::CLOUD_CONTACT_EPSILON && v>=-Config::CLOUD_CONTACT_EPSILON &&
            u+v<=1+Config::CLOUD_CONTACT_EPSILON)
        {
            const float y=t.a.y+t.slopeX*dx+t.slopeZ*dz;
            if (!found || y>height) height=y;
            found=true;
        }
    }
    return found;
}

float CloudSurface::sourceHeight(float x, float z) const
{
    float height=0;
    if (meshHeight(x,z,height)) return height;
    float nearest=1e30f,nearestHeight=0;
    // Khe giữa các múi dùng độ cao mép gần nhất, không dựng lại sàn ngang ở Y=0.
    for (const auto& t : roof_)
    {
        const std::array<Vec3,3> corners{t.a,t.b,t.c};
        for (size_t i=0;i<3;++i)
        {
            const Vec3& a=corners[i];const Vec3& b=corners[(i+1)%3];
            const float dx=b.x-a.x,dz=b.z-a.z,length=dx*dx+dz*dz;
            const float f=length>0 ? std::clamp(((x-a.x)*dx+(z-a.z)*dz)/length,0.0f,1.0f) : 0;
            const float px=a.x+f*dx-x,pz=a.z+f*dz-z,distance=px*px+pz*pz;
            const float y=a.y+f*(b.y-a.y);
            if (distance<nearest || (distance==nearest && y>nearestHeight))
            {
                nearest=distance;nearestHeight=y;
            }
        }
    }
    return nearestHeight;
}

void CloudSurface::bakeHeightfield()
{
    for (int z=0;z<Config::CLOUD_HEIGHT_ROWS;++z) for(int x=0;x<Config::CLOUD_HEIGHT_COLS;++x)
    {
        const float px=-Config::CLOUD_HALF_W+x*Config::CLOUD_HEIGHT_GRID_STEP;
        const float pz=-Config::CLOUD_HALF_D+z*Config::CLOUD_HEIGHT_GRID_STEP;
        const float crown=Config::CLOUD_CROWN_HEIGHT*std::exp(-.018f*px*px-.012f*pz*pz);
        const float ripple=Config::CLOUD_RIPPLE_HEIGHT*std::sin(px*.6f)*std::sin(pz*.45f);
        float lobes=0;
        for (const auto& puff : Config::CLOUD_PUFFS)
        {
            const float dx=(px-puff.x)/puff.rx,dz=(pz-puff.z)/puff.rz;
            const float peak=Config::CLOUD_LOBE_HEIGHT*(.75f+.25f*puff.ry/2.5f);
            lobes=std::max(lobes,peak*std::exp(-Config::CLOUD_LOBE_SHARPNESS*(dx*dx+dz*dz)));
        }
        heights_[static_cast<size_t>(z*Config::CLOUD_HEIGHT_COLS+x)]=
            sourceHeight(px,pz)*Config::CLOUD_SOURCE_RELIEF+crown+ripple+lobes;
    }
    for (int pass=0;pass<Config::CLOUD_SURFACE_SMOOTH_PASSES;++pass)
    {
        const auto previous=heights_;
        for (int z=0;z<Config::CLOUD_HEIGHT_ROWS;++z) for(int x=0;x<Config::CLOUD_HEIGHT_COLS;++x)
        {
            float sum=0;
            for (int dz=-1;dz<=1;++dz) for(int dx=-1;dx<=1;++dx)
            {
                const int col=std::clamp(x+dx,0,Config::CLOUD_HEIGHT_COLS-1);
                const int row=std::clamp(z+dz,0,Config::CLOUD_HEIGHT_ROWS-1);
                sum+=previous[static_cast<size_t>(row*Config::CLOUD_HEIGHT_COLS+col)];
            }
            heights_[static_cast<size_t>(z*Config::CLOUD_HEIGHT_COLS+x)]=sum/9;
        }
    }
    baked_=true;
}

float CloudSurface::sampledHeight(float x, float z) const
{
    if (!baked_) return sourceHeight(x,z);
    const float u=std::clamp((x+Config::CLOUD_HALF_W)/Config::CLOUD_HEIGHT_GRID_STEP,
                             0.0f,static_cast<float>(Config::CLOUD_HEIGHT_COLS-1));
    const float v=std::clamp((z+Config::CLOUD_HALF_D)/Config::CLOUD_HEIGHT_GRID_STEP,
                             0.0f,static_cast<float>(Config::CLOUD_HEIGHT_ROWS-1));
    const int col=std::min(static_cast<int>(u),Config::CLOUD_HEIGHT_COLS-2);
    const int row=std::min(static_cast<int>(v),Config::CLOUD_HEIGHT_ROWS-2);
    const float fx=u-col,fz=v-row;
    auto at=[&](int dx,int dz){return heights_[static_cast<size_t>((row+dz)*Config::CLOUD_HEIGHT_COLS+col+dx)];};
    // Cùng đường chéo với mesh vẽ: mặt nhìn thấy và mặt đáp trùng nhau.
    if (fx+fz<=1) return at(0,0)+(at(1,0)-at(0,0))*fx+(at(0,1)-at(0,0))*fz;
    return at(1,1)+(at(0,1)-at(1,1))*(1-fx)+(at(1,0)-at(1,1))*(1-fz);
}

bool CloudSurface::cross(const Vec3& from, const Vec3& to, CloudContact& contact) const
{
    if (from.y<=to.y || to.y>Config::CLOUD_TOP_OFFSET) return false;
    auto point=[&](float t){return Vec3{from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t,from.z+(to.z-from.z)*t};};
    auto gap=[&](float t){const auto p=point(t);return p.y-sampledHeight(p.x,p.z);};
    float previous=gap(0),left=0;
    for (int i=1;i<=Config::CLOUD_CONTACT_SUBSTEPS;++i)
    {
        const float right=static_cast<float>(i)/Config::CLOUD_CONTACT_SUBSTEPS;
        const float current=gap(right);
        if (previous>=-Config::CLOUD_CONTACT_EPSILON && current<=Config::CLOUD_CONTACT_EPSILON && previous>current)
        {
            float low=left,high=right;
            for (int j=0;j<Config::CLOUD_CONTACT_BISECTIONS;++j)
            {
                const float mid=(low+high)*.5f;
                if (gap(mid)>0) low=mid; else high=mid;
            }
            const Vec3 p=point(high);
            if (high<=contact.fraction && contains(p.x,p.z))
            {
                constexpr float step=Config::CLOUD_HEIGHT_GRID_STEP;
                contact={{p.x,sampledHeight(p.x,p.z),p.z},high,
                    (sampledHeight(p.x+step,p.z)-sampledHeight(p.x-step,p.z))/(2*step),
                    (sampledHeight(p.x,p.z+step)-sampledHeight(p.x,p.z-step))/(2*step)};
                return true;
            }
        }
        previous=current;left=right;
    }
    return false;
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

void CloudField::reset(unsigned int seed)
{
    head_ = 0;
    count_ = 0;
    nextId_ = 0;
    time_ = 0.0;
    previousTime_ = 0.0;
    std::mt19937 rng(seed == 0 ? std::random_device{}() : seed);
    std::uniform_real_distribution<float> turn(-Config::STAIR_TURN_MAX, Config::STAIR_TURN_MAX);
    std::uniform_int_distribution<int> model(0, static_cast<int>(Config::CLOUD_PLATFORM_COUNT)-1);
    float x=0, y=0, z=0, heading=0;
    for (int i=0; i<Config::STAIR_COUNT; ++i) {
        if (i>0) {
            const auto& stage = Config::stairDifficulty(i+1);
            std::uniform_real_distribution<float> distance(stage.distanceMin,stage.distanceMax);
            std::uniform_real_distribution<float> rise(stage.riseMin,stage.riseMax);
            heading=std::clamp(heading+turn(rng),-Config::STAIR_HEADING_LIMIT,Config::STAIR_HEADING_LIMIT);
            const float step=distance(rng);
            x+=std::sin(heading)*step; z+=std::cos(heading)*step; y+=rise(rng);
        }
        addCloud(x,y,z);
        Cloud& cloud=pool_[static_cast<size_t>(i)];
        cloud.model=i==0 ? 0 : static_cast<size_t>(model(rng));
        cloud.movable=i>0 && i!=Config::MOTH_CLOUD_ID && i!=Config::MOTH_LAST_CLOUD_ID && i!=Config::STAIR_COUNT-1;
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

    float roof=0;
    if (!surfaces_[cloud->model].roofHeight(localX,localZ,roof))
    {
        return false;
    }

    height = p.y + roof;
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

}

void CloudField::extend(float /*playerZ*/)
{
}

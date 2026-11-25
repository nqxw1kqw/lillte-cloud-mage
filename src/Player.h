#pragma once

class CloudField;
struct Cloud;

// Vector 3 chiều tối giản để logic không cần kiểu VECTOR của DxLib.
struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Điểm phù thủy sẽ đáp xuống nếu giữ chuột đứng yên; Render vẽ dấu tại đây.
struct LandingMarker
{
    bool visible = false;
    Vec3 pos{};
    float slopeX = 0.0f;
    float slopeZ = 0.0f;
};

// Phù thủy: nhảy, bay, rơi và đáp lên mây. Không gọi DxLib.
class Player
{
public:
    enum class Motion { Idle, Fly, Jump, Fall, Land, Bounce, Count };
    // Đặt phù thủy đứng yên trên mặt mây có độ cao top.
    void reset(float top);

    // Giữ hướng đã chọn trên mây khi lấy đà.
    void jump(const Vec3& direction);

    // collide = false khi đã thua, để phù thủy rơi xuyên qua mây như bản gốc.
    void update(float dt, const Vec3& direction, const CloudField& clouds, bool collide, int& score, float turn = 0.0f);

    bool isBelowSea() const;

    // Mô phỏng đường bay với hướng điều khiển giữ nguyên, cùng luật như update().
    // Trả về true và điểm trên mặt mây đầu tiên sẽ đáp; false nếu sẽ rơi biển.
    bool predictLanding(const Vec3& direction, const CloudField& clouds, LandingMarker& marker) const;

    // Sau khi thua: chạm mặt biển thì dừng hẳn, để camera không bám theo xuống dưới biển.
    void stopAtSea(float surfaceY);

    const Vec3& position() const { return pos_; }
    float rotX() const { return rotX_; }
    float rotZ() const { return rotZ_; }
    float yaw() const { return yaw_; }
    float runTime() const { return runTime_; }
    bool grounded() const { return grounded_; }
    Motion motion() const { return motion_; }
    float motionTime() const { return motionTime_; }

private:
    void walk(float dt, const Vec3& direction, float turn, const CloudField& clouds);
    void land(const Cloud& c, const Vec3& point, float impact, int& score);
    void setMotion(Motion motion);
    void updateMotion(float dt);
    float takeoffSpeed() const;

    Vec3 pos_{};
    Vec3 vel_{};
    float rotX_ = 0.0f;
    float rotZ_ = 0.0f;
    float yaw_ = 0.0f;
    float launchYaw_ = 0.0f;
    float runTime_ = 0.0f;
    float runupDistance_ = 0.0f;
    float launchSpeed_ = 0.0f;
    float groundSpeed_ = 0.0f;
    float jumpBuffer_ = 0.0f;
    Vec3 bufferedDirection_{};
    int bounces_ = 0;
    int supportId_ = 0;
    bool grounded_ = true;
    bool atSea_ = false;
    Motion motion_ = Motion::Idle;
    float motionTime_ = 0.0f;
};

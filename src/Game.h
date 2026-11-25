#pragma once

#include "Camera.h"
#include "Cloud.h"
#include "Input.h"
#include "Player.h"
#include "Render.h"

// Trạng thái và vòng đời của một ván chơi.
class Game
{
public:
    enum class State
    {
        Ready,
        Play,
        Over
    };

    // Gọi sau DxLib_Init. Trả về false nếu không thiết lập được phần vẽ;
    // initError() cho biết bước nào hỏng để main báo lỗi đúng nguyên nhân.
    bool Init();
    void Update(float dt);
    void Render();
    void Reset();

    ::Render::InitError initError() const { return initError_; }
    bool quitRequested() const { return quitRequested_; }

private:
    void action();
    void step(float dt);
    // Đang tạm dừng (mất focus) hoặc đang hỏi thoát: đóng băng thời gian ván chơi.
    bool frozen() const { return paused_ || confirmQuit_; }
    bool canRestart() const { return overTicks_ >= Config::RESTART_DELAY_TICKS; }

    Input input_;
    Player player_;
    CloudField clouds_;
    Camera camera_;
    ::Render render_;
    State state_ = State::Ready;
    ::Render::InitError initError_ = ::Render::InitError::None;
    int score_ = 0;
    LandingMarker landingMarker_{};
    int overTicks_ = 0;
    double physicsTime_ = 0.0;
    bool paused_ = false;
    bool confirmQuit_ = false;
    bool quitRequested_ = false;
};

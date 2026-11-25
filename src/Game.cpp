#include "Game.h"

#include "Config.h"

bool Game::Init()
{
    initError_ = render_.init(clouds_);
    if (initError_ != ::Render::InitError::None)
    {
        return false;
    }
    Reset();
    return true;
}

void Game::Reset()
{
    clouds_.reset();
    player_.reset(clouds_.startHeight());
    clouds_.extend(player_.position().z);
    camera_.reset(player_.position());
    score_ = 0;
    landingMarker_ = LandingMarker{};
    overTicks_ = 0;
    physicsTime_ = 0.0;
    windTarget_ = -1;
    heldCloud_ = -1;
    pullingWind_ = false;
    paused_ = false;
    confirmQuit_ = false;
    state_ = State::Ready;
}

void Game::action()
{
    if (state_ == State::Over || state_ == State::Won)
    {
        // Chờ một chút để cú bấm lúc đang rơi không vô tình chơi lại ngay.
        if (!canRestart())
        {
            return;
        }
        Reset();
        return;
    }
    if (state_ == State::Ready)
    {
        state_ = State::Play;
    }
    player_.jump(camera_.worldDirection(input_.moveX(), input_.moveZ()), input_.sprintHeld());
}

void Game::Update(float dt)
{
    input_.update(!confirmQuit_ && state_ != State::Over && state_ != State::Won);
    paused_ = state_ != State::Over && state_ != State::Won && !input_.windowActive();

    if (!input_.windHeld() || !input_.windowActive() || input_.quitPressed())
    {
        clouds_.releaseCloud(heldCloud_);
        heldCloud_ = -1;
        pullingWind_ = false;
    }

    if (input_.quitPressed())
    {
        if (state_ == State::Play && !confirmQuit_)
        {
            confirmQuit_ = true;
            input_.releaseMouse();
        }
        else
        {
            quitRequested_ = true;
            input_.releaseMouse();
            return;
        }
    }
    else if (confirmQuit_ && input_.actionPressed())
    {
        // Cú bấm để chơi tiếp chỉ đóng hộp hỏi, không tính là nhảy.
        confirmQuit_ = false;
        return;
    }

    if (frozen())
    {
        return;
    }
    if (input_.restartPressed())
    {
        Reset();
        input_.releaseMouse();
        return;
    }
    camera_.orbit(input_.mouseDx(), input_.mouseDy());
    camera_.update(0.0f, player_.position());
    windTarget_ = clouds_.aimCloud(camera_.position(), camera_.target(), player_.position(), player_.supportingCloud());
    if (input_.windHeld() && heldCloud_ == -1)
    {
        heldCloud_ = windTarget_;
    }
    pullingWind_ = heldCloud_ != -1 && input_.windHeld();
    if (pullingWind_)
    {
        windTarget_ = heldCloud_;
    }
    if (input_.actionPressed())
    {
        action();
        if (state_ == State::Ready)
        {
            return;
        }
    }

    physicsTime_ += static_cast<double>(dt);
    while (physicsTime_ >= Config::PHYSICS_DT)
    {
        step(static_cast<float>(Config::PHYSICS_DT));
        physicsTime_ -= Config::PHYSICS_DT;
    }

    landingMarker_.visible = state_ != State::Over && state_ != State::Won &&
        player_.predictLanding(camera_.worldDirection(input_.moveX(), input_.moveZ()), clouds_, landingMarker_);
    camera_.update(dt, player_.position());
}

void Game::step(float dt)
{
    if (pullingWind_)
    {
        const Vec3 p = player_.position();
        const Vec3 c = clouds_.cloudPosition(heldCloud_);
        const float dx = c.x - p.x;
        const float dy = c.y - p.y;
        const float dz = c.z - p.z;
        if (heldCloud_ == player_.supportingCloud() ||
            dx * dx + dy * dy + dz * dz > Config::WIND_RANGE * Config::WIND_RANGE)
        {
            clouds_.releaseCloud(heldCloud_);
            heldCloud_ = -1;
            pullingWind_ = false;
        }
        else
        {
            const Vec3 forward = camera_.worldDirection(0.0f, 1.0f);
            const Vec3 anchor = clouds_.cloudPosition(player_.supportingCloud());
            clouds_.pullCloud(heldCloud_, {anchor.x + forward.x * Config::WIND_PLACE_DISTANCE, c.y,
                                          anchor.z + forward.z * Config::WIND_PLACE_DISTANCE});
        }
    }
    if (state_ == State::Over || state_ == State::Won)
    {
        if (!canRestart())
        {
            ++overTicks_;
        }
        return;
    }
    clouds_.advance(dt);

    const Vec3 direction = camera_.worldDirection(input_.moveX(), input_.moveZ());
    player_.update(dt, direction, clouds_, state_ == State::Play, score_, input_.turn(), input_.sprintHeld());
    if (state_ == State::Play && player_.isBelowSea())
    {
        state_ = State::Over;
        overTicks_ = 0;
        player_.stopAtSea(Config::SEA_Y);
        input_.releaseMouse();
    }
    if (state_ == State::Play && player_.grounded() &&
        player_.supportingCloud() == Config::STAIR_COUNT - 1)
    {
        state_ = State::Won;
        overTicks_ = 0;
        input_.releaseMouse();
    }
    if (state_ == State::Over || state_ == State::Won)
    {
        clouds_.releaseCloud(heldCloud_);
        heldCloud_ = -1;
        pullingWind_ = false;
    }
    clouds_.extend(player_.position().z);
}

void Game::Render()
{
    render_.beginFrame(camera_);
    render_.drawWorld(player_, clouds_, camera_, landingMarker_);
    render_.drawHud(score_, player_.position().y);
    if (state_ == State::Ready || state_ == State::Play)
    {
        render_.drawWindLesson(player_, clouds_, windTarget_, pullingWind_);
        render_.drawJumpGuide(player_, landingMarker_);
    }
    if (paused_)
    {
        render_.drawPausedMessage();
    }
    else if (confirmQuit_)
    {
        render_.drawQuitConfirmMessage();
    }
    else if (state_ == State::Ready)
    {
        render_.drawReadyMessage();
    }
    else if (state_ == State::Over)
    {
        render_.drawOverMessage(score_, canRestart());
    }
    else if (state_ == State::Won)
    {
        render_.drawWonMessage(canRestart());
    }
}

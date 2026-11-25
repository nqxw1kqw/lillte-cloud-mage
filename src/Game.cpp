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
    paused_ = false;
    confirmQuit_ = false;
    state_ = State::Ready;
}

void Game::action()
{
    if (state_ == State::Over)
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
    player_.jump(camera_.worldDirection(input_.moveX(), input_.moveZ()));
}

void Game::Update(float dt)
{
    input_.update(!confirmQuit_ && state_ != State::Over);
    paused_ = state_ != State::Over && !input_.windowActive();

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
    camera_.orbit(input_.mouseDx(), input_.mouseDy());
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

    landingMarker_.visible = state_ != State::Over &&
        player_.predictLanding(camera_.worldDirection(input_.moveX(), input_.moveZ()), clouds_, landingMarker_);
    camera_.update(dt, player_.position());
}

void Game::step(float dt)
{
    clouds_.advance(dt);
    if (state_ == State::Over)
    {
        if (!canRestart())
        {
            ++overTicks_;
        }
        return;
    }

    const Vec3 direction = camera_.worldDirection(input_.moveX(), input_.moveZ());
    player_.update(dt, direction, clouds_, state_ == State::Play, score_, input_.turn());
    if (state_ == State::Play && player_.isBelowSea())
    {
        state_ = State::Over;
        overTicks_ = 0;
        player_.stopAtSea(Config::SEA_Y);
        input_.releaseMouse();
    }
    clouds_.extend(player_.position().z);
}

void Game::Render()
{
    render_.beginFrame(camera_);
    render_.drawWorld(player_, clouds_, camera_, landingMarker_);
    render_.drawHud(score_, player_.position().y);
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
}

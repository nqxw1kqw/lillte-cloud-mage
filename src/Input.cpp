#include "Input.h"

#include "DxLib.h"
#include "Config.h"

void Input::releaseMouse()
{
    mouseCentered_ = false;
    mouseDx_ = 0.0f;
    mouseDy_ = 0.0f;
    if (cursorHidden_ && SetMouseDispFlag(TRUE) != -1)
    {
        cursorHidden_ = false;
    }
}

void Input::update(bool captureMouse)
{
    const bool active = GetWindowActiveFlag() == TRUE;
    const bool focusRegained = active && !windowActive_;
    windowActive_ = active;

    actionPressed_ = false;
    quitPressed_ = false;
    windHeld_ = false;
    restartPressed_ = false;
    moveX_ = 0.0f;
    moveZ_ = 0.0f;
    turn_ = 0.0f;
    mouseDx_ = 0.0f;
    mouseDy_ = 0.0f;
    if (!active)
    {
        releaseMouse();
        return;
    }

    const bool space = CheckHitKey(KEY_INPUT_SPACE) == 1;
    const bool esc = CheckHitKey(KEY_INPUT_ESCAPE) == 1;
    const bool restart = CheckHitKey(KEY_INPUT_R) == 1;
    const int buttons = GetMouseInput();
    const bool left = (buttons & MOUSE_INPUT_LEFT) != 0;
    const bool right = (buttons & MOUSE_INPUT_RIGHT) != 0;
    if (focusRegained || !captureMouse)
    {
        windArmed_ = false;
    }
    if (!right)
    {
        windArmed_ = true;
    }
    windHeld_ = captureMouse && !focusRegained && right && windArmed_;
    int mx = 0;
    int my = 0;
    const bool hasMousePosition = GetMousePoint(&mx, &my) == 0;
    const bool inClient = hasMousePosition && mx >= 0 && mx < Config::WINDOW_W &&
        my >= 0 && my < Config::WINDOW_H;
    actionPressed_ = !focusRegained && ((space && !prevSpace_) || (inClient && left && !prevLeft_));
    quitPressed_ = !focusRegained && esc && !prevEsc_;
    restartPressed_ = !focusRegained && restart && !prevRestart_;
    prevSpace_ = space;
    prevLeft_ = left;
    prevEsc_ = esc;
    prevRestart_ = restart;

    auto held = [](int key, int arrow) {
        return CheckHitKey(key) == 1 || CheckHitKey(arrow) == 1;
    };
    moveX_ = static_cast<float>(held(KEY_INPUT_D, KEY_INPUT_RIGHT)) -
        static_cast<float>(held(KEY_INPUT_A, KEY_INPUT_LEFT));
    moveZ_ = static_cast<float>(held(KEY_INPUT_W, KEY_INPUT_UP)) -
        static_cast<float>(held(KEY_INPUT_S, KEY_INPUT_DOWN));
    turn_ = static_cast<float>(CheckHitKey(KEY_INPUT_E) == 1) -
        static_cast<float>(CheckHitKey(KEY_INPUT_Q) == 1);

    if (!captureMouse || !hasMousePosition)
    {
        releaseMouse();
        return;
    }

    // Mỗi khung trả chuột về tâm để người chơi quay tròn mà không vướng biên màn hình.
    // Bỏ mẫu đầu sau khi lấy lại focus để camera không giật theo vị trí con trỏ cũ.
    constexpr int centerX = Config::WINDOW_W / 2;
    constexpr int centerY = Config::WINDOW_H / 2;
    if ((mx != centerX || my != centerY) && SetMousePoint(centerX, centerY) == -1)
    {
        releaseMouse();
        return;
    }
    if (mouseCentered_ && !focusRegained)
    {
        mouseDx_ = static_cast<float>(mx - centerX);
        mouseDy_ = static_cast<float>(my - centerY);
    }
    mouseCentered_ = true;
    if (!cursorHidden_ && SetMouseDispFlag(FALSE) != -1)
    {
        cursorHidden_ = true;
    }
}

#pragma once

// Chỉ lớp này đọc phím và chuột từ DxLib.
class Input
{
public:
    ~Input() { releaseMouse(); }

    void update(bool captureMouse);
    void releaseMouse();

    bool actionPressed() const { return actionPressed_; }
    bool quitPressed() const { return quitPressed_; }
    bool windowActive() const { return windowActive_; }
    float mouseDx() const { return mouseDx_; }
    float mouseDy() const { return mouseDy_; }
    float moveX() const { return moveX_; }
    float moveZ() const { return moveZ_; }
    float turn() const { return turn_; }

private:
    bool prevSpace_ = false;
    bool prevLeft_ = false;
    bool prevEsc_ = false;
    bool actionPressed_ = false;
    bool quitPressed_ = false;
    bool windowActive_ = true;
    bool mouseCentered_ = false;
    bool cursorHidden_ = false;
    float mouseDx_ = 0.0f;
    float mouseDy_ = 0.0f;
    float moveX_ = 0.0f;
    float moveZ_ = 0.0f;
    float turn_ = 0.0f;
};

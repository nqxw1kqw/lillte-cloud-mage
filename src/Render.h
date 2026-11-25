#pragma once

#include <array>
#include <memory>

#include "Config.h"
#include "Player.h"

class Camera;
class CloudField;

// Nơi duy nhất gọi hàm vẽ, font và thiết lập 3D của DxLib.
class Render
{
public:
    // Bước khởi tạo bị hỏng, để main báo lỗi đúng nguyên nhân cho người chơi.
    enum class InitError
    {
        None,
        ZBuffer,
        Culling,
        Fog,
        Light,
        Material,
        Font,
        Models
    };

    Render();
    ~Render();
    Render(const Render&) = delete;
    Render& operator=(const Render&) = delete;

    // Gọi sau DxLib_Init. Trả về InitError::None nếu mọi bước quan trọng đều thành công.
    InitError init(CloudField& clouds);
    void shutdown();

    void beginFrame(const Camera& camera);
    void drawWorld(const Player& player, const CloudField& clouds, const Camera& camera, const LandingMarker& marker);
    void drawHud(int score, float altitude);
    void drawReadyMessage();
    void drawOverMessage(int score, bool canRestart);
    void drawPausedMessage();
    void drawQuitConfirmMessage();
    void drawWindLesson(const Player& player, const CloudField& clouds, int targetId, bool pulling);
    void drawWonMessage(bool canRestart);

private:
    // Đỉnh của sao dùng kiểu VERTEX3D của DxLib; giấu trong Render.cpp để
    // Game.h (include Render.h) không kéo theo DxLib.h vào logic game.
    struct StarBatch;
    struct WitchRun;
    struct CloudPuffMesh;

    bool loadModels(CloudField& clouds);
    bool loadCloudSurface(int model, CloudField& clouds, size_t variant);
    void loadFonts();
    int createFont(const wchar_t* face, int size, int thick);
    void drawEnvironment(const Vec3& playerPos, const Camera& camera);
    void drawStars(const Vec3& playerPos, const Camera& camera);
    void initCloudPuffs(const CloudField& clouds);
    void drawCloudPuffs(const Vec3& p, size_t variant);
    void drawClouds(const CloudField& clouds);
    void drawSceneryClouds(const CloudField& clouds, const Camera& camera, bool transparent);
    // Đĩa bán trong suốt trên mặt mây sẽ đáp, chỉ vẽ khi marker.visible.
    void drawLandingMarker(const LandingMarker& marker);
    void drawWitch(const Player& player);
    void updateRunPose(float runTime);
    void drawCenterLines(const wchar_t* const* lines, const int* fonts, int lineCount,
                         int centerY = Config::WINDOW_H / 2);
    // Ghi log một lần cho mỗi chỗ gọi DxLib bị lỗi, tránh tràn Log.txt mỗi khung.
    void check(int result, const wchar_t* what);

    std::array<Vec3, Config::STAR_COUNT> stars_{};
    // Cấp phát một lần khi khởi tạo, dùng lại mọi khung.
    std::unique_ptr<StarBatch> starBatch_;
    std::unique_ptr<CloudPuffMesh> cloudPuffs_;
    // Cạnh sao theo đơn vị thế giới, tính một lần trong init từ STAR_POINT_SIZE_THREE.
    float starWorldSize_ = 0.0f;
    // Số font đã nạp riêng từ assets/fonts, để gỡ ra khi thoát.
    static constexpr int MAX_PRIVATE_FONTS = 16;
    std::array<std::array<wchar_t, 260>, MAX_PRIVATE_FONTS> privateFontPaths_{};
    int privateFontCount_ = 0;
    static constexpr int MAX_LOGGED_ERRORS = 32;
    std::array<const wchar_t*, MAX_LOGGED_ERRORS> loggedErrors_{};
    int loggedErrorCount_ = 0;
    int fontHud_ = -1;
    int fontMsg_ = -1;
    int fontMsgSmall_ = -1;
    int fillLight_ = -1;
    int witchModel_ = -1;
    std::unique_ptr<WitchRun> witchRun_;
    std::array<int, Config::CLOUD_MODELS.size()> cloudModels_{};
    struct WitchClip { int handle = -1; float frames = 0.0f; };
    std::array<WitchClip, static_cast<size_t>(Player::Motion::Count)> witchClips_{};
    Player::Motion activeWitchClip_ = Player::Motion::Idle;
    bool initialized_ = false;
};

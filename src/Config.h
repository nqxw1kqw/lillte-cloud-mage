#pragma once

#include <array>

// Toàn bộ hằng số chỉnh được của game. Số liệu lấy theo bản Three.js tham chiếu
// (phu_thuy_adventures.md). Trục Z đã đổi dấu: DxLib dùng hệ tay trái nên
// "phía trước" là +Z (z_dx = -z_three).
namespace Config
{
    // Phiên bản DxLib đã kiểm thử. Đổi phiên bản phải được người dùng cho phép.
    constexpr const wchar_t* DXLIB_VERSION_USED = L"3.24f";

    // ===== Cửa sổ =====
    constexpr int WINDOW_W = 1280;
    constexpr int WINDOW_H = 720;
    constexpr int COLOR_BIT_DEPTH = 32;
    constexpr const wchar_t* WINDOW_TITLE = L"lillte cloud mage";

    // Hai đường mô phỏng dùng chung một bước để kết quả đáp không tùy FPS.
    constexpr double PHYSICS_DT = 1.0 / 60.0;
    constexpr float MAX_DT = 0.05f;

    // ===== Nhảy và bay =====
    constexpr float JUMP_VY = 11.4f;
    constexpr float JUMP_STAND_SPEED = 10.0f;
    constexpr float JUMP_RUN_SPEED = 16.0f;
    constexpr float JUMP_RUNUP_DISTANCE = 2.5f;
    constexpr float GROUND_MOVE_SPEED = 7.0f;
    constexpr float SPRINT_MOVE_SPEED = 11.0f;
    constexpr float JUMP_SPRINT_SPEED = 21.0f;
    constexpr float DOUBLE_JUMP_VY = 10.8f;
    constexpr int MAX_PLAYER_JUMPS = 2;
    constexpr float GROUND_ACCELERATION = 56.0f;
    constexpr float JUMP_BUFFER_TIME = 0.12f;
    constexpr float GROUND_TURN_SPEED = 2.2f;  // radian mỗi giây
    constexpr float GROUND_STEP_HEIGHT = 0.55f;
    constexpr int GROUND_FOOTING_SUBSTEPS = 8;
    constexpr float AIR_STEER_RATE = 3.0f;
    constexpr float GRAVITY = 18.0f;

    // ===== Đáp và nảy =====
    // Đáp cùng độ cao với lực rơi giữ nguyên thường không nảy.
    constexpr float BOUNCE_IMPACT_MIN = 13.0f;
    constexpr float BOUNCE_FACTOR = 0.4f;
    // Khóa chuyển động ngang khi nảy để phù thủy rơi lại đúng điểm chạm.
    constexpr int MAX_BOUNCES = 3;

    // Mặt biển cũng là ngưỡng thua.
    constexpr float SEA_Y = -20.0f;

    // ===== Nghiêng người khi bay =====
    constexpr float TILT_FACTOR = 0.03f;
    constexpr float TILT_MAX = 0.6f;

    // ===== Mây =====
    struct CloudModel
    {
        const wchar_t* path;
        float scaleX, scaleY, scaleZ;
        const wchar_t* alphaMaterial = nullptr;
    };
    // Năm mẫu đầu có mặt chạy. Cả bộ v2 đều lấy mặt trên (Y=0) làm gốc.
    constexpr std::array CLOUD_MODELS{
        CloudModel{L"assets/models/clouds_v2/sc_stratocumulus.x", 19.0f / 9.0f, 1.25f, 22.0f / 5.5f},
        CloudModel{L"assets/models/clouds_v2/cu_mediocris.x", 17.0f / 5.2f, 1.05f, 22.0f / 4.0f},
        CloudModel{L"assets/models/clouds_v2/cu_humilis.x", 15.0f / 3.0f, 1.30f, 22.0f / 2.4f},
        CloudModel{L"assets/models/clouds_v2/ac_lenticularis.x", 17.5f / 6.5f, 1.35f, 22.0f / 4.0f},
        CloudModel{L"assets/models/clouds_v2/nlc_noctilucent.x", 18.0f / 7.08f, 1.25f, 22.0f / 4.06f},
        CloudModel{L"assets/models/clouds_v2/cu_congestus.x", 2.8f, 1.8f, 2.6f},
        CloudModel{L"assets/models/clouds_v2/ac_floccus.x", 2.5f, 1.8f, 2.7f},
        CloudModel{L"assets/models/clouds_v2/ci_fibratus.x", 2.9f, 2.8f, 3.0f, L"cv2_cirrus"},
        CloudModel{L"assets/models/clouds_v2/cb_incus.x", 2.9f, 2.2f, 2.5f},
        CloudModel{L"assets/models/clouds_v2/ns_virga.x", 2.3f, 2.1f, 2.2f, L"cv2_virga"}
    };
    constexpr size_t CLOUD_PLATFORM_COUNT = 5;
    constexpr float CLOUD_HALF_W = 9.6f;
    constexpr float CLOUD_HALF_D = 11.1f;
    constexpr float CLOUD_TOP_OFFSET = 4.0f;
    constexpr float CLOUD_BOTTOM_OFFSET = -8.0f;
    constexpr float CLOUD_CONTACT_EPSILON = 0.0001f;
    constexpr float CLOUD_HEIGHT_GRID_STEP = 0.35f;
    constexpr int CLOUD_HEIGHT_COLS = static_cast<int>(2*CLOUD_HALF_W/CLOUD_HEIGHT_GRID_STEP)+2;
    constexpr int CLOUD_HEIGHT_ROWS = static_cast<int>(2*CLOUD_HALF_D/CLOUD_HEIGHT_GRID_STEP)+2;
    constexpr int CLOUD_CONTACT_SUBSTEPS = 4;
    constexpr int CLOUD_CONTACT_BISECTIONS = 12;
    constexpr float CLOUD_CROWN_HEIGHT = 1.35f;
    constexpr float CLOUD_LOBE_HEIGHT = 2.2f;
    constexpr float CLOUD_LOBE_SHARPNESS = 0.9f;
    constexpr float CLOUD_SOURCE_RELIEF = 0.35f;
    constexpr int CLOUD_SURFACE_SMOOTH_PASSES = 8;
    constexpr float CLOUD_RIPPLE_HEIGHT = 0.12f;
    constexpr float CLOUD_SHELL_DEPTH = 3.0f;
    constexpr float CLOUD_SHELL_EDGE = 0.8f;
    constexpr int CLOUD_SHELL_RINGS = 6;
    constexpr std::array CLOUD_SURFACE_COLORS{0xeff3ff,0xf3f5ff,0xeaf2ff,0xe3ebff,0xa8c7ff};
    constexpr float CLOUD_DRIFT_X = 0.80f;
    constexpr float CLOUD_DRIFT_Y = 0.36f;
    constexpr float CLOUD_DRIFT_Z = 0.52f;
    constexpr float CLOUD_DRIFT_RATE_X = 0.81f;
    constexpr float CLOUD_DRIFT_RATE_Y = 1.30f;
    constexpr float CLOUD_DRIFT_RATE_Z = 0.68f;
    constexpr float CLOUD_DRIFT_PHASE = 1.83f;

    struct CloudPuff
    {
        float x, y, z;
        float rx, ry, rz;
    };
    // Các múi nằm dưới bệ, để mặt chạy của model luôn giữ nguyên.
    constexpr std::array CLOUD_PUFFS{
        CloudPuff{-3.8f, -2.0f, -3.8f, 3.4f, 1.65f, 3.9f},
        CloudPuff{ 2.9f, -2.5f, -4.1f, 3.8f, 2.15f, 3.6f},
        CloudPuff{-4.2f, -1.9f,  1.0f, 3.2f, 1.50f, 4.1f},
        CloudPuff{ 0.1f, -2.9f, -0.1f, 4.2f, 2.50f, 4.8f},
        CloudPuff{ 4.1f, -2.2f,  2.1f, 2.8f, 1.85f, 3.6f},
        CloudPuff{-2.1f, -2.2f,  5.2f, 3.5f, 1.85f, 3.6f},
        CloudPuff{ 2.4f, -1.85f, 5.7f, 3.2f, 1.50f, 3.4f}
    };
    constexpr float CLOUD_PUFF_SURFACE_CLEARANCE = 1.15f;
    constexpr float CLOUD_PUFF_OUTSIDE_CAP = -2.30f;
    constexpr int CLOUD_PUFF_SLICES = 18;
    constexpr int CLOUD_PUFF_RINGS = 10;
    // Chỉ dùng mặt đủ thoải để đứng. Loại vách nghiêng trên 45 độ.
    constexpr float CLOUD_MAX_LANDING_SLOPE_SQUARED = 1.0f;
    constexpr int CLOUD_DECOR_PER_PLATFORM = 2;
    constexpr float CLOUD_DECOR_SIDE = 44.0f;
    constexpr float CLOUD_SCENERY_CLEARANCE = 32.0f;
    constexpr float CLOUD_DECOR_SIDE_VARIATION = 4.0f;
    constexpr size_t CLOUD_LANDMARK_MODEL = 8;
    constexpr float CLOUD_DECOR_LANDMARK_MARGIN = 14.0f;
    constexpr float CLOUD_DECOR_DEPTH = 13.0f;
    constexpr float CLOUD_DECOR_UP = 14.0f;
    constexpr float CLOUD_DECOR_DOWN = -8.0f;
    constexpr float DESPAWN_BEHIND = 40.0f;
    // Chừa thêm ô cho mây mới sinh và phần mép còn trong tầm người chơi.
    constexpr int STAIR_COUNT = 40;
    constexpr int CLOUD_POOL_SIZE = STAIR_COUNT;
    struct StairDifficulty
    {
        int lastStair;
        float distanceMin, distanceMax;
        float riseMin, riseMax;
    };
    constexpr std::array STAIR_DIFFICULTY{
        StairDifficulty{9, 24.0f, 26.0f, 2.2f, 2.8f},
        StairDifficulty{20, 28.0f, 31.0f, 3.0f, 3.6f},
        StairDifficulty{30, 33.0f, 36.0f, 3.7f, 4.3f},
        StairDifficulty{40, 38.0f, 41.0f, 4.4f, 5.0f}
    };
    constexpr const StairDifficulty& stairDifficulty(int stair)
    {
        for (const auto& stage : STAIR_DIFFICULTY)
            if (stair <= stage.lastStair) return stage;
        return STAIR_DIFFICULTY.back();
    }
    constexpr float STAIR_HEADING_LIMIT = 0.75f;
    constexpr float STAIR_TURN_MAX = 0.40f;
    constexpr float CLOUD_DRAW_DISTANCE = 170.0f;
    constexpr int MOTH_STAIR = 10;
    constexpr int MOTH_CLOUD_ID = MOTH_STAIR - 1;
    constexpr int MOTH_LAST_STAIR = 20;
    constexpr int MOTH_LAST_CLOUD_ID = MOTH_LAST_STAIR - 1;
    constexpr int MOTH_COUNT = 14; // Mỗi bệ trong vùng, tổng cộng 154 con.
    constexpr std::array BUTTERFLY_PALETTES{0xb49aff,0x69cfff,0x77dfb2,0xff9cbd,0xffce70,0xf29d68};
    constexpr int RARE_BUTTERFLY_INTERVAL = 37;
    constexpr int RARE_BUTTERFLY_OFFSET = 17;
    constexpr float RARE_BUTTERFLY_PERIOD = 14.0f;
    constexpr float RARE_BUTTERFLY_COLOR_MIN = 0.25f;
    constexpr float BUTTERFLY_EMISSION = 0.6f;
    constexpr float RARE_BUTTERFLY_EMISSION = 0.9f;
    constexpr bool inButterflyRegion(int cloudId)
    {
        return cloudId >= MOTH_CLOUD_ID && cloudId <= MOTH_LAST_CLOUD_ID;
    }
    constexpr bool rareButterfly(int cloudId, int index)
    {
        return inButterflyRegion(cloudId) &&
            ((cloudId-MOTH_CLOUD_ID)*MOTH_COUNT+index)%RARE_BUTTERFLY_INTERVAL == RARE_BUTTERFLY_OFFSET;
    }
    constexpr float MOTH_REGION_RADIUS = 24.0f;
    constexpr float MOTH_REGION_HEIGHT = 10.0f;
    constexpr float MOTH_DRAW_DISTANCE = 70.0f;
    constexpr float MOTH_ORBIT_MIN = 4.0f;
    constexpr float MOTH_ORBIT_SPREAD = 16.0f;
    constexpr float MOTH_HEIGHT_MIN = 2.0f;
    constexpr float MOTH_HEIGHT_SPREAD = 9.0f;
    constexpr float MOTH_SPEED = 0.22f;
    constexpr float MOTH_SCALE = 15.0f;
    constexpr int MOTH_COLOR = 0xbcb4ff;
    constexpr int MOTH_TITLE_Y = 118;
    constexpr float WIND_RANGE = 65.0f;
    constexpr float WIND_AIM_RADIUS = 8.0f;
    constexpr float WIND_PLACE_DISTANCE = 23.0f;
    constexpr float WIND_MAX_OFFSET = 44.0f;
    constexpr float WIND_RESPONSE = 1.7f;
    constexpr int WIND_COLOR = 0x7fe9ed;
    constexpr int WIND_ACTIVE_COLOR = 0xffd27f;
    constexpr int WIND_HUD_Y = 50;
    constexpr float WIND_RING_RADIUS = 4.0f;
    constexpr int WIND_RING_SEGMENTS = 36;
    constexpr float WIND_RING_HEIGHT = 0.2f;
    constexpr float WIND_BEAM_HEIGHT = 1.5f;
    constexpr float WIND_PULSE_RATE = 3.0f;
    constexpr int WIND_BEAM_SEGMENTS = 14;
    constexpr float WIND_BEAM_SWIRL = 0.45f;
    constexpr float WIND_GOAL_HEIGHT = 3.0f;
    constexpr float WIND_GOAL_RADIUS = 0.75f;
    constexpr int WIND_GOAL_DIVISIONS = 12;
    constexpr int WIND_AIM_CROSS_SIZE = 5;
    static_assert(CLOUD_POOL_SIZE >= STAIR_COUNT, "Bộ đệm phải chứa đủ 40 bậc");

    // ===== Camera =====
    constexpr float FULL_TURN = 6.2831853f;
    constexpr float CAMERA_DISTANCE = 14.0f;
    constexpr float CAMERA_LOOK_Y = 2.1f;
    constexpr float CAMERA_LOOK_AHEAD = 2.6f;
    constexpr float CAMERA_START_PITCH = 0.37f;
    constexpr float CAMERA_MIN_PITCH = 0.20f;
    constexpr float CAMERA_MAX_PITCH = 1.25f;
    constexpr float CAMERA_MOUSE_SENSITIVITY = 0.0035f;
    constexpr float CAMERA_FOLLOW_RATE = 18.0f;
    constexpr float CAMERA_VERTICAL_FOLLOW_RATE = 9.0f;
    constexpr float CAMERA_FOV_DEG = 70.0f;
    constexpr float CAMERA_NEAR = 0.1f;
    constexpr float CAMERA_FAR = 400.0f;
    // Lúc thua camera không được xuống sát hay dưới mặt biển, nếu không sẽ thấy mặt dưới của biển.
    constexpr float CAMERA_MIN_ABOVE_SEA = 2.0f;

    // ===== Môi trường =====
    constexpr int BG_COLOR = 0x2b3a7a;
    constexpr int FOG_COLOR = 0x2b3a7a;
    constexpr float FOG_START = 60.0f;
    constexpr float FOG_END = 230.0f;
    constexpr int SEA_COLOR = 0x1d2c5a;
    constexpr float SEA_HALF_SIZE = 600.0f;
    constexpr int MOON_COLOR = 0xfff6d5;
    constexpr float MOON_RADIUS = 8.0f;
    constexpr float MOON_OFFSET_X = -50.0f;
    constexpr float MOON_Y = 80.0f;
    constexpr float MOON_OFFSET_Z = 200.0f;
    constexpr int STAR_COUNT = 800;
    constexpr float STAR_X_RANGE = 250.0f;
    constexpr float STAR_Y_MIN = 10.0f;
    constexpr float STAR_Y_MAX = 200.0f;
    constexpr float STAR_Z_MIN = -100.0f;
    constexpr float STAR_Z_MAX = 300.0f;
    constexpr float STAR_X_FOLLOW = 1.0f;
    constexpr unsigned int STAR_SEED = 12345u;
    // PointsMaterial size 1.3 của bản Three.js (tính theo pixel, có sizeAttenuation).
    // Render::init quy ra cạnh hình vuông thế giới = 1.3 * tan(FOV/2) (~0.91 ở FOV 70)
    // để sao to bằng bản gốc, và vẫn khớp nếu CAMERA_FOV_DEG đổi.
    constexpr float STAR_POINT_SIZE_THREE = 1.3f;

    // Cảnh mẫu bên đường: bố trí theo bậc, dùng chung mesh tạo lúc khởi tạo.
    struct EnvironmentSite { int cloudId; float side; float scale; };
    constexpr std::array ENVIRONMENT_SITES{
        EnvironmentSite{3,-1.0f,0.8f}, EnvironmentSite{9,1.0f,1.05f},
        EnvironmentSite{13,-1.0f,1.0f}, EnvironmentSite{17,1.0f,0.9f},
        EnvironmentSite{19,-1.0f,1.1f}
    };
    constexpr float ENV_SIDE_DISTANCE = 48.0f;
    constexpr float ENV_PLACEMENT_STEP = 8.0f;
    constexpr int ENV_PLACEMENT_ATTEMPTS = 7;
    constexpr float ENV_PATH_MARGIN = 28.0f;
    constexpr float ENV_SCENERY_MARGIN = 22.0f;
    constexpr float ENV_FORWARD_OFFSET = 22.0f;
    constexpr float ENV_HEIGHT_OFFSET = -3.0f;
    constexpr float ENV_DRAW_DISTANCE = 190.0f;
    constexpr float ENV_BOB_HEIGHT = 0.65f;
    constexpr float ENV_BOB_SPEED = 0.5f;
    constexpr int ENV_ISLAND_SEGMENTS = 13;
    constexpr float ENV_ISLAND_RADIUS = 14.0f;
    constexpr float ENV_ISLAND_DEPTH_RATIO = 0.78f;
    constexpr float ENV_ISLAND_DROP = 17.0f;
    constexpr int ENV_SOIL_COLOR = 0x657095;
    constexpr int ENV_GRASS_COLOR = 0x659e9a;
    constexpr int ENV_STONE_COLOR = 0xb4b3cd;
    constexpr int ENV_WOOD_COLOR = 0x685873;
    constexpr int ENV_CRYSTAL_COLOR = 0x9cf5e6;
    constexpr int ENV_RUNE_COLOR = 0x8ac7f0;
    constexpr int ENV_BRANCH_SIDES = 7;
    constexpr float ENV_ARCH_X = -4.0f;
    constexpr float ENV_ARCH_Z = 1.5f;
    constexpr float ENV_ARCH_HEIGHT = 5.2f;
    constexpr float ENV_ARCH_INNER_RADIUS = 2.7f;
    constexpr float ENV_ARCH_OUTER_RADIUS = 3.8f;
    constexpr float ENV_ARCH_HALF_DEPTH = 0.65f;
    constexpr int ENV_ARCH_SEGMENTS = 9;
    constexpr float ENV_RUNE_SPEED = 0.18f;
    constexpr int ENV_SPARK_COUNT = 32;
    constexpr int ENV_SPARK_SEGMENTS = 6;
    constexpr float ENV_SPARK_RADIUS = 0.14f;
    constexpr float ENV_SPARK_RISE_SPEED = 0.55f;
    constexpr float ENV_SPARK_HEIGHT = 13.0f;
    constexpr float ENV_CANOPY_HALO_SCALE = 1.35f;
    constexpr int ENV_CANOPY_HALO_ALPHA = 30;
    struct EnvironmentCanopy { float x,y,z,rx,ry,rz; int color; };
    constexpr std::array ENV_CANOPIES{
        EnvironmentCanopy{4.2f,9.0f,-2.0f,3.1f,2.0f,2.7f,0x91e8d7},
        EnvironmentCanopy{1.7f,7.7f,-2.4f,2.5f,1.6f,2.1f,0xa8c7f4},
        EnvironmentCanopy{6.6f,7.7f,-0.8f,2.8f,1.7f,2.4f,0xc0b2f0},
        EnvironmentCanopy{4.9f,7.3f,-4.5f,2.5f,1.6f,2.1f,0x7cdecf},
        EnvironmentCanopy{4.0f,10.6f,-2.3f,2.0f,1.2f,1.8f,0xc5f4dc}
    };

    // Màu RGB dạng số thực 0..1 cho đèn và vật liệu.
    struct RgbF
    {
        float r;
        float g;
        float b;
    };

    // Hướng ánh trăng: từ (-30, 60, 20) chiếu về gốc tọa độ.
    constexpr float LIGHT_DIR_X = 30.0f;
    constexpr float LIGHT_DIR_Y = -60.0f;
    constexpr float LIGHT_DIR_Z = -20.0f;
    // Ánh trăng hơi xanh, không quá gắt để mây trắng không bị cháy sáng.
    constexpr RgbF LIGHT_DIFFUSE{ 0.75f, 0.78f, 0.88f };
    // Đèn phụ chiếu từ phía camera, thay cho phần ánh sáng trời của HemisphereLight.
    constexpr float FILL_LIGHT_DIR_X = 0.2f;
    constexpr float FILL_LIGHT_DIR_Y = -0.4f;
    constexpr float FILL_LIGHT_DIR_Z = 1.0f;
    constexpr RgbF FILL_LIGHT_DIFFUSE{ 0.55f, 0.6f, 0.8f };
    // Ambient của DxLib làm mọi thứ xám đều nên giữ thấp.
    constexpr RgbF GLOBAL_AMBIENT{ 0.12f, 0.13f, 0.18f };
    constexpr int SPHERE_DIV_MOON = 24;

    // ===== Mô hình phù thủy =====
    constexpr const wchar_t* WITCH_MODEL_PATH = L"assets/models/witch_inplace.x";
    constexpr std::array MODEL_TEXTURE_PATHS{
        L"assets/models/witch_diffuse.png",
        L"assets/models/clouds_v2/cv2_white.png", L"assets/models/clouds_v2/cv2_noctilucent.png",
        L"assets/models/clouds_v2/cv2_cirrus_alpha.png", L"assets/models/clouds_v2/cv2_grey.png",
        L"assets/models/clouds_v2/cv2_virga_alpha.png"
    };
    constexpr float WITCH_ANIMATION_FPS = 30.0f;
    // Bướm chỉ trang trí, dùng thời gian mô phỏng để dừng cùng màn chơi.
    constexpr const wchar_t* BUTTERFLY_MODEL_PATH = L"assets/models/butterfly/butterfly.mv1";
    constexpr int BUTTERFLIES_PER_CLOUD = 2;
    constexpr int BUTTERFLY_FLY_CLIP = 1;
    constexpr float BUTTERFLY_ANIMATION_FPS = 240.0f;
    constexpr float BUTTERFLY_SCALE = 12.0f;
    constexpr float BUTTERFLY_ORBIT_RADIUS = 5.0f;
    constexpr float BUTTERFLY_ORBIT_VARIATION = 1.5f;
    constexpr float BUTTERFLY_ORBIT_DEPTH = 0.72f;
    constexpr float BUTTERFLY_ORBIT_SPEED = 0.65f;
    constexpr float BUTTERFLY_HEIGHT = 3.0f;
    constexpr float BUTTERFLY_HEIGHT_VARIATION = 0.6f;
    constexpr float BUTTERFLY_BOB = 0.4f;
    constexpr float BUTTERFLY_BOB_RATE = 2.3f;
    constexpr float BUTTERFLY_BANK = 0.18f;
    constexpr float BUTTERFLY_PHASE = 1.73f;
    constexpr float BUTTERFLY_DRAW_DISTANCE = 75.0f;
    constexpr int JUMP_GUIDE_X = 18;
    constexpr int JUMP_GUIDE_BOTTOM = 170;
    constexpr int JUMP_GUIDE_LINE_HEIGHT = 26;
    constexpr int JUMP_GUIDE_BAR_WIDTH = 160;
    constexpr int JUMP_GUIDE_BAR_HEIGHT = 6;
    constexpr int JUMP_GUIDE_SAFE_COLOR = 0x87efad;
    constexpr float WITCH_JUMP_TAKEOFF_FRAME = 7.0f;
    constexpr float WITCH_LAND_CONTACT_FRAME = 3.0f;
    // Cho phép khung khom người phát khi còn đứng trên mây.
    constexpr float JUMP_WINDUP = 0.065f;
    constexpr float WITCH_JUMP_TIME = 0.36f;
    constexpr float WITCH_LAND_TIME = 0.45f;
    constexpr float WITCH_BOUNCE_TIME = 0.25f;
    constexpr float WITCH_RUN_CYCLES_PER_SECOND = 2.8f;
    constexpr float WITCH_RUN_HIP_SWING = 0.52f;
    constexpr float WITCH_RUN_KNEE_BEND = 0.68f;

    // ===== Dấu điểm đáp =====
    // Bao phủ cả cú rơi từ mây cao nhất tới biển.
    constexpr int LANDING_PREDICT_MAX_STEPS = 360;
    constexpr float LANDING_MARKER_RADIUS = 0.72f;
    // Cũng là kích thước mảng đỉnh trên stack trong Render.
    constexpr int LANDING_MARKER_SEG = 24;
    static_assert(LANDING_MARKER_SEG >= 3, "Hình tròn cần ít nhất 3 cạnh");
    constexpr float LANDING_MARKER_Y_OFFSET = 0.05f;  // nhấc lên khỏi mặt mây để không z-fight
    constexpr int LANDING_MARKER_COLOR = 0xffd23f;
    constexpr int LANDING_MARKER_ALPHA = 150;

    // ===== Trạng thái =====
    constexpr int RESTART_DELAY_TICKS = 48; // 0,8 giây ở 60 Hz

    // ===== Font và HUD =====
    constexpr const wchar_t* FONT_DIR = L"assets/fonts/";
    constexpr const wchar_t* FONT_FACE = L"Be Vietnam Pro";
    // Font hệ thống dự phòng, cả hai đều có dấu tiếng Việt.
    constexpr const wchar_t* FONT_FALLBACK_1 = L"Segoe UI";
    constexpr const wchar_t* FONT_FALLBACK_2 = L"Arial";
    constexpr int FONT_SIZE_HUD = 18;
    constexpr int FONT_SIZE_MSG = 28;
    constexpr int FONT_SIZE_MSG_SMALL = 16;
    constexpr int READY_HINT_CENTER_Y = 635;
}

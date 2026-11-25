#include "Render.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <random>

#include "DxLib.h"
#include "Camera.h"
#include "Cloud.h"

namespace
{
    constexpr float PI_F = 3.14159265358979f;
    // Mỗi ngôi sao là một hình vuông gồm 2 tam giác.
    constexpr int STAR_VERTS_PER_STAR = 6;

    unsigned int hexColor(int c)
    {
        return GetColor((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff);
    }

    COLOR_U8 hexColorU8(int c)
    {
        return GetColorU8((c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff, 255);
    }

    COLOR_F colorF(float r, float g, float b)
    {
        return GetColorF(r, g, b, 1.0f);
    }

    COLOR_F colorF(const Config::RgbF& c)
    {
        return colorF(c.r, c.g, c.b);
    }

    MATERIALPARAM makeMaterial(COLOR_F emissive)
    {
        MATERIALPARAM m{};
        m.Diffuse = colorF(1.0f, 1.0f, 1.0f);
        m.Ambient = colorF(1.0f, 1.0f, 1.0f);
        m.Specular = colorF(0.0f, 0.0f, 0.0f);
        m.Emissive = emissive;
        m.Power = 1.0f;
        return m;
    }


}

// Bộ đệm đỉnh cho toàn bộ sao, để vẽ cả bầu trời bằng một lệnh DrawPolygon3D.
struct Render::StarBatch
{
    std::array<VERTEX3D, Config::STAR_COUNT * STAR_VERTS_PER_STAR> verts{};
};

struct Render::WitchRun
{
    std::array<int, 4> bones{};
    std::array<MATRIX, 4> rest{};
    bool applied = false;
};

struct Render::CloudPuffMesh
{
    static constexpr int verticesPerPuff = (Config::CLOUD_PUFF_RINGS - 1) * Config::CLOUD_PUFF_SLICES + 2;
    static constexpr int trianglesPerPuff = 2 * (Config::CLOUD_PUFF_RINGS - 1) * Config::CLOUD_PUFF_SLICES;
    static constexpr size_t vertexCount = Config::CLOUD_PUFFS.size() * verticesPerPuff;
    static_assert(vertexCount < 65536, "DxLib dùng chỉ số đỉnh 16 bit");

    std::array<std::array<VERTEX3D, vertexCount>, Config::CLOUD_PLATFORM_COUNT> local{};
    std::array<VERTEX3D, vertexCount> world{};
    std::array<unsigned short, Config::CLOUD_PUFFS.size() * trianglesPerPuff * 3> indices{};
};

Render::Render()
{
    cloudModels_.fill(-1);
}

Render::~Render()
{
    shutdown();
}

void Render::check(int result, const wchar_t* what)
{
    if (result != -1)
    {
        return;
    }
    for (int i = 0; i < loggedErrorCount_; ++i)
    {
        if (loggedErrors_[static_cast<size_t>(i)] == what)
        {
            return;
        }
    }
    // Hết chỗ ghi nhớ thì thôi không ghi nữa, chấp nhận mất bớt log còn hơn ghi mỗi khung.
    if (loggedErrorCount_ >= MAX_LOGGED_ERRORS)
    {
        return;
    }
    loggedErrors_[static_cast<size_t>(loggedErrorCount_++)] = what;
    // Ghi log lỗi mà lỗi tiếp thì không còn cách báo nào khác, nên bỏ qua kết quả.
    static_cast<void>(ErrorLogFmtAdd(L"Lỗi DxLib: %s thất bại.\n", what));
}

Render::InitError Render::init(CloudField& clouds)
{
    // Sao được rải một lần, chỉ dịch theo phù thủy, nên không tốn cấp phát mỗi khung.
    std::mt19937 rng(Config::STAR_SEED);
    std::uniform_real_distribution<float> dx(-Config::STAR_X_RANGE, Config::STAR_X_RANGE);
    std::uniform_real_distribution<float> dy(Config::STAR_Y_MIN, Config::STAR_Y_MAX);
    std::uniform_real_distribution<float> dz(Config::STAR_Z_MIN, Config::STAR_Z_MAX);
    for (auto& s : stars_)
    {
        s = Vec3{ dx(rng), dy(rng), dz(rng) };
    }

    // Three.js tính cỡ điểm theo pixel: size * (H/2) / depth; quad thế giới cạnh S hiện
    // S * (H/2) / (depth * tan(FOV/2)). Cho hai bên bằng nhau thì S = size * tan(FOV/2).
    starWorldSize_ = Config::STAR_POINT_SIZE_THREE * std::tan(Config::CAMERA_FOV_DEG * PI_F / 360.0f);

    // Màu và pháp tuyến của sao không đổi, chỉ vị trí được tính lại mỗi khung.
    starBatch_ = std::make_unique<StarBatch>();
    const COLOR_U8 starColor = GetColorU8(255, 255, 255, 255);
    const COLOR_U8 noSpecular = GetColorU8(0, 0, 0, 0);
    for (auto& v : starBatch_->verts)
    {
        v.norm = VGet(0.0f, 0.0f, -1.0f);
        v.dif = starColor;
        v.spc = noSpecular;
    }

    // Đánh dấu sớm để shutdown dọn được cả khi khởi tạo hỏng giữa chừng.
    initialized_ = true;

    if (SetUseZBuffer3D(TRUE) == -1 || SetWriteZBuffer3D(TRUE) == -1)
    {
        ErrorLogAdd(L"Không bật được Z-buffer 3D.\n");
        return InitError::ZBuffer;
    }
    if (SetUseBackCulling(DX_CULLING_NONE) == -1)
    {
        ErrorLogAdd(L"Không tắt được back-face culling.\n");
        return InitError::Culling;
    }
    if (SetFogEnable(TRUE) == -1 ||
        SetFogMode(DX_FOGMODE_LINEAR) == -1 ||
        SetFogColor((Config::FOG_COLOR >> 16) & 0xff, (Config::FOG_COLOR >> 8) & 0xff, Config::FOG_COLOR & 0xff) == -1 ||
        SetFogStartEnd(Config::FOG_START, Config::FOG_END) == -1)
    {
        ErrorLogAdd(L"Không thiết lập được sương mù.\n");
        return InitError::Fog;
    }

    // Bản gốc dùng HemisphereLight + ánh trăng; ở đây xấp xỉ bằng ambient xanh nhạt + đèn hướng.
    // Ambient của DxLib không lấy màu đỉnh nên làm mọi thứ xám đều; giữ ambient thấp
    // và thêm đèn phụ hướng từ phía camera để mặt sau vẫn thấy rõ màu.
    if (SetUseLighting(TRUE) == -1 ||
        SetLightDirection(VNorm(VGet(Config::LIGHT_DIR_X, Config::LIGHT_DIR_Y, Config::LIGHT_DIR_Z))) == -1 ||
        SetLightDifColor(colorF(Config::LIGHT_DIFFUSE)) == -1 ||
        SetLightAmbColor(colorF(0.0f, 0.0f, 0.0f)) == -1 ||
        SetGlobalAmbientLight(colorF(Config::GLOBAL_AMBIENT)) == -1)
    {
        ErrorLogAdd(L"Không thiết lập được ánh sáng.\n");
        return InitError::Light;
    }

    // Đèn phụ chỉ làm cảnh đẹp hơn; thiếu nó game vẫn chơi được nên chỉ cảnh báo.
    fillLight_ = CreateDirLightHandle(VNorm(VGet(Config::FILL_LIGHT_DIR_X, Config::FILL_LIGHT_DIR_Y, Config::FILL_LIGHT_DIR_Z)));
    if (fillLight_ == -1)
    {
        ErrorLogAdd(L"Cảnh báo: không tạo được đèn phụ, cảnh sẽ tối hơn.\n");
    }
    else if (SetLightDifColorHandle(fillLight_, colorF(Config::FILL_LIGHT_DIFFUSE)) == -1 ||
             SetLightAmbColorHandle(fillLight_, colorF(0.0f, 0.0f, 0.0f)) == -1 ||
             SetLightSpcColorHandle(fillLight_, colorF(0.0f, 0.0f, 0.0f)) == -1)
    {
        ErrorLogAdd(L"Cảnh báo: không đặt được màu đèn phụ.\n");
    }

    if (SetMaterialParam(makeMaterial(colorF(0.0f, 0.0f, 0.0f))) == -1)
    {
        ErrorLogAdd(L"Không thiết lập được vật liệu 3D.\n");
        return InitError::Material;
    }
    // Sai màu nền chỉ làm khác màu trời, không chặn việc chơi.
    if (SetBackgroundColor((Config::BG_COLOR >> 16) & 0xff, (Config::BG_COLOR >> 8) & 0xff, Config::BG_COLOR & 0xff) == -1)
    {
        ErrorLogAdd(L"Cảnh báo: không đặt được màu nền.\n");
    }

    if (!loadModels(clouds))
    {
        return InitError::Models;
    }
    initCloudPuffs(clouds);

    loadFonts();
    if (fontHud_ == -1 || fontMsg_ == -1 || fontMsgSmall_ == -1)
    {
        ErrorLogAdd(L"Không tạo được font.\n");
        return InitError::Font;
    }
    return InitError::None;
}

bool Render::loadModels(CloudField& clouds)
{
    auto loadModel = [this](const wchar_t* path) {
        const int handle = MV1LoadModel(path);
        if (handle == -1)
        {
            check(handle, path);
            return -1;
        }

        const int textures = MV1GetTextureNum(handle);
        bool ready = textures > 0;
        for (int i = 0; i < textures; ++i)
        {
            ready = MV1GetTextureGraphHandle(handle, i) != -1 && ready;
        }
        if (ready && MV1SetUseZBuffer(handle, TRUE) != -1 && MV1SetWriteZBuffer(handle, TRUE) != -1)
        {
            return handle;
        }

        check(-1, path);
        check(MV1DeleteModel(handle), L"MV1DeleteModel (model lỗi)");
        return -1;
    };

    witchModel_ = loadModel(Config::WITCH_MODEL_PATH);
    if (witchModel_ == -1)
    {
        return false;
    }

    witchRun_ = std::make_unique<WitchRun>();
    constexpr std::array legs{L"upper_leg_L", L"lower_leg_L", L"upper_leg_R", L"lower_leg_R"};
    for (size_t i = 0; i < legs.size(); ++i)
    {
        const int bone = MV1SearchFrame(witchModel_, legs[i]);
        if (bone == -1)
        {
            check(-1, legs[i]);
            return false;
        }
        witchRun_->bones[i] = bone;
        witchRun_->rest[i] = MV1GetFrameLocalMatrix(witchModel_, bone);
    }

    constexpr std::array names{L"idle", L"fly", L"jump", L"fall", L"land", L"bounce"};
    static_assert(names.size() == static_cast<size_t>(Player::Motion::Count));
    const int animationCount = MV1GetAnimNum(witchModel_);
    for (int i = 0; i < animationCount; ++i)
    {
        const wchar_t* name = MV1GetAnimName(witchModel_, i);
        if (name == nullptr)
        {
            check(-1, L"MV1GetAnimName");
            return false;
        }

        const auto found = std::find_if(names.begin(), names.end(), [name](const wchar_t* clip) {
            return std::wcscmp(name, clip) == 0;
        });
        if (found == names.end())
        {
            continue;
        }

        const size_t index = static_cast<size_t>(found - names.begin());
        WitchClip& clip = witchClips_[index];
        if (clip.handle != -1)
        {
            check(-1, L"Animation phù thủy trùng tên");
            return false;
        }
        clip.handle = MV1AttachAnim(witchModel_, i);
        if (clip.handle == -1)
        {
            check(-1, L"MV1AttachAnim");
            return false;
        }
        clip.frames = MV1GetAttachAnimTotalTime(witchModel_, clip.handle);
        const float blend = index == static_cast<size_t>(activeWitchClip_) ? 1.0f : 0.0f;
        if (clip.frames <= 0.0f || MV1SetAttachAnimBlendRate(witchModel_, clip.handle, blend) == -1)
        {
            check(-1, L"Khởi tạo animation phù thủy");
            return false;
        }
    }

    for (const WitchClip& clip : witchClips_)
    {
        if (clip.handle == -1)
        {
            check(-1, L"Thiếu animation phù thủy");
            return false;
        }
    }

    for (size_t i = 0; i < cloudModels_.size(); ++i)
    {
        const auto& spec = Config::CLOUD_MODELS[i];
        const int model = loadModel(spec.path);
        cloudModels_[i] = model;
        if (model == -1 || (i < Config::CLOUD_PLATFORM_COUNT && !loadCloudSurface(model, clouds, i)))
        {
            return false;
        }
        if (MV1SetScale(model, VGet(spec.scaleX, spec.scaleY, spec.scaleZ)) == -1)
        {
            check(-1, L"MV1SetScale (mây)");
            return false;
        }

        if (spec.alphaMaterial != nullptr)
        {
            const int count = MV1GetMaterialNum(model);
            bool applied = false;
            for (int j = 0; j < count; ++j)
            {
                const wchar_t* name = MV1GetMaterialName(model, j);
                if (name != nullptr && std::wcscmp(name, spec.alphaMaterial) == 0)
                {
                    applied = MV1SetMaterialDrawBlendMode(model, j, DX_BLENDMODE_ALPHA) != -1 &&
                        MV1SetMaterialDrawBlendParam(model, j, 255) != -1;
                    break;
                }
            }

            bool meshReady = true;
            const int meshes = MV1GetMeshNum(model);
            for (int j = 0; j < meshes; ++j)
            {
                meshReady = MV1SetMeshBackCulling(model, j, DX_CULLING_LEFT) != -1 && meshReady;
            }
            if (!applied || !meshReady || meshes <= 0 || MV1SetWriteZBuffer(model, FALSE) == -1)
            {
                check(-1, spec.path);
                return false;
            }
        }
    }
    return true;
}

bool Render::loadCloudSurface(int model, CloudField& clouds, size_t variant)
{
    // Lấy tọa độ đã áp dụng transform của file .x khi model vẫn ở gốc.
    if (MV1SetupReferenceMesh(model, -1, TRUE) == -1)
    {
        check(-1, L"MV1SetupReferenceMesh (mây)");
        return false;
    }

    CloudSurface& surface = clouds.surface(variant);
    const auto& spec = Config::CLOUD_MODELS[variant];
    auto scaled = [&spec](const VECTOR& v) {
        return Vec3{v.x * spec.scaleX, v.y * spec.scaleY, v.z * spec.scaleZ};
    };
    const MV1_REF_POLYGONLIST mesh = MV1GetReferenceMesh(model, -1, TRUE);
    if (mesh.Polygons != nullptr && mesh.Vertexs != nullptr)
    {
        for (int i = 0; i < mesh.PolygonNum; ++i)
        {
            const MV1_REF_POLYGON& polygon = mesh.Polygons[i];
            const auto& a = mesh.Vertexs[polygon.VIndex[0]];
            const auto& b = mesh.Vertexs[polygon.VIndex[1]];
            const auto& c = mesh.Vertexs[polygon.VIndex[2]];
            surface.addTriangle(scaled(a.Position), scaled(b.Position), scaled(c.Position));
        }
    }
    const int released = MV1TerminateReferenceMesh(model, -1, TRUE);
    CloudContact center;
    if (released == -1 ||
        !surface.cross({0.0f, Config::CLOUD_TOP_OFFSET, 0.0f}, {0.0f, Config::CLOUD_BOTTOM_OFFSET, 0.0f}, center))
    {
        check(-1, L"Nạp mặt đáp của mây");
        return false;
    }
    return true;
}

void Render::loadFonts()
{
    // Nạp mọi font .ttf/.otf trong assets/fonts chỉ cho tiến trình này (FR_PRIVATE).
    const wchar_t* patterns[] = { L"*.ttf", L"*.otf" };
    for (const wchar_t* pattern : patterns)
    {
        wchar_t search[260];
        swprintf_s(search, L"%s%s", Config::FONT_DIR, pattern);
        WIN32_FIND_DATAW fd{};
        HANDLE h = FindFirstFileW(search, &fd);
        if (h == INVALID_HANDLE_VALUE)
        {
            continue;
        }
        do
        {
            if (privateFontCount_ >= MAX_PRIVATE_FONTS)
            {
                break;
            }
            auto& path = privateFontPaths_[static_cast<size_t>(privateFontCount_)];
            swprintf_s(path.data(), path.size(), L"%s%s", Config::FONT_DIR, fd.cFileName);
            if (AddFontResourceExW(path.data(), FR_PRIVATE, nullptr) > 0)
            {
                ++privateFontCount_;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }

    const wchar_t* face = nullptr;
    if (privateFontCount_ > 0 && CheckFontName(Config::FONT_FACE) == TRUE)
    {
        face = Config::FONT_FACE;
    }
    else
    {
        ErrorLogAdd(L"Cảnh báo: không tìm thấy font tiếng Việt trong assets/fonts, dùng font hệ thống.\n");
        if (CheckFontName(Config::FONT_FALLBACK_1) == TRUE)
        {
            face = Config::FONT_FALLBACK_1;
        }
        else
        {
            face = Config::FONT_FALLBACK_2;
        }
    }

    fontHud_ = createFont(face, Config::FONT_SIZE_HUD, -1);
    fontMsg_ = createFont(face, Config::FONT_SIZE_MSG, -1);
    fontMsgSmall_ = createFont(face, Config::FONT_SIZE_MSG_SMALL, -1);
}

int Render::createFont(const wchar_t* face, int size, int thick)
{
    // Viền đen thay cho text-shadow của bản HTML, giúp chữ đọc được trên nền mây trắng.
    return CreateFontToHandle(face, size, thick, DX_FONTTYPE_ANTIALIASING_EDGE_4X4, DX_CHARSET_DEFAULT, 2);
}

void Render::shutdown()
{
    if (!initialized_)
    {
        return;
    }
    if (witchModel_ != -1)
    {
        check(MV1DeleteModel(witchModel_), L"MV1DeleteModel (phù thủy)");
        witchModel_ = -1;
    }
    for (int& model : cloudModels_)
    {
        if (model != -1)
        {
            check(MV1DeleteModel(model), L"MV1DeleteModel (mây)");
            model = -1;
        }
    }
    for (int* f : { &fontHud_, &fontMsg_, &fontMsgSmall_ })
    {
        if (*f != -1)
        {
            check(DeleteFontToHandle(*f), L"DeleteFontToHandle");
            *f = -1;
        }
    }
    for (int i = 0; i < privateFontCount_; ++i)
    {
        RemoveFontResourceExW(privateFontPaths_[static_cast<size_t>(i)].data(), FR_PRIVATE, nullptr);
    }
    privateFontCount_ = 0;
    if (fillLight_ != -1)
    {
        check(DeleteLightHandle(fillLight_), L"DeleteLightHandle");
        fillLight_ = -1;
    }
    initialized_ = false;
}

void Render::beginFrame(const Camera& camera)
{
    check(ClearDrawScreen(), L"ClearDrawScreen");
    // SetDrawScreen xóa cài đặt camera, nên đặt lại mỗi khung cho chắc.
    check(SetCameraNearFar(Config::CAMERA_NEAR, Config::CAMERA_FAR), L"SetCameraNearFar");
    check(SetupCamera_Perspective(Config::CAMERA_FOV_DEG * PI_F / 180.0f), L"SetupCamera_Perspective");
    const Vec3& p = camera.position();
    const Vec3& t = camera.target();
    check(SetCameraPositionAndTarget_UpVecY(VGet(p.x, p.y, p.z), VGet(t.x, t.y, t.z)), L"SetCameraPositionAndTarget_UpVecY");
}

void Render::drawWorld(const Player& player, const CloudField& clouds, const Camera& camera, const LandingMarker& marker)
{
    drawEnvironment(player.position(), camera);
    // Màu và bóng của hai nhóm model đã nằm trong texture.
    check(SetUseLighting(FALSE), L"SetUseLighting(FALSE) (model)");
    drawClouds(clouds);
    drawSceneryClouds(clouds, camera, false);
    drawWitch(player);
    drawSceneryClouds(clouds, camera, true);
    drawLandingMarker(marker);
    check(SetUseLighting(TRUE), L"SetUseLighting(TRUE) (model)");
}

void Render::drawLandingMarker(const LandingMarker& marker)
{
    if (!marker.visible)
    {
        return;
    }
    // Vẽ theo độ dốc của tam giác đích, giữ kiểm tra Z để mây gần và phù thủy che dấu đúng.
    constexpr int seg = Config::LANDING_MARKER_SEG;
    std::array<VERTEX3D, seg * 3> verts{};
    const float y = marker.pos.y + Config::LANDING_MARKER_Y_OFFSET;
    const float r = Config::LANDING_MARKER_RADIUS;
    const VECTOR center = VGet(marker.pos.x, y, marker.pos.z);
    const VECTOR up = VNorm(VGet(-marker.slopeX, 1.0f, -marker.slopeZ));
    const COLOR_U8 dif = hexColorU8(Config::LANDING_MARKER_COLOR);
    const COLOR_U8 spc = GetColorU8(0, 0, 0, 0);
    size_t n = 0;
    auto put = [&](VECTOR pos) {
        VERTEX3D& v = verts[n++];
        v.pos = pos;
        v.norm = up;
        v.dif = dif;
        v.spc = spc;
    };
    for (int i = 0; i < seg; ++i)
    {
        const float t0 = 2.0f * PI_F * static_cast<float>(i) / static_cast<float>(seg);
        const float t1 = 2.0f * PI_F * static_cast<float>(i + 1) / static_cast<float>(seg);
        auto point = [&](float angle) {
            const float dx = r * std::cos(angle);
            const float dz = r * std::sin(angle);
            return VGet(marker.pos.x + dx, y + marker.slopeX * dx + marker.slopeZ * dz, marker.pos.z + dz);
        };
        put(center);
        put(point(t1));
        put(point(t0));
    }

    check(SetWriteZBuffer3D(FALSE), L"SetWriteZBuffer3D(FALSE) (dấu đáp)");
    check(SetDrawBlendMode(DX_BLENDMODE_ALPHA, Config::LANDING_MARKER_ALPHA), L"SetDrawBlendMode (dấu đáp)");
    check(DrawPolygon3D(verts.data(), seg, DX_NONE_GRAPH, FALSE), L"DrawPolygon3D (dấu đáp)");
    check(SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0), L"SetDrawBlendMode (noblend, dấu đáp)");
    check(SetWriteZBuffer3D(TRUE), L"SetWriteZBuffer3D(TRUE) (dấu đáp)");
}

void Render::drawEnvironment(const Vec3& pp, const Camera& camera)
{
    // Biển: mặt phẳng lớn đi theo phù thủy theo trục Z.
    {
        const float h = Config::SEA_HALF_SIZE;
        const float y = Config::SEA_Y;
        const VECTOR corners[4] = {
            VGet(-h, y, pp.z - h), VGet(h, y, pp.z - h), VGet(h, y, pp.z + h), VGet(-h, y, pp.z + h)
        };
        const int idx[6] = { 0, 2, 1, 0, 3, 2 };
        VERTEX3D verts[6]{};
        for (int i = 0; i < 6; ++i)
        {
            verts[i].pos = corners[idx[i]];
            verts[i].norm = VGet(0.0f, 1.0f, 0.0f);
            verts[i].dif = hexColorU8(Config::SEA_COLOR);
            verts[i].spc = GetColorU8(0, 0, 0, 0);
        }
        check(DrawPolygon3D(verts, 2, DX_NONE_GRAPH, FALSE), L"DrawPolygon3D (biển)");
    }

    // Trăng và sao không chịu sương mù và ánh sáng để luôn sáng rõ như bản gốc.
    check(SetUseLighting(FALSE), L"SetUseLighting(FALSE)");
    check(SetFogEnable(FALSE), L"SetFogEnable(FALSE)");
    check(DrawSphere3D(VGet(pp.x + Config::MOON_OFFSET_X, Config::MOON_Y + std::max(0.0f, pp.y), pp.z + Config::MOON_OFFSET_Z),
                       Config::MOON_RADIUS, Config::SPHERE_DIV_MOON, hexColor(Config::MOON_COLOR), GetColor(0, 0, 0), TRUE),
          L"DrawSphere3D (trăng)");
    drawStars(pp, camera);
    check(SetFogEnable(TRUE), L"SetFogEnable(TRUE)");
    check(SetUseLighting(TRUE), L"SetUseLighting(TRUE)");
}

void Render::drawStars(const Vec3& pp, const Camera& camera)
{
    // Hình vuông luôn quay mặt về camera, giống PointsMaterial có sizeAttenuation:
    // sao gần to hơn sao xa mà chỉ cần một lệnh vẽ.
    const Vec3& cp = camera.position();
    const Vec3& ct = camera.target();
    VECTOR forward = VSub(VGet(ct.x, ct.y, ct.z), VGet(cp.x, cp.y, cp.z));
    if (VSquareSize(forward) < 1e-6f)
    {
        forward = VGet(0.0f, 0.0f, 1.0f);
    }
    forward = VNorm(forward);
    VECTOR right = VCross(VGet(0.0f, 1.0f, 0.0f), forward);
    // Camera nhìn thẳng lên/xuống thì phép nhân chéo suy biến, lấy trục X làm mặc định.
    right = VSquareSize(right) < 1e-6f ? VGet(1.0f, 0.0f, 0.0f) : VNorm(right);
    const VECTOR up = VCross(forward, right);

    const float half = starWorldSize_ * 0.5f;
    const VECTOR r = VScale(right, half);
    const VECTOR u = VScale(up, half);
    const float sx = pp.x * Config::STAR_X_FOLLOW;
    const float sy = std::max(0.0f, pp.y);

    auto& verts = starBatch_->verts;
    size_t n = 0;
    for (const auto& s : stars_)
    {
        const VECTOR c = VGet(s.x + sx, s.y + sy, s.z + pp.z);
        const VECTOR p0 = VSub(VSub(c, r), u);
        const VECTOR p1 = VSub(VAdd(c, r), u);
        const VECTOR p2 = VAdd(VAdd(c, r), u);
        const VECTOR p3 = VAdd(VSub(c, r), u);
        verts[n++].pos = p0;
        verts[n++].pos = p1;
        verts[n++].pos = p2;
        verts[n++].pos = p0;
        verts[n++].pos = p2;
        verts[n++].pos = p3;
    }
    check(DrawPolygon3D(verts.data(), Config::STAR_COUNT * 2, DX_NONE_GRAPH, FALSE), L"DrawPolygon3D (sao)");
}

void Render::initCloudPuffs(const CloudField& clouds)
{
    cloudPuffs_ = std::make_unique<CloudPuffMesh>();
    auto& mesh = *cloudPuffs_;
    constexpr int slices = Config::CLOUD_PUFF_SLICES;
    constexpr int rings = Config::CLOUD_PUFF_RINGS;
    size_t vertex = 0;
    size_t index = 0;
    for (const auto& puff : Config::CLOUD_PUFFS)
    {
        const size_t base = vertex;
        auto addVertex = [&](float x, float y, float z) {
            auto& v = mesh.local.front()[vertex++];
            v.pos = VGet(puff.x + x * puff.rx, puff.y + y * puff.ry, puff.z + z * puff.rz);
            v.norm = VNorm(VGet(x / puff.rx, y / puff.ry, z / puff.rz));
            // Giữ mặt trên sáng, cho bụng mây ngả tím lam để thấy rõ các múi.
            const float light = std::clamp(0.48f + 0.41f * v.norm.y - 0.18f * v.norm.x
                                          - 0.12f * v.norm.z, 0.0f, 1.0f);
            v.dif = GetColorU8(static_cast<int>(125.0f + 110.0f * light),
                               static_cast<int>(141.0f + 104.0f * light),
                               static_cast<int>(182.0f + 69.0f * light), 255);
        };
        auto triangle = [&](size_t a, size_t b, size_t c) {
            mesh.indices[index++] = static_cast<unsigned short>(base + a);
            mesh.indices[index++] = static_cast<unsigned short>(base + b);
            mesh.indices[index++] = static_cast<unsigned short>(base + c);
        };

        addVertex(0.0f, 1.0f, 0.0f);
        for (int r = 1; r < rings; ++r)
        {
            const float polar = PI_F * static_cast<float>(r) / rings;
            const float radius = std::sin(polar);
            const float y = std::cos(polar);
            for (int s = 0; s < slices; ++s)
            {
                const float azimuth = Config::FULL_TURN * static_cast<float>(s) / slices;
                addVertex(radius * std::cos(azimuth), y, radius * std::sin(azimuth));
            }
        }
        addVertex(0.0f, -1.0f, 0.0f);

        const size_t bottom = CloudPuffMesh::verticesPerPuff - 1;
        for (int s = 0; s < slices; ++s)
        {
            const int next = (s + 1) % slices;
            triangle(0, 1 + s, 1 + next);
            for (int r = 0; r < rings - 2; ++r)
            {
                const int first = 1 + r * slices;
                triangle(first + s, first + s + slices, first + next);
                triangle(first + next, first + s + slices, first + next + slices);
            }
            triangle(bottom, bottom - slices + next, bottom - slices + s);
        }
    }

    // Làm theo hình của từng model để múi tròn không nhô lên trên mặt chạy.
    mesh.world = mesh.local.front();
    for (size_t variant = 0; variant < mesh.local.size(); ++variant)
    {
        const float mirror = variant % 2 == 0 ? 1.0f : -1.0f;
        for (size_t i = 0; i < mesh.world.size(); ++i)
        {
            auto& v = mesh.local[variant][i];
            v = mesh.world[i];
            v.pos.x *= mirror;
            v.norm.x *= mirror;
            float roof = 0.0f;
            const float cap = clouds.surface(variant).roofHeight(v.pos.x, v.pos.z, roof)
                ? roof - Config::CLOUD_PUFF_SURFACE_CLEARANCE : Config::CLOUD_PUFF_OUTSIDE_CAP;
            v.pos.y = std::min(v.pos.y, cap);
        }
    }
}

void Render::drawCloudPuffs(const Vec3& p, size_t variant)
{
    auto& mesh = *cloudPuffs_;
    const auto& local = mesh.local[variant];
    for (size_t i = 0; i < local.size(); ++i)
    {
        mesh.world[i] = local[i];
        auto& v = mesh.world[i];
        v.pos = VGet(p.x + v.pos.x, p.y + v.pos.y, p.z + v.pos.z);
    }

    check(DrawPolygonIndexed3D(mesh.world.data(), static_cast<int>(mesh.world.size()), mesh.indices.data(),
                               static_cast<int>(mesh.indices.size() / 3), DX_NONE_GRAPH, FALSE),
          L"Vẽ phần bụng mây");
}

void Render::drawClouds(const CloudField& clouds)
{
    for (int i = 0; i < clouds.count(); ++i)
    {
        const Cloud& c = clouds.at(i);
        const int model = cloudModels_[c.model];
        const Vec3 p = c.positionAt(clouds.time());
        check(MV1SetPosition(model, VGet(p.x, p.y, p.z)), L"MV1SetPosition (mây)");
        check(MV1DrawModel(model), L"MV1DrawModel (mây)");
        drawCloudPuffs(p, c.model);
    }
}

void Render::drawSceneryClouds(const CloudField& clouds, const Camera& camera, bool transparent)
{
    struct Scenery
    {
        Vec3 pos;
        size_t model;
        float depth;
    };
    std::array<Scenery, Config::CLOUD_POOL_SIZE * Config::CLOUD_DECOR_PER_PLATFORM> batch;
    size_t count = 0;

    const Vec3& eye = camera.position();
    const Vec3& look = camera.target();
    const Vec3 forward{look.x - eye.x, look.y - eye.y, look.z - eye.z};
    constexpr size_t variants = Config::CLOUD_MODELS.size() - Config::CLOUD_PLATFORM_COUNT;
    for (int i = 0; i < clouds.count(); ++i)
    {
        const Cloud& cloud = clouds.at(i);
        const Vec3 drifted = cloud.positionAt(clouds.time());
        for (int j = 0; j < Config::CLOUD_DECOR_PER_PLATFORM; ++j)
        {
            const size_t key = static_cast<size_t>(cloud.id) * Config::CLOUD_DECOR_PER_PLATFORM + j;
            const size_t model = Config::CLOUD_PLATFORM_COUNT + key % variants;
            if ((Config::CLOUD_MODELS[model].alphaMaterial != nullptr) != transparent)
            {
                continue;
            }

            const float side = (cloud.id + j) % 2 == 0 ? -1.0f : 1.0f;
            const float margin = model == Config::CLOUD_LANDMARK_MODEL ? Config::CLOUD_DECOR_LANDMARK_MARGIN : 0.0f;
            const float distance = Config::CLOUD_DECOR_SIDE + margin +
                static_cast<float>(key % 3) * Config::CLOUD_DECOR_SIDE_VARIATION;
            const float tier = j == 0 ? Config::CLOUD_DECOR_DOWN : Config::CLOUD_DECOR_UP;
            const Vec3 pos{drifted.x + side * distance,
                           drifted.y + tier, drifted.z + Config::CLOUD_DECOR_DEPTH * (j + 1)};
            const float depth = (pos.x - eye.x) * forward.x + (pos.y - eye.y) * forward.y +
                (pos.z - eye.z) * forward.z;
            batch[count++] = {pos, model, depth};
        }
    }

    if (transparent)
    {
        // Mây alpha ở xa vẽ trước, sau cảnh đặc để dải mưa không che sai chiều sâu.
        std::sort(batch.begin(), batch.begin() + count, [](const Scenery& a, const Scenery& b) {
            return a.depth > b.depth;
        });
    }
    for (size_t i = 0; i < count; ++i)
    {
        const auto& cloud = batch[i];
        const int model = cloudModels_[cloud.model];
        check(MV1SetPosition(model, VGet(cloud.pos.x, cloud.pos.y, cloud.pos.z)), L"Đặt mây nền");
        check(MV1DrawModel(model), L"Vẽ mây nền");
    }
}

void Render::updateRunPose(float runTime)
{
    WitchRun& run = *witchRun_;
    if (runTime == 0.0f)
    {
        if (run.applied)
        {
            for (const int bone : run.bones)
            {
                check(MV1ResetFrameUserLocalMatrix(witchModel_, bone), L"Khôi phục khung chân");
            }
            run.applied = false;
        }
        return;
    }

    // Hai chân luân phiên sải khi chạy rồi trả lại chuyển động của clip nhảy.
    const float phase = runTime * Config::FULL_TURN * Config::WITCH_RUN_CYCLES_PER_SECOND;
    const float stride = std::sin(phase);
    const std::array angles{
        Config::WITCH_RUN_HIP_SWING * stride, -Config::WITCH_RUN_KNEE_BEND * std::max(0.0f, -stride),
        -Config::WITCH_RUN_HIP_SWING * stride, -Config::WITCH_RUN_KNEE_BEND * std::max(0.0f, stride)
    };
    for (size_t i = 0; i < run.bones.size(); ++i)
    {
        const MATRIX pose = MMult(MGetRotX(angles[i]), run.rest[i]);
        check(MV1SetFrameUserLocalMatrix(witchModel_, run.bones[i], pose), L"Đặt khung chân chạy");
    }
    run.applied = true;
}

void Render::drawWitch(const Player& player)
{
    using Motion = Player::Motion;
    const Motion motion = player.motion();
    const WitchClip& clip = witchClips_[static_cast<size_t>(motion)];
    if (motion != activeWitchClip_)
    {
        const WitchClip& previous = witchClips_[static_cast<size_t>(activeWitchClip_)];
        check(MV1SetAttachAnimBlendRate(witchModel_, previous.handle, 0.0f), L"Tắt clip phù thủy");
        check(MV1SetAttachAnimBlendRate(witchModel_, clip.handle, 1.0f), L"Bật clip phù thủy");
        activeWitchClip_ = motion;
    }

    float frame = player.motionTime() * Config::WITCH_ANIMATION_FPS;
    switch (motion)
    {
    case Motion::Jump:
    {
        constexpr float takeoffFrame = Config::WITCH_JUMP_TAKEOFF_FRAME;
        const float time = player.motionTime();
        if (time < Config::JUMP_WINDUP)
        {
            frame = takeoffFrame * time / Config::JUMP_WINDUP;
        }
        else
        {
            const float airborneTime = time - Config::JUMP_WINDUP;
            frame = takeoffFrame + (clip.frames - takeoffFrame) * airborneTime /
                (Config::WITCH_JUMP_TIME - Config::JUMP_WINDUP);
        }
        break;
    }
    case Motion::Land:
    {
        // Khung đầu clip còn co chân trên không; vật lý đã chạm mặt mây.
        constexpr float contactFrame = Config::WITCH_LAND_CONTACT_FRAME;
        frame = contactFrame + (clip.frames - contactFrame) * player.motionTime() / Config::WITCH_LAND_TIME;
        break;
    }
    case Motion::Bounce:
        frame = clip.frames * player.motionTime() / Config::WITCH_BOUNCE_TIME;
        break;
    default:
        frame = std::fmod(frame, clip.frames);
        break;
    }
    check(MV1SetAttachAnimTime(witchModel_, clip.handle, std::clamp(frame, 0.0f, clip.frames)),
          L"MV1SetAttachAnimTime");

    const Vec3& pos = player.position();
    check(MV1SetPosition(witchModel_, VGet(pos.x, pos.y, pos.z)), L"MV1SetPosition (phù thủy)");
    const MATRIX tilt = MMult(MGetRotZ(player.rotZ()), MGetRotX(-player.rotX()));
    const MATRIX rotation = MMult(tilt, MGetRotY(player.yaw()));
    check(MV1SetRotationMatrix(witchModel_, rotation), L"MV1SetRotationMatrix");
    updateRunPose(player.runTime());
    check(MV1DrawModel(witchModel_), L"MV1DrawModel (phù thủy)");
}
void Render::drawHud(int score, float altitude)
{
    const unsigned int white = GetColor(255, 255, 255);
    const unsigned int edge = GetColor(0, 0, 0);
    wchar_t line[128];
    const int alt = std::max(0, static_cast<int>(std::lround(altitude)));
    swprintf_s(line, L"Mây xa nhất: %d · Độ cao: %dm", score, alt);
    check(DrawStringToHandle(12, 12, line, white, fontHud_, edge), L"DrawStringToHandle (HUD)");
}
void Render::drawCenterLines(const wchar_t* const* lines, const int* fonts, int lineCount, int centerY)
{
    constexpr int gap = 8;
    // Lỗi lấy cỡ chữ trả -1; coi như 0 để bố cục không bị lệch âm.
    auto fontSize = [this](int font) {
        const int size = GetFontSizeToHandle(font);
        check(size, L"GetFontSizeToHandle");
        return std::max(0, size);
    };
    int total = 0;
    for (int i = 0; i < lineCount; ++i)
    {
        total += fontSize(fonts[i]) + (i > 0 ? gap : 0);
    }
    int y = centerY - total / 2;
    const unsigned int white = GetColor(255, 255, 255);
    const unsigned int edge = GetColor(0, 0, 0);
    for (int i = 0; i < lineCount; ++i)
    {
        const int w = GetDrawStringWidthToHandle(lines[i], static_cast<int>(std::wcslen(lines[i])), fonts[i]);
        check(w, L"GetDrawStringWidthToHandle");
        check(DrawStringToHandle((Config::WINDOW_W - std::max(0, w)) / 2, y, lines[i], white, fonts[i], edge),
              L"DrawStringToHandle (thông báo)");
        y += fontSize(fonts[i]) + gap;
    }
}

void Render::drawWindLesson(const Player& player, const CloudField& clouds, int targetId, bool pulling)
{
    const auto cyan = hexColor(Config::WIND_COLOR);
    const auto gold = hexColor(Config::WIND_ACTIVE_COLOR);
    const auto white = GetColor(255, 255, 255);
    const auto edge = GetColor(0, 0, 0);
    const float phase = static_cast<float>(clouds.time()) * Config::WIND_PULSE_RATE;
    for (int i = 0; i < clouds.count(); ++i)
    {
        const Cloud& cloud = clouds.at(i);
        if (!cloud.movable)
        {
            continue;
        }
        const Vec3 p = cloud.positionAt(clouds.time());
        const bool selected = cloud.id == targetId;
        const unsigned int color = selected ? gold : cyan;
        const float radius = Config::WIND_RING_RADIUS;
        for (int j = 0; j < Config::WIND_RING_SEGMENTS; ++j)
        {
            const float a = Config::FULL_TURN * j / Config::WIND_RING_SEGMENTS;
            const float b = Config::FULL_TURN * (j + 1) / Config::WIND_RING_SEGMENTS;
            const float y = p.y + Config::WIND_RING_HEIGHT;
            check(DrawLine3D(VGet(p.x + radius * std::cos(a), y, p.z + radius * std::sin(a)),
                             VGet(p.x + radius * std::cos(b), y, p.z + radius * std::sin(b)), color),
                  L"Vòng mây có thể kéo");
        }
    }

    const Vec3 goal = clouds.cloudPosition(static_cast<int>(Config::WIND_LESSON.size()) - 1);
    check(SetUseLighting(FALSE), L"Tắt ánh sáng quả cầu đích");
    check(DrawSphere3D(VGet(goal.x, goal.y + Config::WIND_GOAL_HEIGHT, goal.z), Config::WIND_GOAL_RADIUS,
                        Config::WIND_GOAL_DIVISIONS, gold, gold, TRUE), L"Vẽ quả cầu đích");
    if (pulling && targetId != -1)
    {
        const Vec3 p = player.position();
        const Vec3 target = clouds.cloudPosition(targetId);
        VECTOR previous = VGet(p.x, p.y + Config::WIND_BEAM_HEIGHT, p.z);
        for (int i = 1; i <= Config::WIND_BEAM_SEGMENTS; ++i)
        {
            const float t = static_cast<float>(i) / Config::WIND_BEAM_SEGMENTS;
            const float spiral = Config::WIND_BEAM_SWIRL * std::sin(t * PI_F);
            const float angle = phase + t * Config::FULL_TURN;
            const VECTOR next = VGet(p.x + (target.x - p.x) * t + spiral * std::cos(angle),
                                      p.y + Config::WIND_BEAM_HEIGHT + (target.y - p.y) * t + spiral * std::sin(angle),
                                      p.z + (target.z - p.z) * t);
            check(DrawLine3D(previous, next, cyan), L"Vẽ luồng gió");
            previous = next;
        }
    }
    check(SetUseLighting(TRUE), L"Khôi phục ánh sáng sau phép gió");

    const int cx = Config::WINDOW_W / 2;
    const int cy = Config::WINDOW_H / 2;
    const int cross = Config::WIND_AIM_CROSS_SIZE;
    const auto color = targetId == -1 ? white : gold;
    check(DrawLine(cx - cross, cy, cx + cross, cy, color), L"Tâm ngắm ngang");
    check(DrawLine(cx, cy - cross, cx, cy + cross, color), L"Tâm ngắm dọc");

    const wchar_t* objective = L"Tới quả cầu vàng ở bệ cuối";
    const int support = player.supportingCloud();
    if (support == 0)
    {
        objective = L"Chạy lấy đà và nhảy tới bệ đầu phía trước";
    }
    else if (support == 1)
    {
        objective = L"Đường phía trước bị đứt: kéo mây viền xanh về làm cầu";
    }
    else if (support == 2)
    {
        objective = L"Đường đã nối: chạy lấy đà để tới bệ nghỉ phía trước";
    }
    else if (support == 3)
    {
        objective = L"Tìm đám mây viền xanh bên trái để nối đường lên tầng trên";
    }
    const wchar_t* hint = pulling ? L"Đang kéo mây · Rê chuột để chọn chỗ đặt · Thả chuột phải để dừng" :
        (targetId != -1 ? L"Giữ chuột phải để kéo đám mây đang ngắm" : L"Ngắm mây viền xanh · Chuột phải: kéo · R: chơi lại màn");
    check(DrawStringToHandle(12, Config::WIND_HUD_Y, objective, white, fontMsgSmall_, edge), L"Mục tiêu màn gió");
    check(DrawStringToHandle(12, Config::WIND_HUD_Y + Config::FONT_SIZE_MSG_SMALL + 8, hint,
                             color, fontMsgSmall_, edge), L"Hướng dẫn phép gió");
}

void Render::drawWonMessage(bool canRestart)
{
    const wchar_t* lines[] = {L"Đã nối đường qua tầng mây!", L"Nhấn Space để chơi lại"};
    const int fonts[] = {fontMsg_, fontMsgSmall_};
    drawCenterLines(lines, fonts, canRestart ? 2 : 1);
}

void Render::drawReadyMessage()
{
    const wchar_t* lines[] = {
        L"Dùng gió nối đường qua mây",
        L"WASD / phím mũi tên: chạy · Space hoặc chuột trái: nhảy",
        L"Rê chuột: ngắm · Giữ chuột phải: kéo mây viền xanh"
    };
    const int fonts[] = { fontMsg_, fontMsgSmall_, fontMsgSmall_ };
    drawCenterLines(lines, fonts, 3, Config::READY_HINT_CENTER_Y);
}

void Render::drawOverMessage(int score, bool canRestart)
{
    wchar_t scoreLine[64];
    swprintf_s(scoreLine, L"Mây xa nhất: %d", score);
    const wchar_t* lines[] = { L"Rơi xuống biển rồi!", scoreLine, L"Nhấn Space hoặc chuột trái để chơi lại" };
    const int fonts[] = { fontMsg_, fontMsgSmall_, fontMsgSmall_ };
    drawCenterLines(lines, fonts, canRestart ? 3 : 2);
}

void Render::drawPausedMessage()
{
    const wchar_t* lines[] = { L"Tạm dừng", L"Nhấp vào cửa sổ để chơi tiếp" };
    const int fonts[] = { fontMsg_, fontMsgSmall_ };
    drawCenterLines(lines, fonts, 2);
}

void Render::drawQuitConfirmMessage()
{
    const wchar_t* lines[] = { L"Thoát game?", L"Nhấn Esc lần nữa để thoát · Space hoặc chuột trái để chơi tiếp" };
    const int fonts[] = { fontMsg_, fontMsgSmall_ };
    drawCenterLines(lines, fonts, 2);
}

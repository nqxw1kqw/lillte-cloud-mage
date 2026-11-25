#include "Render.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <random>
#include <vector>

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

struct Render::CloudSurfaceMesh
{
    std::array<std::vector<VERTEX3D>,Config::CLOUD_PLATFORM_COUNT> local,world;
};

struct Render::EnvironmentScene
{
    struct Mesh
    {
        std::vector<VERTEX3D> local, world;

        void triangle(VECTOR a, VECTOR b, VECTOR c, int color, bool glow=false)
        {
            const VECTOR cross=VCross(VSub(b,a),VSub(c,a));
            if (VSquareSize(cross)<1e-8f) return;
            const VECTOR normal=VNorm(cross);
            const float light=std::clamp(0.5f+0.4f*normal.y+0.18f*normal.x-0.15f*normal.z,0.25f,1.0f);
            const float shade=glow ? 0.75f+0.25f*light : light;
            VERTEX3D v{};
            v.norm=normal;
            v.dif=GetColorU8(static_cast<int>(((color>>16)&255)*shade),
                              static_cast<int>(((color>>8)&255)*shade),
                              static_cast<int>((color&255)*shade),255);
            v.pos=a; local.push_back(v); v.pos=b; local.push_back(v); v.pos=c; local.push_back(v);
        }
        void quad(VECTOR a,VECTOR b,VECTOR c,VECTOR d,int color,bool glow=false)
        {
            triangle(a,b,c,color,glow); triangle(a,c,d,color,glow);
        }
        void block(float x,float y,float z,float rx,float ry,float rz,int color)
        {
            const VECTOR a=VGet(x-rx,y-ry,z-rz),b=VGet(x+rx,y-ry,z-rz);
            const VECTOR c=VGet(x+rx,y+ry,z-rz),d=VGet(x-rx,y+ry,z-rz);
            const VECTOR e=VGet(x-rx,y-ry,z+rz),f=VGet(x+rx,y-ry,z+rz);
            const VECTOR g=VGet(x+rx,y+ry,z+rz),h=VGet(x-rx,y+ry,z+rz);
            quad(a,d,c,b,color);quad(e,f,g,h,color);quad(d,h,g,c,color);
            quad(a,b,f,e,color);quad(b,c,g,f,color);quad(a,e,h,d,color);
        }
        void branch(VECTOR from,VECTOR to,float bottom,float top,int color)
        {
            const VECTOR axis=VNorm(VSub(to,from));
            VECTOR right=VCross(axis,VGet(0,1,0));
            right=VSquareSize(right)<1e-6f ? VGet(1,0,0) : VNorm(right);
            const VECTOR across=VCross(axis,right);
            auto point=[&](VECTOR center,float radius,float angle) {
                return VAdd(center,VAdd(VScale(right,radius*std::cos(angle)),VScale(across,radius*std::sin(angle))));
            };
            for (int i=0;i<Config::ENV_BRANCH_SIDES;++i)
            {
                const float a=Config::FULL_TURN*i/Config::ENV_BRANCH_SIDES;
                const float b=Config::FULL_TURN*(i+1)/Config::ENV_BRANCH_SIDES;
                const VECTOR p=point(from,bottom,a),q=point(from,bottom,b);
                const VECTOR r=point(to,top,b),s=point(to,top,a);
                quad(p,q,r,s,color);triangle(to,s,r,color);
            }
        }
        void canopy(const Config::EnvironmentCanopy& spec)
        {
            constexpr int slices=8, rings=4;
            auto point=[&](int r,int s) {
                const float polar=PI_F*r/rings, angle=Config::FULL_TURN*s/slices;
                return VGet(spec.x+spec.rx*std::sin(polar)*std::cos(angle),
                            spec.y+spec.ry*std::cos(polar),spec.z+spec.rz*std::sin(polar)*std::sin(angle));
            };
            for (int r=0;r<rings;++r) for(int s=0;s<slices;++s)
                quad(point(r,s),point(r+1,s),point(r+1,s+1),point(r,s+1),spec.color,true);
        }
    };
    Mesh solid, glow;
    std::array<VERTEX3D,(Config::ENV_SPARK_COUNT+Config::ENV_CANOPIES.size())*Config::ENV_SPARK_SEGMENTS*3> sparks{};
};

Render::Render()
{
    cloudModels_.fill(-1);
    butterflyPaletteModels_.fill(-1);
    butterflyPaletteClips_.fill(-1);
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

    if (!loadModels(clouds) || !loadButterfly())
    {
        return InitError::Models;
    }
    initCloudPuffs(clouds);
    initCloudSurfaceMesh(clouds);
    initEnvironmentScene();

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
    // Lấp các khe giữa múi, cho đứng trên toàn hình chiếu của đám mây.
    surface.buildFootprint();
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
    if (butterflyModel_ != -1)
    {
        check(MV1DeleteModel(butterflyModel_), L"Xóa model bướm");
        butterflyModel_ = -1;
        butterflyClip_ = -1;
        butterflyClipTicks_ = 0.0f;
    }
    if (mothModel_ != -1)
    {
        check(MV1DeleteModel(mothModel_), L"Xóa bướm đêm");
        mothModel_ = -1;
        mothClip_ = -1;
    }
    for (int& model : cloudModels_)
    {
        if (model != -1)
        {
            check(MV1DeleteModel(model), L"MV1DeleteModel (mây)");
            model = -1;
        }
    }
    for (int& model : butterflyPaletteModels_)
    {
        if (model != -1)
        {
            check(MV1DeleteModel(model), L"Xóa bảng màu bướm");
            model = -1;
        }
    }
    butterflyPaletteClips_.fill(-1);
    environmentScene_.reset();
    cloudSurfaceMesh_.reset();
    environmentVisible_.fill(false);
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
    drawEnvironmentScene(clouds,player,camera);
    // Màu và bóng của hai nhóm model đã nằm trong texture.
    check(SetUseLighting(FALSE), L"SetUseLighting(FALSE) (model)");
    drawClouds(clouds, player.position());
    drawSceneryClouds(clouds, camera, false);
    drawWitch(player);
    drawButterflies(clouds, player.position());
    drawMothRegion(clouds, player);
    drawSceneryClouds(clouds, camera, true);
    drawLandingMarker(marker);
    check(SetUseLighting(TRUE), L"SetUseLighting(TRUE) (model)");
}

void Render::initEnvironmentScene()
{
    environmentScene_=std::make_unique<EnvironmentScene>();
    auto& solid=environmentScene_->solid;
    auto& glow=environmentScene_->glow;
    solid.local.reserve(10000); glow.local.reserve(6000);
    std::array<VECTOR,Config::ENV_ISLAND_SEGMENTS> rim{},middle{},lower{};
    for (int i=0;i<Config::ENV_ISLAND_SEGMENTS;++i)
    {
        const float a=Config::FULL_TURN*i/Config::ENV_ISLAND_SEGMENTS;
        const float rough=1.0f+0.09f*std::sin(i*2.7f)+0.04f*std::cos(i*5.4f);
        const float x=Config::ENV_ISLAND_RADIUS*rough*std::cos(a);
        const float z=Config::ENV_ISLAND_RADIUS*rough*Config::ENV_ISLAND_DEPTH_RATIO*std::sin(a);
        rim[static_cast<size_t>(i)]=VGet(x,0,z);
        middle[static_cast<size_t>(i)]=VGet(x*.82f,-3.2f+std::sin(i*1.9f),z*.82f);
        lower[static_cast<size_t>(i)]=VGet(x*.43f,-Config::ENV_ISLAND_DROP*.62f+std::cos(i*2.2f),z*.43f);
    }
    for (size_t i=0;i<rim.size();++i)
    {
        const size_t j=(i+1)%rim.size();
        solid.triangle(VGet(0,.15f,0),rim[j],rim[i],Config::ENV_GRASS_COLOR);
        solid.quad(rim[i],rim[j],middle[j],middle[i],Config::ENV_SOIL_COLOR);
        solid.quad(middle[i],middle[j],lower[j],lower[i],Config::ENV_SOIL_COLOR);
        solid.triangle(lower[i],lower[j],VGet(1.0f,-Config::ENV_ISLAND_DROP,-.5f),Config::ENV_SOIL_COLOR);
    }
    const float ax=Config::ENV_ARCH_X, az=Config::ENV_ARCH_Z, height=Config::ENV_ARCH_HEIGHT;
    for (int side : {-1,1})
    {
        const float x=ax+side*(Config::ENV_ARCH_INNER_RADIUS+Config::ENV_ARCH_OUTER_RADIUS)*.5f;
        solid.block(x,.35f,az,1.05f,.35f,1.15f,Config::ENV_STONE_COLOR);
        for (int tier=0;tier<3;++tier)
            solid.block(x+side*.05f*tier,.7f+(tier+.5f)*(height-.7f)/3,az,
                        .63f,(height-.7f)/6-.06f,.72f,Config::ENV_STONE_COLOR);
    }
    for (int i=0;i<Config::ENV_ARCH_SEGMENTS;++i)
    {
        // Chừa một đoạn vỡ để cổng trông như tàn tích thay vì công trình mới.
        if (i==2) continue;
        const float a=PI_F*i/Config::ENV_ARCH_SEGMENTS+.018f;
        const float b=PI_F*(i+1)/Config::ENV_ARCH_SEGMENTS-.018f;
        const float r=Config::ENV_ARCH_OUTER_RADIUS, t=Config::ENV_ARCH_INNER_RADIUS;
        const float z=az-Config::ENV_ARCH_HALF_DEPTH, back=az+Config::ENV_ARCH_HALF_DEPTH;
        const VECTOR p=VGet(ax+r*std::cos(a),height+r*std::sin(a),z);
        const VECTOR q=VGet(ax+r*std::cos(b),height+r*std::sin(b),z);
        const VECTOR u=VGet(ax+t*std::cos(b),height+t*std::sin(b),z);
        const VECTOR v=VGet(ax+t*std::cos(a),height+t*std::sin(a),z);
        auto rear=[back](VECTOR point){point.z=back;return point;};
        solid.quad(p,q,u,v,Config::ENV_STONE_COLOR);
        solid.quad(rear(q),rear(p),rear(v),rear(u),Config::ENV_STONE_COLOR);
        solid.quad(p,rear(p),rear(q),q,Config::ENV_STONE_COLOR);
        solid.quad(v,u,rear(u),rear(v),Config::ENV_STONE_COLOR);
        solid.quad(q,rear(q),rear(u),u,Config::ENV_STONE_COLOR);
        solid.quad(p,v,rear(v),rear(p),Config::ENV_STONE_COLOR);
    }
    solid.block(-8.5f,.45f,4.0f,1.1f,.45f,.7f,Config::ENV_STONE_COLOR);
    solid.block(-9.1f,1.25f,3.7f,.8f,.35f,.6f,Config::ENV_STONE_COLOR);
    solid.block(-1.5f,.3f,5.0f,1.2f,.3f,.9f,Config::ENV_STONE_COLOR);
    solid.branch(VGet(4,.2f,-2),VGet(4.4f,7.5f,-2.1f),.72f,.27f,Config::ENV_WOOD_COLOR);
    for (const auto& canopy : Config::ENV_CANOPIES)
    {
        solid.branch(VGet(4.3f,4.2f,-2.0f),VGet(canopy.x,canopy.y-.4f,canopy.z),.29f,.1f,Config::ENV_WOOD_COLOR);
        glow.canopy(canopy);
    }
    const std::array<VECTOR,4> crystals{VGet(-10.2f,1.5f,1),VGet(1,1.7f,6.5f),VGet(9,1.4f,3.5f),
                                         VGet(ax,height-.9f,az)};
    for (VECTOR p : crystals)
    {
        const VECTOR top=VAdd(p,VGet(0,1.3f,0)),bottom=VAdd(p,VGet(0,-.8f,0));
        for (int i=0;i<4;++i)
        {
            const float a=Config::FULL_TURN*i/4, b=Config::FULL_TURN*(i+1)/4;
            const VECTOR q=VAdd(p,VGet(.48f*std::cos(a),0,.48f*std::sin(a)));
            const VECTOR r=VAdd(p,VGet(.48f*std::cos(b),0,.48f*std::sin(b)));
            glow.triangle(top,q,r,Config::ENV_CRYSTAL_COLOR,true);
            glow.triangle(bottom,r,q,Config::ENV_CRYSTAL_COLOR,true);
        }
    }
    solid.world.resize(solid.local.size());
    glow.world.resize(glow.local.size());
}

void Render::drawEnvironmentScene(const CloudField& clouds, const Player& player, const Camera& camera)
{
    environmentVisible_.fill(false);
    if (!environmentScene_) return;
    check(SetUseLighting(FALSE),L"Màu cảnh đảo nổi");
    const Vec3 playerPos=player.position();
    const double time=clouds.time();
    const Vec3 eye=camera.position(),look=camera.target();
    const VECTOR forward=VNorm(VGet(look.x-eye.x,look.y-eye.y,look.z-eye.z));
    VECTOR right=VCross(forward,VGet(0,1,0));
    right=VSquareSize(right)<1e-6f ? VGet(1,0,0) : VNorm(right);
    const VECTOR up=VCross(right,forward);
    for (size_t siteIndex=0;siteIndex<Config::ENVIRONMENT_SITES.size();++siteIndex)
    {
        const auto& site=Config::ENVIRONMENT_SITES[siteIndex];
        if (site.cloudId>=clouds.count()) continue;
        const Vec3 origin=clouds.at(site.cloudId).positionAt(time);
        const Vec3 next=clouds.at(std::min(site.cloudId+1,clouds.count()-1)).positionAt(time);
        const float length=std::hypot(next.x-origin.x,next.z-origin.z);
        const float tx=length>0 ? (next.x-origin.x)/length : 0;
        const float tz=length>0 ? (next.z-origin.z)/length : 1;
        const float nx=tz*site.side,nz=-tx*site.side;
        Vec3 center{};
        bool clear=false;
        for (int attempt=0;attempt<Config::ENV_PLACEMENT_ATTEMPTS;++attempt)
        {
            const float offset=Config::ENV_SIDE_DISTANCE+attempt*Config::ENV_PLACEMENT_STEP;
            center={origin.x+nx*offset+tx*Config::ENV_FORWARD_OFFSET,
                    origin.y+Config::ENV_HEIGHT_OFFSET+Config::ENV_BOB_HEIGHT*
                    static_cast<float>(std::sin(time*Config::ENV_BOB_SPEED+siteIndex)),
                    origin.z+nz*offset+tz*Config::ENV_FORWARD_OFFSET};
            clear=true;
            const float margin=Config::ENV_ISLAND_RADIUS*1.2f*site.scale+Config::ENV_PATH_MARGIN;
            for (int k=0;k<clouds.count();++k)
            {
                const Vec3 p=clouds.at(k).positionAt(time);
                if (std::hypot(center.x-p.x,center.z-p.z)<margin) {clear=false;break;}
            }
            if (clear) break;
        }
        if (!clear) continue;
        environmentCenters_[siteIndex]=center;
        if (std::hypot(center.x-playerPos.x,center.z-playerPos.z)>Config::ENV_DRAW_DISTANCE) continue;
        environmentVisible_[siteIndex]=true;
        const float yaw=std::atan2(nx,nz),s=std::sin(yaw),c=std::cos(yaw),scale=site.scale;
        auto world=[&](VECTOR v) {
            return VGet(center.x+(v.x*c+v.z*s)*scale,center.y+v.y*scale,
                        center.z+(-v.x*s+v.z*c)*scale);
        };
        for (auto* mesh : {&environmentScene_->solid,&environmentScene_->glow})
        {
            for (size_t i=0;i<mesh->local.size();++i)
            {
                mesh->world[i]=mesh->local[i];mesh->world[i].pos=world(mesh->local[i].pos);
            }
            check(DrawPolygon3D(mesh->world.data(),static_cast<int>(mesh->world.size()/3),DX_NONE_GRAPH,FALSE),
                  L"Vẽ mesh cảnh đảo nổi");
        }
        for (int i=0;i<Config::WIND_RING_SEGMENTS;++i)
        {
            const float a=Config::FULL_TURN*i/Config::WIND_RING_SEGMENTS+static_cast<float>(time*Config::ENV_RUNE_SPEED);
            const float b=a+Config::FULL_TURN/Config::WIND_RING_SEGMENTS*.65f;
            const float radius=Config::ENV_ISLAND_RADIUS;
            check(DrawLine3D(world(VGet(radius*std::cos(a),-2.0f,radius*Config::ENV_ISLAND_DEPTH_RATIO*std::sin(a))),
                             world(VGet(radius*std::cos(b),-2.0f,radius*Config::ENV_ISLAND_DEPTH_RATIO*std::sin(b))),
                             hexColor(Config::ENV_RUNE_COLOR)),L"Vòng phép quanh đảo");
        }
        auto& sparks=environmentScene_->sparks;
        size_t vertex=0;
        auto disk=[&](VECTOR p,float size,COLOR_U8 bright) {
            for (int k=0;k<Config::ENV_SPARK_SEGMENTS;++k)
            {
                const float a1=Config::FULL_TURN*k/Config::ENV_SPARK_SEGMENTS;
                const float a2=Config::FULL_TURN*(k+1)/Config::ENV_SPARK_SEGMENTS;
                VERTEX3D v{};v.pos=p;v.dif=bright;sparks[vertex++]=v;
                v.dif.a=0;v.pos=VAdd(p,VAdd(VScale(right,size*std::cos(a1)),VScale(up,size*std::sin(a1))));sparks[vertex++]=v;
                v.pos=VAdd(p,VAdd(VScale(right,size*std::cos(a2)),VScale(up,size*std::sin(a2))));sparks[vertex++]=v;
            }
        };
        for (int i=0;i<Config::ENV_SPARK_COUNT;++i)
        {
            const float phase=i*Config::BUTTERFLY_PHASE+static_cast<float>(siteIndex);
            const float a=phase+static_cast<float>(time*.15);
            const float radius=3.0f+static_cast<float>(i%7)*1.3f;
            const float y=static_cast<float>(std::fmod(time*Config::ENV_SPARK_RISE_SPEED+i*.41,Config::ENV_SPARK_HEIGHT));
            const VECTOR p=world(VGet(radius*std::cos(a),y,radius*.7f*std::sin(a)));
            const float size=Config::ENV_SPARK_RADIUS*(1.0f+.4f*std::sin(phase+static_cast<float>(time)));
            const COLOR_U8 bright=hexColorU8(i%2==0 ? Config::ENV_CRYSTAL_COLOR : Config::MOTH_COLOR);
            disk(p,size,bright);
        }
        for (const auto& canopy : Config::ENV_CANOPIES)
        {
            COLOR_U8 color=hexColorU8(canopy.color);
            color.a=Config::ENV_CANOPY_HALO_ALPHA;
            disk(world(VGet(canopy.x,canopy.y,canopy.z)),
                 std::max(canopy.rx,canopy.rz)*scale*Config::ENV_CANOPY_HALO_SCALE,color);
        }
        check(SetWriteZBuffer3D(FALSE),L"Không ghi độ sâu bụi phép");
        check(SetDrawBlendMode(DX_BLENDMODE_ADD,255),L"Sáng cộng bụi phép");
        check(DrawPolygon3D(sparks.data(),static_cast<int>(vertex/3),DX_NONE_GRAPH,TRUE),L"Bụi phép trên đảo");
        check(SetDrawBlendMode(DX_BLENDMODE_NOBLEND,255),L"Khôi phục hòa màu sau bụi phép");
        check(SetWriteZBuffer3D(TRUE),L"Khôi phục độ sâu sau bụi phép");
    }
    check(SetUseLighting(TRUE),L"Khôi phục đèn sau cảnh đảo");
}

bool Render::loadButterfly()
{
    // Model dùng màu vật liệu, không áp dụng điều kiện phải có texture của witch/cloud.
    butterflyModel_ = MV1LoadModel(Config::BUTTERFLY_MODEL_PATH);
    if (butterflyModel_ == -1)
    {
        check(-1, L"Nạp model bướm");
        return false;
    }
    butterflyClip_ = MV1AttachAnim(butterflyModel_, Config::BUTTERFLY_FLY_CLIP);
    if (butterflyClip_ == -1)
    {
        check(-1, L"Nạp animation bướm");
        return false;
    }
    butterflyClipTicks_ = MV1GetAttachAnimTotalTime(butterflyModel_, butterflyClip_);
    if (butterflyClipTicks_ <= 0.0f ||
        MV1SetUseVertDifColor(butterflyModel_, FALSE) == -1 ||
        MV1SetUseZBuffer(butterflyModel_, TRUE) == -1 ||
        MV1SetWriteZBuffer(butterflyModel_, TRUE) == -1 ||
        MV1SetScale(butterflyModel_, VGet(Config::BUTTERFLY_SCALE, Config::BUTTERFLY_SCALE,
                                         Config::BUTTERFLY_SCALE)) == -1)
    {
        check(-1, L"Thiết lập bướm");
        return false;
    }
    mothModel_ = MV1LoadModel(Config::BUTTERFLY_MODEL_PATH);
    if (mothModel_ == -1)
    {
        check(-1,L"Nạp bướm đêm");
        return false;
    }
    mothClip_ = MV1AttachAnim(mothModel_,Config::BUTTERFLY_FLY_CLIP);
    if (mothClip_ == -1 || MV1SetUseVertDifColor(mothModel_,FALSE)==-1 ||
        MV1SetScale(mothModel_,VGet(Config::MOTH_SCALE,Config::MOTH_SCALE,Config::MOTH_SCALE))==-1 ||
        MV1SetUseZBuffer(mothModel_,TRUE)==-1 || MV1SetWriteZBuffer(mothModel_,TRUE)==-1)
    {
        check(-1,L"Thiết lập bướm đêm");
        return false;
    }
    for (size_t i=0;i<butterflyPaletteModels_.size();++i)
    {
        int& model=butterflyPaletteModels_[i];
        model=MV1LoadModel(Config::BUTTERFLY_MODEL_PATH);
        if (model==-1) { check(-1,L"Nạp biến thể màu bướm"); return false; }
        butterflyPaletteClips_[i]=MV1AttachAnim(model,Config::BUTTERFLY_FLY_CLIP);
        const int color=Config::BUTTERFLY_PALETTES[i];
        const Config::RgbF rgb{((color>>16)&255)/255.0f,((color>>8)&255)/255.0f,(color&255)/255.0f};
        if (butterflyPaletteClips_[i]==-1 || MV1SetUseVertDifColor(model,FALSE)==-1 ||
            MV1SetUseZBuffer(model,TRUE)==-1 || MV1SetWriteZBuffer(model,TRUE)==-1 ||
            !tintButterfly(model,rgb,Config::BUTTERFLY_EMISSION))
        {
            check(-1,L"Thiết lập biến thể màu bướm");
            return false;
        }
    }
    return true;
}

bool Render::tintButterfly(int model, const Config::RgbF& color, float emission)
{
    const int materials=MV1GetMaterialNum(butterflyModel_);
    if (materials<=0) { check(-1,L"Đọc vật liệu bướm"); return false; }
    for (int i=0;i<materials;++i)
    {
        // Luôn lấy màu nguồn bất biến: đổi màu không tích lũy làm cánh tối dần.
        const COLOR_F source=MV1GetMaterialDifColor(butterflyModel_,i);
        const float light=std::max({source.r,source.g,source.b});
        const float dark=std::min({source.r,source.g,source.b});
        COLOR_F tint=source;
        if (light>0.5f && light-dark>light*0.35f)
            tint=GetColorF(color.r*light,color.g*light,color.b*light,1);
        if (MV1SetMaterialDifColor(model,i,tint)==-1 ||
            MV1SetMaterialEmiColor(model,i,GetColorF(tint.r*emission,tint.g*emission,tint.b*emission,1))==-1)
        {
            check(-1,L"Đổi màu cánh bướm");
            return false;
        }
    }
    return true;
}

Config::RgbF Render::rareButterflyColor(double time, int key) const
{
    const double phase=time/Config::RARE_BUTTERFLY_PERIOD*Config::FULL_TURN+key*Config::BUTTERFLY_PHASE;
    auto channel=[phase](double offset) {
        const float wave=static_cast<float>((1.0+std::cos(phase+offset))*0.5);
        return Config::RARE_BUTTERFLY_COLOR_MIN+(1.0f-Config::RARE_BUTTERFLY_COLOR_MIN)*wave;
    };
    return {channel(0),channel(Config::FULL_TURN/3),channel(2*Config::FULL_TURN/3)};
}

void Render::drawMothRegion(const CloudField& clouds, const Player& player)
{
    const Vec3 playerPos=player.position();
    const double time=clouds.time();
    check(SetUseLighting(TRUE),L"Ánh sáng vùng bướm");
    for (int c=0;c<clouds.count();++c)
    {
        const Cloud& cloud=clouds.at(c);
        if (!Config::inButterflyRegion(cloud.id)) continue;
        const Vec3 center=cloud.positionAt(time);
        if (std::hypot(playerPos.x-center.x,playerPos.z-center.z)>Config::MOTH_DRAW_DISTANCE) continue;
        for (int i=0;i<Config::MOTH_COUNT;++i)
        {
            const int key=(cloud.id-Config::MOTH_CLOUD_ID)*Config::MOTH_COUNT+i;
            const float phase=key*Config::BUTTERFLY_PHASE;
            const float radius=Config::MOTH_ORBIT_MIN+Config::MOTH_ORBIT_SPREAD*(i%7)/6.0f;
            const float a=static_cast<float>(time*Config::MOTH_SPEED)+phase;
            const float y=center.y+Config::MOTH_HEIGHT_MIN+Config::MOTH_HEIGHT_SPREAD*(i%5)/4.0f+
                          Config::BUTTERFLY_BOB*std::sin(a*Config::BUTTERFLY_BOB_RATE);
            const size_t palette=static_cast<size_t>(key)%butterflyPaletteModels_.size();
            const bool rare=Config::rareButterfly(cloud.id,i);
            const int model=rare ? mothModel_ : butterflyPaletteModels_[palette];
            const int clip=rare ? mothClip_ : butterflyPaletteClips_[palette];
            if (rare) static_cast<void>(tintButterfly(model,rareButterflyColor(time,key),Config::RARE_BUTTERFLY_EMISSION));
            check(MV1SetScale(model,VGet(Config::MOTH_SCALE,Config::MOTH_SCALE,Config::MOTH_SCALE)),L"Cỡ bướm vùng");
            check(MV1SetPosition(model,VGet(center.x+radius*std::cos(a),y,center.z+radius*std::sin(a))),L"Vị trí bướm vùng");
            check(MV1SetRotationXYZ(model,VGet(0,std::atan2(std::sin(a),-std::cos(a)),Config::BUTTERFLY_BANK*std::sin(a))),L"Hướng bướm vùng");
            const float tick=static_cast<float>(std::fmod(time*Config::BUTTERFLY_ANIMATION_FPS+
                                    phase*Config::BUTTERFLY_ANIMATION_FPS,static_cast<double>(butterflyClipTicks_)));
            check(MV1SetAttachAnimTime(model,clip,tick),L"Cánh bướm vùng");
            check(MV1DrawModel(model),L"Vẽ bướm vùng");
        }
    }
    if (Config::inButterflyRegion(player.supportingCloud()))
    {
        check(DrawStringToHandle(Config::JUMP_GUIDE_X,Config::MOTH_TITLE_Y,
              L"Bậc 10–20 · Miền bướm · Có bướm cầu vồng hiếm",hexColor(Config::MOTH_COLOR),fontMsgSmall_,GetColor(0,0,0)),
              L"Tên miền bướm đêm");
    }
}

void Render::drawButterflies(const CloudField& clouds, const Vec3& playerPos)
{
    if (butterflyModel_ == -1 || butterflyClipTicks_ <= 0.0f)
    {
        return;
    }
    const double time = clouds.time();
    check(SetUseLighting(TRUE), L"Ánh sáng bướm nhiều màu");
    for (int c = 0; c < clouds.count(); ++c)
    {
        const Cloud& cloud = clouds.at(c);
        if (Config::inButterflyRegion(cloud.id)) continue;
        const Vec3 center = cloud.positionAt(time);
        if (std::hypot(center.x - playerPos.x, center.z - playerPos.z) > Config::BUTTERFLY_DRAW_DISTANCE)
        {
            continue;
        }
        for (int i = 0; i < Config::BUTTERFLIES_PER_CLOUD; ++i)
        {
            const size_t palette=static_cast<size_t>(cloud.id+i)%butterflyPaletteModels_.size();
            const int model=butterflyPaletteModels_[palette];
            const int clip=butterflyPaletteClips_[palette];
            check(MV1SetScale(model,VGet(Config::BUTTERFLY_SCALE,Config::BUTTERFLY_SCALE,Config::BUTTERFLY_SCALE)),L"Cỡ bướm thường");
            const float phase = (cloud.id * Config::BUTTERFLIES_PER_CLOUD + i) * Config::BUTTERFLY_PHASE;
            const float angle = static_cast<float>(time * Config::BUTTERFLY_ORBIT_SPEED) + phase;
            const float radius = Config::BUTTERFLY_ORBIT_RADIUS + i * Config::BUTTERFLY_ORBIT_VARIATION;
            const float x = center.x + radius * std::cos(angle);
            const float z = center.z + radius * Config::BUTTERFLY_ORBIT_DEPTH * std::sin(angle);
            const float y = center.y + Config::BUTTERFLY_HEIGHT + i * Config::BUTTERFLY_HEIGHT_VARIATION +
                            Config::BUTTERFLY_BOB * std::sin(angle * Config::BUTTERFLY_BOB_RATE);
            const float yaw = std::atan2(std::sin(angle), -Config::BUTTERFLY_ORBIT_DEPTH * std::cos(angle));
            const float tick = static_cast<float>(std::fmod(time * Config::BUTTERFLY_ANIMATION_FPS +
                                                           phase * Config::BUTTERFLY_ANIMATION_FPS,
                                                           static_cast<double>(butterflyClipTicks_)));
            check(MV1SetAttachAnimTime(model, clip, tick), L"Vỗ cánh bướm");
            check(MV1SetPosition(model, VGet(x, y, z)), L"Vị trí bướm");
            check(MV1SetRotationXYZ(model, VGet(0.0f, yaw, Config::BUTTERFLY_BANK * std::sin(angle))),
                  L"Hướng bay bướm");
            check(MV1DrawModel(model), L"Vẽ bướm");
        }
    }
    check(SetUseLighting(TRUE), L"Khôi phục đèn sau bướm");
}

void Render::drawJumpGuide(const Player& player, const LandingMarker& marker)
{
    const int x = Config::JUMP_GUIDE_X;
    const int y = Config::WINDOW_H - Config::JUMP_GUIDE_BOTTOM;
    const bool nextCloud = marker.visible && marker.cloudId > player.supportingCloud();
    const unsigned int color = hexColor(nextCloud ? Config::JUMP_GUIDE_SAFE_COLOR : Config::WIND_ACTIVE_COLOR);
    const unsigned int edge = GetColor(0, 0, 0);
    const int width = Config::JUMP_GUIDE_BAR_WIDTH;
    const int charge = static_cast<int>(width * player.jumpCharge());
    const int textY = y - Config::JUMP_GUIDE_LINE_HEIGHT;
    if (player.grounded())
    {
        check(DrawFormatStringToHandle(x, textY, color, fontMsgSmall_, L"Đà nhảy: %d%%",
                                       static_cast<int>(player.jumpCharge() * 100.0f)), L"Báo đà nhảy");
        check(DrawBox(x, y, x + width, y + Config::JUMP_GUIDE_BAR_HEIGHT, edge, TRUE), L"Nền thanh đà");
        if (charge > 0)
        {
            check(DrawBox(x, y, x + charge, y + Config::JUMP_GUIDE_BAR_HEIGHT, color, TRUE), L"Thanh đà");
        }
    }
    const wchar_t* hint = nextCloud ? L"Điểm đáp nằm trên mây tiếp theo · Space để bật nhảy" :
        (marker.visible ? L"Cú nhảy còn đáp lại mây này · Chạy gần mép rồi căn hướng" :
                          L"Bệ cao / xa: lấy đà với Shift, dùng nhảy đôi hoặc kéo mây gần hơn");
    if (!player.grounded())
    {
        hint = player.canDoubleJump() ? L"Còn một lần nhảy · Bấm Space gần đỉnh cú bật để lên bệ cao" :
            L"Đã dùng nhảy đôi · Dùng WASD căn vòng đáp lên mây";
    }
    check(DrawStringToHandle(x, y + Config::JUMP_GUIDE_LINE_HEIGHT, hint, color, fontMsgSmall_, edge),
          L"Hướng dẫn điểm đáp");
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
            VGet(pp.x-h, y, pp.z-h), VGet(pp.x+h, y, pp.z-h),
            VGet(pp.x+h, y, pp.z+h), VGet(pp.x-h, y, pp.z+h)
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

void Render::initCloudPuffs(CloudField& clouds)
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
        for (size_t i=0;i<mesh.indices.size();i+=3)
        {
            auto point=[&](size_t index) {
                const auto p=mesh.local[variant][mesh.indices[index]].pos;
                return Vec3{p.x,p.y,p.z};
            };
            clouds.surface(variant).addTriangle(point(i),point(i+1),point(i+2));
        }
        clouds.surface(variant).buildFootprint();
        clouds.surface(variant).bakeHeightfield();
    }
}

void Render::initCloudSurfaceMesh(const CloudField& clouds)
{
    cloudSurfaceMesh_=std::make_unique<CloudSurfaceMesh>();
    for (size_t variant=0;variant<Config::CLOUD_PLATFORM_COUNT;++variant)
    {
        const auto& surface=clouds.surface(variant);
        const auto& outline=surface.outline();
        auto& vertices=cloudSurfaceMesh_->local[variant];
        vertices.reserve(static_cast<size_t>(Config::CLOUD_HEIGHT_COLS*Config::CLOUD_HEIGHT_ROWS)*12);
        const int color=Config::CLOUD_SURFACE_COLORS[variant];
        auto lower=[&](Vec3 p) {
            const float r2=p.x*p.x/(Config::CLOUD_HALF_W*Config::CLOUD_HALF_W)+
                           p.z*p.z/(Config::CLOUD_HALF_D*Config::CLOUD_HALF_D);
            p.x*=.78f;p.z*=.78f;
            p.y=-Config::CLOUD_SHELL_EDGE-Config::CLOUD_SHELL_DEPTH*std::exp(-.7f*r2);
            return p;
        };
        auto add=[&](Vec3 a,Vec3 b,Vec3 c,bool top) {
            const VECTOR cross=VCross(VGet(b.x-a.x,b.y-a.y,b.z-a.z),VGet(c.x-a.x,c.y-a.y,c.z-a.z));
            if (VSquareSize(cross)<1e-10f) return;
            const VECTOR normal=VNorm(cross);
            for (const Vec3& p : {a,b,c})
            {
                VERTEX3D v{};v.pos=VGet(p.x,p.y,p.z);v.norm=normal;
                if (top)
                {
                    constexpr float step=Config::CLOUD_HEIGHT_GRID_STEP;
                    const float dx=(surface.surfaceHeight(p.x+step,p.z)-surface.surfaceHeight(p.x-step,p.z))/(2*step);
                    const float dz=(surface.surfaceHeight(p.x,p.z+step)-surface.surfaceHeight(p.x,p.z-step))/(2*step);
                    v.norm=VNorm(VGet(-dx,1,-dz));
                }
                if (!top)
                    v.norm=VNorm(VGet(p.x/(Config::CLOUD_HALF_W*Config::CLOUD_HALF_W),
                                      p.y/9.0f,p.z/(Config::CLOUD_HALF_D*Config::CLOUD_HALF_D)));
                const float shade=top ? std::clamp(.70f+.10f*v.norm.y+.025f*v.norm.x-.06f*v.norm.z+.05f*p.y,.65f,1.0f) :
                    std::clamp(.86f+.07f*v.norm.y+.025f*v.norm.x-.05f*v.norm.z,.72f,.96f);
                v.dif=GetColorU8(static_cast<int>(((color>>16)&255)*shade),
                                   static_cast<int>(((color>>8)&255)*shade),
                                   static_cast<int>((color&255)*shade),255);
                vertices.push_back(v);
            }
        };
        std::vector<Vec3> polygon,scratch;
        polygon.reserve(outline.size()+4);scratch.reserve(outline.size()+4);
        auto clipped=[&](Vec3 a,Vec3 b,Vec3 c) {
            polygon.clear();polygon.push_back(a);polygon.push_back(b);polygon.push_back(c);
            for (size_t edge=0;edge<outline.size() && !polygon.empty();++edge)
            {
                const Vec3& p=outline[edge];const Vec3& q=outline[(edge+1)%outline.size()];
                auto gap=[&](const Vec3& v){return (q.x-p.x)*(v.z-p.z)-(q.z-p.z)*(v.x-p.x);};
                scratch.clear();Vec3 previous=polygon.back();float old=gap(previous);
                for (const Vec3& current : polygon)
                {
                    const float now=gap(current);
                    if ((old>=0)!=(now>=0))
                    {
                        const float t=old/(old-now);
                        scratch.push_back({previous.x+(current.x-previous.x)*t,
                                           previous.y+(current.y-previous.y)*t,
                                           previous.z+(current.z-previous.z)*t});
                    }
                    if (now>=0) scratch.push_back(current);
                    previous=current;old=now;
                }
                polygon.swap(scratch);
            }
            for (size_t i=1;i+1<polygon.size();++i)
            {
                add(polygon[0],polygon[i],polygon[i+1],true);
                add(lower(polygon[0]),lower(polygon[i+1]),lower(polygon[i]),false);
            }
        };
        for (int z=0;z<Config::CLOUD_HEIGHT_ROWS-1;++z) for(int x=0;x<Config::CLOUD_HEIGHT_COLS-1;++x)
        {
            auto point=[&](int dx,int dz) {
                const float px=-Config::CLOUD_HALF_W+(x+dx)*Config::CLOUD_HEIGHT_GRID_STEP;
                const float pz=-Config::CLOUD_HALF_D+(z+dz)*Config::CLOUD_HEIGHT_GRID_STEP;
                return Vec3{px,surface.surfaceHeight(px,pz),pz};
            };
            const Vec3 a=point(0,0),b=point(1,0),c=point(0,1),d=point(1,1);
            clipped(a,c,b);clipped(b,c,d);
        }
        for (size_t i=0;i<outline.size();++i)
        {
            Vec3 a=outline[i],b=outline[(i+1)%outline.size()];
            a.y=surface.surfaceHeight(a.x,a.z);b.y=surface.surfaceHeight(b.x,b.z);
            const Vec3 la=lower(a),lb=lower(b);
            Vec3 previousA=a,previousB=b;
            for (int ring=1;ring<=Config::CLOUD_SHELL_RINGS;++ring)
            {
                const float angle=PI_F*.5f*ring/Config::CLOUD_SHELL_RINGS;
                const float inset=.78f+.22f*std::cos(angle);
                const Vec3 nextA{a.x*inset,a.y+(la.y-a.y)*std::sin(angle),a.z*inset};
                const Vec3 nextB{b.x*inset,b.y+(lb.y-b.y)*std::sin(angle),b.z*inset};
                add(previousA,previousB,nextB,false);add(previousA,nextB,nextA,false);
                previousA=nextA;previousB=nextB;
            }
        }
        for (auto& v : cloudPuffs_->local[variant])
            v.pos.y=std::min(v.pos.y,surface.surfaceHeight(v.pos.x,v.pos.z)-Config::CLOUD_PUFF_SURFACE_CLEARANCE);
        cloudSurfaceMesh_->world[variant].resize(vertices.size());
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

void Render::drawClouds(const CloudField& clouds, const Vec3& playerPos)
{
    for (int i = 0; i < clouds.count(); ++i)
    {
        const Cloud& c = clouds.at(i);
        const Vec3 p = c.positionAt(clouds.time());
        if (std::hypot(p.x-playerPos.x,p.z-playerPos.z) > Config::CLOUD_DRAW_DISTANCE) continue;
        const auto& local=cloudSurfaceMesh_->local[c.model];
        auto& world=cloudSurfaceMesh_->world[c.model];
        for (size_t j=0;j<local.size();++j)
        {
            world[j]=local[j];world[j].pos=VAdd(local[j].pos,VGet(p.x,p.y,p.z));
        }
        check(DrawPolygon3D(world.data(),static_cast<int>(world.size()/3),DX_NONE_GRAPH,FALSE),L"Mặt mây cong chung va chạm");
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
        if (std::hypot(drifted.x-eye.x,drifted.z-eye.z) > Config::CLOUD_DRAW_DISTANCE) continue;
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
            // Đường rẽ có thể đi qua chỗ mây nền cũ: chừa hành lang quanh cả 40 bậc.
            bool blocksRoute = false;
            for (int k=0;k<clouds.count();++k)
            {
                const Vec3 step = clouds.at(k).positionAt(clouds.time());
                if (std::hypot(pos.x-step.x,pos.z-step.z) < Config::CLOUD_SCENERY_CLEARANCE)
                {
                    blocksRoute = true;
                    break;
                }
            }
            if (blocksRoute) continue;
            bool blocksIsland=false;
            for (size_t k=0;k<environmentVisible_.size();++k)
            {
                if (!environmentVisible_[k]) continue;
                const Vec3& island=environmentCenters_[k];
                const float radius=Config::ENV_ISLAND_RADIUS*1.2f*Config::ENVIRONMENT_SITES[k].scale+
                                   Config::ENV_SCENERY_MARGIN;
                if (std::hypot(pos.x-island.x,pos.z-island.z)<radius) {blocksIsland=true;break;}
            }
            if (blocksIsland) continue;
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
    swprintf_s(line, L"Bậc xa nhất: %d / %d · Độ cao: %dm", score+1, Config::STAIR_COUNT, alt);
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
        if (cloud.id != player.supportingCloud()+1)
        {
            continue;
        }
        const Vec3 p = cloud.positionAt(clouds.time());
        const bool selected = cloud.id == targetId;
        const unsigned int color = selected ? gold :
            (cloud.id == Config::MOTH_CLOUD_ID ? hexColor(Config::MOTH_COLOR) : cyan);
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
        const VECTOR screen = ConvWorldPosToScreenPos(VGet(p.x,p.y+Config::WIND_GOAL_HEIGHT,p.z));
        if (screen.z>0 && screen.z<1 && screen.x>=0 && screen.x<Config::WINDOW_W &&
            screen.y>=0 && screen.y<Config::WINDOW_H)
        {
            check(DrawFormatStringToHandle(static_cast<int>(screen.x),static_cast<int>(screen.y),color,
                                           fontHud_,L"Bậc %d",cloud.id+1),L"Nhãn bậc tiếp theo");
        }
    }

    const Vec3 goal = clouds.cloudPosition(Config::STAIR_COUNT - 1);
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

    const int nextStair=std::min(Config::STAIR_COUNT,player.supportingCloud()+2);
    int stage=1;
    for (const auto& tier : Config::STAIR_DIFFICULTY)
    {
        if (nextStair<=tier.lastStair) break;
        ++stage;
    }
    wchar_t objective[160];
    swprintf_s(objective,L"Chặng %d / %zu · Shift: lấy đà nhanh · Space lần hai: vượt bệ cao",stage,Config::STAIR_DIFFICULTY.size());
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
        L"Leo 40 bậc mây · Miền bướm nhiều màu từ bậc 10 đến 20",
        L"WASD: chạy · Giữ Shift: tăng tốc · Space hai lần: nhảy đôi",
        L"Rê chuột: camera · Chuột phải: kéo mây kế tiếp · R: lượt mới"
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

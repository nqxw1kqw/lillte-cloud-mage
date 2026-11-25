#include <exception>
#include <memory>

#include "DxLib.h"
#include "Config.h"
#include "Game.h"

namespace
{
    // Đảm bảo DxLib_End luôn được gọi, kể cả khi thoát sớm hoặc có ngoại lệ.
    struct DxLibGuard
    {
        DxLibGuard() = default;
        DxLibGuard(const DxLibGuard&) = delete;
        DxLibGuard& operator=(const DxLibGuard&) = delete;
        ~DxLibGuard() { DxLib_End(); }
    };

    bool hasAssetFile(const wchar_t* dir, const wchar_t* relativePath)
    {
        wchar_t path[MAX_PATH];
        if (swprintf_s(path, L"%s/%s", dir, relativePath) < 0)
        {
            return false;
        }
        const DWORD attr = GetFileAttributesW(path);
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool hasAssets(const wchar_t* dir)
    {
        if (!hasAssetFile(dir, Config::WITCH_MODEL_PATH))
        {
            return false;
        }
        for (const auto& model : Config::CLOUD_MODELS)
        {
            if (!hasAssetFile(dir, model.path))
            {
                return false;
            }
        }
        for (const wchar_t* texture : Config::MODEL_TEXTURE_PATHS)
        {
            if (!hasAssetFile(dir, texture))
            {
                return false;
            }
        }
        return true;
    }

    bool locateAssetsDirectory()
    {
        // Ưu tiên bộ tài nguyên đi cùng exe, kể cả khi shortcut đặt thư mục làm việc ở nơi khác.
        wchar_t dir[MAX_PATH];
        const DWORD len = GetModuleFileNameW(nullptr, dir, MAX_PATH);
        if (len > 0 && len < MAX_PATH)
        {
            for (int level = 0; level <= 3; ++level)
            {
                wchar_t* slash = wcsrchr(dir, L'\\');
                if (slash == nullptr)
                {
                    break;
                }
                *slash = L'\0';
                if (hasAssets(dir) && SetCurrentDirectoryW(dir) != 0)
                {
                    return true;
                }
            }
        }
        return hasAssets(L".");
    }

    void showError(const wchar_t* text)
    {
        MessageBoxW(nullptr, text, Config::WINDOW_TITLE, MB_OK | MB_ICONERROR);
    }

    // Câu báo lỗi cho người chơi theo đúng bước khởi tạo phần vẽ bị hỏng.
    const wchar_t* initErrorText(Render::InitError e)
    {
        switch (e)
        {
        case Render::InitError::ZBuffer:
            return L"Không bật được Z-buffer 3D. Hãy cập nhật driver card đồ họa rồi thử lại.";
        case Render::InitError::Culling:
            return L"Không thiết lập được chế độ vẽ mặt đa giác 3D. Hãy cập nhật driver card đồ họa rồi thử lại.";
        case Render::InitError::Fog:
            return L"Không thiết lập được hiệu ứng sương mù. Hãy cập nhật driver card đồ họa rồi thử lại.";
        case Render::InitError::Light:
            return L"Không thiết lập được ánh sáng 3D. Hãy cập nhật driver card đồ họa rồi thử lại.";
        case Render::InitError::Material:
            return L"Không thiết lập được vật liệu 3D. Hãy cập nhật driver card đồ họa rồi thử lại.";
        case Render::InitError::Font:
            return L"Không tạo được font để hiển thị tiếng Việt. Hãy kiểm tra thư mục assets/fonts.";
        case Render::InitError::Models:
            return L"Không nạp được model hoặc texture. Hãy kiểm tra thư mục assets/models đi cùng game.";
        case Render::InitError::None:
            break;
        }
        return L"Không khởi tạo được phần vẽ của game.";
    }
}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
    if (!locateAssetsDirectory())
    {
        showError(initErrorText(Render::InitError::Models));
        return -1;
    }

    // Các cài đặt này phải gọi trước DxLib_Init. Nếu hỏng thì chưa khởi tạo DxLib,
    // nên chỉ báo lỗi rồi thoát, không cần DxLib_End.
    // Tắt Log.txt ở bản Release trước mọi lời gọi DxLib khác, nếu không DxLib vẫn ghi vài dòng.
#ifndef _DEBUG
    if (SetOutApplicationLogValidFlag(FALSE) == -1)
    {
        showError(L"Không tắt được file log của DxLib.");
        return -1;
    }
#endif
    if (ChangeWindowMode(TRUE) != DX_CHANGESCREEN_OK)
    {
        showError(L"Không chuyển được sang chế độ cửa sổ.");
        return -1;
    }
    if (SetGraphMode(Config::WINDOW_W, Config::WINDOW_H, Config::COLOR_BIT_DEPTH) != DX_CHANGESCREEN_OK)
    {
        showError(L"Không đặt được kích thước màn hình game. Hãy kiểm tra card đồ họa và thử lại.");
        return -1;
    }
    if (SetMainWindowText(Config::WINDOW_TITLE) == -1)
    {
        showError(L"Không đặt được tiêu đề cửa sổ.");
        return -1;
    }
    // Vẫn chạy vòng lặp khi mất focus để ProcessMessage/ScreenFlip đều đặn và dt
    // không dồn lại; Game tự tạm dừng ván chơi khi cửa sổ không được chọn.
    if (SetAlwaysRunFlag(TRUE) == -1)
    {
        showError(L"Không thiết lập được chế độ chạy nền.");
        return -1;
    }

    if (DxLib_Init() == -1)
    {
        showError(L"Không khởi tạo được DxLib. Hãy kiểm tra card đồ họa và DirectX rồi thử lại.");
        return -1;
    }
    DxLibGuard guard;

    try
    {
        if (SetDrawScreen(DX_SCREEN_BACK) == -1)
        {
            showError(L"Không thiết lập được màn hình vẽ.");
            return -1;
        }

        // Game chứa mảng mây và sao cố định nên đặt trên heap, cấp phát một lần.
        auto game = std::make_unique<Game>();
        if (!game->Init())
        {
            showError(initErrorText(game->initError()));
            return -1;
        }

        // Biến cục bộ (không phải toàn cục) để chỉ ghi log lỗi ScreenFlip một lần.
        bool flipErrorLogged = false;
        LONGLONG last = GetNowHiPerformanceCount();
        // ProcessMessage trả -1 khi cửa sổ bị đóng hoặc lỗi, lúc đó thoát vòng lặp.
        while (ProcessMessage() == 0)
        {
            const LONGLONG now = GetNowHiPerformanceCount();
            float dt = static_cast<float>(now - last) / 1000000.0f;
            last = now;
            if (dt > Config::MAX_DT)
            {
                dt = Config::MAX_DT;
            }

            game->Update(dt);
            if (game->quitRequested())
            {
                break;
            }
            game->Render();
            // Một khung lật màn hình lỗi chưa đáng dừng game; ghi log để còn dò nguyên nhân.
            if (ScreenFlip() == -1 && !flipErrorLogged)
            {
                ErrorLogAdd(L"Lỗi DxLib: ScreenFlip thất bại.\n");
                flipErrorLogged = true;
            }
        }
    }
    catch (const std::exception&)
    {
        showError(L"Game gặp lỗi không mong muốn và phải đóng.");
        return -1;
    }

    return 0;
}

# DxLib

- **Phiên bản đang dùng:** 3.24f (cũng ghi trong `src/Config.h`, hằng `Config::DXLIB_VERSION_USED`).
- **Đường dẫn cài đặt:** mặc định `C:\DxLib\` (chứa `DxLib.h` và các file `.lib`). Có thể đổi bằng biến môi trường `DXLIB_DIR` hoặc tham số MSBuild `/p:DxLibDir=...`, xem `docs/build.md`.
- Không đổi phiên bản DxLib khi chưa được người dùng cho phép.

## Cách dự án liên kết DxLib

- `LillteCloudMage.vcxproj` định nghĩa thuộc tính `DxLibDir` (ưu tiên `/p:DxLibDir`, rồi `$(DXLIB_DIR)`, cuối cùng `C:\DxLib\`). Dự án tự thêm dấu `\` cuối nếu thiếu, ở cả ba nguồn. `/p:DxLibDir` là thuộc tính toàn cục, nên thẻ `<Project>` khai báo `TreatAsLocalProperty="DxLibDir"` để dự án được phép sửa giá trị này. Đường dẫn có dấu cách thì không ghi dấu `\` cuối trên dòng lệnh, xem `docs/build.md`.
- `DxLib.h` tự liên kết thư viện bằng `#pragma comment(lib, ...)`, nên dự án chỉ cần:
  - thư mục include `$(DxLibDir)`, khai báo qua `/external:I "$(DxLibDir)."` (dấu `.` để dấu `\` cuối không thoát mất dấu ngoặc kép);
  - thư mục thư viện `$(DxLibDir)` (`AdditionalLibraryDirectories`).
- **Character set:** Unicode. DxLib sẽ liên kết bản `DxLibW_vs2015_x64_MT(d).lib`, chuỗi dùng `wchar_t` (`L"..."`).
- **Runtime:** `/MTd` (Debug) và `/MT` (Release), khớp với thư viện `_MT` / `_MTd` của DxLib.
- **Toolset:** v143 (Visual Studio 2022), nền tảng x64.

## Mã hóa

- Mã nguồn dự án lưu UTF-8 có BOM và biên dịch với `/utf-8`.
- `DxLib.h` lưu ở mã Shift-JIS. Vì vậy DxLib được khai báo là header ngoài (`/external:I`, `ExternalWarningLevel = TurnOffAllWarnings`). Nếu không làm vậy, `/utf-8` sẽ sinh hàng loạt cảnh báo C4828/C4010 từ header của DxLib. Cảnh báo `/W4` trong mã của dự án vẫn được bật đầy đủ.

## Nơi được gọi DxLib

- `src/Render.cpp`: vẽ, font, camera, ánh sáng, sương mù.
- `src/Input.cpp`: bàn phím và chuột.
- `src/main.cpp`: khởi tạo, vòng lặp chính (`ProcessMessage`, `ScreenFlip`, đo thời gian) và `DxLib_End` qua RAII.
- `Game`, `Player`, `Cloud`, `Camera` không include `DxLib.h`.

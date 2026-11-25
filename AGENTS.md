# Quy tắc cho AI agent: lillte cloud mage (C++ + DxLib)

Tài liệu này dành cho AI agent (Claude Code, Cursor, Copilot hoặc agent khác) khi phát triển dự án game này. Đọc kỹ trước khi thay đổi bất kỳ dòng code nào.

## 1. Tổng quan dự án

- **Ngôn ngữ:** C++ (chuẩn C++17 trở lên).
- **Thư viện chính:** DxLib (thư viện game của Takumi Yamada, chạy trên Windows).
- **Thư viện khác:** được phép dùng nếu cần, theo mục 4.
- **Nền tảng mục tiêu:** Windows (Visual Studio). Các nền tảng khác chỉ làm khi người dùng yêu cầu.
- **Giao diện và thông báo trong game:** tiếng Việt.
- **Bản HTML/Three.js cũ** (`witch.html`, `phu_thuy_nhay_may.md`): chỉ là bản tham chiếu về cơ chế. Không sửa bản này trừ khi người dùng yêu cầu.

## 2. Cấu trúc dự án

Giữ cấu trúc sau, không tự ý thay đổi:

```
LillteCloudMage/
├── src/
│   ├── main.cpp          // WinMain/main, khởi tạo và vòng lặp chính
│   ├── Game.h / Game.cpp // Trạng thái game, reset, cập nhật logic
│   ├── Player.h / .cpp   // Phù thủy: nhảy, bay, rơi, va chạm
│   ├── Cloud.h / .cpp    // Mây: vị trí, kích thước, sinh và xóa
│   ├── Camera.h / .cpp   // Camera bám theo phù thủy
│   ├── Input.h / .cpp    // Bàn phím và chuột, gom về một chỗ
│   ├── Render.h / .cpp   // Vẽ phù thủy, mây, biển, HUD
│   └── Config.h          // Toàn bộ hằng số chỉnh được
├── assets/
│   ├── fonts/            // Font hỗ trợ tiếng Việt
│   ├── images/           // Ảnh (nếu có)
│   └── sounds/           // Âm thanh (nếu có)
├── lib/                  // Thư viện bên thứ ba
├── docs/                 // Ghi chú thiết kế
└── AGENTS.md
```

Quy tắc:
- Mỗi lớp một cặp `.h` và `.cpp`.
- Logic game không gọi trực tiếp hàm vẽ của DxLib. Gọi qua `Render`.
- Logic game không đọc trực tiếp phím hay chuột. Đọc qua `Input`.
- Mọi hằng số (tốc độ, lực nhảy, kích thước mây, thời gian chờ) đặt trong `Config.h`.

## 3. Quy tắc bắt buộc

1. **Dùng DxLib làm thư viện đồ họa, âm thanh và input chính.** Không dùng DirectX hay OpenGL trực tiếp trừ khi người dùng yêu cầu.
2. **Không đổi phiên bản DxLib** khi chưa được phép. Ghi rõ phiên bản đang dùng trong `docs/` và trong `Config.h`.
3. **Mọi lời gọi DxLib phải kiểm tra lỗi.** Hàm `DxLib_Init()` trả về `-1` khi thất bại, phải thoát có thông báo rõ ràng.
4. **Luôn gọi `DxLib_End()`** trước khi thoát, kể cả khi có lỗi (dùng RAII hoặc `try/catch`).
5. **Không dùng `using namespace std;` trong file `.h`.**
6. **Không dùng biến toàn cục** ngoài `Config.h`. Trạng thái nằm trong các lớp.
7. **Font phải hỗ trợ tiếng Việt.** Không dùng font mặc định nếu không chắc có dấu tiếng Việt. Tải font bằng `AddFontResourceEx` hoặc `LoadFontDataToHandle` từ thư mục `assets/fonts/`.
8. **Mã nguồn lưu UTF-8 có BOM** để Visual Studio hiển thị đúng chữ tiếng Việt trong chuỗi. Khi viết chuỗi tiếng Việt, kiểm tra hiển thị trên màn hình thật.
9. **Thời gian khung hình:** dùng `dt` (giây giữa hai khung) cho mọi chuyển động. Giới hạn `dt` tối đa 0.05 giây để tránh nhảy vọt khi máy lag.
10. **Không chặn luồng chính** (không `Sleep` dài trong vòng lặp). Dùng `ProcessMessage()` và `ScreenFlip()` mỗi khung hình.

## 4. Thư viện khác

Được phép dùng thêm thư viện C++ nếu cần, với các điều kiện sau:

- **Ưu tiên** thư viện nhẹ, có giấy phép tương thích (MIT, zlib, BSD, Boost, LGPL với liên kết động).
- **Không dùng game engine khác** (Unreal, Unity, Godot, Cocos2d-x, Ogre, Irrlicht, SFML nếu DxLib đã đủ). Nếu cần, phải hỏi người dùng trước.
- **Ghi rõ** tên, phiên bản, giấy phép và lý do vào `docs/libraries.md`.
- **Đặt mã thư viện** trong `lib/` hoặc quản lý qua vcpkg/CMake, không sao chép rải rác.
- **Báo người dùng** khi thêm thư viện, kèm dung lượng và cách cài đặt.

Ví dụ thư viện có thể cân nhắc khi cần: nlohmann/json (đọc cấu hình), spdlog (ghi log), Catch2 (kiểm thử đơn vị).

## 5. Cơ chế gameplay cần giữ

- Phù thủy **không tự bay**. Chỉ bay khi nhảy từ mây.
- **Space** hoặc **chuột trái**: nhảy khi đang đứng trên mây.
- **Chuột trái/phải** (trục X) khi đang bay: điều khiển hướng bay.
- **Chuột lên/xuống** (trục Y) khi đang bay: điều khiển lực rơi. Chuột ở trên thì rơi chậm và bay xa hơn. Chuột ở dưới thì lao nhanh.
- Rơi trúng mây thì đứng lại. Rơi mạnh thì nảy, tối đa 3 lần (`MAX_BOUNCES` trong `Config.h`).
- Rơi xuống dưới ngưỡng biển (`SEA_Y` trong `Config.h`) là thua.
- Điểm là số đám mây đã nhảy qua.

Nếu một thay đổi làm đổi các cơ chế trên, phải nói rõ với người dùng.

## 6. Quy ước code C++

- Tên lớp: `PascalCase`. Tên hàm và biến: `camelCase`. Hằng số: `UPPER_SNAKE_CASE`.
- Tên biến và hàm bằng tiếng Anh. Chuỗi hiển thị cho người chơi bằng tiếng Việt.
- Dùng `constexpr` cho hằng số, `const` cho giá trị không đổi, `auto` khi kiểu rõ ràng.
- Ưu tiên `std::unique_ptr` và `std::vector` thay cho con trỏ thô và mảng cấp phát tay.
- Không cấp phát bộ nhớ trong vòng lặp chính mỗi khung hình. Tái sử dụng đối tượng mây đã bay qua.
- Viết comment bằng tiếng Việt, ngắn gọn, giải thích **vì sao** thay vì **làm gì**.

## 7. Quy trình làm việc

1. **Đọc trước khi sửa.** Xem file liên quan và `Config.h`.
2. **Sửa nhỏ, đúng phạm vi.** Không viết lại toàn bộ dự án nếu không cần.
3. **Biên dịch thử.** Code phải biên dịch không lỗi, không cảnh báo nghiêm trọng, ở chế độ Debug và Release.
4. **Chạy thử.** Kiểm tra nhảy, rơi, va mây, rơi biển và reset.
5. **Báo cáo ngắn gọn:** file nào đổi, đổi gì, cách kiểm tra.
6. **Không thêm tính năng ngoài yêu cầu.** Đề xuất thay vì tự làm.

## 8. Điều không được làm

- Không chuyển game về HTML/JavaScript khi người dùng chưa yêu cầu.
- Không dùng `system("pause")` hay các lệnh gọi hệ thống phụ thuộc nền tảng.
- Không xóa hoặc đổi tên hàm trong `Game` (`Init`, `Update`, `Render`, `Reset`) nếu không có lý do rõ ràng.
- Không tắt kiểm tra va chạm để game dễ hơn mà không hỏi người dùng.
- Không ghi đè file người dùng đã sửa thủ công mà không hỏi.

## 9. Khi không chắc chắn

Hỏi người dùng một câu ngắn gọn trước khi làm, nhất là khi yêu cầu có thể đổi cơ chế chơi, phiên bản DxLib, nền tảng mục tiêu hoặc cấu trúc dự án.

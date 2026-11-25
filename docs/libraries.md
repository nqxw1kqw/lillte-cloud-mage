# Thư viện và tài nguyên bên thứ ba

| Tên | Phiên bản | Giấy phép | Vị trí | Lý do |
|---|---|---|---|---|
| DxLib (Takumi Yamada) | 3.24f | Giấy phép DxLib: dùng miễn phí, cả mục đích thương mại; cần ghi chú bản quyền của các thư viện mà DxLib dùng bên trong (libjpeg, libpng, zlib, Ogg Vorbis, Opus, Bullet...) theo hướng dẫn trên trang chủ DxLib | `C:\DxLib` (không chép vào repo) | Thư viện đồ họa, âm thanh và input chính theo AGENTS.md |
| Be Vietnam Pro Regular | Bản trên kho google/fonts (nhánh main, tải ngày triển khai) | SIL Open Font License 1.1 (`assets/fonts/OFL.txt`) | `assets/fonts/BeVietnamPro-Regular.ttf` (≈130 KB) | Font có đầy đủ dấu tiếng Việt cho HUD và thông báo |

Không dùng thêm thư viện C++ nào khác. Phần còn lại chỉ dùng thư viện chuẩn C++17 và Win32 API (`AddFontResourceExW`, `FindFirstFileW`, `MessageBoxW`).

## Font dự phòng

Nếu `assets/fonts` trống hoặc không nạp được Be Vietnam Pro, game dùng font hệ thống `Segoe UI`, sau đó `Arial` (cả hai đều có dấu tiếng Việt). Game không chép font của Windows vào repo vì giấy phép không cho phép phân phối lại.

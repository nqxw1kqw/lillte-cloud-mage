# Font

- `BeVietnamPro-Regular.ttf`: font Be Vietnam Pro, giấy phép SIL OFL 1.1 (`OFL.txt`), lấy từ [google/fonts](https://github.com/google/fonts/tree/main/ofl/bevietnampro).
- Game nạp mọi file `.ttf`/`.otf` trong thư mục này bằng `AddFontResourceExW(FR_PRIVATE)`, rồi tạo font theo tên `Config::FONT_FACE` ("Be Vietnam Pro").
- Muốn đổi font: chép file font có dấu tiếng Việt vào đây và sửa `FONT_FACE` trong `src/Config.h`.
- Nếu thư mục trống, game dùng Segoe UI rồi đến Arial và ghi cảnh báo vào `Log.txt`.
- Không chép font của Windows vào đây vì giấy phép không cho phép phân phối lại.

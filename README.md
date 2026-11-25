# lillte cloud mage

Game C++17 và DxLib dành cho Windows. Điều khiển một phù thủy chạy lấy đà trên những bệ mây trôi, nhảy qua khoảng trống và leo lên các tầng mây cao hơn.

## Điều khiển

- WASD hoặc phím mũi tên để chạy theo góc camera.
- Rê chuột để xoay camera góc nhìn thứ ba.
- Space hoặc chuột trái để nhảy. Chạy tới gần mép mây để lấy đà.
- Q/E để xoay nhân vật tại chỗ.
- Esc khi đang chơi để mở hộp tạm dừng và xác nhận thoát.
- Sau khi thua, bấm Space để về mây đầu rồi bấm lần nữa để nhảy.

## Biên dịch

Cần Windows 10/11 x64, Visual Studio 2022 với bộ công cụ C++ v143 và DxLib 3.24f.

1. Đặt DxLib tại `C:\DxLib`, hoặc khai báo biến môi trường `DXLIB_DIR`.
2. Mở `LillteCloudMage.sln`.
3. Chọn Debug x64 hoặc Release x64 và biên dịch.
4. Chạy `bin\Release\LillteCloudMage.exe`. Giữ thư mục `assets` tại gốc dự án, hoặc đặt cạnh file chạy khi đóng gói.

Hướng dẫn đầy đủ nằm trong [docs/build.md](docs/build.md).

## Tài nguyên

Dự án gồm model phù thủy với sáu animation, mười mẫu mây, texture và font tiếng Việt. Các bệ mây chuyển động mang theo nhân vật đang đứng. Dấu vàng dự báo điểm đáp trên bệ đang trôi.

Thông tin thư viện và font nằm trong [docs/libraries.md](docs/libraries.md). DxLib được cài riêng tại máy biên dịch.

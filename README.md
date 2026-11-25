# lillte cloud mage

Game C++17 và DxLib dành cho Windows. Chạy lấy đà, tăng tốc và nhảy đôi qua 40 bậc mây lên cao, với bốn chặng khó dần. Mỗi lượt sinh hướng rẽ và mẫu mây khác nhau; bậc 10–20 là miền bướm nhiều màu, có vài con hiếm chuyển màu liên tục.

## Điều khiển

- WASD hoặc phím mũi tên để chạy theo góc camera.
- Giữ Shift trái hoặc phải để chạy nhanh; lấy đủ đà rồi bật nhảy sẽ bay xa hơn.
- Rê chuột để xoay camera góc nhìn thứ ba.
- Space hoặc chuột trái để nhảy. Chạy tới gần mép mây để lấy đà.
- Thả rồi bấm Space/chuột trái thêm lần nữa trên không để nhảy đôi. Chỉ được thêm một lần trước khi đáp; giữ nút không tự lặp.
- Chạy khoảng 2,5 đơn vị để đầy thanh đà. Bệ cao ở chặng sau cần canh nhảy lần hai gần đỉnh cú bật đầu; dùng Shift để có thêm tầm ngang. Dấu đáp chỉ dự đoán với vận tốc hiện tại, chưa tính lần bấm nhảy đôi tương lai.
- Giữ chuột phải khi ngắm mây viền xanh để kéo. Rê camera về phía muốn đặt bệ rồi thả nút để dừng.
- Phép kéo là hỗ trợ tùy chọn. Bậc đầu, bậc 10, 20 và 40 giữ cố định; nhãn đánh số chỉ mây tiếp theo.
- Q/E để xoay nhân vật tại chỗ. R để chơi lại màn.
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

Dự án gồm model phù thủy với sáu animation, mười mẫu mây, bướm có animation vỗ cánh, texture và font tiếng Việt. Ngoài vùng đặc biệt, mỗi bệ có hai bướm nhiều màu bay quanh. Các bệ mây chuyển động mang theo nhân vật đang đứng. Dấu vàng dự báo điểm đáp trên bệ đang trôi.

Thông tin thư viện và font nằm trong [docs/libraries.md](docs/libraries.md). DxLib được cài riêng tại máy biên dịch.

Cảnh 3D mẫu có năm đảo trang trí với cổng đá vỡ, cây phát sáng, pha lê và bụi phép
bên đường leo. Xem [mô tả và ảnh cảnh mẫu](docs/environment.md).

Vùng bậc 10–20 có 14 bướm mỗi bệ, tổng cộng 154 con với 6 bảng màu; trong đó 4 con hiếm chuyển màu theo chu kỳ 14 giây. Đây là các biến thể màu của model Polyfork. Chuyển động và màu cùng dừng khi tạm dừng. Mặt đứng phủ bao ngoài của cả model và múi mây bổ sung; sát mép sẽ dừng lại khi chạy, chỉ rời bệ khi bật nhảy.

Mặt mây được dựng lượn theo độ cao, dùng chung hình học cho vẽ và va chạm.
Chân nhân vật bám theo mặt thay vì đứng trên một sàn ngang. Khi cần bật lên bệ
cao, lấy đà từ phần đỉnh mây sẽ có lợi hơn phần sườn thấp.

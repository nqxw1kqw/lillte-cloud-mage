# Cảnh 3D mẫu: đảo tàn tích giữa mây

Cảnh được dựng trực tiếp bằng mesh C++ trong `Render.cpp`, không thêm thư viện.
Các tham số bố trí, màu, kích thước và tốc độ nằm trong nhóm `ENV_*` của `Config.h`.

## Thành phần

- Đảo đá nhiều mặt với mặt đất xanh, đáy thu nhỏ về một mũi nhọn.
- Cổng đá vỡ gồm hai trụ, các khối vòm và một đoạn bị khuyết.
- Cây phân nhánh với tán xanh ngọc, xanh lam và tím, có quầng sáng mềm.
- Pha lê nhỏ, vòng phép xoay dưới đảo và bụi sáng bay lên quanh cây.

Năm đảo neo theo bậc 4, 10, 14, 18 và 20, ở hai bên đường. Đảo đầu cho thấy
phong cách cảnh từ sớm; các đảo còn lại tạo khung cho vùng bướm. Đây là cảnh
trang trí, không bổ sung mặt đáp hay mục tiêu gameplay.

## Bố trí và hiệu năng

Mesh đá và cây được tạo một lần lúc khởi tạo, các đảo dùng chung dữ liệu.
Bộ đệm đỉnh thế giới và bụi phép được tái sử dụng, không cấp phát mỗi khung.
Mỗi đảo cần hai lượt vẽ mesh và một lượt vẽ bụi/quầng sáng, cộng các nét vòng phép.
Quầng sáng dùng billboard trong suốt với blend cộng; không tạo đèn động.

Tính hướng đường tại bậc neo, rồi đặt đảo lệch sang bên. Trước khi vẽ, kiểm tra
khoảng cách tới cả 40 tâm mây; nếu gần đường thì đẩy đảo ra thêm từng bước.
Mây nền cũng được loại khỏi khoảng trống quanh đảo. Chỉ vẽ đảo trong bán kính
190 đơn vị. Nhấp nhô, bụi và vòng phép dùng thời gian CloudField nên dừng cùng
game khi tạm dừng, mất focus hoặc kết thúc lượt.

## Kiểm tra ngày 2026-11-26

- Build Debug và Release thành công.
- Xem ảnh thật tại đầu đường, giữa vùng bướm, góc nhìn ngang và góc xem cận đảo.
- Kiểm tra 120 vị trí đảo nhìn thấy trên 30 seed: chừa đủ hành lang quanh đường.
- Bộ kiểm tra tích hợp đạt 30/30, gồm 780 đoạn nhảy trên 20 đường sinh, bướm
  đổi màu, tạm dừng, thắng và chơi lại; không ghi nhận lỗi vẽ DxLib.
- Ảnh cận dùng camera kiểm tra để xem mesh, không phải một chế độ camera mới
  của game. Điều khiển chơi vẫn là camera góc nhìn thứ ba hiện tại.

Ảnh: [cận cảnh đảo](previews/environment-sample.png),
[cảnh trong đường chơi](previews/environment-gameplay.png).

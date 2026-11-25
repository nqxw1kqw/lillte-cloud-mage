# Thiết kế hiện tại: 40 bậc mây

Game C++17/DxLib 3.24f, camera góc nhìn thứ ba quay bằng chuột.
Mọi hằng số chỉnh được ở `src/Config.h`.

## Chạy và nhảy

- WASD theo góc camera, tốc độ thường 7; giữ Shift chạy tốc độ 11.
- Đủ đà sau 2,5 đơn vị chạy. Tốc độ ngang khi bật: 10 nếu chưa lấy đà,
  tối đa 16 khi chạy thường hoặc 21 khi giữ Shift. Đà giữ lại khi dừng ở mép.
- Nhảy đầu có lực dọc 11,4 và thời gian chuẩn bị 0,065 giây. Trọng lực 18.
- Bấm nhảy riêng lần nữa trên không đặt tốc độ dọc thành 10,8; giữ tốc độ ngang
  của lần bật trước, cho chọn lại hướng nếu đang giữ WASD. Không có nhảy lần ba.
- Khi đáp hẳn, số lần nhảy được phục hồi. Nhảy đôi không cần đứng trên mây,
  đây là thay đổi có chủ đích so với cơ chế một lần nhảy của bản cũ.
- Cú rơi mạnh trên 13 có thể nảy thẳng đứng tối đa 3 lần. Không phục hồi nhảy
  đôi chỉ vì đang nảy. Buffer nhảy sát lúc đáp vẫn là 0,12 giây.
- HUD hiển thị thanh đà, điểm đáp và trạng thái còn/đã dùng nhảy lần hai.
  Dự đoán tính đường bay theo vận tốc hiện tại; không tự giả định một lần bấm
  nhảy đôi trong tương lai. WASD có thể thay đổi đường đáp trên không.

## Vùng đứng trên mây

Lấy hình chiếu X/Z của model và các múi bổ sung làm vùng mép an toàn. Từ mái
model, tạo lưới độ cao 0,35 đơn vị, thêm các múi tròn và làm mượt. Mesh mặt mây
được vẽ từ chính lưới đó, với cùng đường chéo tam giác dùng trong va chạm.
Không dùng mặt đứng ngang ở Y gốc nữa: nhân vật lên xuống theo mặt mây.
Nhân vật được chặn ở mép; bước di chuyển giảm dần khi gặp sườn dốc, tránh kẹt
do chênh độ cao và tránh nhảy cóc vị trí. Xem [chi tiết mặt mây](cloud-surface.md).

Khi bay, chỉ đáp từ trên xuống qua mặt đứng; bên ngoài bao mây vẫn là khoảng
trống thật. Cùng phép cắt đoạn chuyển động được dùng cho va chạm và dự đoán.
Mây trôi trên ba trục, nhân vật đứng/nảy được mang theo bệ. Mặt biển Y=-20
là ngưỡng thua. Reset trả về đứng yên, không tự nhảy lượt mới.

## Đường 40 bậc

- Tính cả bệ xuất phát là bậc 1; bậc 40 có quả cầu vàng và kết thúc lượt.
- Mỗi lượt dùng seed mới, chọn ngẫu nhiên một trong năm model mặt rộng.
  `CloudField::reset(seed)` nhận seed cố định để kiểm tra tái lập.
- Khoảng cách và độ cao theo chặng trong bảng bên dưới. Góc rẽ thay đổi dần,
  giới hạn ±0,75 radian so với hướng tiến chung để đường không tự quay ngược.
- Giữ đủ 40 bệ trong bộ đệm, không xóa theo trục Z. Nhãn số và vòng trên bệ
  kế tiếp giúp nhận ra hướng cần nhảy. Mây nền chừa hành lang quanh toàn đường.
- Phép gió là hỗ trợ tùy chọn, thay cho hai cổng gió bắt buộc của màn 7 bệ cũ.
  Chỉ chọn bệ kế tiếp, trừ bậc 10, 20 và 40 cố định. Giữ chuột phải để kéo theo
  hướng camera, thả để giữ chỗ; dịch chuyển tối đa 44 đơn vị từ vị trí gốc.
  Đặt sai có thể kéo lại hoặc R để sinh lượt mới.

| Chặng | Bậc đích | Khoảng cách tâm | Độ cao mỗi bước |
|---|---|---|---|
| 1 | 2–9 | 24–26 | 2,2–2,8 |
| 2 | 10–20 | 28–31 | 3,0–3,6 |
| 3 | 21–30 | 33–36 | 3,7–4,3 |
| 4 | 31–40 | 38–41 | 4,4–5,0 |

Chặng đầu làm quen; chặng giữa dạy phối hợp đà và nhảy đôi; chặng cuối buộc
canh lần bật thứ hai để vượt độ cao. Shift giúp tăng biên an toàn theo phương
ngang. Không đổi lực nhảy, trọng lực hay kích thước mây trong lần tăng khó này.
Phép gió vẫn là cách hỗ trợ nếu người chơi muốn thu hẹp khoảng trống.

## Miền bướm bậc 10–20

Toàn bộ bậc 10 đến hết bậc 20, chỉ số nội bộ 9–19, có 14 bướm mỗi bệ, tổng
cộng 154 con. Dùng sáu bảng màu tím, xanh trời, xanh ngọc, hồng, vàng, cam của
model Butterfly từ Polyfork, giữ họa tiết cánh sáng/tối. Ngoài vùng vẫn có hai
bướm mỗi bệ, cũng dùng các bảng màu này. Bướm chỉ trang trí, không va chạm.

Bốn con hiếm có khóa cố định 17, 54, 91, 128 trong vùng, khoảng 2,6% đàn,
chuyển màu liên tục theo chu kỳ 14 giây với pha riêng. Màu nguồn được giữ bất
biến để đổi màu nhiều lần không làm cánh tối dần. Bướm thường giữ màu cố định.

Animation và quỹ đạo theo thời gian mô phỏng của CloudField: dừng khi mất
focus, tạm dừng, thắng/thua; reset về pha đầu. Màu hiếm cũng dùng cùng thời gian.
Dùng các handle bảng màu tái sử dụng, một handle riêng cho bướm hiếm,
không cấp phát mỗi khung. Vẽ bướm và mây trong khoảng cách giới hạn. Biển,
trăng và sao bám theo hành trình cả X/Z để hỗ trợ đường rẽ dài.

## Kiểm tra ngày 2026-11-26

- Chạy controller thật qua 20 seed × 39 đoạn = 780 đoạn: tới đúng mây đích.
  Thử điểm bật và thời điểm nhảy lần hai trên cùng controller; không dịch chuyển
  nhân vật đến đích. Tất cả 200 đoạn của chặng 4 trong mẫu dùng nhảy đôi.
  Phép kéo không được dùng trong các phép thử này.
- Kiểm tra riêng vùng mép của cả 5 model, khoảng trống ngoài mây, Shift,
  nhảy lần hai và chặn lần ba, đủ 40 bệ, seed, thắng, thua, reset, tạm dừng.
- Kiểm tra Input thật với phím giả lập: Shift trái/phải, mất focus, nhấn-thả-nhấn
  Space, giữ phím không tự nhảy đôi; phép gió và reset vẫn hoạt động.
- Kiểm tra đủ 154 bướm, 4 con hiếm, sáu bảng màu, chuyển màu thực tế của vật
  liệu trong DxLib, màu thường bất biến và đóng băng màu hiếm khi tạm dừng:
  24 kiểm tra đạt. Debug và Release build thành công, bản Release mở và phản hồi.
- Xem ảnh render DxLib tại đầu đường, bậc 15, bậc 20 và sau vùng ở bậc 21. Kiểm tra tự động không
  thay thế chơi tay để đánh giá độ khó và cảm giác điều khiển.

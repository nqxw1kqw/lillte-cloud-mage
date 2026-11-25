# Mặt mây lượn và vị trí đứng

Bản mặt đứng Y=0 giúp mép an toàn nhưng khiến nhân vật lơ lửng hoặc xuyên múi
mây. Bản mới dùng một bề mặt chung cho mesh nhìn thấy, đi bộ, va chạm rơi và
dự đoán điểm đáp.

## Cách dựng

- Giữ vùng bao X/Z từ năm model mây và các múi dưới, không thay asset nguồn.
- Thu các tam giác mái để lấy độ cao tham chiếu. Phần khe dùng độ cao mép
  gần nhất. Tạo heightfield bước 0,35, giảm các gờ quá sắc, thêm vòm và các múi
  mềm, rồi làm mượt lúc khởi tạo.
- Dựng mặt vẽ từ đúng hai tam giác mỗi ô heightfield; cắt theo bao mây.
  Mặt dưới và các múi tạo thể tích, màu đỉnh làm rõ độ cong. Mây nền tiếp tục
  dùng model gốc. Các tham số chỉnh được ở `Config.h`.
- `roofHeight` và phép cắt đường rơi dùng cùng nội suy tam giác. Không cấp phát
  dữ liệu hình học mỗi khung; các biến thể dùng lại mesh và bộ đệm đỉnh.

## Điều khiển

Vị trí Y khi đi bộ theo mặt mây. Nếu một bước đi gây chênh độ cao lớn hoặc qua
mép, giảm chiều dài bước thay vì dừng cứng hoặc dịch chuyển tức thì. Nhân vật
vẫn không tự chạy rơi khỏi mây. Nhảy từ sườn thấp có thể thiếu độ cao; quay về
phần đỉnh để lấy đà trước các bậc cao. Shift và nhảy đôi giữ nguyên lực đã chỉnh.

Đây là chỉnh hình mặt mây và vị trí gốc nhân vật, chưa phải hệ IK riêng cho
từng bàn chân. Các clip nhân vật hiện có vẫn được sử dụng.

## Xác minh

- Kiểm tra các tam giác mái được vẽ với độ cao vật lý tại tâm tam giác.
- Cả năm biến thể có độ cao thay đổi và vẫn bắt được chân ở vùng bao mép.
- Bộ kiểm tra DxLib kiểm tra 780 đoạn nhảy trên 20 seed,
  có đi bộ về đỉnh để lấy đà giữa các bước;
  đạt 35 kiểm tra với hình ảnh đứng trên sườn và toàn cảnh.
- Cảnh nền và miền bướm giữ nguyên chức năng. Cần chơi tay để đánh giá cảm giác
  di chuyển và mức độ tròn mềm của mây.

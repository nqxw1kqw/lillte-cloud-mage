# Thiết kế: từ bản Three.js sang C++ + DxLib

| Bản tham chiếu (`phu_thuy_adventures.md`) | Bản C++ |
|---|---|
| `reset()`, `action()`, `crash()`, biến `state` | `Game::Reset`, `Game::action`, `Game::Update`, `Game::State` |
| `jump()`, `land()`, phần bay trong `update()` | `Player::jump`, `Player::land`, `Player::update` |
| `addCloud()`, `extendPath()`, mảng `clouds` | `CloudField` (ring buffer cố định `CLOUD_POOL_SIZE` = 32, tái sử dụng mây) |
| `camera.position.lerp(want, 0.12)` | `Camera` bám theo phù thủy và xoay quanh nhân vật bằng chuột |
| sự kiện `keydown`, `pointerdown`, `pointermove` | `Input::update` đọc phím di chuyển, cạnh nhấn nhảy và độ dời chuột |
| Scene, mesh, HUD HTML | `Render` (nơi duy nhất gọi hàm vẽ DxLib) |

Mọi số liệu nằm trong `src/Config.h`.

## Khác biệt nhỏ so với bản gốc

- Trục Z đổi dấu (`z_dx = -z_three`), phía trước là +Z.
- Camera đặt ngay vào vị trí khi chơi lại. Bản gốc trượt camera từ gốc tọa độ ở khung đầu tiên.
- HemisphereLight được xấp xỉ bằng ánh sáng môi trường thấp, đèn trăng và một đèn phụ chiếu từ phía camera.
- Sao vẽ bằng một cụm hình vuông hướng camera. Kích thước biểu kiến thay đổi theo khoảng cách.
- Chữ có viền đen thay cho `text-shadow`.

## Thay đổi cơ chế (Phase B)

Các thay đổi cơ chế so với bản mẫu ban đầu gồm

- **Cú nảy tự động khóa chuyển động ngang.** Khi đáp mạnh, phù thủy bật thẳng lên rồi rơi lại cùng vị trí. Lực rơi giữ cố định. Chuột dành cho camera.
- **Ngưỡng nảy `BOUNCE_IMPACT_MIN` là 13.** Đáp cùng độ cao thường không nảy. Cú rơi đủ mạnh có thể vượt ngưỡng. Giới hạn là `MAX_BOUNCES = 3`.
- **Dấu điểm đáp.** Trước khi nhảy, dấu dựa theo hướng phù thủy đang nhìn. Khi đang bay, `Player::predictLanding` dùng điều khiển hiện tại và cùng bước 60 Hz với `Player::update`. Đĩa vàng nằm theo độ dốc tại điểm cắt và chịu che khuất như mây. Di chuột làm thay đổi điểm đáp dự kiến.

## Model và bề mặt đáp

- Phù thủy dùng `witch_inplace.x` và sáu clip đứng, nhảy, bay, rơi, đáp và nảy. Điểm gốc model là vị trí vật lý của nhân vật.
- Màn thử dùng bảy bệ leo cao theo bố cục cố định, gồm năm mẫu mặt rộng của bộ mây v2. Năm mẫu còn lại làm mây nền ở nhiều độ cao và ngoài đường đáp. Tỉ lệ vẽ và lưới va chạm của bệ giống nhau. Các múi phồng bổ sung nằm dưới mặt chạy. Mỗi bệ trôi theo nhịp riêng trên ba trục. Người đang đứng hoặc nảy trên bệ nhận độ dời của bệ. Dấu đáp dự đoán điểm chạm trong tương lai và vẽ điểm đó trên vị trí hiện tại của mây. Khoảng trống giữa các bệ buộc người chơi tiến gần mép trước khi bật nhảy. Khi khởi tạo, DxLib lấy lưới từng model ở gốc. Game giữ các tam giác hướng lên để tìm mặt trên. Chỉ các mặt lộ ra có độ dốc tối đa 45 độ mới nhận đáp. Mỗi bước rơi chọn giao điểm hợp lệ sớm nhất trên đoạn chuyển động, kể cả khi vị trí cuối bước đã qua mép mây.
- Mặt biển và ngưỡng thua cùng ở Y = -20. HUD hiển thị số thứ tự mây xa nhất đã chạm, kể cả khi người chơi bay qua một mây trung gian.

## Màn thử phép gió

Câu hỏi cần kiểm chứng là người chơi có thấy việc chọn chỗ đặt mây thú vị hơn chỉ nhảy theo đường sẵn có hay không. Kết luận về độ vui cần phản hồi chơi tay.

| Bệ | Nhịp chơi | Đường sang bệ sau |
|---|---|---|
| 0 | Làm quen chạy và lấy đà | Nhảy thường tới 1 |
| 1 | Dạy phép gió trong lúc đứng an toàn | Kéo mây 2 bên phải vào đường đi |
| 2 | Thử bệ do người chơi đặt | Nhảy tới 3 |
| 3 | Nghỉ và quan sát chỗ đứt tiếp theo | Kéo mây 4 bên trái vào đường đi |
| 4 | Áp dụng lại phép gió | Nhảy tới 5 |
| 5 | Đoạn kết | Nhảy tới quả cầu vàng ở 6 |
| 6 | Hoàn thành | Space chơi lại sau nhịp nghỉ ngắn |

- Mây viền xanh là mục tiêu của phép. Giữ chuột phải khóa mây kế tiếp đang ngắm. Rê camera để đặt mây phía trước bệ đang đứng, thả nút để dừng dịch chuyển. Quỹ đạo nhấp nhô tự nhiên tiếp tục sau khi đặt.
- Vị trí phép được tính theo hàm đáp ứng theo thời gian trong Cloud. Cùng một hàm được dùng cho hình vẽ, chuyển động người đứng trên mây và dự đoán đáp. Tất cả thay đổi chạy trong bước vật lý 60 Hz.
- Mây đang đỡ nhân vật được loại khỏi mục tiêu. Vị trí kéo được giới hạn quanh vị trí ban đầu. R khôi phục toàn màn nếu đặt sai.
- Phản hồi gồm vòng chọn đổi màu, luồng gió nối tới mây và trạng thái đang kéo. Mây nền giữ vai trò trang trí.

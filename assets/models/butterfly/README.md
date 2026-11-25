# Bướm Polyfork cho DxLib 3.24f

Nguồn: https://polyfork.dev/asset/butterfly-base. Shin cung cấp gói
`butterfly-base-default.zip`; giữ nguyên model gốc và giấy phép đi kèm.

## Các file

- `butterfly-base.glb`: nguồn nguyên bản, màu trên đỉnh, xương và animation.
- `butterfly.x`: bản DirectX text cho DxLib, chuyển bằng script trong thư mục này.
- `butterfly.mv1`: bản dùng trực tiếp với `MV1LoadModel`, lưu bằng DxLib 3.24f.
- `preview.png`: ảnh chụp thật khi vẽ bản MV1 trong DxLib.
- `convert_glb_to_x.py`: công cụ chuyển riêng cho model này; cần Python và NumPy
  (đã dùng Python 3.12 và NumPy 2.5.4). Không thêm thư viện vào game.
- `README_original.txt`, `LICENSE.txt`: tài liệu nguyên gốc của gói tải.

Giữ 3.611 tam giác, 26 xương, 8 màu. Vì mỗi tam giác chỉ có một màu,
script chuyển màu đỉnh thành 8 vật liệu, đổi màu tuyến tính sang sRGB.
Không cần texture. Không xuất `MeshVertexColors`: phép đọc màu của bản DxLib
đang dùng làm giá trị float 0–1 thành màu gần đen với alpha gần 0.
Các vật liệu giữ màu đúng khi nạp và lưu lại thành MV1.

Đổi hệ tọa độ bằng phản chiếu Z, đảo chiều tam giác và chuyển cả ma trận xương,
ma trận bind ngược cùng animation. Giữ đơn vị nguồn: chiều cao tư thế nghỉ khoảng
0,069 đơn vị; cần đặt scale khi dùng trong cảnh game. Mặt trước nguồn +Z thành -Z.
Cánh cần vẽ hai mặt (`MV1SetMeshBackCulling(..., DX_CULLING_NONE)`), đã lưu vào MV1.

## Animation

Animation được lấy mẫu ma trận 240 lần/giây để giảm sai lệch khi cánh vỗ nhanh.
Thời gian DxLib trả về là tick; cập nhật bằng `seconds * 240`, lặp theo tổng tick.

| Chỉ số | Tên trong file | Nghĩa | Giây | Tổng tick |
|---|---|---|---|---|
| 0 | Idle | Đứng yên | 7 | 1680 |
| 1 | Fly | Bay | 2,8 | 672 |
| 2 | Fly_fast | Bay nhanh | 1,65 | 396 |

Nạp bằng `MV1LoadModel(L"assets/models/butterfly/butterfly.mv1")` và kiểm tra -1.
Dùng `MV1AttachAnim`, `MV1SetAttachAnimTime`, `MV1DrawModel`, kiểm tra kết quả.
Giải phóng bằng `MV1DeleteModel` trước `DxLib_End`.

Để tái tạo: chạy `python convert_glb_to_x.py`, nạp X trong DxLib 3.24f,
đặt `MV1SetUseVertDifColor(handle, FALSE)` và tắt back-face culling cho từng mesh,
rồi gọi `MV1SaveModelToMV1File(handle, path, MV1_SAVETYPE_NORMAL, -1, TRUE, 0, 0, 0, 0)`.
Các cờ cuối giữ độ chính xác float của vị trí, animation và trọng số.

Render nạp MV1 bằng bộ nạp riêng dành cho màu vật liệu, có sáu bảng màu và bướm
hiếm chuyển màu. Bậc 10–20 có đàn dày hơn các bệ còn lại, xem `docs/design.md`.
Bướm dùng thời gian mô phỏng của CloudField để đi theo mây,
dừng cùng màn chơi và quay về pha đầu sau reset. Bộ nạp witch/cloud kiểm tra
phải có texture nên không dùng chung cho bướm. Bướm chỉ trang trí, không va chạm.

## Ghi công

Chọn cách phân phối mesh theo CC BY 4.0 trong giấy phép đi kèm:

Butterfly by Polyfork (https://polyfork.dev/asset/butterfly-base), CC BY 4.0.

Nghĩa: Bướm của Polyfork, giấy phép CC BY 4.0.
Thay đổi: chuyển GLB sang X/MV1, đổi hệ tọa độ, chuyển màu thành vật liệu,
lấy mẫu lại animation. Giữ ghi công này khi phân phối cùng game.

## Kiểm tra ngày 2026-11-25

- Nạp/vẽ X và MV1 bằng DxLib 3.24f: 0 lỗi; mỗi bản có 8 mesh, 8 vật liệu,
  3 animation, 3.611 tam giác. Kiểm tra ảnh thật ở tư thế nghỉ, bay và bay nhanh.
- Đối chiếu 5 thời điểm của mỗi animation cho cả hai định dạng (30 tư thế), gồm
  thời điểm nằm giữa các key. Sai lệch lớn nhất của tập đỉnh so với phép skin
  GLB gốc: 0,000036821 đơn vị (khoảng 0,037 mm nếu đơn vị nguồn là mét), dưới
  ngưỡng kiểm tra 0,05 mm. Đây là kiểm tra mẫu, không khẳng định mọi thời điểm.
- Kiểm tra tích hợp gameplay và tạm dừng được ghi trong `docs/design.md`.

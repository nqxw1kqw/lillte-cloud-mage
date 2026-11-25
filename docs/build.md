# Biên dịch và chạy

## Yêu cầu

- Windows 10/11 x64.
- Visual Studio 2022 có workload "Desktop development with C++" (toolset v143).
- DxLib 3.24f cài tại `C:\DxLib`. Xem `docs/dxlib.md`. Nếu cài ở chỗ khác, xem mục "Đổi thư mục DxLib" bên dưới.

## Biên dịch

Cách 1: mở `LillteCloudMage.sln` bằng Visual Studio 2022, chọn `Debug|x64` hoặc `Release|x64`, rồi Build.

Cách 2: dùng MSBuild trong PowerShell, đứng ở thư mục gốc dự án:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe' LillteCloudMage.sln /p:Configuration=Debug /p:Platform=x64 /m
& 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe' LillteCloudMage.sln /p:Configuration=Release /p:Platform=x64 /m
```

File exe nằm ở `bin\Debug\` hoặc `bin\Release\`. File trung gian nằm ở `obj\`.

## Đổi thư mục DxLib

Dự án đọc thư mục DxLib từ thuộc tính MSBuild `DxLibDir`, theo thứ tự ưu tiên:

1. Tham số dòng lệnh, ví dụ `/p:DxLibDir=D:\Libs\DxLib`.
2. Biến môi trường `DXLIB_DIR`, ví dụ `$env:DXLIB_DIR = 'D:\Libs\DxLib'` trước khi chạy MSBuild hoặc mở Visual Studio.
3. Mặc định `C:\DxLib\`.

Dấu `\` ở cuối đường dẫn không bắt buộc. Dự án tự thêm nếu thiếu, ở cả ba cách trên.

Nếu đường dẫn có dấu cách, đặt cả tham số trong ngoặc và **không** ghi dấu `\` cuối. Lý do: PowerShell tự thêm ngoặc kép quanh tham số có dấu cách, và dấu `\` cuối sẽ thoát mất dấu ngoặc kép đóng, làm hỏng các tham số phía sau.

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe' LillteCloudMage.sln /p:Configuration=Release /p:Platform=x64 /p:DxLibDir=D:\Libs\DxLib /m
& 'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe' LillteCloudMage.sln /p:Configuration=Release /p:Platform=x64 '/p:DxLibDir=D:\My Libs\DxLib' /m
```

## Chạy

- Từ Visual Studio: F5. Thư mục làm việc là thư mục gốc dự án nên `assets/` được tìm thấy.
- Chạy exe trực tiếp cũng được. Game tìm đủ model và texture trong thư mục chứa exe và tối đa 3 cấp cha, rồi mới xét thư mục làm việc. Khi chép game sang nơi khác, đặt `assets/` cạnh exe và giữ các texture PNG cạnh model `.x` tương ứng.
- Bản Debug ghi `Log.txt` của DxLib vào thư mục làm việc. Bản Release không ghi log.

## Điều khiển

- **WASD / phím mũi tên:** chạy theo hướng camera. **Q/E:** xoay phù thủy tại chỗ.
- **Chuột:** xoay camera góc nhìn thứ ba trực tiếp. Con trỏ trở lại khi tạm dừng, chuyển cửa sổ hoặc thua.
- **Space / chuột trái:** bật nhảy. Chạy tới gần mép bệ để lấy đà rồi nhảy sang bệ kế tiếp. Có thể dừng ở mép để căn camera mà vẫn giữ đà. Khi ở trên không, WASD chỉnh hướng nhẹ. Lực rơi giữ cố định.
- **Dấu đáp:** lúc đứng, đĩa vàng cho biết điểm có thể đáp theo hướng chuẩn bị nhảy. Khi đang bay, dấu cập nhật theo điều khiển hiện tại. Mây hoặc phù thủy ở phía trước có thể che khuất dấu. Mô phỏng dự đoán và chuyển động thật cùng dùng bước 60 Hz.
- **Esc** khi đang chơi: hiện hộp hỏi "Thoát game?" và dừng ván chơi. Nhấn **Esc** lần nữa để thoát, hoặc **Space**/**chuột trái** để chơi tiếp (cú bấm này không làm phù thủy nhảy).
- **Esc** ở màn hình chờ hoặc sau khi thua: thoát ngay.
- **Tạm dừng tự động:** khi chuyển sang cửa sổ khác lúc đang chơi, game dừng lại và hiện "Tạm dừng". Nhấp vào cửa sổ game để chơi tiếp. Cú nhấp để chọn lại cửa sổ không tính là nhảy.
- Sau khi rơi xuống biển, chờ 0,8 giây rồi nhấn Space hoặc chuột trái để trở về mây đầu. Chọn hướng và bấm thêm một lần để nhảy.

## Ghi chú

- Game dùng `witch_inplace.x` và bộ 10 model trong `assets/models/clouds_v2/`. Năm mẫu mặt rộng làm bệ chạy và đáp. Năm mẫu còn lại nằm hai bên đường mây hoặc ở tầng nền. Cứ bốn bệ có một bước leo cao hơn.
- Hệ trục: DxLib dùng hệ tay trái, nên mọi tọa độ Z của bản Three.js được đổi dấu (`z_dx = -z_three`). Phía trước là +Z.

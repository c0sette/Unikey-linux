### Tại sao
Ibus-bamboo ,unikey,fcxit5 like a shit, quá nhiều gạch chân và không tập trung vấn đề chính
Bộ gõ này sẽ đỡ hơn 1 chút , nhưng nó sẽ cần thêm thời gian để cải thiện
Cảm ơn
# Unikey-linux

Bộ gõ tiếng Việt cho Wayland Linux.

- Kiểu gõ: **Telex**
- Bảng mã: **Unicode**
- Tốc độ gõ nhanh, không lag
- Không có preedit (gõ trực tiếp)
- Toggle VI/EN: **Ctrl+Space**

## Yêu cầu

- Linux với Wayland (Sway, Hyprland, GNOME Wayland, KDE Wayland, ...)
- `libevdev`
- `wtype`

## Cài đặt dependencies

### Arch Linux / Manjaro

```bash
sudo pacman -S libevdev wtype
```

### Ubuntu / Debian

```bash
sudo apt install libevdev-dev wtype
```

### Fedora

```bash
sudo dnf install libevdev-devel wtype
```

### openSUSE

```bash
sudo zypper install libevdev-devel wtype
```

### Void Linux

```bash
sudo xbps-install libevdev-devel wtype
```

### NixOS

```nix
# configuration.nix
environment.systemPackages = with pkgs; [
  libevdev
  wtype
];
```

### Gentoo

```bash
sudo emerge dev-libs/libevdev gui-apps/wtype
```

### Build wtype từ source (nếu không có trong repo)

```bash
git clone https://github.com/atx/wtype.git
cd wtype
meson setup build
ninja -C build
sudo ninja -C build install
```

## Cấu hình tốc độ gõ

Mở file `keyboard.c` và chỉnh 2 thông số sau phù hợp với tốc độ gõ của bạn:

### 1. WORD_TIMEOUT_MS (dòng 28)

```c
#define WORD_TIMEOUT_MS 400
```

Thời gian (milliseconds) để reset buffer từ khi ngừng gõ.
- **Gõ nhanh**: giảm xuống `200-300`
- **Gõ chậm**: tăng lên `500-800`
- Nếu từ bị reset giữa chừng khi gõ → tăng giá trị này
- Nếu từ trước dính vào từ sau → giảm giá trị này

### 2. usleep (dòng 225)

```c
usleep(1000);
```

Thời gian nghỉ (microseconds) khi không có sự kiện bàn phím.
- `1000` = 1ms (mặc định, cân bằng)
- `500` = 0.5ms (phản hồi nhanh hơn, CPU cao hơn chút)
- `2000` = 2ms (tiết kiệm CPU hơn)

> **Lưu ý**: Sau khi chỉnh sửa, cần compile lại: `make clean && make`

## Compile

```bash
git clone https://github.com/c0sette/Unikey-linux.git
cd Unikey-linux
make
```

## Chạy thử

```bash
sudo ./unikey
```

> **Lưu ý**: Cần quyền root để đọc `/dev/input/event*`

## Cài đặt systemd service (tự khởi động)

### 1. Copy binary vào /usr/local/bin

```bash
sudo cp unikey /usr/local/bin/
```

### 2. Tạo thư mục service

```bash
mkdir -p ~/.config/systemd/user
```

### 3. Copy service file

```bash
cp unikey.service ~/.config/systemd/user/
```

### 4. Sửa đường dẫn trong service file (nếu cần)

```bash
nano ~/.config/systemd/user/unikey.service
```

Đảm bảo `ExecStart` trỏ đúng đường dẫn:

```ini
ExecStart=/usr/local/bin/unikey
```

### 5. Enable và start service

```bash
systemctl --user daemon-reload
systemctl --user enable unikey.service
systemctl --user start unikey.service
```

### 6. Kiểm tra trạng thái

```bash
systemctl --user status unikey.service
```

### 7. Xem log

```bash
journalctl --user -u unikey.service -f
```

## Quản lý service

```bash
# Dừng
systemctl --user stop unikey.service

# Khởi động lại
systemctl --user restart unikey.service

# Tắt tự khởi động
systemctl --user disable unikey.service
```

## Sử dụng

| Phím | Chức năng |
|------|-----------|
| Ctrl+Space | Chuyển đổi VI/EN |
| a, e, o, u, i + dấu | Gõ tiếng Việt |

### Bảng dấu Telex

| Phím | Dấu |
|------|-----|
| s | sắc (á) |
| f | huyền (à) |
| r | hỏi (ả) |
| x | ngã (ã) |
| j | nặng (ạ) |
| z | xóa dấu |
| w | ư, ơ, ă |
| aa | â |
| ee | ê |
| oo | ô |
| dd | đ |

## License

MIT

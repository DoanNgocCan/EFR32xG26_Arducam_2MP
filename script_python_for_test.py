import serial
import time
import struct
import os
from PIL import Image

# --- CẤU HÌNH ---
SERIAL_PORT = 'COM13'
BAUD_RATE = 115200      # Tốc độ an toàn
IMAGE_WIDTH = 112
IMAGE_HEIGHT = 112
EXPECTED_IMAGE_SIZE = IMAGE_WIDTH * IMAGE_HEIGHT * 2
CHUNK_SIZE = 1024       # Phải khớp với EFR32

# Thư mục lưu trữ
RGB565_FOLDER = "data_rgb565"
RGB888_FOLDER = "image_rgb888" # Sửa lại tên biến để nhất quán

# --- ĐỊNH NGHĨA GÓI TIN (phải khớp với EFR32) ---
SOP = b'\xDE\xAD\xBE\xEF'
ACK = b'\x06'

def setup_directories():
    """Kiểm tra và tạo các thư mục lưu trữ nếu cần."""
    if not os.path.exists(RGB565_FOLDER):
        os.makedirs(RGB565_FOLDER)
        print(f"Đã tạo thư mục: {RGB565_FOLDER}")
    if not os.path.exists(RGB888_FOLDER):
        os.makedirs(RGB888_FOLDER)
        print(f"Đã tạo thư mục: {RGB888_FOLDER}")

def convert_and_save_png(raw_data, base_filename):
    """Chuyển đổi dữ liệu thô RGB565 sang ảnh PNG (RGB888)."""
    try:
        img = Image.new('RGB', (IMAGE_WIDTH, IMAGE_HEIGHT))
        pixels = img.load()
        for y in range(IMAGE_HEIGHT):
            for x in range(IMAGE_WIDTH):
                index = (y * IMAGE_WIDTH + x) * 2
                pixel_565 = int.from_bytes(raw_data[index:index+2], 'little')
            
                r5 = (pixel_565 >> 11) & 0x1F
                g6 = (pixel_565 >> 5)  & 0x3F
                b5 = pixel_565        & 0x1F
            
                r8 = (r5 << 3) | (r5 >> 2)
                g8 = (g6 << 2) | (g6 >> 4)
                b8 = (b5 << 3) | (b5 >> 2)
                pixels[x, y] = (r8, g8, b8)

        # Sửa lỗi: Tạo đường dẫn file PNG hoàn chỉnh
        output_filepath = os.path.join(RGB888_FOLDER, f"{base_filename}.png")
        img.save(output_filepath)
        print(f"    -> Đã chuyển đổi và lưu thành '{output_filepath}'")
    except Exception as e:
        print(f"    -> Lỗi khi chuyển đổi sang PNG: {e}")

def receive_and_process_packet(ser):
    print(f"\n[{time.strftime('%H:%M:%S')}] Đang tìm kiếm gói tin...")
    
    # 1. Tìm SOP
    ser.read_until(SOP)
    print(f"[{time.strftime('%H:%M:%S')}] Tìm thấy SOP! Đang đọc header...")

    # 2. Đọc Header (Total Size)
    header_data = ser.read(4)
    if len(header_data) != 4:
        print("    LỖI: Không đọc được header. Đang đồng bộ lại...")
        return
    
    total_size = struct.unpack('<I', header_data)[0]
    print(f"    - Header: Tổng kích thước ảnh = {total_size} bytes")

    if total_size != EXPECTED_IMAGE_SIZE:
        print(f"    LỖI: Kích thước không hợp lệ. Đang đồng bộ lại...")
        ser.reset_input_buffer()
        return

    # 3. Gửi ACK đầu tiên để bắt đầu truyền
    print("    - Gửi ACK đầu tiên...")
    ser.write(ACK)
    
    # 4. Nhận dữ liệu theo từng chunk và gửi ACK sau mỗi chunk
    image_data = bytearray()
    while len(image_data) < total_size:
        bytes_to_read = min(CHUNK_SIZE, total_size - len(image_data))
        
        chunk = ser.read(bytes_to_read)
        if len(chunk) < bytes_to_read:
            print(f"\n    LỖI: Timeout. Đã nhận {len(image_data) + len(chunk)}/{total_size} bytes.")
            return
        
        image_data.extend(chunk)
        
        ser.write(ACK)
        print(f"\r    - Đã nhận: {len(image_data)}/{total_size} bytes...", end="")
        
    print("\n    - Đã nhận đủ dữ liệu ảnh.")

    # 5. Lưu và chuyển đổi
    timestamp = int(time.time())
    base_filename = f"image_{timestamp}"
    rgb565_filepath = os.path.join(RGB565_FOLDER, f"{base_filename}.rgb565")
    with open(rgb565_filepath, 'wb') as f:
        f.write(image_data)
    print(f"    -> Đã lưu file gốc vào '{rgb565_filepath}'")
    
    # Sửa lỗi: Truyền base_filename thay vì đường dẫn thư mục
    convert_and_save_png(image_data, base_filename)

if __name__ == "__main__":
    setup_directories()
    try:
        # Tăng timeout lên 5 giây cho an toàn ở baudrate thấp
        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=5) as ser:
            print(f"Đã mở cổng serial {ser.name} thành công. Tốc độ: {BAUD_RATE} bps.")
            ser.reset_input_buffer()
            while True:
                receive_and_process_packet(ser)
    except Exception as e:
        print(f"LỖI: {e}")
import sys
from PIL import Image

# --- Cấu hình Kích thước Ảnh ---
IMAGE_WIDTH = 112
IMAGE_HEIGHT = 112

def convert_rgb565_to_png(input_filepath):
    """
    Đọc file dữ liệu thô RGB565 và chuyển đổi nó thành file ảnh PNG.

    Args:
        input_filepath (str): Đường dẫn đến file .rgb565 đầu vào.
    """
    try:
        # 1. Đọc toàn bộ dữ liệu nhị phân từ file
        with open(input_filepath, 'rb') as f:
            raw_data = f.read()
    except FileNotFoundError:
        print(f"Lỗi: Không tìm thấy file '{input_filepath}'")
        return

    expected_size = IMAGE_WIDTH * IMAGE_HEIGHT * 2
    if len(raw_data) != expected_size:
        print(f"Lỗi: Kích thước file không hợp lệ.")
        print(f"  - Kích thước mong đợi: {expected_size} bytes")
        print(f"  - Kích thước thực tế: {len(raw_data)} bytes")
        return

    print(f"Đã đọc thành công {len(raw_data)} bytes từ '{input_filepath}'.")

    # 2. Tạo một ảnh mới ở chế độ RGB (8-bit mỗi kênh)
    img = Image.new('RGB', (IMAGE_WIDTH, IMAGE_HEIGHT))
    pixels = img.load()

    # 3. Lặp qua từng pixel để chuyển đổi
    # raw_data là một chuỗi byte, chúng ta cần đọc 2 byte một lúc
    for y in range(IMAGE_HEIGHT):
        for x in range(IMAGE_WIDTH):
            # Tính toán vị trí của pixel trong buffer dữ liệu thô
            index = (y * IMAGE_WIDTH + x) * 2
            
            # Đọc 2 byte (16-bit) cho pixel hiện tại (little-endian)
            pixel_565 = int.from_bytes(raw_data[index:index+2], 'little')
            
            # Trích xuất các thành phần màu 5, 6, 5 bit
            #   GGGG GGRR RRRx BBBB B
            #   [15..11]   [10..5]    [4..0]
            r5 = (pixel_565 >> 11) & 0x1F
            g6 = (pixel_565 >> 5)  & 0x3F
            b5 = pixel_565        & 0x1F
            
            # Chuyển đổi (scale up) sang 8-bit
            # Cách làm chính xác là nhân với 255 và chia cho giá trị max của từng kênh
            # r8 = (r5 * 255) / 31
            # g8 = (g6 * 255) / 63
            # b8 = (b5 * 255) / 31
            # Tuy nhiên, dịch bit (bit-shifting) nhanh hơn và cho kết quả gần tương đương:
            r8 = (r5 << 3) | (r5 >> 2) # Trải 5 bit ra 8 bit
            g8 = (g6 << 2) | (g6 >> 4) # Trải 6 bit ra 8 bit
            b8 = (b5 << 3) | (b5 >> 2) # Trải 5 bit ra 8 bit

            # Gán giá trị pixel RGB888 vào ảnh mới
            pixels[x, y] = (r8, g8, b8)

    # 4. Lưu ảnh kết quả
    output_filepath = input_filepath.replace('.rgb565', '.png')
    img.save(output_filepath)
    print(f"Thành công! Ảnh đã được chuyển đổi và lưu thành '{output_filepath}'")

# --- Chương trình chính ---
if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Sử dụng: python convert_rgb565.py <tên_file_ảnh.rgb565>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    convert_rgb565_to_png(input_file)
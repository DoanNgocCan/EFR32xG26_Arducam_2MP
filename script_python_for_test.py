import serial
import time

# --- CẤU HÌNH ---
SERIAL_PORT = 'COM13'  # << THAY ĐỔI thành cổng COM của bạn
BAUD_RATE = 115200    # Tốc độ của VCOM
IMAGE_WIDTH = 112
IMAGE_HEIGHT = 112
IMAGE_SIZE = IMAGE_WIDTH * IMAGE_HEIGHT * 2  # 25088 bytes for RGB565

# --- HÀM ---
def receive_image(ser):
    """Lắng nghe, nhận và lưu một file ảnh."""
    print(f"\nWaiting for IMAGE_START signal on {ser.name}...")
    
    # Đợi tín hiệu bắt đầu
    ser.read_until(b'IMAGE_START\n')
    print("Start signal received! Receiving image data...")

    # Đọc chính xác số byte của ảnh
    image_data = ser.read(IMAGE_SIZE)

    # Đọc tín hiệu kết thúc để dọn dẹp buffer
    ser.read_until(b'IMAGE_END\n')

    if len(image_data) == IMAGE_SIZE:
        filename = f"received_image_{int(time.time())}.rgb565"
        with open(filename, 'wb') as f:
            f.write(image_data)
        print(f"SUCCESS: Image received and saved as '{filename}'")
        print(f" -> Size: {len(image_data)} bytes")
    else:
        print(f"ERROR: Expected {IMAGE_SIZE} bytes, but received {len(image_data)}")
        print(" -> Data transfer might have failed or timed out.")

# --- CHƯƠNG TRÌNH CHÍNH ---
if __name__ == "__main__":
    try:
        # Mở cổng serial với timeout
        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=5) as ser:
            print(f"Successfully opened serial port {ser.name}")
            
            # Xóa buffer đầu vào để bắt đầu sạch sẽ
            ser.reset_input_buffer()

            # Vòng lặp vô tận để nhận nhiều ảnh
            while True:
                receive_image(ser)

    except serial.SerialException as e:
        print(f"Error opening or reading from serial port: {e}")
    except KeyboardInterrupt:
        print("\nProgram terminated by user.")
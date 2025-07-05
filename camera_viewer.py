import os
import time
import threading
import numpy as np
import cv2  # Thư viện OpenCV

# Thư viện quan trọng từ MLTK để giao tiếp qua J-Link
try:
    from mltk.utils.jlink_stream import JlinkStream, JlinkStreamOptions
except ImportError:
    print("Lỗi: Không tìm thấy thư viện MLTK.")
    print("Vui lòng cài đặt bằng lệnh: pip install silabs-mltk")
    exit(-1)

# ------------------- CẤU HÌNH ------------------- #
# Các tham số này phải khớp chính xác với firmware của bạn
IMAGE_WIDTH = 96
IMAGE_HEIGHT = 96
# Firmware đang gửi định dạng RGB565, tức là 2 byte mỗi pixel
IMAGE_CHANNELS = 1

# Tên của JLink stream mà firmware đang ghi dữ liệu vào
JLINK_STREAM_NAME = "image"

# --- CẢI TIẾN QUAN TRỌNG: Cơ chế đồng bộ Magic Bytes ---
# Chuỗi byte này phải khớp chính xác với chuỗi trong file .cc
MAGIC_BYTES = b'\xDE\xAD\xBE\xEF'

# Thư mục để lưu ảnh (nếu bật)
OUTPUT_DIR = "captured_images"
SAVE_IMAGES = False  # Đặt là True để lưu ảnh, False để chỉ xem
# -------------------------------------------------- #

# Biến toàn cục để báo hiệu cho luồng dừng lại
stop_event = threading.Event()

def convert_rgb565_to_bgr(rgb565_image):
    """
    Chuyển đổi một ảnh định dạng RGB565 (dưới dạng mảng NumPy 16-bit)
    sang định dạng BGR (mảng NumPy 8-bit 3 kênh) để OpenCV hiển thị.
    """
    # Chuyển sang kiểu dữ liệu lớn hơn để tránh tràn số khi dịch bit
    img_u32 = rgb565_image.astype(np.uint32)
    # Tách các kênh màu từ dữ liệu 16-bit theo chuẩn RRRRRGGGGGGBBBBB
    red_channel   = (img_u32 & 0xF800) >> 8
    green_channel = (img_u32 & 0x07E0) >> 3
    blue_channel  = (img_u32 & 0x001F) << 3
    # Ghép các kênh lại thành một ảnh 3 kênh theo thứ tự B, G, R
    bgr_image = np.stack([blue_channel, green_channel, red_channel], axis=-1).astype(np.uint8)
    return bgr_image

def jlink_image_processor():
    """
    Hàm này chạy trong một luồng riêng, chịu trách nhiệm kết nối J-Link,
    đọc dữ liệu ảnh, hiển thị và lưu lại.
    """
    print("Luồng xử lý J-Link đang bắt đầu...")

    # Tính toán số byte cho mỗi ảnh
    image_byte_count = IMAGE_WIDTH * IMAGE_HEIGHT * IMAGE_CHANNELS
    print(f"Định dạng ảnh: RGB565 ({IMAGE_WIDTH}x{IMAGE_HEIGHT})")
    print(f"Kích thước mỗi khung hình: {image_byte_count} bytes")
    print(f"Chuỗi đồng bộ (Magic Bytes): {MAGIC_BYTES.hex().upper()}")

    # Tạo thư mục lưu ảnh nếu chưa có
    if SAVE_IMAGES:
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        print(f"Sẽ lưu ảnh vào thư mục: '{OUTPUT_DIR}'")

    jlink_stream = None
    image_stream = None

    try:
        # Khởi tạo và kết nối J-Link stream
        opts = JlinkStreamOptions()
        opts.polling_period = 0.01  # Giảm thời gian chờ để phản hồi nhanh hơn
        jlink_stream = JlinkStream(opts)
        jlink_stream.connect()
        print("Đã kết nối với J-Link.")

        # Mở stream có tên khớp với firmware
        while not stop_event.is_set():
            try:
                image_stream = jlink_stream.open(JLINK_STREAM_NAME, mode='r')
                print(f"Đã mở thành công J-Link stream '{JLINK_STREAM_NAME}'.")
                break
            except Exception as e:
                print(f"Chưa tìm thấy stream '{JLINK_STREAM_NAME}', đang thử lại...")
                time.sleep(1)

        # Vòng lặp chính của luồng
        while not stop_event.is_set():

            # --- CẢI TIẾN QUAN TRỌNG: Tìm Magic Bytes để đồng bộ ---
            sync_buffer = b''
            found_magic_bytes = False
            while not stop_event.is_set():
                # Đọc từng byte một để tìm chuỗi đồng bộ
                byte = image_stream.read(1, timeout=1.0)
                if not byte: # Timeout, có thể do kit bị treo hoặc ngắt kết nối
                    print("Timeout khi chờ dữ liệu. Đang kiểm tra lại kết nối...")
                    break
                
                sync_buffer += byte
                
                # Nếu tìm thấy chuỗi magic bytes, thoát khỏi vòng lặp tìm kiếm
                if MAGIC_BYTES in sync_buffer:
                    found_magic_bytes = True
                    break
                
                # Giới hạn buffer để tránh tràn bộ nhớ nếu nhận phải dữ liệu rác
                if len(sync_buffer) > len(MAGIC_BYTES) * 2:
                    sync_buffer = sync_buffer[-len(MAGIC_BYTES):]
            
            # Nếu không tìm thấy (do timeout hoặc stop_event), tiếp tục vòng lặp ngoài
            if not found_magic_bytes:
                continue

            # --- Đọc dữ liệu ảnh sau khi đã đồng bộ ---
            # Giờ chúng ta chắc chắn rằng các byte tiếp theo là một khung hình hoàn chỉnh
            img_bytes = image_stream.read_all(image_byte_count, timeout=1.0)

            # Kiểm tra xem có đọc đủ byte không
            if not img_bytes or len(img_bytes) != image_byte_count:
                print("Đọc ảnh thất bại sau khi đồng bộ, đang tìm lại Magic Bytes...")
                continue

            # --- Xử lý dữ liệu ảnh ---
            # 1. Chuyển đổi dữ liệu byte thành mảng NumPy 16-bit
            #    Sử dụng '<u2' để chỉ định rõ là số nguyên không dấu 16-bit, little-endian
            #img_buffer_16bit = np.frombuffer(img_bytes, dtype='<u2').reshape((IMAGE_HEIGHT, IMAGE_WIDTH))
            img_buffer = np.frombuffer(img_bytes, dtype=np.uint8)

            # 2. Chuyển đổi màu từ RGB565 sang BGR bằng hàm tự viết
            #img = convert_rgb565_to_bgr(img_buffer_16bit)
            img = np.reshape(img_buffer, (IMAGE_HEIGHT, IMAGE_WIDTH))

            # 3. Hiển thị ảnh lên màn hình
            cv2.imshow('Live Camera Feed from EFR32xG26', img)

            # 4. Lưu ảnh vào file nếu được bật
            if SAVE_IMAGES:
                timestamp = time.strftime("%Y%m%d_%H%M%S")
                millis = int((time.time() - int(time.time())) * 1000)
                filename = f"{timestamp}_{millis:03d}.png"
                filepath = os.path.join(OUTPUT_DIR, filename)
                cv2.imwrite(filepath, img)

            # Đợi 1ms và kiểm tra phím bấm 'q'. Cần thiết để cửa sổ imshow hoạt động
            if cv2.waitKey(1) & 0xFF == ord('q'):
                print("Đã nhấn 'q', đang dừng...")
                stop_event.set()

    except Exception as e:
        print(f"\nLỗi nghiêm trọng trong luồng J-Link: {e}")
    finally:
        # Dọn dẹp tài nguyên
        print("\nĐang dọn dẹp và đóng kết nối...")
        if jlink_stream and jlink_stream.is_connected:
            jlink_stream.disconnect()
            print("Đã ngắt kết nối J-Link.")
        cv2.destroyAllWindows()
        print("Luồng xử lý đã dừng.")

if __name__ == "__main__":
    print("--- Trình xem Camera qua J-Link (Phiên bản tối ưu) ---")
    print("Bắt đầu khởi tạo kết nối...")
    print("Nhấn 'q' trong cửa sổ camera hoặc CTRL+C trong terminal để thoát.")

    # Tạo và khởi động luồng xử lý
    image_thread = threading.Thread(target=jlink_image_processor, daemon=True)
    image_thread.start()

    try:
        # Giữ luồng chính chạy để chờ tín hiệu CTRL+C
        while image_thread.is_alive():
            image_thread.join(timeout=0.5)
    except KeyboardInterrupt:
        print("\nĐã nhận tín hiệu CTRL+C. Đang yêu cầu luồng dừng lại...")
        stop_event.set()
        image_thread.join()

    print("Chương trình đã kết thúc.")
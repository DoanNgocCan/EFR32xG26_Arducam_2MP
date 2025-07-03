import os
import time
import threading
import numpy as np
import cv2  # Thư viện OpenCV

# Thư viện quan trọng từ MLTK để giao tiếp qua J-Link
# Đảm bảo bạn đã cài đặt: pip install silabs-mltk
try:
    from mltk.utils.jlink_stream import JlinkStream, JlinkStreamOptions
except ImportError:
    print("Lỗi: Không tìm thấy thư viện MLTK.")
    print("Vui lòng cài đặt bằng lệnh: pip install silabs-mltk")
    exit(-1)

# ------------------- CẤU HÌNH ------------------- #
# THAY ĐỔI CÁC THAM SỐ NÀY ĐỂ KHỚP VỚI FIRMWARE CỦA BẠN
IMAGE_WIDTH = 96
IMAGE_HEIGHT = 96
# 3 cho ảnh màu (RGB888), 1 cho ảnh xám (Grayscale)
# Phải khớp với TEST_DATA_FORMAT trong file .cc của bạn
IMAGE_CHANNELS = 2

# Tên của JLink stream mà firmware đang ghi dữ liệu vào
JLINK_STREAM_NAME = "image"

# Thư mục để lưu ảnh (nếu bật)
OUTPUT_DIR = "captured_images"
SAVE_IMAGES = True  # Đặt là True để lưu ảnh, False để chỉ xem
# -------------------------------------------------- #

# Biến toàn cục để báo hiệu cho luồng dừng lại
stop_event = threading.Event()

def convert_rgb565_to_bgr(rgb565_image):
    """
    Chuyển đổi một ảnh định dạng RGB565 (dưới dạng mảng NumPy 16-bit)
    sang định dạng BGR (mảng NumPy 8-bit 3 kênh).
    """
    # Tách các kênh màu từ dữ liệu 16-bit
    # RRRRRGGGGGGBBBBB
    # & 0b1111100000000000 -> lấy 5 bit Red
    # & 0b0000011111100000 -> lấy 6 bit Green
    # & 0b0000000000011111 -> lấy 5 bit Blue
    
    # Chuyển sang kiểu dữ liệu lớn hơn để tránh tràn số khi dịch bit
    img_u32 = rgb565_image.astype(np.uint32)

    red_channel   = (img_u32 & 0xF800) >> 8
    green_channel = (img_u32 & 0x07E0) >> 3
    blue_channel  = (img_u32 & 0x001F) << 3

    # Ghép các kênh lại thành một ảnh 3 kênh theo thứ tự B, G, R
    # và chuyển về kiểu dữ liệu 8-bit
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
    print(f"Đang chờ ảnh có kích thước: {IMAGE_WIDTH}x{IMAGE_HEIGHT}x{IMAGE_CHANNELS} = {image_byte_count} bytes")

    # Tạo thư mục lưu ảnh nếu chưa có
    if SAVE_IMAGES:
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        print(f"Sẽ lưu ảnh vào thư mục: '{OUTPUT_DIR}'")

    jlink_stream = None
    image_stream = None

    try:
        # Khởi tạo và kết nối J-Link stream
        opts = JlinkStreamOptions()
        opts.polling_period = 0.01 # Giảm thời gian chờ để phản hồi nhanh hơn
        jlink_stream = JlinkStream(opts)
        jlink_stream.connect()
        print("Đã kết nối với J-Link.")

        # Vòng lặp chính của luồng
        while not stop_event.is_set():
            # Nếu chưa mở được stream, thử mở lại
            if image_stream is None:
                try:
                    # Mở stream có tên khớp với firmware
                    image_stream = jlink_stream.open(JLINK_STREAM_NAME, mode='r')
                    print(f"Đã mở thành công J-Link stream '{JLINK_STREAM_NAME}'. Đang chờ dữ liệu...")
                except Exception as e:
                    # Chờ một chút rồi thử lại
                    print(f"Chưa tìm thấy stream '{JLINK_STREAM_NAME}', đang thử lại... Lỗi: {e}")
                    time.sleep(1)
                    continue

            # Đọc chính xác số byte cần thiết cho một ảnh
            # timeout: thời gian chờ tối đa trước khi bỏ qua
            img_bytes = image_stream.read_all(image_byte_count, timeout=1.0, throw_exception=False)

            # Nếu không đọc được dữ liệu (timeout) thì tiếp tục vòng lặp
            if not img_bytes:
                continue

            # --- Xử lý dữ liệu ảnh ---
            # 1. Chuyển đổi dữ liệu byte thành mảng NumPy
            #img_buffer = np.frombuffer(img_bytes, dtype=np.uint8)
            img_buffer_16bit = np.frombuffer(img_bytes, dtype='<u2').reshape((IMAGE_HEIGHT, IMAGE_WIDTH))

            # 2. Định hình lại mảng thành ảnh 2D hoặc 3D
            #if IMAGE_CHANNELS == 1:
                #img = np.reshape(img_buffer, (IMAGE_HEIGHT, IMAGE_WIDTH))
            #else:
                #img = np.reshape(img_buffer, (IMAGE_HEIGHT, IMAGE_WIDTH, IMAGE_CHANNELS))
                # OpenCV mặc định dùng BGR, firmware của bạn gửi RGB. Cần chuyển đổi.
                #img = cv2.cvtColor(img, cv2.COLOR_RGB2BGR)
            img = convert_rgb565_to_bgr(img_buffer_16bit)



            # 3. Hiển thị ảnh lên màn hình
            cv2.imshow('Live Camera Feed from EFR32xG26', img)

            # 4. Lưu ảnh vào file nếu được bật
            if SAVE_IMAGES:
                timestamp = time.strftime("%Y%m%d_%H%M%S")
                millis = int((time.time() - int(time.time())) * 1000)
                filename = f"{timestamp}_{millis:03d}.png"
                filepath = os.path.join(OUTPUT_DIR, filename)
                cv2.imwrite(filepath, img)

            # Đợi 1ms và kiểm tra phím bấm. Cần thiết để cửa sổ imshow hoạt động
            if cv2.waitKey(1) & 0xFF == ord('q'):
                print("Đã nhấn 'q', đang dừng...")
                stop_event.set() # Báo hiệu dừng

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
    print("--- Trình xem Camera qua J-Link ---")
    print("Bắt đầu khởi tạo kết nối...")
    print("Nhấn CTRL+C trong cửa sổ terminal này để thoát chương trình.")

    # Tạo và khởi động luồng xử lý
    image_thread = threading.Thread(target=jlink_image_processor, daemon=True)
    image_thread.start()

    try:
        # Giữ luồng chính chạy để chờ tín hiệu CTRL+C
        while image_thread.is_alive():
            image_thread.join(timeout=0.5)
    except KeyboardInterrupt:
        print("\nĐã nhận tín hiệu CTRL+C. Đang yêu cầu luồng dừng lại...")
        # Gửi tín hiệu dừng cho luồng con
        stop_event.set()
        # Đợi luồng con kết thúc hoàn toàn
        image_thread.join()

    print("Chương trình đã kết thúc.")
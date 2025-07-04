import os
import time
import threading
import numpy as np
import cv2

try:
    from mltk.utils.jlink_stream import JlinkStream, JlinkStreamOptions
except ImportError:
    print("Lỗi: Không tìm thấy thư viện MLTK. Cài đặt bằng: pip install silabs-mltk")
    exit(-1)

# ------------------- CẤU HÌNH (Chế độ Debug Thuật toán) ------------------- #
IMAGE_WIDTH = 96
IMAGE_HEIGHT = 96
# Firmware luôn gửi RGB565 (2 byte/pixel)
IMAGE_CHANNELS = 2

JLINK_STREAM_NAME = "image"
MAGIC_BYTES = b'\xDE\xAD\xBE\xEF'
# ----------------------------------------------------------------------- #

stop_event = threading.Event()

def simulate_c_algorithm_conversion(rgb565_bytes):
    """
    Mô phỏng CHÍNH XÁC thuật toán chuyển đổi RGB565 -> Grayscale từ code C.
    Hàm này nhận vào một chuỗi byte thô của ảnh RGB565.
    """
    print("--- Đang mô phỏng thuật toán C ---")
    
    # Tạo một mảng trống để chứa kết quả grayscale (1 byte/pixel)
    grayscale_data = bytearray(IMAGE_WIDTH * IMAGE_HEIGHT)
    
    # Lặp qua từng pixel (mỗi pixel là 2 byte)
    # Tương đương với: for(uint32_t pixel_count = ...; ...; --pixel_count)
    for i in range(0, len(rgb565_bytes), 2):
        # Đọc 2 byte của pixel hiện tại
        # Tương đương với: const uint8_t hb = src[0]; const uint8_t lb = src[1];
        hb = rgb565_bytes[i]   # High Byte
        lb = rgb565_bytes[i+1] # Low Byte

        # --- Áp dụng chính xác logic từ code C ---
        # const uint16_t r = (lb & 0x1F) << 3;
        r = (lb & 0x1F) << 3
        # const uint16_t g = (hb & 0x07) << 5 | (lb & 0xE0) >> 3;
        g = ((hb & 0x07) << 5) | ((lb & 0xE0) >> 3)
        # const uint16_t b = hb & 0xF8;
        b = hb & 0xF8

        # *dst++ = (uint8_t)((r + g + b) / 3);
        # Lưu ý: Phép chia số nguyên trong Python là //
        grayscale_value = (r + g + b) // 3
        
        # Ghi giá trị grayscale vào mảng kết quả
        # i // 2 là chỉ số của pixel
        grayscale_data[i // 2] = grayscale_value

    # Chuyển đổi bytearray kết quả thành ảnh NumPy để hiển thị
    grayscale_image = np.frombuffer(grayscale_data, dtype=np.uint8).reshape((IMAGE_HEIGHT, IMAGE_WIDTH))
    
    return grayscale_image

def jlink_image_processor():
    image_byte_count = IMAGE_WIDTH * IMAGE_HEIGHT * IMAGE_CHANNELS
    print("--- Chế độ Debug Thuật toán C ---")
    print(f"Đang nhận dữ liệu RGB565 thô ({image_byte_count} bytes/khung hình)")

    try:
        opts = JlinkStreamOptions(); opts.polling_period = 0.01
        jlink_stream = JlinkStream(opts); jlink_stream.connect()
        image_stream = jlink_stream.open(JLINK_STREAM_NAME, mode='r')
        print("Đã kết nối và mở stream thành công.")

        while not stop_event.is_set():
            # Tìm Magic Bytes để đồng bộ
            sync_buffer = b''; found_magic_bytes = False
            while not stop_event.is_set():
                byte = image_stream.read(1, timeout=1.0)
                if not byte: break
                sync_buffer += byte
                if MAGIC_BYTES in sync_buffer: found_magic_bytes = True; break
                if len(sync_buffer) > 2*len(MAGIC_BYTES): sync_buffer = sync_buffer[-len(MAGIC_BYTES):]
            if not found_magic_bytes: continue

            # Đọc dữ liệu ảnh RGB565 thô
            img_bytes_raw = image_stream.read_all(image_byte_count, timeout=1.0)
            if not img_bytes_raw or len(img_bytes_raw) != image_byte_count: continue

            # --- THỰC HIỆN MÔ PHỎNG ---
            # Chuyển đổi dữ liệu thô sang grayscale bằng thuật toán C
            img_grayscale_simulated = simulate_c_algorithm_conversion(img_bytes_raw)

            # Hiển thị kết quả của thuật toán C
            cv2.imshow('Grayscale from C Algorithm Simulation', img_grayscale_simulated)
            
            if cv2.waitKey(1) & 0xFF == ord('q'):
                stop_event.set()

    except Exception as e:
        import traceback
        print(f"\nLỗi: {e}"); traceback.print_exc()
    finally:
        if 'jlink_stream' in locals() and jlink_stream.is_connected: jlink_stream.disconnect()
        cv2.destroyAllWindows()

if __name__ == "__main__":
    print("--- Bắt đầu Debugging ---")
    
    # 1. Chỉnh firmware của bạn để chỉ gửi RGB565 thô.
    #    (Đặt TEST_DATA_FORMAT = ARDUCAM_DATA_FORMAT_RGB565 trong .cc)
    # 2. Build và nạp lại firmware.
    # 3. Chạy script này.
    
    image_thread = threading.Thread(target=jlink_image_processor, daemon=True)
    image_thread.start()
    try:
        image_thread.join()
    except KeyboardInterrupt:
        stop_event.set()
        image_thread.join()
    print("Debug session kết thúc.")
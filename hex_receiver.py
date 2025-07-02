#!/usr/bin/env python3
"""
Simple Hex Data Receiver
Just saves raw hex data from EFR32 to text files
"""
import serial
import struct
import os
from datetime import datetime
import argparse

class SimpleHexReceiver:
    def __init__(self, port, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.serial_conn = None
        self.image_count = 0
        
        # Create output directory
        self.output_dir = "hex_images"
        os.makedirs(self.output_dir, exist_ok=True)
        
    def connect(self):
        """Connect to serial port"""
        try:
            self.serial_conn = serial.Serial(self.port, self.baudrate, timeout=1)
            print(f"✓ Connected to {self.port} at {self.baudrate} baud")
            return True
        except Exception as e:
            print(f"✗ Failed to connect: {e}")
            return False
    
    def save_raw_hex(self, line, img_number):
        """Save raw hex line to file"""
        try:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]  # Include milliseconds
            filename = f"{self.output_dir}/raw_{img_number:04d}_{timestamp}.txt"
            
            with open(filename, 'w') as f:
                f.write(f"# Raw ArduCam Data\n")
                f.write(f"# Image #{img_number}\n")
                f.write(f"# Timestamp: {timestamp}\n")
                f.write(f"# Grayscale 64x64 (4096 bytes expected)\n")
                f.write("# =====================================\n\n")
                f.write(line.strip())
                f.write("\n")
            
            print(f"✓ Saved raw hex #{img_number}: {filename}")
            return True
            
        except Exception as e:
            print(f"✗ Error saving hex: {e}")
            return False
    
    def extract_hex_data(self, line):
        """Extract just the hex data part"""
        try:
            # Find the DATA section
            data_idx = line.find("DATA:")
            end_idx = line.find("END:")
            
            if data_idx != -1 and end_idx != -1:
                hex_data = line[data_idx+5:end_idx]
                return hex_data.strip()
            return None
            
        except Exception as e:
            return None
    
    def save_clean_hex(self, hex_data, img_number):
        """Save clean hex data to file"""
        try:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"{self.output_dir}/clean_{img_number:04d}_{timestamp}.txt"
            
            # Format hex nicely (32 chars per line = 16 bytes per line)
            formatted_hex = ""
            for i in range(0, len(hex_data), 32):
                line_hex = hex_data[i:i+32]
                # Add spaces between bytes for readability
                spaced_hex = ' '.join([line_hex[j:j+2] for j in range(0, len(line_hex), 2)])
                formatted_hex += spaced_hex + "\n"
            
            with open(filename, 'w') as f:
                f.write(f"# Clean ArduCam Hex Data\n")
                f.write(f"# Image #{img_number}\n")
                f.write(f"# Size: {len(hex_data)//2} bytes\n")
                f.write(f"# Expected: 4096 bytes (64x64 grayscale)\n")
                f.write(f"# Format: Grayscale, 1 byte per pixel\n")
                f.write(f"# Timestamp: {timestamp}\n")
                f.write("# ================================\n")
                f.write("# Each line = 16 bytes (32 hex chars)\n")
                f.write("# Each byte = 1 pixel value (0-255)\n")
                f.write("# ================================\n\n")
                f.write(formatted_hex)
            
            print(f"✓ Saved clean hex #{img_number}: {filename} ({len(hex_data)//2} bytes)")
            return True
            
        except Exception as e:
            print(f"✗ Error saving clean hex: {e}")
            return False
    
    def run(self):
        """Main receive loop"""
        if not self.connect():
            return
        
        print("🔄 Waiting for hex data... (Press Ctrl+C to stop)")
        print(f"📁 Files will be saved to: {self.output_dir}/")
        
        try:
            image_counter = 0
            
            while True:
                if self.serial_conn.in_waiting > 0:
                    line = self.serial_conn.readline().decode('utf-8', errors='ignore')
                    
                    # Check for image data
                    if line.startswith("IMG_START:"):
                        image_counter += 1
                        print(f"📷 Receiving image #{image_counter}...")
                        
                        # Save raw line
                        self.save_raw_hex(line, image_counter)
                        
                        # Extract and save clean hex
                        hex_data = self.extract_hex_data(line)
                        if hex_data:
                            self.save_clean_hex(hex_data, image_counter)
                            
                        self.image_count += 1
                        print(f"📊 Total images saved: {self.image_count}")
                    
                    # Print other messages
                    elif line.strip() and not line.startswith("Image #"):
                        print(f"📢 {line.strip()}")
                        
        except KeyboardInterrupt:
            print(f"\n🛑 Stopped. Total images saved: {self.image_count}")
        except Exception as e:
            print(f"✗ Error: {e}")
        finally:
            if self.serial_conn:
                self.serial_conn.close()
                print("🔌 Serial connection closed")

def main():
    parser = argparse.ArgumentParser(description='Simple Hex Data Receiver')
    parser.add_argument('port', help='Serial port (e.g., COM3)')
    parser.add_argument('-b', '--baudrate', type=int, default=115200, 
                       help='Baud rate (default: 115200)')
    
    args = parser.parse_args()
    
    receiver = SimpleHexReceiver(args.port, args.baudrate)
    receiver.run()

if __name__ == "__main__":
    main()

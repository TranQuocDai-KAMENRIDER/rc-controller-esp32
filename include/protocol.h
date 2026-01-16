#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>

// --- CẤU HÌNH GIAO THỨC ---
// Định nghĩa cấu trúc gói tin gửi đi
typedef struct {
    uint8_t startByte;    // Luôn là 0xAA (Dấu hiệu nhận biết)
    uint32_t packetId;    // Số đếm gói tin (để biết sóng tốt hay rớt gói)
    int16_t throttle;     // Ga: -255 (Lùi) đến 255 (Tiến)
    int16_t steering;     // Lái: 0 (Trái) đến 180 (Phải)
    uint8_t mode;         // 0: Manual, 1: Auto
    uint8_t checksum;     // Tổng kiểm tra lỗi
} ControlPacket;

// Hàm tính Checksum (Chống nhiễu sóng)
// Logic: Cộng dồn tất cả các byte dữ liệu lại
inline uint8_t calculateChecksum(ControlPacket *pkg) {
    return pkg->startByte + (pkg->packetId & 0xFF) + 
           (pkg->throttle & 0xFF) + (pkg->steering & 0xFF) + pkg->mode;
}

#endif
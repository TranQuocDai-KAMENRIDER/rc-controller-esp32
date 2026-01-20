#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "protocol.h"
// 1. CẤU HÌNH PHẦN CỨNG (S3 N16R8 AN TOÀN)
// Tuyệt đối tránh chân 35, 36, 37 (PSRAM) và 43, 44 (UART)
#define PIN_STEERING  1   // Biến trở Lái (ADC1_CH0)
#define PIN_THROTTLE  2   // Biến trở Ga (ADC1_CH1)
// 2. CẤU HÌNH LOGIC
// Bộ lọc EMA (Exponential Moving Average) giúp joystick siêu mượt
// Giá trị càng nhỏ càng mượt (nhưng trễ hơn), càng lớn càng nhạy. 0.25 là đẹp.
const float EMA_ALPHA = 0.25f; 
float emaThrottle = 0; // Biến lưu giá trị lọc Ga
float emaSteering = 0; // Biến lưu giá trị lọc Lái
// 3. CẤU HÌNH KẾT NỐI (MAC SPOOFING)
// Địa chỉ MAC tự quy định (Thay đổi tùy thích, miễn là khớp với Xe)
uint8_t macTayCam[] = {0x27, 0xAE, 0xAE, 0xAE, 0xAE, 0x02}; 
uint8_t macCuaXe[]  = {0x02, 0xAE, 0xAE, 0xAE, 0xAE, 0x27}; 

ControlPacket myPacket;
esp_now_peer_info_t peerInfo;

// 3. CÁC HÀM XỬ LÝ SỐ LIỆU
// Hàm map có lọc nhiễu cho GA
int processThrottle(int input) {
    int center = 2048;
    int deadzone = 200; 

    // 1. Lọc nhiễu (EMA Filter)
    emaThrottle = (1.0f - EMA_ALPHA) * emaThrottle + EMA_ALPHA * (float)input;
    int smoothInput = (int)emaThrottle;

    // 2. Xử lý vùng chết
    if (abs(smoothInput - center) < deadzone) return 0;

    // 3. Map sang dải tốc độ (-255 đến 255)
    return map(smoothInput, 0, 4095, -255, 255); 
}

// Hàm map có lọc nhiễu cho LÁI
int processSteering(int input) {
    int center = 2048; 
    int deadzone = 100; // Vùng chết nhỏ hơn cho lái chính xác

    // 1. Lọc nhiễu
    emaSteering = (1.0f - EMA_ALPHA) * emaSteering + EMA_ALPHA * (float)input;
    int smoothInput = (int)emaSteering;

    // 2. Xử lý vùng chết (về giữa)
    if (abs(smoothInput - center) < deadzone) return 90;

    // 3. Map sang góc lái (0 đến 180)
    return map(smoothInput, 0, 4095, 0, 180); 
}

// ================================================================
// 5. SETUP & LOOP
// ================================================================
void setup() {
    Serial.begin(115200);
    
    pinMode(PIN_STEERING, INPUT);
    pinMode(PIN_THROTTLE, INPUT);
    // KHÔNG CÒN pinMode(PIN_MODE_BTN) nữa

    WiFi.mode(WIFI_STA);
    esp_wifi_set_mac(WIFI_IF_STA, &macTayCam[0]);
    
    if (esp_now_init() != ESP_OK) return;

    memcpy(peerInfo.peer_addr, macCuaXe, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    Serial.println("TAY CAM PRO READY!");
}

void loop() {
    // 1. Đọc ADC
    int rawSteer = analogRead(PIN_STEERING);
    int rawThrot = analogRead(PIN_THROTTLE);

    // 2. Xử lý & Đóng gói
    myPacket.startByte = 0xAA;
    myPacket.packetId++;
    
    // Logic mới (có lọc nhiễu)
    myPacket.steering = processSteering(rawSteer);
    myPacket.throttle = processThrottle(rawThrot);
    
    // Vì bỏ nút bấm, ta luôn để mode = 0 (Manual) để khớp với Protocol cũ
    myPacket.mode = 0; 
    
    myPacket.checksum = calculateChecksum(&myPacket);

    // 3. Gửi đi
    esp_err_t result = esp_now_send(macCuaXe, (uint8_t *) &myPacket, sizeof(myPacket));

    // 4. Debug
    Serial.printf("Ga: %d | Lai: %d | Mode: 0 (Fixed) | %s\n", 
        myPacket.throttle,   
        myPacket.steering,
        result == ESP_OK ? "OK" : ".."
    );

    delay(20); 
}

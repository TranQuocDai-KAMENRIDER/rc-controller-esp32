#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "protocol.h"
// 1. CẤU HÌNH PHẦN CỨNG (S3 N16R8 AN TOÀN)
// Tuyệt đối tránh chân 35, 36, 37 (PSRAM) và 43, 44 (UART)
#define PIN_STEERING  1   // Biến trở Lái (ADC1_CH0)
#define PIN_THROTTLE  2   // Biến trở Ga (ADC1_CH1)

// 2. CẤU HÌNH KẾT NỐI (MAC SPOOFING)
// Địa chỉ MAC tự quy định (Thay đổi tùy thích, miễn là khớp với Xe)
uint8_t macTayCam[] = {0xA0, 0x22, 0x22, 0x22, 0x22, 0x22}; 
uint8_t macCuaXe[]  = {0xA0, 0x11, 0x11, 0x11, 0x11, 0x11}; 

ControlPacket myPacket;
esp_now_peer_info_t peerInfo;

// Biến trạng thái
bool isAutoMode = false;       // Mặc định là lái tay
unsigned long lastDebounce = 0; // Chống rung nút bấm

// 3. CÁC HÀM XỬ LÝ LOGIC
// Hàm xử lý Joystick: Biến đổi ADC (0-4095) sang giá trị điều khiển
int processJoystick(int input, int minOut, int maxOut, int deadzone) {
    int center = 2048; // Điểm giữa lý thuyết của ESP32 (12-bit)
    
    // Nếu giá trị nằm trong vùng chết -> Trả về điểm giữa
    if (abs(input - center) < deadzone) {
        return (minOut + maxOut) / 2;
    }
    return map(input, 0, 4095, minOut, maxOut);
}

// Hàm xử lý Cò ga riêng (vì điểm giữa phải là 0)
int processThrottle(int input) {
    int center = 2048;
    if (abs(input - center) < 200) return 0; // Deadzone 200
    return map(input, 0, 4095, -255, 255);
}

// 4. SETUP & LOOP
void setup() {
    Serial.begin(115200);
    
    // Cấu hình chân IO
    pinMode(PIN_STEERING, INPUT);
    pinMode(PIN_THROTTLE, INPUT);
    pinMode(PIN_MODE_BTN, INPUT_PULLUP); // Quan trọng: Kéo lên nguồn

    // --- CÀI ĐẶT ESP-NOW & MAC ---
    WiFi.mode(WIFI_STA);
    esp_wifi_set_mac(WIFI_IF_STA, &macTayCam[0]); // Ép dùng MAC A0:22...
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("Lỗi khởi tạo ESP-NOW!");
        return;
    }

    // Đăng ký địa chỉ của XE
    memcpy(peerInfo.peer_addr, macCuaXe, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Lỗi kết nối với Xe!");
        return;
    }

    Serial.println("TAY CAM S3-N16R8 DA SAN SANG!");
    Serial.print("Dia chi MAC cua toi: ");
    Serial.println(WiFi.macAddress());
}

void loop() {
    // A. ĐỌC NÚT BẤM (Chuyển chế độ)
    if (digitalRead(PIN_MODE_BTN) == LOW) {
        if (millis() - lastDebounce > 300) { // Chỉ nhận 1 lần mỗi 300ms
            isAutoMode = !isAutoMode;
            lastDebounce = millis();
            Serial.println(isAutoMode ? ">>> CHẾ ĐỘ: AUTO" : ">>> CHẾ ĐỘ: MANUAL");
        }
    }

    // B. ĐỌC & XỬ LÝ JOYSTICK
    // Đọc ADC (Nếu chưa cắm phần cứng, chân này sẽ nhảy loạn xạ - Bình thường)
    int rawSteer = analogRead(PIN_STEERING);
    int rawThrot = analogRead(PIN_THROTTLE);

    // Chuẩn bị gói tin
    myPacket.startByte = 0xAA;
    myPacket.packetId++; // Tăng số thứ tự gói tin
    
    // Xử lý logic
    myPacket.steering = processJoystick(rawSteer, 0, 180, 150); // Góc servo 0-180
    myPacket.throttle = processThrottle(rawThrot);              // Tốc độ -255 đến 255
    myPacket.mode = isAutoMode ? 1 : 0;
    
    // Tính checksum cuối cùng
    myPacket.checksum = calculateChecksum(&myPacket);

    // C. GỬI DỮ LIỆU
    esp_err_t result = esp_now_send(macCuaXe, (uint8_t *) &myPacket, sizeof(myPacket));

    // D. DEBUG (In ra để bạn kiểm tra logic trên màn hình)
    // Dùng printf để in gọn 1 dòng
    Serial.printf("ID: %d | Mode: %s | Lai: %d | Ga: %d | Checksum: %d | Gui: %s\n", 
        myPacket.packetId,
        isAutoMode ? "AUTO" : "MANUAL",
        myPacket.steering,
        myPacket.throttle,
        myPacket.checksum,
        result == ESP_OK ? "OK" : "LOI"
    );

    delay(20); // Gửi 50 lần/giây
}

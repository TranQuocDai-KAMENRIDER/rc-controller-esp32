#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("ESP32-S3 project ready");
}

void loop() {
  Serial.println("Loop running...");
  delay(1000);
}

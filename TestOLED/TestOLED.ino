#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ===== LoRa pins =====
#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  26

// ===== OLED (I2C) =====
#define OLED_SDA   21
#define OLED_SCL   22
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  32
#define OLED_ADDR      0x3C
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ดึงเฉพาะข้อความ Distance: ... cm
String extractDistanceText(const String& raw) {
  int start = raw.indexOf("Distance:");
  if (start == -1) return "--.- cm";
  int end = raw.indexOf("|", start); // ตัดตรงก่อนเครื่องหมาย | ถ้ามี
  if (end == -1) end = raw.length();
  String distPart = raw.substring(start, end);
  distPart.trim();
  return distPart;
}

void showDistanceAndRSSI(const String& distText, long rssi) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // บรรทัดบน: Distance (ตัวเล็ก)
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(distText);

  // เว้นบรรทัดแล้วแสดง RSSI (ตัวเล็ก)
  display.setCursor(0, 16);
  display.print("RSSI: ");
  display.print(rssi);

  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // OLED init
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed");
  }
  display.clearDisplay();
  display.display();

  // LoRa init
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433200000)) { // ต้องตรงกับฝั่งส่ง
    Serial.println("LoRa init failed");
    while (1);
  }
  LoRa.setSpreadingFactor(12);

  Serial.println("Receiver ready");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    String incoming;
    while (LoRa.available()) incoming += (char)LoRa.read();
    long rssi = LoRa.packetRssi();

    Serial.println("Msg: " + incoming);
    Serial.println("RSSI: " + String(rssi));

    String distText = extractDistanceText(incoming);
    showDistanceAndRSSI(distText, rssi);
  }
}

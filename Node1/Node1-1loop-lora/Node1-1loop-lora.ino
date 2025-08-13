#include <LoRa.h>
#include "esp_sleep.h"
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <Adafruit_INA219.h>

// ===== LoRa (SPI2 - VSPI) =====
#define LORA_SCK   25
#define LORA_MISO  27
#define LORA_MOSI  32
#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  26
SPIClass spiLoRa(VSPI);

// ===== SD Card (SPI1 - HSPI) =====
#define SD_CS      4
#define SD_SCK     18
#define SD_MISO    19
#define SD_MOSI    23
SPIClass spiSD(HSPI);

// ===== RTC (I2C1) =====
#define RTC_SDA    16
#define RTC_SCL    17
TwoWire I2C_RTC = TwoWire(1);
RTC_DS3231 rtc;

// ===== INA219 (I2C0 - Default) =====
#define INA_SDA    21
#define INA_SCL    22
Adafruit_INA219 ina219;

// ===== Ultrasonic =====
#define TRIG_PIN   12
#define ECHO_PIN   13

#define SLEEP_TIME_US   (50ULL * 1000000ULL)

// ======= LoRa helpers =======
const long LORA_FREQ = 433200000; // ต้องตรงกับฝั่งรับ

// init LoRa หนึ่งครั้ง (ไม่วน)
// คืนค่า true ถ้าสำเร็จ
bool initLoRa() {
  // ตั้งค่า SPI ให้ชิป LoRa
  spiLoRa.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setSPI(spiLoRa);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // ลองเริ่ม LoRa
  if (!LoRa.begin(LORA_FREQ)) {
    return false;
  }
  LoRa.setSpreadingFactor(12);
  // LoRa.enableCrc(); // เปิดถ้าฝั่งรับเปิด CRC
  return true;
}

// กดรีเซ็ตวิทยุสั้น ๆ (กันชิปค้าง)
void pulseRadioReset() {
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, LOW);
  delay(10);
  digitalWrite(LORA_RST, HIGH);
  delay(10);
}

// วนเชื่อม LoRa จนกว่าจะสำเร็จ (พิมพ์สถานะทุก 500ms)
void waitLoRaReady() {
  int attempt = 0;
  while (true) {
    attempt++;
    Serial.printf("LoRa init attempt %d...\n", attempt);
    pulseRadioReset();
    if (initLoRa()) {
      Serial.println("LoRa ready");
      return;
    }
    Serial.println("LoRa init failed, retrying...");
    delay(500);
  }
}

// ส่งแพ็กเก็ตด้วยการรีทราย: ถ้าส่งไม่ผ่าน จะรี-init LoRa แล้วลองใหม่จนกว่าจะสำเร็จ
void sendWithRetry(const char* payload) {
  int attempt = 0;
  while (true) {
    attempt++;
    Serial.printf("LoRa send attempt %d...\n", attempt);

    // เผื่อกรณีชิปถูกปล่อยไปนาน ลอง beginPacket ใหม่
    int ok = 0;
    if (LoRa.beginPacket() == 1) {
      LoRa.print(payload);
      ok = LoRa.endPacket(); // 1 = success
    }

    if (ok == 1) {
      Serial.println("LoRa send OK");
      return;
    } else {
      Serial.println("LoRa send failed -> re-init radio");
      // re-init ทั้งก้อน แล้ววนลองใหม่
      // (กันสถานะชิปค้างหรือสัญญาณหลุด)
      pulseRadioReset();
      // ปิดก่อนก็ได้ แต่ LoRa lib ไม่มี end() อย่างเป็นทางการ
      // จึงเรียก init ใหม่ทับไปเลย
      if (!initLoRa()) {
        // ถ้ายังไม่ติดก็วนใหม่อยู่ดี
        Serial.println("Re-init failed, retrying...");
      }
      delay(300);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Woke from deep sleep");
  } else {
    Serial.println("Booting normally");
  }

  // ===== Ultrasonic Setup =====
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // ===== SD Setup =====
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("SD init failed");
  } else {
    Serial.println("SD ready");
  }

  // ===== RTC Setup =====
  I2C_RTC.begin(RTC_SDA, RTC_SCL);
  if (!rtc.begin(&I2C_RTC)) {
    Serial.println("Couldn't find RTC");
  }

  // ===== INA219 Setup =====
  Wire.begin(INA_SDA, INA_SCL);
  if (!ina219.begin()) {
    Serial.println("Couldn't find INA219");
  }

  // ===== LoRa Setup (วนจนติด) =====
  waitLoRaReady();

  // ===== วัด Ultrasonic =====
  long duration;
  float distanceCm;

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH, 30000);
  distanceCm = (duration == 0) ? -1 : (duration / 2.0) * 0.0343;

  // ===== อ่านเวลา/ไฟ =====
  DateTime now = rtc.now();
  float busVoltage = ina219.getBusVoltage_V();
  float current_mA = ina219.getCurrent_mA();

  // ===== สร้างข้อความส่ง =====
  char buf[128];
  if (distanceCm < 0) {
    snprintf(buf, sizeof(buf),
      "[%04d-%02d-%02d %02d:%02d:%02d] Distance: Out of range | V=%.2fV I=%.2fmA",
      now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(),
      busVoltage, current_mA);
  } else {
    snprintf(buf, sizeof(buf),
      "[%04d-%02d-%02d %02d:%02d:%02d] Distance: %.1f cm | V=%.2fV I=%.2fmA",
      now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(),
      distanceCm, busVoltage, current_mA);
  }

  // ===== ส่งผ่าน LoRa (วนจนกว่าจะส่งได้) =====
  Serial.print("Sending: ");
  Serial.println(buf);
  sendWithRetry(buf);

  // ===== บันทึกลง SD =====
  File file = SD.open("/log.txt", FILE_APPEND);
  if (file) {
    file.println(buf);
    file.close();
    Serial.println("Saved to SD");
  } else {
    Serial.println("Failed to write SD");
  }

  // ===== Sleep =====
  Serial.println("Deep sleep for 50 sec...");
  esp_sleep_enable_timer_wakeup(SLEEP_TIME_US);
  esp_deep_sleep_start();
}

void loop() {
  // ไม่ใช้
}

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
 // rtc.adjust(DateTime(2025, 8, 5, 0, 50, 0));
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

  long duration;
  float distanceCm;

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH, 30000);
  distanceCm = (duration == 0) ? -1 : (duration / 2.0) * 0.0343;

  // ===== RTC Setup =====
  I2C_RTC.begin(RTC_SDA, RTC_SCL);
  if (!rtc.begin(&I2C_RTC)) {
    Serial.println("Couldn't find RTC");
  }

  DateTime now = rtc.now();

  // ===== INA219 Setup =====
  Wire.begin(INA_SDA, INA_SCL);
  if (!ina219.begin()) {
    Serial.println("Couldn't find INA219");
  }
  float busVoltage = ina219.getBusVoltage_V();
  float current_mA = ina219.getCurrent_mA();

  // ===== SD Setup =====
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("SD init failed");
  } else {
    Serial.println("SD ready");
  }

  // ===== LoRa Setup =====
  spiLoRa.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setSPI(spiLoRa);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433200000)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  LoRa.setSpreadingFactor(12);
  Serial.println("LoRa ready");

  // ===== สร้างข้อความส่ง =====
  char buf[128];
  if (distanceCm < 0) {
    snprintf(buf, sizeof(buf), "[%04d-%02d-%02d %02d:%02d:%02d] Distance: Out of range | V=%.2fV I=%.2fmA",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(),
             busVoltage, current_mA);
  } else {
    snprintf(buf, sizeof(buf), "[%04d-%02d-%02d %02d:%02d:%02d] Distance: %.1f cm | V=%.2fV I=%.2fmA",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(),
             distanceCm, busVoltage, current_mA);
  }

  // ===== ส่งผ่าน LoRa =====
  Serial.print("Sending: ");
  Serial.println(buf);
  LoRa.beginPacket();
  LoRa.print(buf);
  LoRa.endPacket();

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
  // ว่าง
}
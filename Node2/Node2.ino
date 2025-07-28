#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_INA219.h>

// --- LoRa1 (รับข้อมูลจาก Node1) บน VSPI ---
#define LORA1_SS    5
#define LORA1_RST   14
#define LORA1_DIO0  26

// --- LoRa2 (ส่งต่อไป Node3) บน HSPI ---
#define LORA2_SS    15
#define LORA2_RST   33
#define LORA2_DIO0  32
#define LORA2_SCK   25
#define LORA2_MISO  27
#define LORA2_MOSI  4

// --- RTC บน I2C1 ---
#define RTC_SDA     16
#define RTC_SCL     17
TwoWire I2C_RTC = TwoWire(1);
RTC_DS3231 rtc;

// --- INA219 บน I2C0 ---
#define INA_SDA     21
#define INA_SCL     22
Adafruit_INA219 ina219;

// --- LoRa2 SPI ---
SPIClass LoRa2SPI(HSPI);

LoRaClass LoRa1;
LoRaClass LoRa2;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // RTC
  I2C_RTC.begin(RTC_SDA, RTC_SCL);
  if (!rtc.begin(&I2C_RTC)) Serial.println("RTC not found!");
  else Serial.println("RTC ready");

  // INA219
  Wire.begin(INA_SDA, INA_SCL);
  if (!ina219.begin()) Serial.println("INA219 not found!");
  else Serial.println("INA219 ready");

  // LoRa1 (VSPI)
  LoRa1.setSPI(SPI);
  LoRa1.setPins(LORA1_SS, LORA1_RST, LORA1_DIO0);
  if (!LoRa1.begin(433200000)) {
    Serial.println("LoRa1 failed!");
    while (1);
  }
  LoRa1.setSpreadingFactor(12);
  Serial.println("LoRa1 (RX) ready");

  // LoRa2 (HSPI)
  LoRa2SPI.begin(LORA2_SCK, LORA2_MISO, LORA2_MOSI, LORA2_SS);
  LoRa2.setSPI(LoRa2SPI);
  LoRa2.setPins(LORA2_SS, LORA2_RST, LORA2_DIO0);
  if (!LoRa2.begin(433100000)) {
    Serial.println("LoRa2 failed!");
    while (1);
  }
  LoRa2.setSpreadingFactor(12);
  Serial.println("LoRa2 (TX) ready");
}

void loop() {
  int packetSize = LoRa1.parsePacket();
  if (packetSize > 0) {
    String incoming = "";
    while (LoRa1.available()) {
      incoming += (char)LoRa1.read();
    }
    long rssiNode1 = LoRa1.packetRssi();

    DateTime now = rtc.now();
    float v2 = ina219.getBusVoltage_V();
    float i2 = ina219.getCurrent_mA();

    String nowStr = now.timestamp();
    String msg = incoming + " [" + nowStr + "] | V=" + String(v2, 2) + "V I=" + String(i2, 2) + "mA" + " | RSSI_Node1=" + String(rssiNode1);

    Serial.println("Received from Node1: " + incoming);
    Serial.println("RSSI Node1: " + String(rssiNode1));
    Serial.println("Forwarding to Node3: " + msg);

    LoRa2.beginPacket();
    LoRa2.print(msg);
    LoRa2.endPacket();
  }

  delay(200);
}

#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_INA219.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>

// WiFi config
const char* ssid = "ESP";
const char* password = "00000000";
const char* serverUrl = "https://backend-water-rf88.onrender.com/distance";

// LoRa pins (VSPI)
#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  26

// SD card (HSPI)
#define SD_CS      27
#define SD_SCK     25
#define SD_MISO    33
#define SD_MOSI    32
SPIClass spiSD(HSPI);

#define RTC_SDA    16
#define RTC_SCL    17
TwoWire I2C_RTC = TwoWire(1);
RTC_DS3231 rtc;

#define INA_SDA    21
#define INA_SCL    22
Adafruit_INA219 ina219;

void setup() {
  Serial.begin(115200);
  delay(1000);

  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("SD Card init failed");
  } else {
    Serial.println("SD Card ready");
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433100000)) {
    Serial.println("LoRa init failed");
    while (1);
  }
  LoRa.setSpreadingFactor(12);
  Serial.println("LoRa ready");

  I2C_RTC.begin(RTC_SDA, RTC_SCL);
  rtc.begin(&I2C_RTC);
  Wire.begin(INA_SDA, INA_SCL);
  ina219.begin();
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    long rssiNode2 = LoRa.packetRssi();

    Serial.println("Received LoRa: " + incoming);
    Serial.println("RSSI Node2: " + String(rssiNode2));
    Serial.println("----------------------------");

    String timeNode1 = extractBetween(incoming, "[", "]");
    String timeNode2 = extractBetweenSecondTime(incoming);

    float distance = parseDistance(incoming);

    float v1 = 0.0, i1 = 0.0, v2 = 0.0, i2 = 0.0;
    parseVoltagesAndCurrents(incoming, v1, i1, v2, i2);

    int rssiNode1 = parseRSSI(incoming);

    if (timeNode1 == "") timeNode1 = "unknown";
    if (timeNode2 == "") timeNode2 = "unknown";
    if (rssiNode1 == -999) rssiNode1 = 0;

    Serial.println("LoRa Node 1 :");
    Serial.println(timeNode1);
    Serial.println("Distance: " + String(distance, 2) + " cm");
    Serial.println("RSSI: " + String(rssiNode1));
    Serial.println("V=" + String(v1, 2) + "V I=" + String(i1, 2) + "mA");

    Serial.println("LoRa Node 2 :");
    Serial.println(timeNode2);
    Serial.println("RSSI: " + String(rssiNode2));
    Serial.println("V=" + String(v2, 2) + "V I=" + String(i2, 2) + "mA");

    String jsonData = "{\"distance\":" + String(distance, 2) +
                      ",\"rssi_node1\":" + String(rssiNode1) +
                      ",\"rssi_node2\":" + String(rssiNode2) +
                      ",\"v_node1\":" + String(v1, 2) +
                      ",\"i_node1\":" + String(i1, 2) +
                      ",\"v_node2\":" + String(v2, 2) +
                      ",\"i_node2\":" + String(i2, 2) +
                      ",\"time_node1\":\"" + timeNode1 + "\"" +
                      ",\"time_node2\":\"" + timeNode2 + "\"" +
                      "}";

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(jsonData);
      if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Sent to server: " + jsonData);
        Serial.println("Server response: " + response);
      } else {
        Serial.print("HTTP POST failed: ");
        Serial.println(httpResponseCode);
        saveToSD(jsonData);
      }
      http.end();
    } else {
      Serial.println("WiFi not connected, saving to SD");
      saveToSD(jsonData);
    }

    delay(1000);
  }
}

String extractBetween(const String &str, const String &startDelim, const String &endDelim) {
  int start = str.indexOf(startDelim);
  if (start == -1) return "";
  start += startDelim.length();
  int end = str.indexOf(endDelim, start);
  if (end == -1) return "";
  return str.substring(start, end);
}

String extractBetweenSecondTime(const String &str) {
  int firstClose = str.indexOf("]");
  if (firstClose == -1) return "";
  int secondOpen = str.indexOf("[", firstClose);
  if (secondOpen == -1) return "";
  int secondClose = str.indexOf("]", secondOpen);
  if (secondClose == -1) return "";
  return str.substring(secondOpen + 1, secondClose);
}

float parseDistance(const String& text) {
  int index = text.indexOf("Distance:");
  if (index == -1) return -1;

  String sub = text.substring(index + 9);
  sub.trim();

  int end = sub.indexOf(" ");
  if (end == -1) end = sub.length();

  return sub.substring(0, end).toFloat();
}

void parseVoltagesAndCurrents(const String& text, float& v1, float& i1, float& v2, float& i2) {
  int firstV = text.indexOf("V=");
  int firstI = text.indexOf("I=");
  int secondV = text.indexOf("V=", firstV + 1);
  int secondI = text.indexOf("I=", firstI + 1);

  if (firstV != -1 && firstI != -1) {
    String vStr = text.substring(firstV + 2, text.indexOf("V", firstV + 2));
    String iStr = text.substring(firstI + 2, text.indexOf("mA", firstI + 2));
    v1 = vStr.toFloat();
    i1 = iStr.toFloat();
  }

  if (secondV != -1 && secondI != -1) {
    String vStr2 = text.substring(secondV + 2, text.indexOf("V", secondV + 2));
    String iStr2 = text.substring(secondI + 2, text.indexOf("mA", secondI + 2));
    v2 = vStr2.toFloat();
    i2 = iStr2.toFloat();
  }
}

int parseRSSI(const String& text) {
  int index = text.indexOf("RSSI_Node1=");
  if (index == -1) return -999; // ไม่มีข้อมูล

  String sub = text.substring(index + 11); // แก้เป็น 11 ตัวอักษร
  sub.trim();

  int end = sub.indexOf(" ");
  if (end == -1) end = sub.length();

  return sub.substring(0, end).toInt();
}

void saveToSD(String data) {
  File file = SD.open("/log.txt", FILE_APPEND);
  if (file) {
    file.println(data);
    file.close();
    Serial.println("Saved to SD");
  } else {
    Serial.println("Failed to write to SD");
  }
}

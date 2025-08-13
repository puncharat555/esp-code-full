#include <Wire.h>
#include <RTClib.h>

#define RTC_SDA    16
#define RTC_SCL    17

TwoWire I2C_RTC = TwoWire(1);
RTC_DS3231 rtc;

void setup() {
  Serial.begin(115200);
  I2C_RTC.begin(RTC_SDA, RTC_SCL);

  if (!rtc.begin(&I2C_RTC)) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // ตั้งเวลาเองแบบแม่นยำ (แก้เลขตามเวลาจริงของคุณ)
  rtc.adjust(DateTime(2025, 7, 31, 20, 01, 00));  // <-- ตั้งตรงนี้

  Serial.println("RTC time manually set");
}

void loop() {
  DateTime now = rtc.now();

  char timestamp[22];
  snprintf(timestamp, sizeof(timestamp), "[%04d-%02d-%02d %02d:%02d:%02d]",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());

  Serial.println(timestamp);
  delay(1000);
}

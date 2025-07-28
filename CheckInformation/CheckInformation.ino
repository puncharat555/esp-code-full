#include <SPI.h>
#include <SD.h>

// SD Card ผ่าน HSPI
#define SD_CS    4
#define SD_SCK   18
#define SD_MISO  19
#define SD_MOSI  23
SPIClass spiSD(HSPI);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("SD Viewer Mode");

  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("SD initialization failed!");
    return;
  }

  Serial.println("SD card initialized.");

  File file = SD.open("/log.txt", FILE_READ);
  if (!file) {
    Serial.println("log.txt not found.");
    return;
  }

  Serial.println("Reading log.txt...\n");

  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();

  Serial.println("\nEnd of file.");
}

void loop() {
  // วนเฉย ๆ รอให้อ่านผ่าน Serial Monitor ได้
}

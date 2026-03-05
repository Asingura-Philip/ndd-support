#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "MAX30105.h"  // Library for MAX30102/MAX30105
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SD.h>
#include <XPT2046_Touchscreen.h>  // Note: This library uses default SPI; for custom SPI, you may need to modify the library or use bit-bang

// Pin definitions based on your notes
#define BUZZER 48
#define GREEN_LED 1
#define YELLOW_LED 2
#define BLUE_LED 42

#define MPU_AD0 44
#define MPU_INT 45
#define SDA_PIN 41
#define SCL_PIN 40

#define TFT_SCK 6
#define TFT_MISO 4
#define TFT_MOSI 5
#define TFT_CS 9
#define TFT_DC 15
#define TFT_RST 18
#define TFT_LED 38

#define TOUCH_CS 7
#define TOUCH_IRQ 17
#define TOUCH_SCK 11
#define TOUCH_MISO 16
#define TOUCH_MOSI 10

#define SD_SCK 8
#define SD_MISO 46
#define SD_MOSI 17
#define SD_CS 39  // Assuming from "17-39"; adjust if it's 3 instead

// SPI instances for separate buses (ESP32 supports this)
//--------->??? SPIClass tftSPI(VSPI);  // VSPI for TFT
SPIClass tftSPI(SPI);   // SPI works for ESP32-S3
SPIClass sdSPI(HSPI);   // HSPI for SD

// Sensor objects
Adafruit_MPU6050 mpu;
MAX30105 particleSensor;

// TFT object
Adafruit_ILI9341 tft = Adafruit_ILI9341(&tftSPI, TFT_DC, TFT_CS, TFT_RST);

// Touch object (uses default SPI; to use custom, modify library or use software SPI)
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

// File for logging
File logFile;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  // Initialize LEDs and buzzer
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(BLUE_LED, LOW);
  digitalWrite(BUZZER, LOW);

  // Test LEDs and buzzer
  testPeripherals();

  // Set MPU AD0 to LOW for default address 0x68
  pinMode(MPU_AD0, OUTPUT);
  digitalWrite(MPU_AD0, LOW);

  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("MPU6050 initialized");

  // Initialize MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("Failed to find MAX30102");
    while (1) delay(10);
  }
  particleSensor.setup();  // Default settings
  particleSensor.setPulseAmplitudeRed(0x0A);  // Turn Red LED to low
  Serial.println("MAX30102 initialized");

  // Initialize TFT SPI and display
  tftSPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, -1);  // No default CS
  tft.begin();
  tft.setRotation(3);  // Adjust as needed (0-3)
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);  // Turn on backlight
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Harmony Guardian");
  Serial.println("TFT initialized");

  // Initialize Touch (uses default SPI; remap if needed)
  touch.begin();
  Serial.println("Touch initialized (default SPI)");

  // Initialize SD SPI and card
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, -1);
  if (!SD.begin(SD_CS, sdSPI)) {
    Serial.println("Failed to initialize SD card");
  } else {
    Serial.println("SD card initialized");
    logFile = SD.open("data.log", FILE_WRITE);
    if (logFile) {
      logFile.println("Timestamp,HR,SpO2,AccelX,AccelY,AccelZ");
      logFile.close();
    }
  }
}

void loop() {
  // Read MPU6050
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Read MAX30102 (basic polling; for efficiency, use interrupts)
  uint32_t irValue = particleSensor.getIR();
  uint32_t redValue = particleSensor.getRed();
  // Simple HR/SpO2 calculation (use full algorithm for accuracy)
  int hr = 0;  // Placeholder; implement full HR detection
  int sp02 = 0;  // Placeholder

  if (irValue > 50000) {  // Finger detected
    // Add full HR/SpO2 logic here (e.g., from examples)
    hr = 80 + random(10);  // Simulated for testing
    sp02 = 95 + random(5);  // Simulated
  }

  // Display data on TFT
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0);
  tft.print("HR: "); tft.println(hr);
  tft.print("SpO2: "); tft.println(sp02);
  tft.print("Accel X: "); tft.println(a.acceleration.x);
  tft.print("Accel Y: "); tft.println(a.acceleration.y);
  tft.print("Accel Z: "); tft.println(a.acceleration.z);

  // Log to SD
  logToSD(hr, sp02, a.acceleration.x, a.acceleration.y, a.acceleration.z);

  // Simple alert logic (e.g., high HR or sudden motion)
  float accelMag = sqrt(a.acceleration.x * a.acceleration.x + a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z);
  if (hr > 100 || accelMag > 12.0) {
    triggerAlert();
  } else if (hr > 80) {
    digitalWrite(YELLOW_LED, HIGH);
    delay(500);
    digitalWrite(YELLOW_LED, LOW);
  } else {
    digitalWrite(GREEN_LED, HIGH);
    delay(500);
    digitalWrite(GREEN_LED, LOW);
  }

  // Check touch (example)
  if (touch.touched()) {
    TS_Point p = touch.getPoint();
    Serial.print("Touch at "); Serial.print(p.x); Serial.print(", "); Serial.println(p.y);
    // Map to screen coordinates and handle input
  }

  delay(1000);  // Update every second
}

void testPeripherals() {
  // Test LEDs
  digitalWrite(GREEN_LED, HIGH); delay(500); digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, HIGH); delay(500); digitalWrite(YELLOW_LED, LOW);
  digitalWrite(BLUE_LED, HIGH); delay(500); digitalWrite(BLUE_LED, LOW);

  // Test buzzer
  tone(BUZZER, 1000, 500);  // Beep at 1kHz for 0.5s
}

void triggerAlert() {
  digitalWrite(BLUE_LED, HIGH);
  tone(BUZZER, 2000, 1000);  // Alert sound
  digitalWrite(BLUE_LED, LOW);
}

void logToSD(int hr, int sp02, float ax, float ay, float az) {
  logFile = SD.open("data.log", FILE_APPEND);
  if (logFile) {
    logFile.print(millis());
    logFile.print(",");
    logFile.print(hr);
    logFile.print(",");
    logFile.print(sp02);
    logFile.print(",");
    logFile.print(ax);
    logFile.print(",");
    logFile.print(ay);
    logFile.print(",");
    logFile.println(az);
    logFile.close();
  }
}

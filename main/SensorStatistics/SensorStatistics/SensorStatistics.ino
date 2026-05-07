/**
 * SensorStatistics.ino
 * Quick program meant for measuring statistics on Herbinator's sensor
 * failures.
 *
 * Author: Landon Wardle
 */

#include <DHT11.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>

// Pin assignments
#define SD_CS    18
#define SD_MOSI  8
#define SD_SCK   3 // Clk
#define SD_MISO  46

/**
 * SPI bus instance so we can pick the pins explicitly
 * On the ESP32-S3 this maps to the FSPI/HSPI peripheral.
 */
SPIClass sdSPI(FSPI);

/**
 * The reference to the name of the file
 */
const char* TEST_FILENAME = "/esp32_sd_test.txt";
/**
 * The name of the sensor being tested.
 */
const char* SENSOR_NAME = "Temperature Sensor";
/**
 * Standard SD sector size.
 */
const size_t BUFFER_SIZE = 512;

const int DHT_SENSOR_PIN = 5;
const int TEMP_READ_FAIL_THRESHOLD = 8;

// Last valid DHT11 readings — seeded to neutral indoor defaults
float lastTempC = 22.0f;
float lastHumidity = 50.0f;

int readFailCount = 0;
bool tempSensorFail = false;

DHT11 dht_sensor(DHT_SENSOR_PIN);

bool readDHT() {
    int tempC = 0;
    int humidity = 0;
    
    int readSuccess = dht_sensor.readTemperatureHumidity(tempC, humidity);
    
    if (readSuccess == 0) {
      tempSensorFail = false;
      readFailCount = 0; // Reset
      lastTempC = static_cast<float>(tempC);
      lastHumidity = static_cast<float>(humidity);
      Serial.printf("Temp success! time = %d tempC = %d, humidity = %d\n", millis(), tempC, humidity);
    } else {
      Serial.printf("Temp fail! time = %d tempC = %d, humidity = %d\n", millis(), tempC, humidity);

      readFailCount++;

      if (readFailCount >= TEMP_READ_FAIL_THRESHOLD) {
        tempSensorFail = true;
      }
    }

    return readSuccess;
}

/**
  * Writes to the file at the given path.
  *
  * @param path: Reference to the path to the file on the SD card.
  * @param content: Reference to the content that will be written to the file.
  * @return: true if the writing is successful, false if it fails due a failure to open.
  */
bool writeFile(const char* path, const char* content) {
  Serial.printf("Writing to %s ... ", path);
  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("FAILED to open for write");
    return false;
  }
  size_t n = f.print(content);
  f.close();
  Serial.printf("wrote %u bytes — OK\n", (unsigned)n);
  return n == strlen(content);
}

/**
  * Writes to the file at the given path.
  *
  * @param path: Reference to the path to the file on the SD card.
  * @param content: Reference to the content that will be written to the file.
  * @return: true if the appending is successful, false if it fails due a failure to open.
  */
bool appendFile(const char* path, const char* content) {
  Serial.printf("Appending to %s ... ", path);
  File f = SD.open(path, FILE_APPEND);
  if (!f) {
    Serial.println("FAILED to open for append");
    return false;
  }
  size_t n = f.print(content);
  f.close();
  Serial.printf("wrote %u bytes — OK\n", (unsigned)n);
  return n == strlen(content);
}

/**
  * Writes to the file at the given path.
  *
  * @param path: Reference to the path to the file on the SD card.
  * @return: true if the reading is successful, false if it fails due a failure to open.
  */
bool readFile(const char* path) {
  Serial.printf("Reading %s:\n", path);
  File f = SD.open(path);
  if (!f) {
    Serial.println("FAILED to open for read");
    return false;
  }
  Serial.println("--- file contents ---");
  while (f.available()) Serial.write(f.read());
  Serial.println("\n--- end of file ---");
  f.close();
  return true;
}

char buffer[BUFFER_SIZE];
size_t currentPos = 0;

void writeToSD(const std::string& data) {
    std::ofstream sdFile("log.txt", std::ios::app);
    if (sdFile.is_open()) {
        sdFile.write(buffer, currentPos);
        sdFile.close();
        currentPos = 0; // Reset buffer after write
    }
}

void addToBuffer(const std::string& str) {
    // If new data exceeds remaining space, flush first
    if (currentPos + str.length() >= BUFFER_SIZE) {
        writeToSD(""); 
    }
    
    // Copy string to buffer
    for (char c : str) {
        if (currentPos < BUFFER_SIZE) {
            buffer[currentPos++] = c;
        }
    }
}

/**
  * Code that runs initially on the ESP32 to initialize the card reader and writer.
  */
void setup() {
  Serial.begin(115200);
  delay(1500);  // Give USB-CDC time to come up on ESP32-S3
  Serial.printf("\n=== %s Test ===\n", SENSOR_NAME);

  // // Bring up the SPI bus on our chosen pins
  // sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  // // Try mounting. The 4th arg (4 MHz) is conservative; bump it to 25000000
  // // (25 MHz) once you've confirmed everything is working.
  // Serial.print("Mounting card... ");
  // if (!SD.begin(SD_CS, sdSPI, 4000000)) {
  //   Serial.println("FAILED.");
  //   Serial.println("Check: wiring, pull-ups, 3.3V power, FAT32 format.");
  //   return;
  // }
  // Serial.println("OK.");

  // printCardInfo();
  // Serial.println();

  // // Clean up any previous run so we start from a known state
  // if (SD.exists(TEST_FILENAME)) {
  //   Serial.printf("Removing old %s\n", TEST_FILENAME);
  //   SD.remove(TEST_FILENAME);
  // }

  // // Write -> read -> append -> read
  // if (!writeFile(TEST_FILENAME, "Hello from ESP32-S3!\n")) return;
  // if (!readFile(TEST_FILENAME))                              return;
  // if (!appendFile(TEST_FILENAME, "Line two, appended.\n"))   return;
  // if (!readFile(TEST_FILENAME))                              return;

  // Serial.println("\n*** SD card test PASSED ***");
}

unsigned long lastMeasurement = 0;
const unsigned long DELAY = 1000UL;

void loop() {
  dht_sensor.setDelay(DELAY);

  unsigned long now = millis();
  if (now - lastMeasurement < DELAY) return;
  lastMeasurement = now;

  // Nothing to do — test runs once in setup()
  readDHT();
}

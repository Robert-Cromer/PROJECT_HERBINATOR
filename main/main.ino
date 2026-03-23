#include "Arduino_LED_Matrix.h"
#include <Servo.h>
#include "dht_nonblocking.h"
#include <EEPROM.h>
#define DHT_SENSOR_TYPE DHT_TYPE_11

// Temporary motor pins

// -------------------- Pins --------------------
const int PUMP_MOTOR = 10; //D10
const int WATER_HIGH = 0; //D0
const int WATER_READ = 5; //A5
const int MOISTURE_READ = 0; //A0
const int TEMP_READ = 1; //A1
const int DHT_SENSOR_PIN = 8; //D8
const int GREEN_LED = 5; //D5
const int BLUE_LED = 6; //D6
const int RED_LED = 7; //D7
const int BUTTON = 13; //D13

Servo pump; // Pump uses PPM signal, so it needs to use the servo library

// How frequently we want to sample the sensors
// Essentially our clock cycle time, which recall is the inverse of frequency.
const unsigned long SENSOR_SAMPLE_MS = 100UL; // read sensors every 1s

// -------------------- Moisture Calibration --------------------
// You MUST calibrate these for your sensor + soil:
// - moistureDry: reading when probe is in dry soil (or in air)
// - moistureWet: reading when probe is in fully wet soil (water-saturated)
// Many capacitive sensors read HIGH when dry and LOWER when wet, but it varies.
const int IDEAL_MOISTURE_WET = 475; // Read from A0 with moisture sensor in the air.
const int IDEAL_MOISTURE_DRY = 210; // Read from A0 with moisture sensor in damp soil/a cup of water

// -------------------- Watering Policy --------------------
// Base thresholds (in % moisture)
const float ON_THRESHOLD_BASE  = 0.25;  // water when moisture drops below this
const float OFF_THRESHOLD_BASE = 0.75;  // stop when moisture rises above this, too wet!
const int WATER_THRESHOLD = 100; // Threshold for when we detect current going thru the water to verify that the water is in the container.

// Temperature adjustment (simple, practical)
// Hotter -> water sooner (raise thresholds slightly) and/or water a bit longer
// Colder -> water later (lower thresholds)
const float HOT_TEMP_C  = 30.0;
const float COLD_TEMP_C = 12.0;
const float TEMP_ADJUST_MAX = 7.0; // max +/- percent points added/subtracted

// Pump timing safety
const char PUMP_ON_DEGREE = 0; // Value from 0 to 80 describing the speed of the pump. 0 fastest 80 slowest.
const unsigned long MAX_PUMP_ON_MS = 10UL * 1000UL; // 10 seconds max per cycle

// -------------------- State --------------------
bool pumpOn = false;
unsigned long pumpStartMs = 0;
unsigned long lastWaterMs = 0;
unsigned long lastSampleMs = 0;

int moistureDry = 0;
int moistureWet = 0;

const unsigned int SENSOR_WET_ADDRESS = 0; //Addresses 0 through 3
const unsigned int SENSOR_DRY_ADDRESS = 4; //Addresses 4 through 7

const int calibrationHoldMs = 2000;
int btnDown = 0;

ArduinoLEDMatrix matrix;
DHT_nonblocking dht_sensor( DHT_SENSOR_PIN, DHT_SENSOR_TYPE );

enum class State {
  MainLoop,
  RedFlash,
  WateringLoop,
  Read,
  EnablePump,
  DisablePump,
  Error,
  CriticalError,
  SensorError,
  PromptDry,
  PromptWet,
  CalibrationDone,
  Communicate
};

enum class LEDColor {
  Green,
  Blue,
  Red
};

State currentState;

int getLEDPin(LEDColor LED) {
  switch (LED) {
    case LEDColor::Green: {
      return GREEN_LED;
    }
    case LEDColor::Blue: {
      return BLUE_LED;
    }
    case LEDColor::Red: {
      return RED_LED;
    }
  }
}

void setLEDEnabled(LEDColor LED, bool enabled) {
  digitalWrite(getLEDPin(LED), enabled ? HIGH : LOW);
}

void disableLEDS() {
  setLEDEnabled(LEDColor::Green, false);
  setLEDEnabled(LEDColor::Blue, false);
  setLEDEnabled(LEDColor::Red, false);
}

void enableLEDS() {
  setLEDEnabled(LEDColor::Green, true);
  setLEDEnabled(LEDColor::Blue, true);
  setLEDEnabled(LEDColor::Red, true);
}

void setup() {
  pump.attach(PUMP_MOTOR);

  // put your setup code here, to run once:
  //pinMode(PUMP_MOTOR, OUTPUT);
  pinMode(WATER_HIGH, OUTPUT);
  digitalWrite(WATER_HIGH, HIGH);
  
  pinMode(LED_BUILTIN, OUTPUT); // Initialize the digital pin as an output.

  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  pinMode(BUTTON, INPUT_PULLUP);

  setLEDEnabled(LEDColor::Green, true);
  setLEDEnabled(LEDColor::Blue, true);
  setLEDEnabled(LEDColor::Red, true);

  Serial.begin(9600);

  delay(1000);

  // Load initial values

  EEPROM.get(SENSOR_WET_ADDRESS, moistureWet);
  EEPROM.get(SENSOR_DRY_ADDRESS, moistureDry);

  Serial.println(moistureDry);

  disableLEDS();
}

int lastLEDFlash = 0;

void processFlash(LEDColor LED, int flashTimeMs) {
  if (millis() - lastLEDFlash < flashTimeMs) return;

  lastLEDFlash = millis();
  setLEDEnabled(LED, digitalRead(getLEDPin(LED)) == LOW);
}

void pumpSet(bool on) {
  pumpOn = on;

  //analogWrite(PUMP_MOTOR, pumpOn ? PUMP_ON_DEGREE : LOW); // enable on
  pump.write(pumpOn ? PUMP_ON_DEGREE : 90); // 90 deg is off

  if (on) pumpStartMs = millis();
}

float getMoistPct(int raw) {
  return (float)(raw - moistureDry) / (moistureWet - moistureDry); // Derived from lerp equation to get moisture pct
}

int flashBegin = 0;
bool lastBtnDown = false;
bool inCriticalError = false;

bool isButtonDown() {
  return digitalRead(BUTTON) == LOW;
}

void beginRedFlash() {
  disableLEDS();
  flashBegin = millis();
  currentState = State::RedFlash;
}

void loop() {
  unsigned long now = millis();

  bool btnDownNow = isButtonDown();
  if (btnDownNow && btnDownNow != lastBtnDown) {
    btnDown = now;
  }

  lastBtnDown = btnDownNow;

  switch (currentState) {
    case State::MainLoop: {
      if (isButtonDown()) {
        if (now - btnDown > calibrationHoldMs) {
          beginRedFlash();
          break;
        }

        currentState = State::MainLoop;
        break;
      }
      
      if (moistureWet < 0 | moistureDry > 1024 | moistureWet >= moistureDry) {
        beginRedFlash(); // Bad values found!
      } else {
        currentState = State::WateringLoop; // Otherwise, just proceed!
      }

      break;
    }
    case State::RedFlash: {
      pumpSet(false);
      processFlash(LEDColor::Red, 500);

      if (now - flashBegin > 3000) {
        disableLEDS();
        currentState = State::PromptDry;
      }

      break;
    }
    case State::PromptDry: {
      setLEDEnabled(LEDColor::Blue, false);
      setLEDEnabled(LEDColor::Red, false);
      processFlash(LEDColor::Green, 1000);

      if (isButtonDown() && now - btnDown > calibrationHoldMs) {
        int wrote = analogRead(MOISTURE_READ);
        EEPROM.put(SENSOR_DRY_ADDRESS, wrote);
        Serial.print("Dry=");
        Serial.println(wrote);
        currentState = State::PromptWet;
      }

      break;
    }
    case State::PromptWet: {
      setLEDEnabled(LEDColor::Green, false);
      setLEDEnabled(LEDColor::Red, false);
      processFlash(LEDColor::Blue, 1000);

      if (isButtonDown() && now - btnDown > calibrationHoldMs) {
        int wrote = analogRead(MOISTURE_READ);
        EEPROM.put(SENSOR_WET_ADDRESS, wrote);
        Serial.print("Wet=");
        Serial.println(wrote);
        currentState = State::CalibrationDone;
      }

      break;
    }
    case State::CalibrationDone: {
      disableLEDS();

      delay(500);

      enableLEDS();

      delay(500);

      disableLEDS();

      delay(500);

      enableLEDS();

      delay(500);

      disableLEDS();
      currentState = State::MainLoop;

      break;
    }
    case State::WateringLoop: {
        if (pumpOn) {
          setLEDEnabled(LEDColor::Blue, true);
          setLEDEnabled(LEDColor::Green, false);
          setLEDEnabled(LEDColor::Red, false);
        } else {
          setLEDEnabled(LEDColor::Blue, false);
          setLEDEnabled(LEDColor::Green, true);
          setLEDEnabled(LEDColor::Red, false);
        }
        if (now - lastSampleMs <= SENSOR_SAMPLE_MS) return;

        currentState = State::Read;
        break;
    }
    case State::Read: {
      lastSampleMs = now;

      // Read moisture
      int rawMoist = analogRead(MOISTURE_READ);
      float moistPct = getMoistPct(rawMoist);

      // Need to store temperature reading across state
      // Only sample when reading successful!

      // Read temperature
      float tempC = 0; 
      float humidity = 0;

      bool readSuccess = dht_sensor.measure(&tempC, &humidity);

      int rawWaterPresent = analogRead(WATER_READ);
      bool waterPresent = rawWaterPresent >= WATER_THRESHOLD;

      bool pumpCdExceeded = now - pumpStartMs > MAX_PUMP_ON_MS;

      if (!pumpOn && waterPresent && moistPct <= ON_THRESHOLD_BASE) {
        currentState = State::EnablePump;
        break;
      } else if (pumpOn && (!waterPresent || moistPct >= OFF_THRESHOLD_BASE || pumpCdExceeded)) {
        currentState = State::DisablePump;
        break;
      } else if (!waterPresent) {
        currentState = State::Error;
        break;
      }

      currentState = State::WateringLoop;

      // Debug output
      Serial.print("rawMoist=");
      Serial.print(rawMoist);
      Serial.print(" rawWater=");
      Serial.print(rawWaterPresent);
      Serial.print(" moist%=");
      Serial.print(moistPct, 1);

      Serial.print(" tempC=");
      Serial.print(tempC, 1);
      // if (tempValid) Serial.print(tempC, 1);
      // else Serial.print("NA");

      Serial.print(" pumpOn=");
      Serial.println(pumpOn ? "YES" : "NO");
      Serial.print(" tempSuccess=");
      Serial.println(readSuccess ? "YES" : "NO");

      break;
    }
    case State::EnablePump: {
      pumpSet(true);
      currentState = State::Communicate;
      break;
    }
    case State::DisablePump: {
      pumpSet(false);
      currentState = State::Communicate;
      break;
    }
    case State::Error: {
        setLEDEnabled(LEDColor::Red, true);
        setLEDEnabled(LEDColor::Green, false);
        setLEDEnabled(LEDColor::Blue, true);
        Serial.println("ERROR! NO WATER PRESENT!!");
        delay(1000);

        currentState = State::DisablePump;
        break;
    }
    case State::CriticalError: {
        pumpSet(false); // Failsafe!
        inCriticalError = true;
        Serial.println("ERROR! SENSOR FAILURE!!!!!");

        setLEDEnabled(LEDColor::Red, true);
        setLEDEnabled(LEDColor::Green, false);
        setLEDEnabled(LEDColor::Blue, false);

        disableLEDS();

        delay(250);

        setLEDEnabled(LEDColor::Red, true);

        delay(250);

        disableLEDS();

        delay(250);

        setLEDEnabled(LEDColor::Red, true);

        delay(250);

        disableLEDS();

        delay(250);

        setLEDEnabled(LEDColor::Red, true);

        delay(1000);

        // Handle error processing here!

        // If the error is resolved, then critical error flag is set to false
        currentState = State::Communicate;

        break;
    }
    case State::Communicate: {
      if (inCriticalError) {
        currentState = State::CriticalError;
        break;
      }

      currentState = State::MainLoop;
      break;
    }
  }
}

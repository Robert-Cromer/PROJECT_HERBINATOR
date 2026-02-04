#include "Arduino_LED_Matrix.h"

// Import drivers for sensors?

// Temporary motor pins

const int ENABLE = 5;
const int DIRA = 3;
const int DIRB = 4;

// -------------------- Pins --------------------
const int PUMP_MOTOR = 10; //D10, PWM
const int WATER_HIGH = 0; //D0
const int WATER_READ = 5; //A5
const int MOISTURE_READ = 0; //A0
const int TEMP_READ = 1; //A1

// How frequently we want to sample the sensors
// Essentially our clock cycle time, which recall is the inverse of frequency.
const unsigned long SENSOR_SAMPLE_MS = 1000UL; // read sensors every 1s

// -------------------- Moisture Calibration --------------------
// You MUST calibrate these for your sensor + soil:
// - MOISTURE_DRY: reading when probe is in dry soil (or in air)
// - MOISTURE_WET: reading when probe is in fully wet soil (water-saturated)
// Many capacitive sensors read HIGH when dry and LOWER when wet, but it varies.
const int MOISTURE_DRY = 0; // Read from A0 with moisture sensor in the air.
const int MOISTURE_WET = 0; // Read from A0 with moisture sensor in damp soil/a cup of water

// -------------------- Watering Policy --------------------
// Base thresholds (in % moisture)
const float ON_THRESHOLD_BASE  = 0;  // water when moisture drops below this
const float OFF_THRESHOLD_BASE = 0;  // stop when moisture rises above this, too wet!
const int WATER_THRESHOLD = 0; // Threshold for when we detect current going thru the water to verify that the water is in the container.

// Temperature adjustment (simple, practical)
// Hotter -> water sooner (raise thresholds slightly) and/or water a bit longer
// Colder -> water later (lower thresholds)
const float HOT_TEMP_C  = 30.0;
const float COLD_TEMP_C = 12.0;
const float TEMP_ADJUST_MAX = 7.0; // max +/- percent points added/subtracted

// Pump timing safety
const char PUMP_PMW_STRENGTH = 128; // Value from 0 to 255 to control the pump speed using PWM.
const unsigned long MAX_PUMP_ON_MS       = 10UL * 1000UL; // 10 seconds max per cycle
const unsigned long MIN_TIME_BETWEEN_MS  = 30UL * 60UL * 1000UL; // 30 minutes between cycles

// -------------------- State --------------------
bool pumpOn = false;
unsigned long pumpStartMs = 0;
unsigned long lastWaterMs = 0;
unsigned long lastSampleMs = 0;

ArduinoLEDMatrix matrix;

void setup() {
  // temporary
  pinMode(ENABLE,OUTPUT);
  pinMode(DIRA,OUTPUT);
  pinMode(DIRB,OUTPUT);

  // put your setup code here, to run once:
  pinMode(PUMP_MOTOR, OUTPUT);
  pinMode(WATER_HIGH, OUTPUT);
  digitalWrite(WATER_HIGH, HIGH);
  
  pinMode(LED_BUILTIN, OUTPUT); // Initialize the digital pin as an output.

  byte frame[8][12] = {
    { 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0 },
    { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 },
    { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 },
    { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 },
    { 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1 },
    { 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 },
    { 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 },
    { 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0 }
  };

  /*
    byte frame[8][12] = {
    { 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0 },
    { 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0 },
    { 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0 },
    { 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0 },
    { 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
  };
  */

  matrix.begin();
  matrix.renderBitmap(frame, 8, 12);
  Serial.begin(9600);
}

float clampf(float x, float a, float b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}

void pumpSet(bool on) {
  pumpOn = on;

  // analogWrite(PUMP_MOTOR, on ? PUMP_PMW_STRENGTH : LOW); // TODO: Validate this arduino board is active low or active high

  digitalWrite(ENABLE, pumpOn ? HIGH : LOW); // enable on
  digitalWrite(DIRA,HIGH); //one way
  digitalWrite(DIRB,LOW);

  if (on) pumpStartMs = millis();
}

void loop() {
  unsigned long now = millis(); 

  if (now - lastSampleMs <= SENSOR_SAMPLE_MS) return;

  lastSampleMs = now;

  // Read moisture
  int rawMoist = analogRead(MOISTURE_READ);
  int moistPct = (1.0f - ((float) rawMoist / 477)) * 100; //moisturePercentFromRaw(rawMoist); Will need to work w/ Sensor to get this part coded.

  // Read temperature
  float tempC = 0; //sensors.getTempCByIndex(0); Call temperature library from here .

  bool tempValid = (tempC > -50.0 && tempC < 80.0); // Check if the sensor read a temperature that makes sense.

  float adj = 0; //tempValid ? tempThresholdAdjust(tempC) : 0.0f; // Get temperature adjustment for the moisture levels based on temperature

  // Adjust on and off thresholds based on the current temperature
  float onTh  = clampf(ON_THRESHOLD_BASE  + adj, 5.0f, 80.0f);
  float offTh = clampf(OFF_THRESHOLD_BASE + adj, onTh + 2.0f, 95.0f);

  int rawWaterPresent = analogRead(WATER_READ);
  bool waterPresent = true; // TEMPORARY OVERRIDE //rawWaterPresent >= WATER_THRESHOLD;

    // Decision logic
  if (!pumpOn) {
    bool allowedByCooldown = true; //(now - lastWaterMs) >= MIN_TIME_BETWEEN_MS;
    bool tooDry = (moistPct <= onTh);

    if (allowedByCooldown && tooDry && waterPresent) {
      pumpSet(true);
      Serial.println("PUMP ON"); // debug
    }
  } else {
    bool reachedOff = (moistPct >= offTh); // Need to make sure there is water present to pump!
    bool timedOut = (now - pumpStartMs) >= MAX_PUMP_ON_MS;

    if (reachedOff || timedOut || !waterPresent) {
      pumpSet(false);
      lastWaterMs = now;
      Serial.println("PUMP OFF"); // debug
    }
  }

    // Debug output
  Serial.print("rawMoist=");
  Serial.print(rawMoist);
  Serial.print(" moist%=");
  Serial.print(moistPct, 1);

  Serial.print(" tempC=");
  if (tempValid) Serial.print(tempC, 1);
  else Serial.print("NA");

  Serial.print(" onTh=");
  Serial.print(onTh, 1);
  Serial.print(" offTh=");
  Serial.print(offTh, 1);

  Serial.print(" pumpOn=");
  Serial.println(pumpOn ? "YES" : "NO");
}

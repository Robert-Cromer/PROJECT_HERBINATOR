#include "Arduino_LED_Matrix.h"

const int MOTOR = 10; //D10, PWM
const int WATER_HIGH = 0; //D0
const int WATER_READ = 5; //A5
const int MOISTURE_READ = 0; //A0
const int TEMP_READ = 1; //A1

ArduinoLEDMatrix matrix;

void setup() {
  // put your setup code here, to run once:
  pinMode(MOTOR, OUTPUT);
  pinMode(WATER_HIGH, OUTPUT);
  digitalWrite(WATER_HIGH, HIGH);
  
  pinMode(LED_BUILTIN, OUTPUT); // Initialize the digital pin as an output.

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

  frame[2][1] = 1;

  matrix.begin();
  matrix.renderBitmap(frame, 8, 12);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH); // Turn the LED on (HIGH is the voltage level)
  delay(100);                     // Wait for a second
  digitalWrite(LED_BUILTIN, LOW);  // Turn the LED off by making the voltage LOW
  delay(100);                     // Wait for a second
}

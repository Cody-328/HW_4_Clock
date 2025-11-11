#include <Arduino.h>
#include <WiFi.h>
#include "time.h"
#include <AccelStepper.h>

#define DRIVER 1

// --------------------
// Pin definitions
// --------------------
#define MOTOR1_STEP_PIN 9   // Hour hand
#define MOTOR1_DIR_PIN 10
#define MOTOR2_STEP_PIN 11  // Minute hand
#define MOTOR2_DIR_PIN 12
#define BUTTON1_PIN 5       // Hour zero (limit) switch
#define BUTTON2_PIN 6       // Minute zero (limit) switch

// --------------------
// Stepper setup
// --------------------
AccelStepper hourHand(DRIVER, MOTOR1_STEP_PIN, MOTOR1_DIR_PIN);
AccelStepper minuteHand(DRIVER, MOTOR2_STEP_PIN, MOTOR2_DIR_PIN);

const long STEPS_PER_REV = 200;  // 1 full revolution
const float HOMING_SPEED = 25.0; // steps/sec for zeroing

// WiFi + NTP
const char* ssid = "NSA Security Van HQ";
const char* password = "windowstothehallway";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600;  // CST (UTC-6)
const int   daylightOffset_sec = 0;

// State variables
bool hourZeroed = false;
bool minuteZeroed = false;
bool clockRunning = false;

// --------------------
// HOMING FUNCTION
// --------------------
void homeMotor(AccelStepper &motor, int limitPin, bool reverseDirection = false) {
  Serial.println("Homing...");

  // Set direction
  float speed = reverseDirection ? -HOMING_SPEED : HOMING_SPEED;
  motor.setSpeed(speed);

  // Move until switch is pressed (switch normally grounded, goes HIGH when triggered)
  while (digitalRead(limitPin) == LOW) {
    motor.runSpeed();
  }

  // Stop and set zero
  motor.setCurrentPosition(0);
  Serial.println("Homed and zeroed at 12:00 position!");
}

// --------------------
// WiFi + Time setup
// --------------------
void setupWiFiAndTime() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Time synchronized.");
}

// --------------------
// Get NTP time
// --------------------
bool getLocalTimeSafe(struct tm * timeinfo) {
  if (!getLocalTime(timeinfo)) {
    Serial.println("Failed to obtain time");
    return false;
  }
  return true;
}

// --------------------
// Setup
// --------------------
void setup() {
  Serial.begin(115200);
  Serial.println("WiFi Stepper Clock");

  pinMode(BUTTON1_PIN, INPUT);  // Limit switches: normally grounded
  pinMode(BUTTON2_PIN, INPUT);

  hourHand.setMaxSpeed(1000);
  minuteHand.setMaxSpeed(1000);

  // Reverse the minute hand direction
  minuteHand.setPinsInverted(true, false, false);

  hourHand.setCurrentPosition(0);
  minuteHand.setCurrentPosition(0);

  setupWiFiAndTime();
  Serial.println("Waiting for limit switch homing...");
}

// --------------------
// Main loop
// --------------------
void loop() {
  // --- Home each axis once ---
  if (!hourZeroed) {
    Serial.println("Hour hand homing...");
    homeMotor(hourHand, BUTTON1_PIN, false);
    hourZeroed = true;
    Serial.println("Hour hand set to 12:00");
    delay(300);
  }

  if (!minuteZeroed) {
    Serial.println("Minute hand homing...");
    homeMotor(minuteHand, BUTTON2_PIN, false);  // reverse direction
    minuteZeroed = true;
    Serial.println("Minute hand set to 00");
    delay(300);
  }

  // --- Wait until both zeroed ---
  if (!clockRunning && hourZeroed && minuteZeroed) {
    clockRunning = true;
    Serial.println("Both hands zeroed — starting real-time clock!");
  }

  // --- Normal clock operation ---
  if (clockRunning) {
    struct tm timeinfo;
    if (getLocalTimeSafe(&timeinfo)) {
      int hours = timeinfo.tm_hour % 12;
      int minutes = timeinfo.tm_min;
      int seconds = timeinfo.tm_sec;

      // Calculate motor positions
      long hourSteps = map(hours * 60 + minutes, 0, 720, 0, STEPS_PER_REV);
      long minuteSteps = map(minutes * 60 + seconds, 0, 3600, 0, STEPS_PER_REV);

      // Move steppers smoothly toward target positions
      hourHand.moveTo(hourSteps);
      minuteHand.moveTo(minuteSteps);
      hourHand.setSpeed((hourSteps - hourHand.currentPosition()) > 0 ? 20 : -20);
      minuteHand.setSpeed((minuteSteps - minuteHand.currentPosition()) > 0 ? 10 : -10);
      hourHand.runSpeedToPosition();
      minuteHand.runSpeedToPosition();

      // Print time for debug
      Serial.printf("Time: %02d:%02d:%02d | Hsteps=%ld | Msteps=%ld\n",
                    hours, minutes, seconds,
                    hourHand.currentPosition(),
                    minuteHand.currentPosition());
    }
  }
}

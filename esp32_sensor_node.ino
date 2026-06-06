/*
  Version 1 - Basic ESP32-C3 distance alarm node
  Basic serial communication for Raspberry Pi / VirtualBox Flask app.
*/

const int TRIG_PIN = 7;
const int ECHO_PIN = 8;
const int RGB_RED_PIN = 10;
const int RGB_GREEN_PIN = 20;
const int RGB_BLUE_PIN = 21;
const int BUZZER_PIN = 3;

const unsigned long ECHO_TIMEOUT_US = 30000;
unsigned long measurementIntervalMs = 1000;
float distanceThresholdCm = 30.0;

bool systemOpen = false;
bool monitoring = false;
unsigned long lastMeasurementMs = 0;

void setRgb(bool red, bool green, bool blue) {
  digitalWrite(RGB_RED_PIN, red ? HIGH : LOW);
  digitalWrite(RGB_GREEN_PIN, green ? HIGH : LOW);
  digitalWrite(RGB_BLUE_PIN, blue ? HIGH : LOW);
}

float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if (duration == 0) return -1.0;
  return duration * 0.0343 / 2.0;
}

void handleCommand(String command) {
  command.trim();

  if (command == "O") {
    systemOpen = true;
    monitoring = false;
    setRgb(false, true, false);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("STATE:OPENED");
  }
  else if (command == "S") {
    if (systemOpen) {
      monitoring = true;
      lastMeasurementMs = 0;
      Serial.println("STATE:MONITORING");
    } else {
      Serial.println("ERROR:PRESS_OPEN_FIRST");
    }
  }
  else if (command == "T") {
    monitoring = false;
    setRgb(false, false, true);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("STATE:STOPPED");
  }
  else if (command == "C") {
    systemOpen = false;
    monitoring = false;
    setRgb(false, false, false);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("STATE:CLOSED");
  }
  else if (command.startsWith("I")) {
    measurementIntervalMs = command.substring(1).toInt();
    if (measurementIntervalMs < 200) measurementIntervalMs = 200;
    Serial.print("INTERVAL_SET:");
    Serial.println(measurementIntervalMs);
  }
  else if (command.startsWith("D")) {
    distanceThresholdCm = command.substring(1).toFloat();
    if (distanceThresholdCm < 1.0) distanceThresholdCm = 1.0;
    Serial.print("THRESHOLD_SET:");
    Serial.println(distanceThresholdCm, 2);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(TRIG_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  setRgb(false, false, false);

  Serial.println("ESP32_READY");
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    handleCommand(command);
  }

  if (!systemOpen || !monitoring) return;

  unsigned long now = millis();
  if (now - lastMeasurementMs < measurementIntervalMs) return;
  lastMeasurementMs = now;

  float distanceCm = readDistanceCm();
  bool alarm = distanceCm > 0 && distanceCm < distanceThresholdCm;

  if (distanceCm < 0) {
    setRgb(true, false, true);
    digitalWrite(BUZZER_PIN, LOW);
  } else if (alarm) {
    setRgb(true, false, false);
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    setRgb(false, true, false);
    digitalWrite(BUZZER_PIN, LOW);
  }

  Serial.print("DATA:");
  Serial.print(distanceCm, 2);
  Serial.print(",");
  Serial.print(distanceThresholdCm, 2);
  Serial.print(",");
  Serial.println(alarm ? 1 : 0);
}

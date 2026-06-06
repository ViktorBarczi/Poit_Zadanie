/*
  Version 2 - ESP32-C3 distance alarm node with enum and switch
  Serial protocol for Raspberry Pi / VirtualBox Flask app.
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
float lastDistanceCm = -1.0;
unsigned long lastMeasurementMs = 0;

enum SystemState {
  CLOSED,
  OPENED,
  MONITORING,
  STOPPED
};

SystemState systemState = CLOSED;

void setRgb(bool red, bool green, bool blue) {
  digitalWrite(RGB_RED_PIN, red ? HIGH : LOW);
  digitalWrite(RGB_GREEN_PIN, green ? HIGH : LOW);
  digitalWrite(RGB_BLUE_PIN, blue ? HIGH : LOW);
}

const char* getStateName() {
  switch (systemState) {
    case CLOSED: return "CLOSED";
    case OPENED: return "OPENED";
    case MONITORING: return "MONITORING";
    case STOPPED: return "STOPPED";
    default: return "UNKNOWN";
  }
}

void setSystemState(SystemState newState) {
  systemState = newState;

  switch (systemState) {
    case CLOSED:
      setRgb(false, false, false);
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("STATE:CLOSED");
      break;
    case OPENED:
      setRgb(false, true, false);
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("STATE:OPENED");
      break;
    case MONITORING:
      setRgb(false, true, false);
      digitalWrite(BUZZER_PIN, LOW);
      lastMeasurementMs = 0;
      Serial.println("STATE:MONITORING");
      break;
    case STOPPED:
      setRgb(false, false, true);
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("STATE:STOPPED");
      break;
  }
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

void sendStatus() {
  Serial.print("STATUS:");
  Serial.print(getStateName());
  Serial.print(",");
  Serial.print(measurementIntervalMs);
  Serial.print(",");
  Serial.print(distanceThresholdCm, 2);
  Serial.print(",");
  Serial.println(lastDistanceCm, 2);
}

void handleCommand(String input) {
  input.trim();
  if (input.length() == 0) return;

  char command = input.charAt(0);
  String value = input.substring(1);
  value.trim();

  switch (command) {
    case 'O':
    case 'o':
      setSystemState(OPENED);
      break;

    case 'S':
    case 's':
      if (systemState == OPENED || systemState == STOPPED) {
        setSystemState(MONITORING);
      } else {
        Serial.println("ERROR:START_NOT_ALLOWED_PRESS_OPEN_FIRST");
      }
      break;

    case 'T':
    case 't':
      if (systemState == MONITORING) setSystemState(STOPPED);
      else Serial.println("ERROR:STOP_NOT_ALLOWED");
      break;

    case 'C':
    case 'c':
      setSystemState(CLOSED);
      break;

    case 'P':
    case 'p':
      sendStatus();
      break;

    case 'I':
    case 'i':
      measurementIntervalMs = value.toInt();
      if (measurementIntervalMs < 200) measurementIntervalMs = 200;
      Serial.print("INTERVAL_SET:");
      Serial.println(measurementIntervalMs);
      break;

    case 'D':
    case 'd':
      distanceThresholdCm = value.toFloat();
      if (distanceThresholdCm < 1.0) distanceThresholdCm = 1.0;
      Serial.print("THRESHOLD_SET:");
      Serial.println(distanceThresholdCm, 2);
      break;

    default:
      Serial.print("UNKNOWN_COMMAND:");
      Serial.println(input);
      break;
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
  setSystemState(CLOSED);
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    handleCommand(input);
  }

  if (systemState != MONITORING) return;

  unsigned long now = millis();
  if (now - lastMeasurementMs < measurementIntervalMs) return;
  lastMeasurementMs = now;

  float distanceCm = readDistanceCm();
  lastDistanceCm = distanceCm;
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
  Serial.print(alarm ? 1 : 0);
  Serial.print(",");
  Serial.println(getStateName());
}

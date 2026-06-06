
// ======== Pins ========
const int TRIG_PIN = 7;
const int ECHO_PIN = 8;

const int RGB_RED_PIN   = 10;
const int RGB_GREEN_PIN = 20;
const int RGB_BLUE_PIN  = 21;

const int BUZZER_PIN = 3;

// true  = common cathode RGB LED
// false = common anode RGB LED
const bool RGB_COMMON_CATHODE = true;

// ======== Distance settings ========
const unsigned long ECHO_TIMEOUT_US = 30000;

unsigned long measurementIntervalMs = 1000;
float distanceThresholdCm = 30.0;

float lastDistanceCm = -1.0;

unsigned long lastMeasurementMs = 0;

// ======== Alert settings ========
bool alertActive = false;
unsigned long alertStartMs = 0;
unsigned long lastBlinkMs = 0;

const unsigned long ALERT_DURATION_MS = 1000;
const unsigned long BLINK_INTERVAL_MS = 150;

bool blinkState = false;

// ======== Enum states ========
enum SystemState {
  CLOSED,
  OPENED,
  MONITORING,
  STOPPED
};

SystemState systemState = CLOSED;

// ======== LED color enum ========
enum LedColor {
  LED_OFF,
  LED_BLUE,
  LED_GREEN,
  LED_RED,
  LED_PURPLE
};

// These two colors can be changed from Raspberry Pi / VirtualBox over Serial.
// Default: normal monitoring = GREEN, alarm = RED.
LedColor generalColor = LED_GREEN;
LedColor alarmColor = LED_RED;

const int BLUE_COLOR = 1;
const int GREEN_COLOR = 2;
const int RED_COLOR = 3;
const int PURPLE_COLOR = 4;

// ======== RGB functions ========
void setRgbRaw(bool r, bool g, bool b) {
  if (RGB_COMMON_CATHODE) {
    digitalWrite(RGB_RED_PIN,   r ? HIGH : LOW);
    digitalWrite(RGB_GREEN_PIN, g ? HIGH : LOW);
    digitalWrite(RGB_BLUE_PIN,  b ? HIGH : LOW);
  } else {
    digitalWrite(RGB_RED_PIN,   r ? LOW : HIGH);
    digitalWrite(RGB_GREEN_PIN, g ? LOW : HIGH);
    digitalWrite(RGB_BLUE_PIN,  b ? LOW : HIGH);
  }
}

void setLedColor(LedColor color) {
  switch (color) {
    case LED_RED:
      setRgbRaw(true, false, false);
      break;

    case LED_GREEN:
      setRgbRaw(false, true, false);
      break;

    case LED_BLUE:
      setRgbRaw(false, false, true);
      break;

    case LED_PURPLE:
      setRgbRaw(true, false, true);
      break;

    case LED_OFF:
    default:
      setRgbRaw(false, false, false);
      break;
  }
}

LedColor colorFromNumber(int colorNumber) {
  switch (colorNumber) {
    case BLUE_COLOR:
      return LED_BLUE;

    case GREEN_COLOR:
      return LED_GREEN;

    case RED_COLOR:
      return LED_RED;

    case PURPLE_COLOR:
      return LED_PURPLE;

    default:
      return LED_GREEN;
  }
}

const char* getColorName(LedColor color) {
  switch (color) {
    case LED_BLUE:
      return "BLUE";

    case LED_GREEN:
      return "GREEN";

    case LED_RED:
      return "RED";

    case LED_PURPLE:
      return "PURPLE";

    case LED_OFF:
    default:
      return "OFF";
  }
}

// ======== State helpers ========
const char* getStateName() {
  switch (systemState) {
    case CLOSED:
      return "CLOSED";

    case OPENED:
      return "OPENED";

    case MONITORING:
      return "MONITORING";

    case STOPPED:
      return "STOPPED";

    default:
      return "UNKNOWN";
  }
}

void setSystemState(SystemState newState) {
  systemState = newState;

  switch (systemState) {
    case CLOSED:
      setLedColor(LED_OFF);
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("STATE:CLOSED");
      break;

    case OPENED:
      setLedColor(generalColor);
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("STATE:OPENED");
      break;

    case MONITORING:
      setLedColor(generalColor);
      digitalWrite(BUZZER_PIN, LOW);
      lastMeasurementMs = 0;
      Serial.println("STATE:MONITORING");
      break;

    case STOPPED:
      setLedColor(LED_BLUE);
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("STATE:STOPPED");
      break;
  }
}

// ======== Distance functions ========
float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);

  if (duration == 0) {
    return -1.0;
  }

  return duration * 0.0343 / 2.0;
}

// ======== Alert functions ========
void startAlert() {
  alertActive = true;
  alertStartMs = millis();
  lastBlinkMs = 0;
  blinkState = false;
  digitalWrite(BUZZER_PIN, HIGH);
}

void updateAlert() {
  if (!alertActive) {
    return;
  }

  unsigned long now = millis();

  if (now - alertStartMs >= ALERT_DURATION_MS) {
    alertActive = false;
    digitalWrite(BUZZER_PIN, LOW);

    if (systemState == MONITORING) {
      setLedColor(generalColor);
    }

    return;
  }

  if (now - lastBlinkMs >= BLINK_INTERVAL_MS) {
    lastBlinkMs = now;
    blinkState = !blinkState;

    if (blinkState) {
      setLedColor(alarmColor);
    } else {
      setLedColor(LED_OFF);
    }
  }
}

// ======== Serial output ========
void sendMeasurement(float distanceCm, bool alarm) {
  Serial.print("DATA:");
  Serial.print(distanceCm, 2);
  Serial.print(",");
  Serial.print(distanceThresholdCm, 2);
  Serial.print(",");
  Serial.print(alarm ? 1 : 0);
  Serial.print(",");
  Serial.println(getStateName());
}

void sendStatus() {
  Serial.print("STATUS:");
  Serial.print(getStateName());
  Serial.print(",");
  Serial.print(measurementIntervalMs);
  Serial.print(",");
  Serial.print(distanceThresholdCm, 2);
  Serial.print(",");
  Serial.print(lastDistanceCm, 2);
  Serial.print(",");
  Serial.print(getColorName(generalColor));
  Serial.print(",");
  Serial.println(getColorName(alarmColor));
}

void sendLog(const String& message) {
  Serial.print("LOG:");
  Serial.println(message);
}

void sendError(const String& message) {
  Serial.print("ERROR:");
  Serial.println(message);
}

// ======== Command handling with switch ========
void handleCommand(String input) {
  input.trim();

  if (input.length() == 0) {
    return;
  }

  char command = input.charAt(0);
  String value = input.substring(1);
  value.trim();

  switch (command) {
    case 'O':   // Open
    case 'o':
      setSystemState(OPENED);
      sendLog("System initialized by Raspberry Pi / VirtualBox app");
      break;

    case 'S':   // Start
    case 's':
      if (systemState == OPENED || systemState == STOPPED) {
        setSystemState(MONITORING);
        sendLog("Monitoring started");
      } else if (systemState == MONITORING) {
        Serial.println("STATE:MONITORING");
      } else {
        sendError("START_NOT_ALLOWED_PRESS_OPEN_FIRST");
      }
      break;

    case 'T':   // Stop
    case 't':
      if (systemState == MONITORING) {
        setSystemState(STOPPED);
        sendLog("Monitoring stopped");
      } else {
        sendError("STOP_NOT_ALLOWED");
      }
      break;

    case 'C':   // Close
    case 'c':
      setSystemState(CLOSED);
      sendLog("System closed");
      break;

    case 'P':   // Status
    case 'p':
      sendStatus();
      break;

    case 'I':   // Interval, example: I1000
    case 'i':
      if (value.length() == 0) {
        sendError("MISSING_INTERVAL_VALUE");
        break;
      }

      measurementIntervalMs = value.toInt();

      if (measurementIntervalMs < 200) {
        measurementIntervalMs = 200;
      }

      Serial.print("INTERVAL_SET:");
      Serial.println(measurementIntervalMs);
      break;

    case 'D':   // Distance threshold, example: D30
    case 'd':
      if (value.length() == 0) {
        sendError("MISSING_THRESHOLD_VALUE");
        break;
      }

      distanceThresholdCm = value.toFloat();

      if (distanceThresholdCm < 1.0) {
        distanceThresholdCm = 1.0;
      }

      Serial.print("THRESHOLD_SET:");
      Serial.println(distanceThresholdCm, 2);
      break;

    case 'G':   // General LED color, example: G1 blue, G2 green, G3 red, G4 purple
    case 'g':
      if (value.length() == 0) {
        sendError("MISSING_GENERAL_COLOR_VALUE");
        break;
      }

      generalColor = colorFromNumber(value.toInt());

      if (systemState == OPENED || systemState == MONITORING || systemState == STOPPED) {
        setLedColor(generalColor);
      }

      Serial.print("GENERAL_COLOR_SET:");
      Serial.println(getColorName(generalColor));
      break;

    case 'A':   // Alarm LED color, example: A1 blue, A2 green, A3 red, A4 purple
    case 'a':
      if (value.length() == 0) {
        sendError("MISSING_ALARM_COLOR_VALUE");
        break;
      }

      alarmColor = colorFromNumber(value.toInt());

      Serial.print("ALARM_COLOR_SET:");
      Serial.println(getColorName(alarmColor));
      break;

    default:
      Serial.print("UNKNOWN_COMMAND:");
      Serial.println(input);
      break;
  }
}

void readSerialCommands() {
  if (!Serial.available()) {
    return;
  }

  String input = Serial.readStringUntil('\n');
  handleCommand(input);
}

// ======== Monitoring ========
void processMonitoring() {
  if (systemState != MONITORING) {
    return;
  }

  unsigned long now = millis();

  if (now - lastMeasurementMs < measurementIntervalMs) {
    return;
  }

  lastMeasurementMs = now;

  float distanceCm = readDistanceCm();
  lastDistanceCm = distanceCm;

  if (distanceCm < 0) {
    setLedColor(LED_PURPLE);
    digitalWrite(BUZZER_PIN, LOW);
    sendMeasurement(distanceCm, false);
    sendLog("No echo from HY-SRF05");
    return;
  }

  bool alarm = distanceCm < distanceThresholdCm;

  if (alarm) {
    startAlert();
  } else if (!alertActive) {
    setLedColor(generalColor);
    digitalWrite(BUZZER_PIN, LOW);
  }

  sendMeasurement(distanceCm, alarm);
}

// ======== Startup test ========
void startupTest() {
  sendLog("Startup test started");

  setLedColor(LED_BLUE);
  delay(300);

  float distanceCm = readDistanceCm();
  lastDistanceCm = distanceCm;

  if (distanceCm < 0) {
    sendLog("Startup test: No echo from HY-SRF05");
    setLedColor(LED_PURPLE);

    digitalWrite(BUZZER_PIN, HIGH);
    delay(300);
    digitalWrite(BUZZER_PIN, LOW);

    return;
  }

  sendLog("Startup test distance: " + String(distanceCm, 1) + " cm");

  setLedColor(LED_GREEN);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(200);
  digitalWrite(BUZZER_PIN, LOW);
}

// ======== Setup and loop ========
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  delay(1000);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(TRIG_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  setLedColor(LED_OFF);

  Serial.println("ESP32_READY");
  sendLog("ESP32-C3 serial node booted");
  sendLog("Waiting for command: O, S, T, C, P, I1000, D30, G1, A3");

  startupTest();

  setSystemState(CLOSED);
}

void loop() {
  readSerialCommands();
  processMonitoring();
  updateAlert();
}

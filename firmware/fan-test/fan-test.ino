#include <Arduino.h>

// --------------------------------------------------
// Pinbelegung
// --------------------------------------------------
constexpr int PIN_FAN_PWM  = 25;
constexpr int PIN_FAN_TACH = 26;

// --------------------------------------------------
// PWM
// --------------------------------------------------
constexpr uint32_t PWM_FREQ_HZ = 25000;
constexpr uint8_t PWM_RES_BITS = 8;

// --------------------------------------------------
// Tach
// --------------------------------------------------
constexpr uint8_t TACH_PULSES_PER_REV = 2;
volatile uint32_t tachPulses = 0;

// --------------------------------------------------
// Testparameter
// --------------------------------------------------
constexpr uint8_t START_SEARCH_STEP = 1;
constexpr uint8_t CURVE_STEP_PWM    = 5;

constexpr uint32_t SETTLE_TIME_MS = 3000;
constexpr uint32_t SAMPLE_TIME_MS = 5000;
constexpr uint32_t STOP_CHECK_TIME_MS = 500;

constexpr uint16_t RPM_START_THRESHOLD = 150;
constexpr uint8_t STABLE_REQUIRED = 3;
constexpr uint8_t STOP_ZERO_REQUIRED = 2;

constexpr size_t MAX_POINTS = 32;

// --------------------------------------------------
// Datenstruktur
// --------------------------------------------------
struct FanCurvePoint {
  uint8_t pwm;
  uint16_t rpm;
};

FanCurvePoint curveUp[MAX_POINTS];
FanCurvePoint curveDown[MAX_POINTS];

size_t curveUpCount = 0;
size_t curveDownCount = 0;

uint8_t detectedStartPwm = 0;
uint8_t detectedHoldPwm  = 0;

// --------------------------------------------------
void IRAM_ATTR onTachPulse() {
  tachPulses++;
}

// --------------------------------------------------
void setFanPwmPercent(uint8_t percent) {
  if (percent > 100) percent = 100;
  uint32_t duty = map(percent, 0, 100, 0, 255);
  ledcWrite(PIN_FAN_PWM, duty);
}

// --------------------------------------------------
void resetTach() {
  noInterrupts();
  tachPulses = 0;
  interrupts();
}

// --------------------------------------------------
uint16_t measureRpm(uint32_t durationMs) {
  resetTach();
  delay(durationMs);

  noInterrupts();
  uint32_t pulses = tachPulses;
  interrupts();

  float seconds = durationMs / 1000.0f;
  return (pulses / (float)TACH_PULSES_PER_REV) * (60.0f / seconds);
}

// --------------------------------------------------
void waitForFanStop(const char* reason) {
  Serial.println();
  Serial.println(reason);

  uint8_t zeroCount = 0;

  while (zeroCount < STOP_ZERO_REQUIRED) {
    uint16_t rpm = measureRpm(STOP_CHECK_TIME_MS);

    Serial.print("  RPM -> ");
    Serial.println(rpm);

    if (rpm == 0) {
      zeroCount++;
    } else {
      zeroCount = 0;
    }
  }

  Serial.println("  Fan steht sicher.");
}

// --------------------------------------------------
uint16_t measureStable(uint8_t pwm) {
  setFanPwmPercent(pwm);
  delay(SETTLE_TIME_MS);

  uint32_t sum = 0;

  for (int i = 0; i < 4; i++) {
    uint16_t rpm = measureRpm(1000);
    sum += rpm;

    Serial.print("  Sample ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(rpm);
  }

  return sum / 4;
}

// --------------------------------------------------
bool isRunning(uint8_t pwm) {
  setFanPwmPercent(pwm);
  delay(SETTLE_TIME_MS);

  int ok = 0;

  for (int i = 0; i < STABLE_REQUIRED; i++) {
    uint16_t rpm = measureRpm(1000);

    Serial.print("  PWM ");
    Serial.print(pwm);
    Serial.print("% -> ");
    Serial.println(rpm);

    if (rpm > RPM_START_THRESHOLD) ok++;
  }

  return ok == STABLE_REQUIRED;
}

// --------------------------------------------------
void stopFan() {
  setFanPwmPercent(0);
  waitForFanStop("Warte auf Stillstand des Luefters...");
}

// --------------------------------------------------
void startBoost() {
  setFanPwmPercent(40);
  delay(2000);
}

// --------------------------------------------------
uint8_t detectStart() {
  Serial.println("\n=== Start PWM suchen ===");

  for (uint8_t pwm = 0; pwm <= 100; pwm += START_SEARCH_STEP) {
    stopFan();

    if (isRunning(pwm)) {
      Serial.print("Start PWM: ");
      Serial.println(pwm);
      return pwm;
    }
  }

  return 100;
}

// --------------------------------------------------
void measureUp(uint8_t startPwm) {
  Serial.println("\n=== Kennlinie aufwärts ===");

  curveUpCount = 0;

  stopFan();
  startBoost();

  for (uint8_t pwm = startPwm; pwm < 100; pwm += CURVE_STEP_PWM) {
    Serial.print("PWM ");
    Serial.println(pwm);

    uint16_t rpm = measureStable(pwm);

    curveUp[curveUpCount++] = {pwm, rpm};

    Serial.print(" -> ");
    Serial.println(rpm);
  }

  if (curveUpCount == 0 || curveUp[curveUpCount - 1].pwm != 100) {
    Serial.println("PWM 100");

    uint16_t rpm = measureStable(100);
    curveUp[curveUpCount++] = {100, rpm};

    Serial.print(" -> ");
    Serial.println(rpm);
  }
}

// --------------------------------------------------
void measureDown() {
  Serial.println("\n=== Kennlinie abwärts ===");

  curveDownCount = 0;

  setFanPwmPercent(100);
  delay(SETTLE_TIME_MS);

  for (int pwm = 100; pwm >= 0; pwm -= CURVE_STEP_PWM) {
    Serial.print("PWM ");
    Serial.println(pwm);

    uint16_t rpm = measureStable(pwm);

    curveDown[curveDownCount++] = {(uint8_t)pwm, rpm};

    Serial.print(" -> ");
    Serial.println(rpm);
  }
}

// --------------------------------------------------
uint8_t detectHold() {
  Serial.println("\n=== Halte PWM suchen ===");

  setFanPwmPercent(100);
  delay(SETTLE_TIME_MS);

  uint8_t lastGood = 100;

  for (int pwm = 100; pwm >= 0; pwm--) {
    setFanPwmPercent(pwm);
    delay(1500);

    uint16_t rpm = measureRpm(1500);

    Serial.print("PWM ");
    Serial.print(pwm);
    Serial.print(" -> ");
    Serial.println(rpm);

    if (rpm > RPM_START_THRESHOLD) {
      lastGood = pwm;
    } else {
      Serial.print("Halte PWM: ");
      Serial.println(lastGood);
      return lastGood;
    }
  }

  return lastGood;
}

// --------------------------------------------------
void printCurve(const char* name, FanCurvePoint* arr, size_t count) {
  Serial.println("\n---");
  Serial.println(name);

  for (size_t i = 0; i < count; i++) {
    Serial.print(arr[i].pwm);
    Serial.print("% -> ");
    Serial.println(arr[i].rpm);
  }
}

// --------------------------------------------------
void printAsCode(FanCurvePoint* arr, size_t count) {
  Serial.println("\nFanCurvePoint curve[] = {");

  for (size_t i = 0; i < count; i++) {
    Serial.print("  {");
    Serial.print(arr[i].pwm);
    Serial.print(", ");
    Serial.print(arr[i].rpm);
    Serial.println("},");
  }

  Serial.println("};");
}

// --------------------------------------------------
bool done = false;

void setup() {
  Serial.begin(115200);
  delay(1500);

  ledcAttach(PIN_FAN_PWM, PWM_FREQ_HZ, PWM_RES_BITS);
  setFanPwmPercent(0);

  pinMode(PIN_FAN_TACH, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACH), onTachPulse, FALLING);

  Serial.println("Fan Auto Test startet...");
}

// --------------------------------------------------
void loop() {

  if (done) return;

  waitForFanStop("Starte Test erst bei 0 RPM...");

  detectedStartPwm = detectStart();
  measureUp(detectedStartPwm);
  measureDown();
  detectedHoldPwm = detectHold();

  Serial.println("\n=== Ergebnis ===");

  Serial.print("Start PWM: ");
  Serial.println(detectedStartPwm);

  Serial.print("Hold PWM: ");
  Serial.println(detectedHoldPwm);

  printCurve("Up", curveUp, curveUpCount);
  printCurve("Down", curveDown, curveDownCount);

  printAsCode(curveUp, curveUpCount);

  done = true;
}

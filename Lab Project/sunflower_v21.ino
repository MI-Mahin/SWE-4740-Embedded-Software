#include <Wire.h>
#include <Servo.h>
#include <Adafruit_NeoPixel.h>
#include <LiquidCrystal_I2C.h> 

// Pin Mappings
const int PIN_HORIZ        = 6;
const int PIN_VERT         = 8;
const int PIN_MODE_SW      = 9;
const int PIN_PIR          = 2;
const int PIN_BUZZER       = 3;
const int PIN_NEOPIXEL     = 4;
const int PIN_CLEAN_MOTOR  = 5;
const int PIN_DUST_SW      = 7;
const int PIN_WIND_SW      = 11;
const int PIN_RAIN_SW      = 13;

const int LDR_TOP   = A0;
const int LDR_BOT   = A1;
const int LDR_LEFT  = A2;
const int LDR_RIGHT = A3;


// Tunable constants
const int SERVO_MIN    =   0;
const int SERVO_MAX    = 180;
const int SERVO_CENTER =  90;

const int TRACK_STEP = 2;
const int TRACK_TOL  = 15;

const int STOW_V       =  10;
const int CLEAN_V      =  90;
const int STATIC_H     =  90;
const int STATIC_V     =  45;

const int BUZZER_FREQ  = 600;  // Hz – passive piezo alarm tone

// System Objects
Servo horizServo;
Servo vertServo;

LiquidCrystal_I2C lcd(0x20, 16, 2); 
Adafruit_NeoPixel ring(12, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Runtime state
int posH = SERVO_CENTER;
int posV = SERVO_CENTER;

// Forward declarations
void trackSun();
void updateDisplay(uint8_t r, uint8_t g, uint8_t b, const char* mode);

// setup
void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Servo init — write center before anything can interfere
  horizServo.attach(PIN_HORIZ);
  vertServo.attach(PIN_VERT);
  horizServo.write(posH);
  vertServo.write(posV);

  pinMode(PIN_MODE_SW, INPUT_PULLUP);
  pinMode(PIN_DUST_SW, INPUT_PULLUP);
  pinMode(PIN_WIND_SW, INPUT_PULLUP);
  pinMode(PIN_RAIN_SW, INPUT_PULLUP);

  pinMode(PIN_PIR,         INPUT);
  pinMode(PIN_BUZZER,      OUTPUT);
  pinMode(PIN_CLEAN_MOTOR, OUTPUT);
  digitalWrite(PIN_CLEAN_MOTOR, LOW);

  lcd.init();
  lcd.backlight();    

  // NeoPixel init
  ring.begin();
  ring.clear();
  ring.show();

  // Startup splash
  lcd.setCursor(0, 0); lcd.print("  SUNFLOWER v2  ");
  lcd.setCursor(0, 1); lcd.print("  Booting...    ");
  delay(1500);
  lcd.clear();
}

void loop() {
  // 1. SECURITY (always evaluated first, non-blocking)
  if (digitalRead(PIN_PIR) == HIGH) {
    tone(PIN_BUZZER, BUZZER_FREQ);
  } else {
    noTone(PIN_BUZZER);
  }

  // 2. STATE LOGIC (Event Priority Management)
  bool weatherAlert = (digitalRead(PIN_WIND_SW) == LOW ||
                       digitalRead(PIN_RAIN_SW)  == LOW);
  bool cleaningReq  =  digitalRead(PIN_DUST_SW)  == LOW;
  bool trackingMode =  digitalRead(PIN_MODE_SW)  == LOW;

  // 3. Priority State Machine
  if (weatherAlert) {
    // PRIORITY 1: Weather protection — Red
    posH = SERVO_CENTER;
    posV = STOW_V;
    digitalWrite(PIN_CLEAN_MOTOR, LOW);
    updateDisplay(255, 0, 0, "WEATHER ALERT");

  } else if (cleaningReq) {
    // PRIORITY 2: Self-cleaning — Blue
    posV = CLEAN_V;
    digitalWrite(PIN_CLEAN_MOTOR, HIGH);
    updateDisplay(0, 0, 255, "CLEANING...");

  } else if (trackingMode) {
    // PRIORITY 3: Dual-axis sun tracking — Green
    digitalWrite(PIN_CLEAN_MOTOR, LOW);
    trackSun();
    updateDisplay(0, 255, 0, "TRACKING");

  } else {
    // PRIORITY 4: Static / standby — White
    posH = STATIC_H;
    posV = STATIC_V;
    digitalWrite(PIN_CLEAN_MOTOR, LOW);
    updateDisplay(255, 255, 255, "STATIC MODE");
  }

  // 4. Write constrained angles to both servos
  horizServo.write(constrain(posH, SERVO_MIN, SERVO_MAX));
  vertServo.write(constrain(posV,  SERVO_MIN, SERVO_MAX));

  delay(100);
}

// Helper: Dual-Axis Sun Tracking Logic
void trackSun() {
  auto avgRead = [](int pin) -> int {
    return (analogRead(pin) + analogRead(pin) + analogRead(pin)) / 3;
  };

  int top   = avgRead(LDR_TOP);
  int bot   = avgRead(LDR_BOT);
  int left  = avgRead(LDR_LEFT);
  int right = avgRead(LDR_RIGHT);

  if (abs(top - bot) > TRACK_TOL) {
    posV += (top > bot) ? TRACK_STEP : -TRACK_STEP;
  }
  if (abs(left - right) > TRACK_TOL) {
    posH += (left > right) ? TRACK_STEP : -TRACK_STEP;
  }

  posH = constrain(posH, SERVO_MIN, SERVO_MAX);
  posV = constrain(posV,  SERVO_MIN, SERVO_MAX);

  Serial.print("T:"); Serial.print(top);
  Serial.print(" B:"); Serial.print(bot);
  Serial.print(" L:"); Serial.print(left);
  Serial.print(" R:"); Serial.print(right);
  Serial.print(" | H:"); Serial.print(posH);
  Serial.print(" V:"); Serial.println(posV);
}

// Helper: Live Telemetry and Status Indicators
void updateDisplay(uint8_t r, uint8_t g, uint8_t b, const char* mode) {
  for (int i = 0; i < 12; i++)
    ring.setPixelColor(i, ring.Color(r, g, b));
  ring.show();

  lcd.setCursor(0, 0);
  lcd.print("M:");
  lcd.print(mode);
  for (int col = 2 + (int)strlen(mode); col < 16; col++) lcd.print(' ');

  int cH = constrain(posH, SERVO_MIN, SERVO_MAX);
  int cV = constrain(posV,  SERVO_MIN, SERVO_MAX);
  lcd.setCursor(0, 1);
  lcd.print("H:");
  lcd.print(cH);
  lcd.print(" V:");
  lcd.print(cV);
  int used = 2 + (cH >= 100 ? 3 : cH >= 10 ? 2 : 1)
               + 3
               + (cV >= 100 ? 3 : cV >= 10 ? 2 : 1);
  for (int col = used; col < 16; col++) lcd.print(' ');
}
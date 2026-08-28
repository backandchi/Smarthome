/*==============================================================================
 *  Smart Home Hub - Arduino UNO
 *  ---------------------------------------------------------------------------
 *  Features:
 *   - Security: PIR motion sensor triggers Buzzer for 3 seconds
 *   - Climate : DHT11 reads temperature/humidity every 2s, shows on LCD,
 *               and turns the DC Motor (fan) ON when T > 28 C
 *   - Lighting: LDR detects darkness and switches the LED ON
 *   - Non-blocking: all timing handled with millis() (no delay())
 *  ---------------------------------------------------------------------------
 *  Author : Embedded Systems Engineer
 *  Board  : Arduino UNO (ATmega328P, 5V, 16 MHz)
 *  Language: Arduino C++ (Arduino IDE 1.8+ / Arduino IDE 2.x)
 *============================================================================*/

// -------- Required Libraries -----------------------------------------------
//  Install via Library Manager:
//   - DHT sensor library     (by Adafruit)
//   - LiquidCrystal_I2C     (by Frank de Brabander)
//   - Adafruit Unified Sensor (dependency of DHT)
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// -------- Pin Definitions --------------------------------------------------
// Sensors / Inputs
const uint8_t PIN_PIR      = 2;     // PIR motion sensor (digital IN)
const uint8_t PIN_DHT      = 3;     // DHT11 data pin
const uint8_t PIN_LDR      = A0;    // LDR voltage divider (analog IN)

// Outputs
const uint8_t PIN_BUZZER   = 8;     // Piezo buzzer
const uint8_t PIN_LED      = 9;     // Indicator / room LED (PWM capable)
const uint8_t PIN_MOTOR    = 10;    // DC motor driver (transistor / L298N EN)

// I2C LCD  (16x2, address 0x27 - change to 0x3F if your module differs)
const uint8_t LCD_ADDR     = 0x27;
const uint8_t LCD_COLS     = 16;
const uint8_t LCD_ROWS     = 2;

// -------- Constants --------------------------------------------------------
const uint8_t  DHT_TYPE    = DHT11;

// Thresholds
const float    TEMP_FAN_ON = 28.0;  // Fan turns ON above this temperature (C)
const int      LDR_DARK    = 500;   // LDR reading below this = "dark"
                                    // (0..1023 ADC, tune for your LDR + R)

// Timing (ms)
const unsigned long INTERVAL_DHT   = 2000;  // Read DHT11 every 2 s
const unsigned long BUZZER_DURATION = 3000; // 3 s alarm
const unsigned long DEBOUNCE_PIR   = 200;   // PIR software debounce

// -------- Globals ----------------------------------------------------------
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
DHT              dht(PIN_DHT, DHT_TYPE);

float    g_temperatureC   = 0.0f;
float    g_humidityPct    = 0.0f;
int      g_ldrValue       = 0;

bool     g_fanState       = false;
bool     g_ledState       = false;
bool     g_buzzerActive   = false;
bool     g_motionDetected = false;

unsigned long g_lastDhtReadMs     = 0;
unsigned long g_buzzerStartMs     = 0;
unsigned long g_lastMotionMs      = 0;

// ======================= Setup ============================================
void setup() {
  Serial.begin(9600);
  while (!Serial) { /* wait for native USB */ }

  // Configure pins
  pinMode(PIN_PIR,    INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED,    OUTPUT);
  pinMode(PIN_MOTOR,  OUTPUT);

  // Default-safe outputs
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED,    LOW);
  digitalWrite(PIN_MOTOR,  LOW);

  // I2C LCD init
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Home Hub");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(1000);  // one short blocking delay is fine at boot
  lcd.clear();

  // DHT sensor
  dht.begin();

  Serial.println(F("[BOOT] Smart Home Hub ready."));
}

// ======================= Main Loop (Non-Blocking) =========================
void loop() {
  unsigned long now = millis();

  // ---- 1) Security: PIR + Buzzer (every loop tick is fine here) -----------
  handleMotionAndBuzzer(now);

  // ---- 2) Climate: DHT11 read every INTERVAL_DHT ms ----------------------
  if (now - g_lastDhtReadMs >= INTERVAL_DHT) {
    g_lastDhtReadMs = now;
    readClimateAndControlFan();
  }

  // ---- 3) Lighting: LDR sample every loop (cheap ADC read) ---------------
  handleLighting(now);

  // ---- 4) LCD: refresh every loop; cheap text I/O ------------------------
  updateLcd();
}

// ======================= Security Subsystem ===============================
//  Reads the PIR pin, debounces it, and runs a 3-second non-blocking buzzer.
void handleMotionAndBuzzer(unsigned long now) {
  bool rawMotion = (digitalRead(PIN_PIR) == HIGH);

  // Edge-trigger with debounce: only start a new alarm on a fresh detection
  if (rawMotion && !g_motionDetected && (now - g_lastMotionMs > DEBOUNCE_PIR)) {
    g_lastMotionMs     = now;
    g_motionDetected   = true;
    g_buzzerActive     = true;
    g_buzzerStartMs    = now;
    digitalWrite(PIN_BUZZER, HIGH);
    Serial.println(F("[ALERT] Motion detected! Buzzer ON"));
  }
  if (!rawMotion && (now - g_lastMotionMs > DEBOUNCE_PIR)) {
    g_motionDetected = false;
  }

  // Turn buzzer off after BUZZER_DURATION without using delay()
  if (g_buzzerActive && (now - g_buzzerStartMs >= BUZZER_DURATION)) {
    g_buzzerActive = false;
    digitalWrite(PIN_BUZZER, LOW);
    Serial.println(F("[ALERT] Buzzer OFF (3s elapsed)"));
  }
}

// ======================= Climate Subsystem ================================
//  Reads DHT11, updates globals, and toggles fan based on temperature.
void readClimateAndControlFan() {
  float t = dht.readTemperature();   // Celsius
  float h = dht.readHumidity();      // %

  // DHT11 occasionally returns NaN; ignore bad samples
  if (isnan(t) || isnan(h)) {
    Serial.println(F("[DHT11] Sensor read FAILED, skipping."));
    return;
  }

  g_temperatureC = t;
  g_humidityPct  = h;

  // Fan control logic
  bool shouldFanRun = (g_temperatureC > TEMP_FAN_ON);
  if (shouldFanRun != g_fanState) {
    g_fanState = shouldFanRun;
    digitalWrite(PIN_MOTOR, g_fanState ? HIGH : LOW);
    Serial.print(F("[FAN] "));
    Serial.println(g_fanState ? F("ON (T > 28C)") : F("OFF"));
  }
}

// ======================= Lighting Subsystem ===============================
//  Reads the LDR voltage divider and switches the LED in dark conditions.
//  Add an optional small sampling interval if you want, but ADC is cheap.
void handleLighting(unsigned long /*now*/) {
  g_ldrValue = analogRead(PIN_LDR);

  bool isDark = (g_ldrValue < LDR_DARK);
  if (isDark != g_ledState) {
    g_ledState = isDark;
    digitalWrite(PIN_LED, g_ledState ? HIGH : LOW);
    Serial.print(F("[LDR] value="));
    Serial.print(g_ldrValue);
    Serial.println(g_ledState ? F(" -> LED ON (dark)") : F(" -> LED OFF (light)"));
  }
}

// ======================= LCD UI ===========================================
//  Renders current state. Kept short to avoid flicker.
void updateLcd() {
  // Line 1: Temperature & Humidity
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(g_temperatureC, 1);
  lcd.print((char)223);  // degree symbol
  lcd.print("C H:");
  lcd.print(g_humidityPct, 0);
  lcd.print("%   ");     // padding to clear leftover chars

  // Line 2: Status of subsystems
  lcd.setCursor(0, 1);
  lcd.print("F:");
  lcd.print(g_fanState ? "ON " : "OFF");
  lcd.print(" L:");
  lcd.print(g_ledState ? "ON " : "OFF");
  lcd.print(" M:");
  lcd.print(g_motionDetected ? "YES" : "NO ");
  lcd.print(" ");
}

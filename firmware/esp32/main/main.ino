#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <esp_wifi.h> // Required for Power Management
#include "time.h"

// --- NEW SAFE PIN MAPPING (ESP32-C3) ---
#define SDA_PIN 8     // Safe neutral pin
#define SCL_PIN 9     // Safe neutral pin
#define DHT_PIN 6      // Neutral GPIO
#define BUZZER_PIN 5   // Neutral GPIO
#define TOUCH_PIN 7    // Neutral GPIO

// --- WIFI CREDENTIALS ---
const char* ssid     = "S23";
const char* password = "64806480@@";

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
DHT dht(DHT_PIN, DHT11);

// --- GLOBALS ---
enum State { HOME, BLINK, HEART, EXCITED, TIMER_MODE, PETTING };
State currentState  = HOME;
State previousState = HOME;
int  clockFormat    = 3;
bool isTimerRunning = false;
int  timerOptions[] = {50, 25, 10, 5, 1};
int  currentOptionIndex = 1;
long remainingSeconds   = 25 * 60;
unsigned long stateStartTime = 0;
unsigned long lastTouchTime  = 0;
unsigned long lastTick       = 0;
unsigned long lastAutoBlink  = 0; 
int  tapCount       = 0;
bool lastTouchState = LOW;
const unsigned long LONG_PRESS_MS = 1000;
const unsigned long TAP_GAP_MS    = 450;
const unsigned long AUTO_BLINK_INTERVAL = 30000; 

void setManualTime() {
  struct tm tm;
  tm.tm_year = 2026 - 1900; 
  tm.tm_mon  = 2;
  tm.tm_mday = 30;
  tm.tm_hour = 12;
  tm.tm_min  = 0;
  tm.tm_sec  = 0;
  time_t t = mktime(&tm);
  struct timeval tv = { .tv_sec = t };
  settimeofday(&tv, NULL);
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Give Serial time to start
  
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Use the new safe pins for I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  dht.begin();

  // 1. BOOT ANIMATION
  for (int i = 0; i < 30; i++) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_logisoso20_tf);
    u8g2.drawStr(15, 40, "HELLO");
    u8g2.drawStr(30, 62, "ROMI");
    u8g2.setDrawColor(2);
    u8g2.drawBox(0, (i * 4) % 64, 128, 2);
    u8g2.setDrawColor(1);
    u8g2.sendBuffer();
    delay(40);
  }

  // 2. WiFi WITH STABILITY FIXES
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); 
  delay(200);
  
  // LIMIT POWER: Prevents "Core Dump" crashes on weak power supplies
  WiFi.setTxPower(WIFI_POWER_8_5dBm); 

  WiFi.begin(ssid, password);
  Serial.println("Connecting to S23...");

  bool forceSkip    = false;
  bool touchWasHigh = false; 
  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED) {
    // Check for skip via touch
    bool touchNow = digitalRead(TOUCH_PIN);
    if (touchNow && !touchWasHigh) {
      forceSkip = true;
      break;
    }
    touchWasHigh = touchNow;

    // Auto-retry if stuck for 15 seconds
    if (millis() - startAttempt > 15000) {
      WiFi.begin(ssid, password);
      startAttempt = millis();
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_haxrcorp4089_tr);
    u8g2.drawStr(25, 18, "CONNECTING...");
    u8g2.drawStr(30, 31, "SSID: S23");
    u8g2.drawStr(12, 57, "TAP TO SKIP SYNC");
    
    // Animation
    int pulseWidth = (int)(sin(millis() / 300.0) * 38) + 50;
    u8g2.drawRBox(64 - (pulseWidth / 2), 58, pulseWidth, 5, 2);
    u8g2.sendBuffer();
    delay(100);
  }

  // 3. NTP SYNC
  if (!forceSkip && WiFi.status() == WL_CONNECTED) {
    configTime(19800, 0, "pool.ntp.org", "time.google.com");
    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 20) { delay(500); retry++; }
    
    if (getLocalTime(&timeinfo)) {
      digitalWrite(BUZZER_PIN, HIGH); delay(50); digitalWrite(BUZZER_PIN, LOW);
    } else {
      setManualTime();
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  } else {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    setManualTime();
  }
  lastAutoBlink = millis(); 
}

void drawHome() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  u8g2.setDrawColor(1);
  u8g2.drawRBox(15, 2, 98, 13, 3);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_haxrcorp4089_tr);
  const char* days[]   = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  const char* months[] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
  char dateBuf[45];
  sprintf(dateBuf, "%s | %s %02d %d", days[timeinfo.tm_wday], months[timeinfo.tm_mon], timeinfo.tm_mday, timeinfo.tm_year + 1900);
  u8g2.drawStr(20, 12, dateBuf);
  u8g2.setDrawColor(1);

  int h = timeinfo.tm_hour;
  char timeBuf[25];
  if (clockFormat == 0) {
    bool isPM = (h >= 12); h = h % 12; if (h == 0) h = 12;
    u8g2.setFont(u8g2_font_logisoso20_tn);
    sprintf(timeBuf, "%02d:%02d:%02d", h, timeinfo.tm_min, timeinfo.tm_sec);
    u8g2.drawStr(5, 42, timeBuf);
    u8g2.setFont(u8g2_font_haxrcorp4089_tr); u8g2.drawStr(105, 42, isPM ? "PM" : "AM");
  } else if (clockFormat == 1) {
    bool isPM = (h >= 12); h = h % 12; if (h == 0) h = 12;
    u8g2.setFont(u8g2_font_logisoso28_tn);
    sprintf(timeBuf, "%02d:%02d", h, timeinfo.tm_min);
    u8g2.drawStr(20, 45, timeBuf);
    u8g2.setFont(u8g2_font_haxrcorp4089_tr); u8g2.drawStr(105, 45, isPM ? "PM" : "AM");
  } else if (clockFormat == 2) {
    u8g2.setFont(u8g2_font_logisoso20_tn);
    sprintf(timeBuf, "%02d:%02d:%02d", h, timeinfo.tm_min, timeinfo.tm_sec);
    u8g2.drawStr(15, 42, timeBuf);
  } else {
    u8g2.setFont(u8g2_font_logisoso28_tn);
    sprintf(timeBuf, "%02d:%02d", h, timeinfo.tm_min);
    u8g2.drawStr(25, 45, timeBuf);
  }

  u8g2.drawLine(0, 52, 128, 52);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(10, 63); u8g2.print("Tem:"); u8g2.print(dht.readTemperature(), 0); u8g2.print("C");
  u8g2.setCursor(75, 63); u8g2.print("Hum:"); u8g2.print((int)dht.readHumidity()); u8g2.print("%");
}

void loop() {
  u8g2.clearBuffer();
  unsigned long now = millis();
  bool touching = digitalRead(TOUCH_PIN);

  // --- AUTOMATIC BLINK every 30s ---
  if (currentState == HOME && (now - lastAutoBlink >= AUTO_BLINK_INTERVAL)) {
    currentState = BLINK;
    stateStartTime = now;
    lastAutoBlink = now;
  }

  // --- TOUCH INPUT ---
  if (touching && !lastTouchState) lastTouchTime = now;
  if (touching && (now - lastTouchTime > LONG_PRESS_MS)) {
    if (currentState != PETTING) previousState = currentState;
    currentState = PETTING;
  }
  if (!touching && lastTouchState) {
    if (currentState == PETTING) currentState = previousState;
    else tapCount++;
    lastTouchTime = now;
  }

  if (tapCount > 0 && (now - lastTouchTime > TAP_GAP_MS)) {
    if (currentState == TIMER_MODE) {
      if (tapCount == 1) {
        isTimerRunning = !isTimerRunning;
        digitalWrite(BUZZER_PIN, HIGH); delay(20); digitalWrite(BUZZER_PIN, LOW);
      } else if (tapCount == 2) {
        long total = (long)timerOptions[currentOptionIndex] * 60;
        if (isTimerRunning || remainingSeconds < total) { remainingSeconds = total; isTimerRunning = false; } 
        else { currentOptionIndex = (currentOptionIndex + 1) % 5; remainingSeconds = (long)timerOptions[currentOptionIndex] * 60; }
        digitalWrite(BUZZER_PIN, HIGH); delay(50); digitalWrite(BUZZER_PIN, LOW);
      } else if (tapCount == 3) {
        currentState = HOME; isTimerRunning = false;
        remainingSeconds = (long)timerOptions[currentOptionIndex] * 60;
      }
    } else {
      if (tapCount == 1)      { currentState = BLINK;   stateStartTime = now; lastAutoBlink = now; } 
      else if (tapCount == 2) { currentState = HEART;   stateStartTime = now; }
      else if (tapCount == 3) { currentState = EXCITED; stateStartTime = now; }
      else if (tapCount == 4)   currentState = TIMER_MODE;
      else if (tapCount == 5)   clockFormat  = (clockFormat + 1) % 4;
    }
    tapCount = 0;
  }
  lastTouchState = touching;

  // --- TIMER LOGIC ---
  if (isTimerRunning && (now - lastTick >= 1000)) {
    lastTick = now;
    if (remainingSeconds > 0) {
      remainingSeconds--;
      digitalWrite(BUZZER_PIN, HIGH); delayMicroseconds(200); digitalWrite(BUZZER_PIN, LOW);
    } else {
      isTimerRunning = false;
      for (int i = 0; i < 3; i++) { digitalWrite(BUZZER_PIN, HIGH); delay(150); digitalWrite(BUZZER_PIN, LOW); delay(100); }
      currentState = HOME;
    }
  }

  if (isTimerRunning && remainingSeconds <= 5 && (now % 500 < 20)) {
    digitalWrite(BUZZER_PIN, HIGH); delayMicroseconds(400); digitalWrite(BUZZER_PIN, LOW);
  }

  // --- DRAW STATES ---
  switch (currentState) {
    case HOME: drawHome(); break;
    case PETTING:
      u8g2.drawDisc(40, 35, 5, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
      u8g2.drawDisc(88, 35, 5, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
      u8g2.drawRBox(48, 50, 32, 8, 4);
      u8g2.setFont(u8g2_font_haxrcorp4089_tr);
      u8g2.drawStr(50, 15, "PURR~");
      if ((now / 100) % 2 == 0) { digitalWrite(BUZZER_PIN, HIGH); delayMicroseconds(100); digitalWrite(BUZZER_PIN, LOW); }
      break;
    case BLINK:
      u8g2.drawRBox(35, 32, 20, 5, 2);
      u8g2.drawRBox(73, 32, 20, 5, 2);
      if (now - stateStartTime > 600) currentState = HOME;
      break;
    case HEART:
      u8g2.drawDisc(54, 30, 7); u8g2.drawDisc(74, 30, 7);
      u8g2.drawTriangle(47, 32, 81, 32, 64, 58);
      if (now - stateStartTime > 2000) currentState = HOME;
      break;
    case EXCITED:
      u8g2.drawDisc(40, 30, 12); u8g2.drawDisc(88, 30, 12);
      u8g2.drawRBox(48, 52, 32, 6, 3);
      if (now - stateStartTime > 2500) currentState = HOME;
      break;
    case TIMER_MODE: {
      u8g2.setFont(u8g2_font_haxrcorp4089_tr); u8g2.drawStr(52, 10, "TIMER");
      u8g2.setFont(u8g2_font_logisoso28_tn);   u8g2.drawStr(60, 48, ":");
      int m = remainingSeconds / 60; char mBuf[4]; sprintf(mBuf, "%d", m);
      u8g2.drawStr(55 - u8g2.getStrWidth(mBuf), 48, mBuf);
      char sBuf[3]; sprintf(sBuf, "%02d", remainingSeconds % 60);
      u8g2.drawStr(72, 48, sBuf);
      break;
    }
  }
  u8g2.sendBuffer();
}
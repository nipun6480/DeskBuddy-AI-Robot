#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <DHT.h>

// --- PINS (ESP32) ---
#define SDA_PIN 8
#define SCL_PIN 9
#define DHT_PIN 2       
#define BUZZER_PIN 3
#define TOUCH_PIN 4

// --- SETTINGS ---
DHT dht(DHT_PIN, DHT11);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- ROBOT STATES ---
enum State { HOME, BLINK, HEART, EXCITED, TIMER_SETUP, TIMER_RUNNING };
State currentState = HOME;

// --- GESTURE VARIABLES ---
unsigned long lastTouchTime = 0;
int tapCount = 0;
bool lastTouchState = LOW;
unsigned long stateStartTime = 0;
const unsigned long LONG_PRESS_MS = 1500;
const unsigned long TAP_GAP_MS = 400; 

// --- TIMER VARIABLES ---
int timerMinutes = 25;
unsigned long timerStartMillis = 0;
unsigned long autoBlinkTimer = 0;

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  dht.begin();
  autoBlinkTimer = millis();
}

// --- DRAWING FUNCTIONS ---
void drawEyes(bool closed) {
  u8g2.setDrawColor(1);
  if (closed) {
    u8g2.drawBox(35, 32, 20, 3); u8g2.drawBox(73, 32, 20, 3);
  } else {
    u8g2.drawRBox(35, 22, 18, 24, 4); u8g2.drawRBox(75, 22, 18, 24, 4);
  }
}

void drawHeart() {
  // Manual pixel-art heart (No font needed!)
  u8g2.drawDisc(54, 30, 6);
  u8g2.drawDisc(74, 30, 6);
  u8g2.drawTriangle(48, 32, 80, 32, 64, 55);
}

void drawHome() {
  u8g2.setFont(u8g2_font_logisoso24_tf);
  u8g2.drawStr(25, 45, "18:45"); // Time placeholder
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(45, 60); u8g2.print(dht.readTemperature(), 1); u8g2.print("C");
}

void loop() {
  u8g2.clearBuffer();
  unsigned long now = millis();
  bool touching = digitalRead(TOUCH_PIN);

  // --- 1. GESTURE ENGINE ---
  if (touching && !lastTouchState) {
    lastTouchTime = now;
    digitalWrite(BUZZER_PIN, HIGH); delay(30); digitalWrite(BUZZER_PIN, LOW);
  }
  
  // LONG PRESS (Return Home)
  if (touching && (now - lastTouchTime > LONG_PRESS_MS)) {
    currentState = HOME;
    tapCount = 0;
    timerStartMillis = 0;
    digitalWrite(BUZZER_PIN, HIGH); delay(150); digitalWrite(BUZZER_PIN, LOW);
    while(digitalRead(TOUCH_PIN)); 
  }

  // MULTI-TAP ENGINE
  if (!touching && lastTouchState) {
    tapCount++;
    lastTouchTime = now;
  }
  
  if (tapCount > 0 && (now - lastTouchTime > TAP_GAP_MS)) {
    if (currentState == TIMER_SETUP || currentState == TIMER_RUNNING) {
        if (tapCount == 1) { timerStartMillis = now; currentState = TIMER_RUNNING; }
        if (tapCount == 2) { timerMinutes = (timerMinutes == 25) ? 5 : 25; }
    } else {
        if (tapCount == 1) currentState = BLINK;
        if (tapCount == 2) currentState = HEART;
        if (tapCount == 3) currentState = EXCITED;
        if (tapCount == 4) currentState = TIMER_SETUP;
    }
    tapCount = 0;
    stateStartTime = now;
  }
  lastTouchState = touching;

  // --- 2. STATE MACHINE ---
  switch (currentState) {
    case HOME:
      drawHome();
      // Auto blink every 30s
      if (now - autoBlinkTimer > 30000) {
        drawEyes(true);
        if (now - autoBlinkTimer > 30200) autoBlinkTimer = now;
      }
      break;

    case BLINK:
      drawEyes(true);
      if (now - stateStartTime > 600) currentState = HOME;
      break;

    case HEART:
      drawHeart();
      if (now - stateStartTime > 2000) currentState = HOME;
      break;

    case EXCITED:
      drawEyes(false);
      u8g2.drawRBox(48, 50, 32, 5, 2); // Mouth
      if (now - stateStartTime > 2000) currentState = HOME;
      break;

    case TIMER_SETUP:
      u8g2.setFont(u8g2_font_haxrcorp4089_tr);
      u8g2.drawStr(35, 15, "TIMER SET");
      u8g2.setFont(u8g2_font_logisoso20_tf);
      u8g2.setCursor(35, 50); u8g2.print(timerMinutes); u8g2.print(":00");
      break;

    case TIMER_RUNNING:
      unsigned long elapsed = (now - timerStartMillis) / 1000;
      int remaining = (timerMinutes * 60) - elapsed;
      if (remaining <= 0) {
          for(int i=0; i<3; i++) { digitalWrite(BUZZER_PIN, HIGH); delay(200); digitalWrite(BUZZER_PIN, LOW); delay(100); }
          currentState = HOME;
      } else {
          u8g2.setFont(u8g2_font_logisoso20_tf);
          u8g2.setCursor(30, 45);
          u8g2.print(remaining / 60); u8g2.print(":"); 
          if (remaining % 60 < 10) u8g2.print("0");
          u8g2.print(remaining % 60);
      }
      break;
  }

  u8g2.sendBuffer();
}
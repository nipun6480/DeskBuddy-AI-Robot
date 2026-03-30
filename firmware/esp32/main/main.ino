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

// --- DRIVERS ---
DHT dht(DHT_PIN, DHT11);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- STATES ---
enum State { HOME, BLINK, HEART, EXCITED, TIMER_MODE };
State currentState = HOME;

// --- GESTURE VARIABLES ---
unsigned long lastTouchTime = 0;
int tapCount = 0;
bool lastTouchState = LOW;
unsigned long stateStartTime = 0;
const unsigned long LONG_PRESS_MS = 1500;
const unsigned long TAP_GAP_MS = 400; 

// --- TIMER VARIABLES ---
int timerOptions[] = {50, 25, 10, 5};
int currentOptionIndex = 1; // Default to 25 mins
long remainingSeconds = 25 * 60;
bool isTimerRunning = false;
unsigned long lastTick = 0;
unsigned long autoBlinkTimer = 0;

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  dht.begin();
  autoBlinkTimer = millis();
}

void drawEyes(bool closed) {
  u8g2.setDrawColor(1);
  if (closed) {
    u8g2.drawBox(35, 32, 20, 3); u8g2.drawBox(73, 32, 20, 3);
  } else {
    u8g2.drawRBox(35, 22, 18, 24, 4); u8g2.drawRBox(75, 22, 18, 24, 4);
  }
}

void drawHome() {
  u8g2.setFont(u8g2_font_logisoso24_tf);
  u8g2.drawStr(25, 45, "18:20"); // Time placeholder
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(45, 60); u8g2.print(dht.readTemperature(), 1); u8g2.print("C");
}

void loop() {
  u8g2.clearBuffer();
  unsigned long now = millis();
  bool touching = digitalRead(TOUCH_PIN);

  // --- GESTURE ENGINE ---
  if (touching && !lastTouchState) {
    lastTouchTime = now;
    digitalWrite(BUZZER_PIN, HIGH); delay(30); digitalWrite(BUZZER_PIN, LOW);
  }
  
  if (touching && (now - lastTouchTime > LONG_PRESS_MS)) {
    currentState = HOME;
    isTimerRunning = false;
    digitalWrite(BUZZER_PIN, HIGH); delay(150); digitalWrite(BUZZER_PIN, LOW);
    while(digitalRead(TOUCH_PIN)); 
  }

  if (!touching && lastTouchState) {
    tapCount++;
    lastTouchTime = now;
  }
  
  if (tapCount > 0 && (now - lastTouchTime > TAP_GAP_MS)) {
    if (currentState == TIMER_MODE) {
        if (tapCount == 1) { 
          isTimerRunning = !isTimerRunning; 
        }
        else if (tapCount == 2) { 
          currentOptionIndex = (currentOptionIndex + 1) % 4;
          remainingSeconds = (long)timerOptions[currentOptionIndex] * 60;
          isTimerRunning = false;
        }
    } else {
        if (tapCount == 1) currentState = BLINK;
        else if (tapCount == 2) currentState = HEART;
        else if (tapCount == 3) currentState = EXCITED;
        else if (tapCount == 4) {
          currentState = TIMER_MODE;
          remainingSeconds = (long)timerOptions[currentOptionIndex] * 60;
          isTimerRunning = false;
        }
    }
    tapCount = 0;
    stateStartTime = now;
  }
  lastTouchState = touching;

  // --- TIMER COUNTDOWN ---
  if (isTimerRunning && (now - lastTick >= 1000)) {
    lastTick = now;
    if (remainingSeconds > 0) remainingSeconds--;
    else {
      isTimerRunning = false;
      for(int i=0; i<5; i++) { digitalWrite(BUZZER_PIN, HIGH); delay(200); digitalWrite(BUZZER_PIN, LOW); delay(100); }
      currentState = HOME;
    }
  }

  // --- RENDERING ---
  switch (currentState) {
    case HOME:
      drawHome();
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
      u8g2.drawDisc(54, 30, 6); u8g2.drawDisc(74, 30, 6); u8g2.drawTriangle(48, 32, 80, 32, 64, 55);
      if (now - stateStartTime > 2000) currentState = HOME;
      break;

    case EXCITED:
      drawEyes(false); u8g2.drawRBox(48, 50, 32, 5, 2);
      if (now - stateStartTime > 2000) currentState = HOME;
      break;

    case TIMER_MODE:
      u8g2.setFont(u8g2_font_haxrcorp4089_tr);
      u8g2.drawStr(35, 12, isTimerRunning ? "RUNNING..." : "PAUSED");
      
      // Fixed Icon Drawing
      if (isTimerRunning) {
          u8g2.drawBox(110, 5, 3, 8); 
          u8g2.drawBox(115, 5, 3, 8);
      } else {
          u8g2.drawTriangle(110, 5, 110, 13, 118, 9);
      }

      u8g2.setFont(u8g2_font_logisoso20_tf);
      u8g2.setCursor(30, 45);
      u8g2.print(remainingSeconds / 60); u8g2.print(":"); 
      if (remainingSeconds % 60 < 10) u8g2.print("0");
      u8g2.print(remainingSeconds % 60);
      
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.drawStr(10, 62, "1-Tap: Play/Pause  2-Tap: Time");
      break;
  }

  u8g2.sendBuffer();
}
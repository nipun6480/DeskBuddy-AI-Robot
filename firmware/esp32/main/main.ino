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
const unsigned long ULTRA_LONG_PRESS_MS = 3000; // 3 Seconds for Clock Toggle
const unsigned long TAP_GAP_MS = 400; 

// --- CLOCK & BATTERY VARS ---
bool showSeconds = false;
int batteryPercent = 85; // Placeholder value
unsigned long autoBlinkTimer = 0;

// --- TIMER VARIABLES ---
int timerOptions[] = {50, 25, 10, 5};
int currentOptionIndex = 1; 
long remainingSeconds = 25 * 60;
bool isTimerRunning = false;
unsigned long lastTick = 0;

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  u8g2.setContrast(255); 
  dht.begin();
  autoBlinkTimer = millis();
}

// --- NEW UI ELEMENTS ---

void drawBattery(int x, int y) {
  u8g2.drawFrame(x, y, 16, 8);      // Body
  u8g2.drawBox(x + 16, y + 2, 2, 4); // Tip
  int fill = map(batteryPercent, 0, 100, 0, 12);
  u8g2.drawBox(x + 2, y + 2, fill, 4); // Charge Level
}

void drawHome() {
  // 1. Header with Battery
  u8g2.setFont(u8g2_font_haxrcorp4089_tr);
  u8g2.drawStr(5, 10, "NIPUN-BOT");
  drawBattery(105, 3);

  // 2. Main Clock (Toggled)
  u8g2.setDrawColor(1);
  if (showSeconds) {
    u8g2.setFont(u8g2_font_logisoso20_tn);
    u8g2.drawStr(15, 42, "18:24:55"); // Placeholder
  } else {
    u8g2.setFont(u8g2_font_logisoso28_tn);
    u8g2.drawStr(25, 42, "18:24");
  }
  
  // 3. Status Bar
  u8g2.drawLine(0, 50, 128, 50);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(10, 62);
  u8g2.print("T:"); u8g2.print(dht.readTemperature(), 0); u8g2.print("C");
  u8g2.setCursor(75, 62);
  u8g2.print("H:"); u8g2.print((int)dht.readHumidity()); u8g2.print("%");
}

void loop() {
  u8g2.clearBuffer();
  unsigned long now = millis();
  bool touching = digitalRead(TOUCH_PIN);

  // --- GESTURE LOGIC ---
  if (touching && !lastTouchState) {
    lastTouchTime = now;
    digitalWrite(BUZZER_PIN, HIGH); delay(30); digitalWrite(BUZZER_PIN, LOW);
  }
  
  // Ultra Long Press (3 Seconds) - Toggle Seconds
  if (touching && (now - lastTouchTime > ULTRA_LONG_PRESS_MS) && currentState == HOME) {
    showSeconds = !showSeconds;
    digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);
    while(digitalRead(TOUCH_PIN)); // Wait for release
  }
  
  // Normal Long Press (1.5 Seconds) - Exit Timer
  else if (touching && (now - lastTouchTime > LONG_PRESS_MS)) {
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
        if (tapCount == 1) isTimerRunning = !isTimerRunning; 
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
        }
    }
    tapCount = 0;
  }
  lastTouchState = touching;

  // --- TIMER LOGIC ---
  if (isTimerRunning && (now - lastTick >= 1000)) {
    lastTick = now;
    if (remainingSeconds > 0) remainingSeconds--;
    else {
      isTimerRunning = false;
      for(int i=0; i<3; i++) { digitalWrite(BUZZER_PIN, HIGH); delay(200); digitalWrite(BUZZER_PIN, LOW); delay(100); }
      currentState = HOME;
    }
  }

  // --- RENDER ---
  switch (currentState) {
    case HOME:
      drawHome();
      if (now - autoBlinkTimer > 30000) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(35, 32, 20, 3); u8g2.drawBox(73, 32, 20, 3);
        if (now - autoBlinkTimer > 30250) autoBlinkTimer = now;
      }
      break;

    case BLINK:
      u8g2.drawBox(35, 32, 20, 3); u8g2.drawBox(73, 32, 20, 3);
      if (now - millis() > 600) currentState = HOME; // Logic for return
      break;

    case HEART:
      u8g2.drawDisc(54, 30, 6); u8g2.drawDisc(74, 30, 6); u8g2.drawTriangle(48, 32, 80, 32, 64, 55);
      if (now - stateStartTime > 2000) currentState = HOME;
      break;

    case TIMER_MODE:
      // Inverted Header with Play Status
      u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, 15);
      u8g2.setDrawColor(0); u8g2.setFont(u8g2_font_haxrcorp4089_tr);
      u8g2.drawStr(5, 11, isTimerRunning ? "TIMER: RUNNING" : "TIMER: PAUSED");
      
      u8g2.setDrawColor(1); u8g2.setFont(u8g2_font_logisoso28_tn);
      u8g2.setCursor(20, 48);
      u8g2.print(remainingSeconds/60); u8g2.print(":"); 
      if (remainingSeconds%60 < 10) u8g2.print("0"); u8g2.print(remainingSeconds%60);
      break;
  }
  u8g2.sendBuffer();
}
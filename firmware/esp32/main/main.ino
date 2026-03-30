#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <esp_wifi.h> 
#include "time.h"

// --- PIN MAPPING ---
#define SDA_PIN 8     
#define SCL_PIN 9     
#define DHT_PIN 6      
#define BUZZER_PIN 5   
#define TOUCH_PIN 7    

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
  tm.tm_year = 2024 - 1900; 
  tm.tm_mon  = 3;           
  tm.tm_mday = 14;          
  tm.tm_hour = 12;          
  tm.tm_min  = 0;
  tm.tm_sec  = 0;
  time_t t = mktime(&tm);
  struct timeval tv = { .tv_sec = t };
  settimeofday(&tv, NULL);
}

// --- UPDATED PORTION: BIGGER CUTE EYES ---
void drawMovingEye(int x, int y, bool closed, int pupilOffset) {
  if (closed) {
    // Extra thick closed lid for the massive eyes
    u8g2.drawRBox(x - 17, y - 3, 34, 6, 2); 
  } else {
    // MAX SQUARISH EYE: 34x34 pixels
    u8g2.drawRBox(x - 17, y - 17, 34, 34, 5); 
    
    u8g2.setDrawColor(0); // Erase color for pupils
    // Drawing two small vertical "pupil" slits for a cuter look
    u8g2.drawRBox(x - 6 + pupilOffset, y - 10, 4, 12, 1); 
    u8g2.drawRBox(x + 2 + pupilOffset, y - 10, 4, 12, 1);
    u8g2.setDrawColor(1); // Back to draw color
  }
}

//mouth for the robbot
void drawUltraMouth(int x, int y, bool surprised) {
  if (surprised) {
    // HUGE "O" MOUTH: Massive gasp
    u8g2.drawEllipse(x, y - 8, 10, 12); // Big outer circle
    u8g2.setDrawColor(0);
    u8g2.drawEllipse(x, y - 8, 7, 9);   // Hollow inner
    u8g2.setDrawColor(1);
  } else {
    // ULTRA WIDE SMILE: 30px wide
    // We use a rounded box with a hollow top to create a "U" shape
    u8g2.drawRBox(x - 15, y - 10, 30, 12, 4); 
    u8g2.setDrawColor(0);
    u8g2.drawBox(x - 15, y - 12, 30, 8); // Cut the top off to make it a "U"
    u8g2.setDrawColor(1);
    
    // Add small "dimple" dots at the corners
    u8g2.drawPixel(x - 16, y - 10);
    u8g2.drawPixel(x + 16, y - 10);
  }
}

//firecrracker
void drawBackgroundFirework(int cx, int cy, int radius) {
  for (int i = 0; i < 10; i++) {
    float angle = random(360) * (PI / 180.0);
    int dist = random(2, radius);
    int px = cx + cos(angle) * dist;
    int py = cy + sin(angle) * dist;
    
    // Draw a small cross '+' for each spark so it's visible
    u8g2.drawPixel(px, py);
    u8g2.drawPixel(px+1, py);
    u8g2.drawPixel(px-1, py);
    u8g2.drawPixel(px, py+1);
    u8g2.drawPixel(px, py-1);
  }
}

//heart
void drawMegaHeartBlink(int x, int y, bool closed, int pupilOffset) {
  if (closed) {
    // Happy "V" / Curved line for the blink
    // We use a thick arc (2 pixels thick)
    u8g2.drawCircle(x, y - 5, 18, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
    u8g2.drawCircle(x, y - 6, 18, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
  } else {
    // 1. THE MASSIVE HEART (Radius 16)
    int sz = 16;
    u8g2.drawDisc(x - (sz / 2), y, sz / 2); // Left lobe
    u8g2.drawDisc(x + (sz / 2), y, sz / 2); // Right lobe
    u8g2.drawTriangle(x - sz, y + 1, x + sz, y + 1, x, y + sz + 10); // Bottom point
    
    // 2. THE CENTERED EYELET (Pupil)
    u8g2.setDrawColor(0); // Erase color
    // Centered but moves left/right based on tap sequence
    u8g2.drawDisc(x + pupilOffset, y + 2, 4); 
    u8g2.setDrawColor(1); // Back to draw color
    
    // 3. THE SHIMMER (Tiny white glint)
    u8g2.drawPixel(x - 6, y - 4);
  }
}

//star eye
void drawCrazyEye(int x, int y, int radius) {
  // Big Squarish Eye
  u8g2.drawRBox(x - radius, y - radius, radius * 2, radius * 2, 5);
  
  u8g2.setDrawColor(0); // Erase for Star
  // Rotating-style cross (switches between '+' and 'x')
  if ((millis() / 50) % 2 == 0) {
    u8g2.drawBox(x - 8, y - 1, 16, 2); u8g2.drawBox(x - 1, y - 8, 2, 16);
  } else {
    u8g2.drawLine(x - 6, y - 6, x + 6, y + 6); u8g2.drawLine(x + 6, y - 6, x - 6, y + 6);
  }
  u8g2.setDrawColor(1);
}

//excited mouth
void drawCrazyMouth(int x, int y) {
  // Huge Triangular/D-shaped mouth
  u8g2.drawTriangle(x - 20, y - 10, x + 20, y - 10, x, y + 10);
  
  // Add a little "tongue" at the bottom
  u8g2.setDrawColor(0);
  u8g2.drawDisc(x, y + 6, 4);
  u8g2.setDrawColor(1);
}

// --- RABBIT PET MODE HELPERS ---
// --- FINAL INTEGRATED HELPERS (Delete any duplicates!) ---

void drawPettingEye(int x, int y) {
  // 1. BIG SQUARISH EYE (34x34) - Matches your single tap look
  u8g2.drawRBox(x - 17, y - 17, 34, 34, 6);
  
  // 2. THE "U" SHAPED EYEBALL (Centered Bliss)
  u8g2.setDrawColor(0); // Erase color to "cut" the U out of the black eye
  
  // We draw two half-circles to make a thick "U" line
  u8g2.drawCircle(x, y - 2, 8, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
  u8g2.drawCircle(x, y - 3, 8, U8G2_DRAW_LOWER_LEFT | U8G2_DRAW_LOWER_RIGHT);
  
  u8g2.setDrawColor(1); // Back to draw color
}

void drawRabbitEars(int x, int y, int offset) {
  // Positioned to stick out from the top boundary of the Mega Eyes
  u8g2.drawRBox(x - 8, y - 30 - offset, 16, 25, 5); 
  u8g2.setDrawColor(0);
  u8g2.drawRBox(x - 4, y - 26 - offset, 8, 15, 2); 
  u8g2.setDrawColor(1);
}

void drawRabbitMouth(int x, int y, bool twitchUp) {
  int yOff = twitchUp ? -2 : 0;
  // Nose Y-shape at the bottom boundary
  u8g2.drawLine(x, y + yOff, x - 3, y - 3 + yOff);
  u8g2.drawLine(x, y + yOff, x + 3, y - 3 + yOff);
  u8g2.drawLine(x, y + yOff, x, y + 4 + yOff);
  // Whiskers
  u8g2.drawLine(x - 10, y, x - 25, y - 5);
  u8g2.drawLine(x - 10, y + 2, x - 25, y + 5);
  u8g2.drawLine(x + 10, y, x + 25, y - 5);
  u8g2.drawLine(x + 10, y + 2, x + 25, y + 5);
}

void setup() {
  Serial.begin(115200);
  delay(500); 
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  dht.begin();

  // --- BOOT ANIMATION ---
  u8g2.setFont(u8g2_font_logisoso20_tf);
  const char* line1 = "HELLO";
  const char* line2 = "ROMI";
  for (int i = 1; i <= 5; i++) {
    u8g2.clearBuffer();
    char buf[10]; strncpy(buf, line1, i); buf[i] = '\0';
    u8g2.drawStr(25, 35, buf);
    u8g2.sendBuffer();
    delay(150);
  }
  for (int i = 1; i <= 4; i++) {
    u8g2.clearBuffer();
    u8g2.drawStr(25, 35, "HELLO");
    char buf[10]; strncpy(buf, line2, i); buf[i] = '\0';
    u8g2.drawStr(35, 60, buf);
    u8g2.sendBuffer();
    delay(150);
  }
  for (int j = 0; j < 15; j++) {
    u8g2.clearBuffer();
    u8g2.drawStr(25, 35, "HELLO");
    u8g2.drawStr(35, 60, "ROMI");
    int lineW = (sin(j / 3.0) * 30) + 40;
    u8g2.drawBox(64 - (lineW / 2), 62, lineW, 2);
    u8g2.sendBuffer();
    delay(50);
  }

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); 
  delay(200);
  WiFi.setTxPower(WIFI_POWER_8_5dBm); 
  WiFi.begin(ssid, password);
  bool forceSkip = false;
  bool touchWasHigh = false; 
  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED) {
    bool touchNow = digitalRead(TOUCH_PIN);
    if (touchNow && !touchWasHigh) { forceSkip = true; break; }
    touchWasHigh = touchNow;
    if (millis() - startAttempt > 10000) { WiFi.begin(ssid, password); startAttempt = millis(); }
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_haxrcorp4089_tr);
    u8g2.drawStr(25, 18, "CONNECTING...");
    int pulseWidth = (int)(sin(millis() / 300.0) * 38) + 50;
    u8g2.drawRBox(64 - (pulseWidth / 2), 58, pulseWidth, 5, 2);
    u8g2.sendBuffer();
    delay(100);
  }

  if (!forceSkip && WiFi.status() == WL_CONNECTED) {
    configTime(19800, 0, "pool.ntp.org", "time.google.com");
    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry < 20) { delay(500); retry++; }
    if (getLocalTime(&timeinfo)) { digitalWrite(BUZZER_PIN, HIGH); delay(50); digitalWrite(BUZZER_PIN, LOW); }
    else { setManualTime(); }
    WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
  } else { WiFi.disconnect(true); WiFi.mode(WIFI_OFF); setManualTime(); }

  tapCount = 0;               
  lastTouchState = HIGH;      
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

  if (currentState == HOME && (now - lastAutoBlink >= AUTO_BLINK_INTERVAL)) {
    currentState = BLINK; stateStartTime = now; lastAutoBlink = now;
  }

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
      if (tapCount == 1) { isTimerRunning = !isTimerRunning; digitalWrite(BUZZER_PIN, HIGH); delay(20); digitalWrite(BUZZER_PIN, LOW); }
      else if (tapCount == 2) { 
        long total = (long)timerOptions[currentOptionIndex] * 60;
        if (isTimerRunning || remainingSeconds < total) { remainingSeconds = total; isTimerRunning = false; } 
        else { currentOptionIndex = (currentOptionIndex + 1) % 5; remainingSeconds = (long)timerOptions[currentOptionIndex] * 60; }
        digitalWrite(BUZZER_PIN, HIGH); delay(50); digitalWrite(BUZZER_PIN, LOW);
      }
      else if (tapCount == 3) { currentState = HOME; isTimerRunning = false; remainingSeconds = (long)timerOptions[currentOptionIndex] * 60; }
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

  if (isTimerRunning && (now - lastTick >= 1000)) {
    lastTick = now;
    if (remainingSeconds > 0) { remainingSeconds--; digitalWrite(BUZZER_PIN, HIGH); delayMicroseconds(200); digitalWrite(BUZZER_PIN, LOW); } 
    else {
      isTimerRunning = false;
      for (int i = 0; i < 3; i++) { digitalWrite(BUZZER_PIN, HIGH); delay(150); digitalWrite(BUZZER_PIN, LOW); delay(100); }
      currentState = HOME;
    }
  }

  // --- DRAW STATES ---
  switch (currentState) {
    case HOME: drawHome(); break;
    case TIMER_MODE: {
      u8g2.setFont(u8g2_font_logisoso28_tn);
      char buf[10];
      sprintf(buf, "%02ld:%02ld", remainingSeconds / 60, remainingSeconds % 60);
      u8g2.drawStr(20, 45, buf);
      u8g2.setFont(u8g2_font_haxrcorp4089_tr);
      u8g2.drawStr(25, 12, isTimerRunning ? "RUNNING..." : "TIMER SET");
      break;
    }
    case PETTING: {
      unsigned long elapsed = now - stateStartTime;
      
      // Slow "Breathing" bounce for the rabbit face
      int bounce = abs(3 * sin(elapsed * 0.004)); 
      
      u8g2.clearBuffer(); 

      // 1. Draw Mega Rabbit Ears (Twitching)
      drawRabbitEars(30, 17 - bounce, bounce); 
      drawRabbitEars(98, 17 - bounce, bounce);

      // 2. MEGA EYES with "U" shaped bliss pupils
      drawPettingEye(30, 17 - bounce);
      drawPettingEye(98, 17 - bounce);

      // 3. Rabbit Sniffing Nose & Whiskers
      drawRabbitMouth(64, 56, (elapsed % 500 < 250));

      // 4. Purr Feedback
      if ((now / 100) % 2 == 0) {
        digitalWrite(BUZZER_PIN, HIGH); delayMicroseconds(80); digitalWrite(BUZZER_PIN, LOW);
      }
      break;
    }
    
   case BLINK: {
      unsigned long elapsed = now - stateStartTime;
      int pupilX = 0;
      bool eyeClosed = false;

      // Timing: Look Left -> Right -> Center -> Big Blink
      if (elapsed < 400)      pupilX = -8; 
      else if (elapsed < 800) pupilX = 8;  
      else if (elapsed < 1000) pupilX = 0; 
      else if (elapsed < 1350) eyeClosed = true;

      // EYE UP: Touches upper boundary
      drawMovingEye(30, 17, eyeClosed, pupilX);
      drawMovingEye(98, 17, eyeClosed, pupilX);

      // MEGA MOUTH: Wide and Thick at the bottom
      drawUltraMouth(64, 58, eyeClosed); 

      if (elapsed > 1500) currentState = HOME;
      break;
    }

   case HEART: {
      unsigned long elapsed = now - stateStartTime;
      int pupilX = 0;
      bool eyeClosed = false;

      // 1. TIMING LOGIC
      if (elapsed < 400)      pupilX = -6; 
      else if (elapsed < 800) pupilX = 6;  
      else if (elapsed < 1000) pupilX = 0; 
      else if (elapsed < 1400) eyeClosed = true;

      // 2. DRAW FIRECRACKERS FIRST (Background Layer)
      // We trigger a burst every 200ms to keep the screen busy
      if (elapsed % 200 < 50) {
        // We spread them across the whole screen: X(0-128), Y(0-64)
        drawBackgroundFirework(random(0, 128), random(0, 64), 15);
      }

      // 3. DRAW HEARTS ON TOP (Foreground Layer)
      drawMegaHeartBlink(32, 25, eyeClosed, pupilX);
      drawMegaHeartBlink(96, 25, eyeClosed, pupilX);

      // 4. OPTIONAL: Add Crackle Sound
      if (elapsed % 150 < 10) {
        digitalWrite(BUZZER_PIN, HIGH); delayMicroseconds(100); digitalWrite(BUZZER_PIN, LOW);
      }

      if (elapsed > 1600) currentState = HOME;
      break;
    }

    case EXCITED: {
      unsigned long elapsed = now - stateStartTime;
      
      // 1. CRAZY JITTER: Randomly shift everything by -2 to +2 pixels
      int shakeX = random(-2, 3); 
      int shakeY = random(-2, 3);
      
      // 2. OPEN "V" MOUTH (Huge Scream/Smile)
      // We shift it by the shake offsets
      drawCrazyMouth(64 + shakeX, 52 + shakeY);

      // 3. WOBBLY MEGA EYES
      // One eye might be slightly larger than the other for a "crazy" look
      int eyePulse = (elapsed % 200 < 100) ? 2 : 0; 
      drawCrazyEye(30 + shakeX, 22 + shakeY, 17 + eyePulse); // Left
      drawCrazyEye(98 + shakeX, 22 + shakeY, 17 - eyePulse); // Right

      // 4. BACKGROUND DEBRIS: Random "Speed Lines" popping everywhere
      for(int i=0; i<4; i++) {
        int rx = random(0, 128);
        int ry = random(0, 64);
        u8g2.drawLine(rx, ry, rx + random(-5, 6), ry + random(-5, 6));
      }

      // 5. BUZZER FEEDBACK: High-pitched "Excitement" beep
      if (elapsed % 100 < 20) {
        digitalWrite(BUZZER_PIN, HIGH); delayMicroseconds(100); digitalWrite(BUZZER_PIN, LOW);
      }

      if (elapsed > 2500) currentState = HOME;
      break;
    }
  }
  u8g2.sendBuffer();
}
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h> // CRITICAL: Added for Weather API
#include <esp_wifi.h> 
#include "time.h"

// --- PIN MAPPING ---
#define SDA_PIN 8     
#define SCL_PIN 9     
#define DHT_PIN 6      
#define BUZZER_PIN 5   
#define TOUCH_PIN 7    

// --- WIFI & API CREDENTIALS ---
const char* ssid     = "S23";
const char* password = "64806480@@";
const char* weatherKey = "ba371108128cd4ff95ec890107d5c6a1"; // OpenWeatherMap Key
const char* city       = "Dimapur,IN";

// --- OBJECT INSTANTIATION ---
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
DHT dht(DHT_PIN, DHT11);

// --- STATE MACHINE ---
enum State { HOME, BLINK, HEART, EXCITED, TIMER_MODE, PETTING, COMPLIMENT };
State currentState  = HOME;
State previousState = HOME;

// --- CLOCK & DISPLAY SETTINGS ---
int clockFormat = 3; // Default clock style
const unsigned long AUTO_BLINK_INTERVAL = 60000; // Auto-blink every 60s
unsigned long lastAutoBlink = 0; 
bool isAutoBlink = false; // True if Romi blinks on his own (Silent/No text)

// --- TIMER (POMODORO) SETTINGS ---
bool isTimerRunning     = false;
int  timerOptions[]     = {50, 25, 10, 5, 1}; // Minutes
int  currentOptionIndex = 1;                  // Default 25 mins
long remainingSeconds   = 25 * 60;
unsigned long lastTick  = 0;                  // For countdown timing

// --- TOUCH & GESTURE TRACKING ---
unsigned long stateStartTime = 0;             // Tracks how long Romi stays in a state
unsigned long lastTouchTime  = 0;             // For debouncing and long press
int  tapCount       = 0;                      // Tracks 1, 2, 3, 4, or 5 taps
bool lastTouchState = LOW;                    // Prevents multiple triggers
const unsigned long LONG_PRESS_MS = 1000;     // 1 second for Rabbit mode
const unsigned long TAP_GAP_MS    = 450;      // Time allowed between taps

// --- WEATHER DATA ---
String currentWeather = "Clear";              // Text description (Cloudy, Rain, etc.)
int currentTemp       = 0;                    // Temperature from API
unsigned long lastWeatherUpdate = 0;          // Track last sync
const unsigned long WEATHER_INTERVAL = 1800000; // Update every 30 mins

// --- PERSONALIZATION ---
int selectedCompliment = 0;                   // Index of the 100 compliments
void updateWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + String(city) + "&units=metric&appid=" + String(weatherKey);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      // Simple parsing (to save memory instead of using ArduinoJson)
      int tempIdx = payload.indexOf("\"temp\":") + 7;
      currentTemp = payload.substring(tempIdx, payload.indexOf(",", tempIdx)).toInt();
      int mainIdx = payload.indexOf("\"main\":\"") + 8;
      currentWeather = payload.substring(mainIdx, payload.indexOf("\"", mainIdx));
    }
    http.end();
  }
}

//wether icon
void drawWeatherIcon(int x, int y) {
  // Check if it's night time for the moon icon
  struct tm t;
  getLocalTime(&t);
  bool isNight = (t.tm_hour >= 18 || t.tm_hour < 6);

  if (currentWeather == "Rain" || currentWeather == "Drizzle") {
    u8g2.drawCircle(x, y-2, 3); u8g2.drawCircle(x+3, y-2, 3); // Small Cloud
    u8g2.drawLine(x, y+2, x-1, y+4); u8g2.drawLine(x+3, y+2, x+2, y+4); // Rain drops
  } 
  else if (isNight && (currentWeather == "Clear")) {
    u8g2.drawCircle(x, y, 4); // Moon base
    u8g2.setDrawColor(0);
    u8g2.drawCircle(x+2, y-2, 4); // Moon crescent cutout
    u8g2.setDrawColor(1);
  }
  else if (currentWeather == "Clouds") {
    u8g2.drawCircle(x-2, y, 3); u8g2.drawCircle(x+2, y, 4); u8g2.drawBox(x-2, y+1, 5, 2);
  }
  else { // Sunny
    u8g2.drawDisc(x, y, 3);
    for(int i=0; i<360; i+=90) { // Simple 4-point sun rays
      u8g2.drawLine(x, y, x+(cos(i)*6), y+(sin(i)*6));
    }
  }
}

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
//complement
const char* compliments[] = {
  "STAY BRIGHT", "CHASE DREAMS", "PURE TALENT", "BORN STAR", "ALWAYS KIND",
  "KEEP GLOWING", "SO SMART", "TRUE GENIUS", "SHINE ON", "VERY BRAVE",
  "STAY UNIQUE", "REALLY WISE", "TOP TIER", "SIMPLY BEST", "STAY STRONG",
  "SO CREATIVE", "HIGH ENERGY", "PURE HEART", "STAY BOLD", "VERY CALM",
  "KEEP RISING", "STAY FIERCE", "SO HUMBLE", "REALLY COOL", "STAY REAL",
  "KEEP GOING", "SO CAPABLE", "TRUE LEADER", "STAY RADIANT", "SO VIBRANT",
  "REALLY GIFTED", "KEEP WINNING", "STAY HAPPY", "SO POLITE", "REALLY FUN",
  "STAY CALM", "KEEP SMILING", "SO HELPFUL", "TRUE ICON", "STAY FRESH",
  "KEEP TRYING", "SO FOCUSED", "REALLY NEAT", "STAY ACTIVE", "KEEP FLYING",
  "SO HONEST", "TRUE GEM", "STAY SWEET", "KEEP WALKING", "SO GRACEFUL",
  "REALLY QUICK", "STAY STEADY", "KEEP ASKING", "SO CHEERFUL", "TRUE PRO",
  "STAY WARM", "KEEP SHINING", "SO LIVELY", "REALLY DEEP", "STAY TOUGH",
  "KEEP CARING", "SO PATIENT", "TRUE ORIGINAL", "STAY OPEN", "KEEP LEARNING",
  "SO GENTLE", "REALLY BRIGHT", "STAY LOVELY", "KEEP HELPING", "SO LOYAL",
  "REALLY FAIR", "STAY PROUD", "KEEP BELIEVING", "SO FRIENDLY", "TRUE WONDER",
  "STAY READY", "KEEP DREAMING", "SO KEEN", "REALLY SMART", "STAY ALERT",
  "KEEP WORKING", "SO SHARP", "REALLY FINE", "STAY VIGILANT", "KEEP SMILING",
  "SO RADIANT", "REALLY STUNNING", "STAY PEACEFUL", "KEEP GROWING", "SO MAGICAL",
  "REALLY SOLID", "STAY CLASSY", "KEEP BUILDING", "SO FEARLESS", "TRUE POWER",
  "STAY BRIGHT", "KEEP INSPIRING", "SO DRIVEN", "REALLY AWESOME", "STAY GOLD"
};
// Change the array size tracker

void drawSleepyEye(int x, int y, bool closed) {
  if (closed) {
    u8g2.drawRBox(x - 17, y + 5, 34, 4, 1); // Very thin closed line
  } else {
    u8g2.drawRBox(x - 17, y - 17, 34, 34, 5); // Main Eye
    u8g2.setDrawColor(0);
    u8g2.drawBox(x - 18, y - 18, 36, 18); // Solid black lid covering top half
    u8g2.drawDisc(x, y + 5, 4); // Tired pupil at the bottom
    u8g2.setDrawColor(1);
  }
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

  // --- TOP DATE BAR ---
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

  // --- MAIN CLOCK ---
  int h = timeinfo.tm_hour;
  char timeBuf[25];
  u8g2.setFont(u8g2_font_logisoso28_tn);
  bool isPM = (h >= 12); h = h % 12; if (h == 0) h = 12;
  sprintf(timeBuf, "%02d:%02d", h, timeinfo.tm_min);
  u8g2.drawStr(20, 45, timeBuf);
  u8g2.setFont(u8g2_font_haxrcorp4089_tr); 
  u8g2.drawStr(105, 45, isPM ? "PM" : "AM");

  // --- BOTTOM DASHBOARD ---
  u8g2.drawLine(0, 50, 128, 50);

  // LEFT SIDE: Local Sensor (e.g., 23.5°C)
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(4, 62); 
  // The ,1 here ensures one decimal place is shown
  u8g2.print(dht.readTemperature(), 1); 
  u8g2.print((char)176); u8g2.print("C"); 
  
  u8g2.setCursor(44, 62); 
  u8g2.print((int)dht.readHumidity()); u8g2.print("%");

  // --- RIGHT SIDE: WIFI/WEATHER ---
  if (WiFi.status() == WL_CONNECTED) {
    u8g2.drawLine(64, 50, 64, 64);
    u8g2.setFont(u8g2_font_haxrcorp4089_tr);
    
    drawWeatherIcon(74, 57); 

    // Outside Condition
    u8g2.setCursor(86, 57);
    u8g2.print(currentWeather); 

    // Outside Temp (e.g., 22.0°C)
    u8g2.setCursor(86, 64);
    u8g2.print((float)currentTemp, 1); 
    u8g2.print((char)176); u8g2.print("C");
  }
}

void loop() {
  u8g2.clearBuffer();
  unsigned long now = millis();
  bool touching = digitalRead(TOUCH_PIN);

  // Update weather every 30 mins
  if (now - lastWeatherUpdate > WEATHER_INTERVAL) {
    updateWeather();
    lastWeatherUpdate = now;
  }

  if (tapCount == 1 && (now - lastTouchTime > TAP_GAP_MS)) {
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    
    // Trigger Sleepy Mode between 7AM and 9AM
    if (timeinfo.tm_hour >= 7 && timeinfo.tm_hour < 9) {
       currentState = BLINK; // We will use a flag to show sleepy eyes in BLINK
    }
    // ... rest of your tap logic
  }

  // --- 1. AUTO BLINK TRIGGER (60 SEC, SILENT) ---
  if (currentState == HOME && (now - lastAutoBlink >= AUTO_BLINK_INTERVAL)) {
    isAutoBlink = true; 
    currentState = BLINK; 
    stateStartTime = now; 
    lastAutoBlink = now;
  }

  // --- 2. TOUCH HANDLING ---
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

  // --- 3. TAP SEQUENCE LOGIC ---
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
      // MANUAL TAPS - NOT AUTO BLINK
      isAutoBlink = false; 
      if (tapCount == 1)      { currentState = BLINK;   stateStartTime = now; lastAutoBlink = now; } 
      else if (tapCount == 2) { currentState = HEART;   stateStartTime = now; }
      else if (tapCount == 3) { currentState = EXCITED; stateStartTime = now; }
      else if (tapCount == 4)   currentState = TIMER_MODE;
      else if (tapCount == 5)   clockFormat  = (clockFormat + 1) % 4;
    }
    tapCount = 0;
  }
  lastTouchState = touching;

  // --- 4. TIMER TICK LOGIC ---
  if (isTimerRunning && (now - lastTick >= 1000)) {
    lastTick = now;
    if (remainingSeconds > 0) { remainingSeconds--; digitalWrite(BUZZER_PIN, HIGH); delayMicroseconds(200); digitalWrite(BUZZER_PIN, LOW); } 
    else {
      isTimerRunning = false;
      for (int i = 0; i < 3; i++) { digitalWrite(BUZZER_PIN, HIGH); delay(150); digitalWrite(BUZZER_PIN, LOW); delay(100); }
      currentState = HOME;
    }
  }

  // --- 5. STATE RENDERING ---
  switch (currentState) {
    case HOME: drawHome(); break;

    case TIMER_MODE:
      u8g2.setFont(u8g2_font_logisoso28_tn);
      char tBuf[10];
      sprintf(tBuf, "%02ld:%02ld", remainingSeconds / 60, remainingSeconds % 60);
      u8g2.drawStr(20, 45, tBuf);
      u8g2.setFont(u8g2_font_haxrcorp4089_tr);
      u8g2.drawStr(25, 12, isTimerRunning ? "RUNNING..." : "TIMER SET");
      break;

    case COMPLIMENT: {
      unsigned long elapsed = now - stateStartTime;
      u8g2.clearBuffer();

      String msg = String(compliments[selectedCompliment]);
      String words[3];
      int wordCount = 0;

      // --- WORD SPLITTER ---
      int spaceIdx = -1;
      int lastIdx = 0;
      while ((spaceIdx = msg.indexOf(' ', lastIdx)) != -1 && wordCount < 2) {
        words[wordCount++] = msg.substring(lastIdx, spaceIdx);
        lastIdx = spaceIdx + 1;
      }
      words[wordCount++] = msg.substring(lastIdx);

      // --- ULTRA-BOLD SCALING ---
      if (wordCount == 1) {
        // One Word: Huge and Thick
        u8g2.setFont(u8g2_font_logisoso20_tf); 
        int w = u8g2.getStrWidth(words[0].c_str());
        u8g2.drawStr(64 - (w / 2), 42, words[0].c_str());
      } 
      else if (wordCount == 2) {
        // Two Words: Very Bold
        u8g2.setFont(u8g2_font_helvB12_tf); 
        int w1 = u8g2.getStrWidth(words[0].c_str());
        int w2 = u8g2.getStrWidth(words[1].c_str());
        u8g2.drawStr(64 - (w1 / 2), 30, words[0].c_str());
        u8g2.drawStr(64 - (w2 / 2), 52, words[1].c_str());
      } 
      else {
        // Three Words: Tight Bold (Maximizing every pixel)
        u8g2.setFont(u8g2_font_helvB10_tf); // Slightly smaller than B12 but much thicker than 7x14
        int w1 = u8g2.getStrWidth(words[0].c_str());
        int w2 = u8g2.getStrWidth(words[1].c_str());
        int w3 = u8g2.getStrWidth(words[2].c_str());
        
        // Tight vertical spacing: 18, 38, 58
        u8g2.drawStr(64 - (w1 / 2), 18, words[0].c_str());
        u8g2.drawStr(64 - (w2 / 2), 38, words[1].c_str());
        u8g2.drawStr(64 - (w3 / 2), 58, words[2].c_str());
      }

      if (elapsed > 3000) currentState = HOME;
      break;
    }

    case PETTING: {
      unsigned long elapsed = now - stateStartTime;
      int bounce = abs(3 * sin(elapsed * 0.004)); 
      drawRabbitEars(30, 17 - bounce, bounce); 
      drawRabbitEars(98, 17 - bounce, bounce);
      drawPettingEye(30, 17 - bounce);
      drawPettingEye(98, 17 - bounce);
      drawRabbitMouth(64, 56, (elapsed % 500 < 250));
      if (elapsed % 150 < 40) {
        digitalWrite(BUZZER_PIN, HIGH); delayMicroseconds(120); digitalWrite(BUZZER_PIN, LOW);
      }
      break;
    }
    
    case BLINK: {
      unsigned long elapsed = now - stateStartTime;
      int pX = 0; bool eClosed = false;
      if (elapsed < 400) pX = -8; 
      else if (elapsed < 800) pX = 8;  
      else if (elapsed < 1000) pX = 0; 
      else if (elapsed < 1350) eClosed = true;

      drawMovingEye(30, 17, eClosed, pX);
      drawMovingEye(98, 17, eClosed, pX);
      drawUltraMouth(64, 58, eClosed); 

      if (elapsed >= 1000 && elapsed < 1020 && !isAutoBlink) {
        digitalWrite(BUZZER_PIN, HIGH); delay(2); digitalWrite(BUZZER_PIN, LOW);
      }

      if (elapsed > 1500) {
        if (isAutoBlink) {
          currentState = HOME;
        } else {
          selectedCompliment = random(0, 100); // Changed from 50 to 100
          currentState = COMPLIMENT;
          stateStartTime = millis();
        }
      }
      break;
    }

    case HEART: {
      unsigned long elapsed = now - stateStartTime;
      int pX = 0; bool eClosed = false;
      if (elapsed < 400) pX = -6; 
      else if (elapsed < 800) pX = 6;  
      else if (elapsed < 1000) pX = 0; 
      else if (elapsed < 1400) eClosed = true;

      if (elapsed % 200 < 50) drawBackgroundFirework(random(0, 128), random(0, 64), 15);
      drawMegaHeartBlink(32, 25, eClosed, pX);
      drawMegaHeartBlink(96, 25, eClosed, pX);

      if (elapsed % 800 < 20 || (elapsed % 800 > 100 && elapsed % 800 < 120)) {
        digitalWrite(BUZZER_PIN, HIGH); delayMicroseconds(150); digitalWrite(BUZZER_PIN, LOW);
      }
      if (elapsed > 1600) currentState = HOME;
      break;
    }

    case EXCITED: {
      unsigned long elapsed = now - stateStartTime;
      int sX = random(-2, 3); int sY = random(-2, 3);
      drawCrazyMouth(64 + sX, 52 + sY);
      int ePulse = (elapsed % 200 < 100) ? 2 : 0; 
      drawCrazyEye(30 + sX, 22 + sY, 17 + ePulse); 
      drawCrazyEye(98 + sX, 22 + sY, 17 - ePulse); 
      for(int i=0; i<4; i++) {
        int rx = random(0, 128); int ry = random(0, 64);
        u8g2.drawLine(rx, ry, rx + random(-5, 6), ry + random(-5, 6));
      }
      digitalWrite(BUZZER_PIN, HIGH); delayMicroseconds(random(50, 200)); digitalWrite(BUZZER_PIN, LOW);
      if (elapsed > 2500) currentState = HOME;
      break;
    }
  } 

  u8g2.sendBuffer(); 
}
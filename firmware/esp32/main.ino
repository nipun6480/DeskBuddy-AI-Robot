#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define TOUCH_PIN 4

void setup() {
  pinMode(TOUCH_PIN, INPUT);
  Serial.begin(115200);
}

void loop() {
  if(digitalRead(TOUCH_PIN)){
    Serial.println("Hello! You're awesome today!");
  }
}
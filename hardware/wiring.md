# Wiring Guide

## ESP32 Connections

| Component    | ESP32 Pin |
| ------------ | --------- |
| OLED SDA     | GPIO 21   |
| OLED SCL     | GPIO 22   |
| Touch Sensor | GPIO 4    |
| Buzzer       | GPIO 25   |
| VCC          | 3.3V      |
| GND          | GND       |

## Notes

* Use pull-up resistors if required.
* Ensure OLED voltage matches ESP32 (3.3V).

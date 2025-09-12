#include <Arduino.h>
#include <LiquidCrystal.h>

// LCD pins: RS=7, E=8, D4=9, D5=10, D6=11, D7=12
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
// LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

void setup() {
  lcd.begin(20, 4);
//  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hello World");
}

void loop() { /* idle */ }
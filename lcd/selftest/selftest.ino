/*
  GLYPH-LCD wiring self-test. Upload, open Serial Monitor at 9600 baud, follow the prompts.
  Only gentle checks: pins are read through the internal ~35k pull-ups (under 0.15 mA), the LCD
  is looked for on I2C, and the buzzer gets a few short beeps. Nothing here can short anything.

  Checks: LCD on I2C (0x27 / 0x3F) - transistor + 1k on D9 - MODE button on D2 - POWER switch on D3 -
  buzzer + SOUND switch (by ear).
*/
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C *lcd = 0;      // set once the screen is found; prompts show on it too

void show(const __FlashStringHelper *a, const __FlashStringHelper *b) {
  if (!lcd) return;
  lcd->clear(); lcd->print(a); lcd->setCursor(0, 1); lcd->print(b);
}

byte okCount = 0, total = 0;

void result(bool ok, const __FlashStringHelper *what, const __FlashStringHelper *fix) {
  total++; if (ok) okCount++;
  Serial.print(ok ? F("  [OK]   ") : F("  [FAIL] ")); Serial.println(what);
  if (!ok) { Serial.print(F("         -> ")); Serial.println(fix); }
}

bool waitFor(byte pin, byte level, unsigned long ms) {   // true if pin reaches level within ms
  unsigned long t = millis();
  while (millis() - t < ms) { if (digitalRead(pin) == level) return true; delay(5); }
  return false;
}

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println(F("\n=== GLYPH-LCD wiring self-test ==="));

  // 1. LCD: does anything answer on the I2C bus?
  Wire.begin();
  byte addr = 0;
  const byte addrs[] = {0x27, 0x3F};
  for (byte i = 0; i < 2; i++) { Wire.beginTransmission(addrs[i]); if (Wire.endTransmission() == 0) addr = addrs[i]; }
  result(addr, F("LCD answers on I2C (step 2)"), F("check LCD VCC -> + rail, GND -> - rail, SDA -> j15/h15 -> board SDA, SCL -> j17/h17 -> board SCL (not swapped)"));
  if (addr) {
    Serial.print(F("         LCD backpack at 0x")); Serial.println(addr, HEX);
    lcd = new LiquidCrystal_I2C(addr, 16, 2); lcd->init(); lcd->backlight();
    show(F("Self-test"), F("LCD OK!")); delay(1500);
  }

  // 2. D9 -> 1k -> transistor base -> emitter -> GND pulls a weak pull-up low (~0.7 V)
  pinMode(9, INPUT_PULLUP); delay(20);
  result(digitalRead(9) == LOW, F("D9 reaches the transistor through the 1k (step 3)"),
         F("check D9 wire -> a53, resistor b46-b53, transistor legs e45 e46 e47 flat side to you, a45 -> - rail"));

  // 3. MODE button: idle high, low when pressed
  pinMode(2, INPUT_PULLUP); delay(20);
  bool idle = digitalRead(2) == HIGH;
  result(idle, F("MODE button not stuck pressed (step 6)"), F("button turned the wrong way: rotate it 90 degrees"));
  if (idle) {
    Serial.println(F("  >> Press the MODE button now (60 s)..."));
    show(F("PRESS the MODE"), F("button now"));
    result(waitFor(2, LOW, 60000), F("MODE button press seen"), F("check h7 -> D2 and c9 -> - rail, button legs in e7 e9 g7 g9"));
  }

  // 4. POWER switch: must change state when flipped
  pinMode(3, INPUT_PULLUP); delay(20);
  byte p0 = digitalRead(3);
  Serial.print(F("  >> POWER switch reads ")); Serial.print(p0 ? F("ON") : F("OFF"));
  Serial.println(F(". Flip it now (60 s)..."));
  show(F("FLIP the POWER"), F("switch now"));
  result(waitFor(3, !p0, 60000), F("POWER switch changes D3 (step 5)"), F("check switch in c23 c24 c25, a23 -> - rail, b24 -> D3"));

  // 5. Buzzer: listen
  pinMode(9, OUTPUT);
  Serial.println(F("  >> Listen: 3 beeps now (SOUND switch ON)..."));
  show(F("LISTEN: 3 beeps"), F("(SOUND on)")); delay(1500);
  const int beeps[] = {440, 660, 880};
  for (byte i = 0; i < 3; i++) { tone(9, beeps[i], 250); delay(400); }
  Serial.println(F("  >> Now flip SOUND off: 3 more beeps should be silent..."));
  show(F("Flip SOUND OFF"), F("beeps in 8 s"));
  delay(8000);
  show(F("Silent? = good"), F(""));
  for (byte i = 0; i < 3; i++) { tone(9, beeps[i], 250); delay(400); }
  noTone(9); digitalWrite(9, LOW);
  Serial.println(F("  (heard the first beeps and not the second? buzzer + SOUND switch are right)"));

  Serial.print(F("\n=== ")); Serial.print(okCount); Serial.print(F(" / ")); Serial.print(total);
  Serial.println(F(" checks passed ==="));
  if (okCount == total) Serial.println(F("All good: upload glyph_lcd.ino and say hi."));
  if (okCount == total) show(F("ALL CHECKS PASS"), F("Tell Claude! :)")); else show(F("Some checks"), F("failed: see chat"));
}

void loop() {}

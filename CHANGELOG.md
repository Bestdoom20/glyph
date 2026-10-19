# Changelog

## Unreleased

- **GLYPH-LCD** (`lcd/glyph_lcd/glyph_lcd.ino`): GLYPH on a 1602 I2C LCD for Arduino Uno. 19 kaomoji faces as 10x8 CGRAM bitmaps. Text-driven speech with letter-to-mouth visemes and punctuation and mood cues. Idle director (blink, look, breathe, hum, doze). Serial command protocol. Diffed CGRAM uploads to stay inside the I2C frame budget. Tables in PROGMEM (40% flash / 53% RAM). Reference sheet: `lcd/ref/robot-face.jpg`.

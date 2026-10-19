# Changelog

## Unreleased

- **GLYPH-LCD voice**: passive-buzzer "mumble speech" on Timer1 hardware PWM (D9). Per-vowel pitch, phrase tune from punctuation, CAPS shout. Swell/hold/release envelope, breath wobble, legato glide. Mouth follows the envelope; mood is set per phrase. New commands `:mute` `:unmute` `:pitch`. Live `> text` caption on Serial.
- **GLYPH-LCD footprint**: replaced `String` with fixed buffers, and `tone()` with direct timer PWM. Moved names, demo, mood words and expression voice params into flash. Flash 16.7 KB -> 12.8 KB, RAM 1173 B -> 915 B.

- **GLYPH-LCD** (`lcd/glyph_lcd/glyph_lcd.ino`): GLYPH on a 1602 I2C LCD for Arduino Uno. 19 kaomoji faces as 10x8 CGRAM bitmaps. Text-driven speech with letter-to-mouth visemes and punctuation and mood cues. Idle director (blink, look, breathe, hum, doze). Serial command protocol. Diffed CGRAM uploads to stay inside the I2C frame budget. Tables in PROGMEM (40% flash / 53% RAM). Reference sheet: `lcd/ref/robot-face.jpg`.

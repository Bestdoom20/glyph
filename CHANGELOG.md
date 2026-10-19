# Changelog

## Unreleased

- **GLYPH-LCD fixes**: pace now follows the text's arrival rate (square-root curve, 30-160% letter time; whole-line bursts stay at 100%) and notes shorten with the pace. Breath noise fades with the envelope squared, which removes the hiss tail at the end of each blip.

- **GLYPH-LCD wavetable voice**: vowel waveforms (buzz + 3 formants) fitted to the reference voice's measured formants and 39 dB spectral tilt. Played through a 16 kHz interrupt into 62.5 kHz PWM, with breath noise. The vowel picks the timbre, not the pitch. `:voice` presets (guard/default/woman/child/old with wobble). Base pitch 130 Hz.
- **GLYPH-LCD streaming**: text is spoken as it arrives; pace follows the backlog for LLM token streams. Real input cuts off the demo.
- **GLYPH-LCD switches**: SOUND slide switch in series with the buzzer; POWER slide switch on D3 (screen off + power-down sleep, pin-change wake).

- **GLYPH-LCD voice**: passive-buzzer "mumble speech" on Timer1 hardware PWM (D9). Per-vowel pitch, phrase tune from punctuation, CAPS shout. Swell/hold/release envelope, breath wobble, legato glide. Mouth follows the envelope; mood is set per phrase. New commands `:mute` `:unmute` `:pitch`. Live `> text` caption on Serial.
- **GLYPH-LCD footprint**: replaced `String` with fixed buffers, and `tone()` with direct timer PWM. Moved names, demo, mood words and expression voice params into flash. Flash 16.7 KB -> 12.8 KB, RAM 1173 B -> 915 B.

- **GLYPH-LCD** (`lcd/glyph_lcd/glyph_lcd.ino`): GLYPH on a 1602 I2C LCD for Arduino Uno. 19 kaomoji faces as 10x8 CGRAM bitmaps. Text-driven speech with letter-to-mouth visemes and punctuation and mood cues. Idle director (blink, look, breathe, hum, doze). Serial command protocol. Diffed CGRAM uploads to stay inside the I2C frame budget. Tables in PROGMEM (40% flash / 53% RAM). Reference sheet: `lcd/ref/robot-face.jpg`.

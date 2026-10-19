# Changelog

## Unreleased

- **GLYPH-LCD buzzer voice**: second vowel set (`VOX` rows 6-11) for real passive buzzers. Additive, band-limited harmonics of ~700 Hz with vowel formants compressed and lifted around 2.7 kHz (43-70% of energy in the loud band for open vowels). Now the default; `:voice default` restores the low reference hum, which is quiet on a buzzer. Profile pitch carries over (x5.4, capped 1.2 kHz).
- **GLYPH-LCD self-test**: prompts on the LCD, 60 s per step. Verified on hardware.

- **GLYPH bridge** (`lcd/bridge/glyph_bridge.py`): terminal/Telegram chat -> Claude (`claude-opus-5-5`, streamed, low effort, server-side refusal fallback) -> face over serial or the Kit Lab relay. Sends `:typing`, `> you`, then reply tokens. `--echo` brain for wiring tests.

- **GLYPH-LCD profiles**: basic, kawaii (+ beauty mark, ROM middle dot), robot, grumpy, brows (new). Sleepy profile retired; its faces stay. 7 brow eyes and 8 brow faces. Each profile has its own curious face.
- **GLYPH-LCD near-zero CPU**: idle-sleep between frames (`nap`), unchanged frames skipped by a 22-bit face key, voice ISR on a 16-bit phase (846 -> 652 bytes, ~2x fewer cycles at 16 kHz). Behaviour unchanged.

- **GLYPH-LCD feelings**: a valence/arousal emotion engine with drift, praise memory, boredom and a per-word lexicon. Feeling drives the face per mode, with 250 ms hysteresis, plus blink rate, gaze, pitch and pace.
- **GLYPH-LCD listening**: `> text` = you talking. It interrupts itself, nods along per word, then reacts with empathy, hurt, its name, curiosity or a startle. `:typing` shows a thinking face and dots.
- **GLYPH-LCD animation**: squash transitions, pop into excited faces, micro-saccades, squint on shouted words, slow tired blinks, yawns, sighs.
- **GLYPH-LCD modes**: classic, kawaii, robot, grumpy, sleepy (`:mode`, MODE button on D2). 11 new faces, new eyes (U, ✧, ■, visor), mouths (zigzag, dot) and effects (sparkle, thinking dots, ?, sweat).
- **GLYPH-LCD footprint/speed**: own blocking TWI master and batched HD44780/PCF8574 driver (no Wire, no LiquidCrystal_I2C; CGRAM slot ~3.5 ms, was ~13 ms). Divide-free envelope ISR, xorshift `rnd()`, direct port I/O, one command table. 61% -> 53% flash, 49% -> 37% RAM.

- **GLYPH-LCD fixes**: pace now follows the text's arrival rate (square-root curve, 30-160% letter time; whole-line bursts stay at 100%) and notes shorten with the pace. Breath noise fades with the envelope squared, which removes the hiss tail at the end of each blip.

- **GLYPH-LCD wavetable voice**: vowel waveforms (buzz + 3 formants) fitted to the reference voice's measured formants and 39 dB spectral tilt. Played through a 16 kHz interrupt into 62.5 kHz PWM, with breath noise. The vowel picks the timbre, not the pitch. `:voice` presets (guard/default/woman/child/old with wobble). Base pitch 130 Hz.
- **GLYPH-LCD streaming**: text is spoken as it arrives; pace follows the backlog for LLM token streams. Real input cuts off the demo.
- **GLYPH-LCD switches**: SOUND slide switch in series with the buzzer; POWER slide switch on D3 (screen off + power-down sleep, pin-change wake).

- **GLYPH-LCD voice**: passive-buzzer "mumble speech" on Timer1 hardware PWM (D9). Per-vowel pitch, phrase tune from punctuation, CAPS shout. Swell/hold/release envelope, breath wobble, legato glide. Mouth follows the envelope; mood is set per phrase. New commands `:mute` `:unmute` `:pitch`. Live `> text` caption on Serial.
- **GLYPH-LCD footprint**: replaced `String` with fixed buffers, and `tone()` with direct timer PWM. Moved names, demo, mood words and expression voice params into flash. Flash 16.7 KB -> 12.8 KB, RAM 1173 B -> 915 B.

- **GLYPH-LCD** (`lcd/glyph_lcd/glyph_lcd.ino`): GLYPH on a 1602 I2C LCD for Arduino Uno. 19 kaomoji faces as 10x8 CGRAM bitmaps. Text-driven speech with letter-to-mouth visemes and punctuation and mood cues. Idle director (blink, look, breathe, hum, doze). Serial command protocol. Diffed CGRAM uploads to stay inside the I2C frame budget. Tables in PROGMEM (40% flash / 53% RAM). Reference sheet: `lcd/ref/robot-face.jpg`.

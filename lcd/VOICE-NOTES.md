# GLYPH-LCD voice: reference analysis

Reference: unitoftime's Mythfall devlog "I Gave My NPCs Voices Without Recording Anything" (youtube.com/watch?v=NaW0d7Tghw0). Mythfall's source is closed. The public part is his engine, [unitoftime/flow](https://github.com/unitoftime/flow) `audio/audio.go`.

## How the reference works (devlog + public engine code)

1. **Letter timer drives everything.** Text types one letter at a time. Each letter's delay depends on the letter: normal ~1/13 s, comma ~3x, `.` `?` `!` ~6x. The sound is triggered by the letter appearing, so voice and text can't drift apart.
2. **One blip per vowel group.** He plays a sound only when a run of vowels starts (≈1 per syllable). That cut the rate from ~30 to ~10 per second. The letter chooses the vowel sound, so the same line always sounds the same. Consonant puffs were tried and dropped.
3. **Blips are short pre-rendered vowel clips.** A buzz has 3 formant bands boosted per vowel, 6 vowels per character, ~0.5 MB for all voices. Playback is `PlayPitched(src, volume, pitch)`: the clip is resampled by `pitch` (pitch and speed move together, which is inaudible on short blips), and `Semitones(n) = 2^(n/12)`.
4. **Making it a voice:** a swell/hold/release envelope (not a drum hit), a little breath noise, and per-buzz imperfection.
5. **Character voices:** pitch plus "tube length" (a formant shift). A pitched-up man doesn't sound like a woman. Old voices get a slow ~2 Hz wobble, not more randomness.
6. **Pitch per blip** = character voice + position in phrase (drifts down, resets at commas) + small random nudge + wobble if old.
7. **Punctuation tune:** `?` raises the last word, `!` starts the phrase high, `...` fades, an ALL-CAPS word shouts.
8. **Notes overlap:** each blip is a bit longer than the gap to the next, so it reads as one stream of babble.

## Measured from the video's demo audio (Whisper-timed segments, STFT onsets, autocorrelation F0)

| segment | onset spacing (median) | F0 |
|---|---|---|
| full-speed line | 136 ms (p25 88) | median 218 Hz, 161-276 |
| repeated-name demo | 112 ms | median 132 Hz, 102-188 |
| three voices | 136 ms | median 121 Hz, 100-219 (one high voice ~270-310) |

The envelope peaks 28-42% into each note.

## Mapping to one passive buzzer

| reference | GLYPH-LCD |
|---|---|
| vowel = formant clip | vowel = pitch step (a 100%, e 112%, i 126%, y 119%, o 89%, u 84%), mouth shape per vowel |
| resampled clip | Timer1 PWM: pitch = ICR1, loudness = duty |
| envelope | ISR per wave period: swell 30%, hold, release 30% |
| breath + imperfection | ±1.6% random period wobble (xorshift8) |
| overlap | 160 ms notes, legato re-swell from 40%, ~40 ms glide |
| phrase tune, punctuation, caps | same rules: −3%/syllable drift (max −21%), ! +20%, ... −15% + shorter, CAPS +30%, ? slides up ~25% |
| character voice | `:pitch N` (base 220 Hz) + per-expression pitch/speed columns in `EX[]` |

A square wave on a ~2.7 kHz resonant buzzer can't do formants or "tube length". Those are the two parts of the reference this hardware can't copy.

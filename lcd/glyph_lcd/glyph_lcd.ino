/*
  GLYPH-LCD: the GLYPH living kaomoji face on a 1602 I2C LCD (HD44780 + PCF8574 backpack).
  https://github.com/Bestdoom20/glyph  ·  Arduino Uno / Freenove Control Board V4

  Wiring: LCD GND -> GND, VCC -> 5V, SDA -> A4, SCL -> A5.
          Passive buzzer via S8050 NPN: D9 -> 1k -> base, emitter -> GND,
          collector -> buzzer (-), buzzer (+) -> 5V.

  Voice = "mumble speech" (no TTS): text types out on a letter timer, and the buzzer chirps once
  per vowel group (~10/s). Each vowel has its own pitch step, so a line always sounds the same.
  Pitch drifts down through a phrase and resets at punctuation; ? rises, ! starts high, ... fades,
  CAPS shouts. The mouth opens on the vowel while it sounds, and each phrase's words set the face.

  Talk to it over Serial (9600 baud, newline ending):
    hello there!     speaks the text: mouth shape per letter, cues on ! ? and mood words
    :happy  :sad     sets the resting face (:list prints every name)
    :sleep  :wake    sleep / wake
    :mute  :unmute   buzzer off / on        :pitch 220   voice base pitch in Hz
  It plays a demo script until the first line arrives. Lines over 127 chars are cut.
  Every spoken line is echoed to Serial as "> text" while it types (a live caption).

  Screen: eyes are 2 cells (10x8 px) each on row 0, the mouth is 2 cells on row 1.
  CGRAM slots: 0-1 left eye, 2-3 right eye, 4-5 mouth, 6-7 effects.
  The text never moves: animation = rewriting CGRAM, and only slots that changed are sent.
  Budget: one slot costs ~13 ms on the 100 kHz PCF8574 bus, LCD crystals need ~100-200 ms
  to settle, so ~8-12 face changes per second is the useful ceiling.
*/
#include <LiquidCrystal_I2C.h>

#define BUZZER 9

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---- eyes: 10 px wide (bit 9 = leftmost pixel), 8 rows ----
#define DOT 0
#define RING 1
#define LINE 2
#define ARC 3
#define HALF 4
#define WEDGE 5
#define CHEVRON 6
#define SPIRAL 7
#define HEART 8
#define LID 9
#define SMALL 10
#define TEE 11
#define BROW 12
#define SLEEPY 13

const uint16_t EYES[][8] PROGMEM = {
  {0b0000000000, 0b0001111000, 0b0011111100, 0b0011111100, 0b0011111100, 0b0011111100, 0b0001111000, 0b0000000000},  // DOT  ●
  {0b0011111100, 0b0110000110, 0b1100000011, 0b1000000001, 0b1000000001, 0b1100000011, 0b0110000110, 0b0011111100},  // RING O
  {0b0000000000, 0b0000000000, 0b0000000000, 0b0000000000, 0b0111111110, 0b0000000000, 0b0000000000, 0b0000000000},  // LINE —
  {0b0000000000, 0b0000000000, 0b0001111000, 0b0110000110, 0b1100000011, 0b1000000001, 0b0000000000, 0b0000000000},  // ARC  ^
  {0b0000000000, 0b0000000000, 0b1111111111, 0b1000000001, 0b1100000011, 0b0110000110, 0b0011111100, 0b0000000000},  // HALF ◡
  {0b1100000000, 0b1111000000, 0b1111110000, 0b0111111100, 0b0111111110, 0b0011111100, 0b0001111000, 0b0000000000},  // WEDGE ◣
  {0b0110000000, 0b0011100000, 0b0000111000, 0b0000001110, 0b0000111000, 0b0011100000, 0b0110000000, 0b0000000000},  // CHEVRON >
  {0b0011111100, 0b0100000010, 0b1001111001, 0b1010000101, 0b1010110101, 0b1001100101, 0b0100000010, 0b0011111100},  // SPIRAL @
  {0b0000000000, 0b0110001100, 0b1111011110, 0b1111111110, 0b0111111100, 0b0011111000, 0b0001110000, 0b0000100000},  // HEART ♥
  {0b0000000000, 0b0000000000, 0b1111111111, 0b0111111110, 0b0111111110, 0b0011111100, 0b0000000000, 0b0000000000},  // LID  ⊖
  {0b0000000000, 0b0000000000, 0b0001111000, 0b0010000100, 0b0010000100, 0b0001111000, 0b0000000000, 0b0000000000},  // SMALL °
  {0b1111111111, 0b0010000100, 0b0010000100, 0b0010000100, 0b0010000100, 0b0010000100, 0b0000000000, 0b0000000000},  // TEE  T
  {0b1100000000, 0b0011100000, 0b0000011000, 0b0000000000, 0b0001110000, 0b0001110000, 0b0000000000, 0b0000000000},  // BROW ò
  {0b0000000000, 0b0000000000, 0b0000000000, 0b0000000000, 0b1111111111, 0b0111111110, 0b0011111100, 0b0000000000},  // SLEEPY
};

// ---- mouths: same 10x8 format ----
#define CAT 0
#define FLAT 1
#define SMILE 2
#define GRIN 3
#define OH 4
#define KISS 5
#define FROWN 6
#define TONGUE 7
#define RECT 8
#define SMIRK 9
#define WAVY 10
#define TRI 11
#define V_SMALL 12
#define V_OPEN 13
#define V_WIDE 14

const uint16_t MOUTHS[][8] PROGMEM = {
  {0b0000000000, 0b0100000010, 0b0100110010, 0b0011001100, 0b0000000000, 0b0000000000, 0b0000000000, 0b0000000000},  // CAT  ω
  {0b0000000000, 0b0000000000, 0b0011111100, 0b0000000000, 0b0000000000, 0b0000000000, 0b0000000000, 0b0000000000},  // FLAT —
  {0b0000000000, 0b0100000010, 0b0010000100, 0b0001111000, 0b0000000000, 0b0000000000, 0b0000000000, 0b0000000000},  // SMILE ‿
  {0b0000000000, 0b0111111110, 0b0100000010, 0b0010000100, 0b0001111000, 0b0000000000, 0b0000000000, 0b0000000000},  // GRIN ▽
  {0b0000000000, 0b0001111000, 0b0010000100, 0b0010000100, 0b0010000100, 0b0001111000, 0b0000000000, 0b0000000000},  // OH   o
  {0b0001110000, 0b0000001000, 0b0000110000, 0b0000001000, 0b0001110000, 0b0000000000, 0b0000000000, 0b0000000000},  // KISS 3
  {0b0000000000, 0b0001111000, 0b0010000100, 0b0100000010, 0b0000000000, 0b0000000000, 0b0000000000, 0b0000000000},  // FROWN ︵
  {0b0000000000, 0b0111111110, 0b0001001000, 0b0001001000, 0b0000110000, 0b0000000000, 0b0000000000, 0b0000000000},  // TONGUE
  {0b0000000000, 0b0011111100, 0b0010000100, 0b0011111100, 0b0000000000, 0b0000000000, 0b0000000000, 0b0000000000},  // RECT ▭
  {0b0000000000, 0b0000000000, 0b0000000110, 0b0000011000, 0b0001100000, 0b0110000000, 0b0000000000, 0b0000000000},  // SMIRK /
  {0b0000000000, 0b0000000000, 0b0010001000, 0b0101010100, 0b1000100010, 0b0000000000, 0b0000000000, 0b0000000000},  // WAVY ~
  {0b0000000000, 0b0000110000, 0b0001001000, 0b0010000100, 0b0011111100, 0b0000000000, 0b0000000000, 0b0000000000},  // TRI  △
  {0b0000000000, 0b0000000000, 0b0011111100, 0b0010000100, 0b0011111100, 0b0000000000, 0b0000000000, 0b0000000000},  // V_SMALL (consonants)
  {0b0000000000, 0b0011111100, 0b0100000010, 0b0100000010, 0b0100000010, 0b0011111100, 0b0000000000, 0b0000000000},  // V_OPEN  (a, h)
  {0b0000000000, 0b0000000000, 0b1111111111, 0b1000000001, 0b0111111110, 0b0000000000, 0b0000000000, 0b0000000000},  // V_WIDE  (e, i, y)
};

// ---- effects: 5x8, one cell ----
const byte FX_TEAR[8] PROGMEM  = {0b00100, 0b00100, 0b01110, 0b01110, 0b11111, 0b11111, 0b01110, 0b00000};
const byte FX_ANGER[8] PROGMEM = {0b00000, 0b01010, 0b11011, 0b00000, 0b11011, 0b01010, 0b00000, 0b00000};
const byte FX_BLUSH[8] PROGMEM = {0b00000, 0b00000, 0b00000, 0b10101, 0b01010, 0b10101, 0b00000, 0b00000};
const byte FX_HEART[8] PROGMEM = {0b00000, 0b01010, 0b11111, 0b11111, 0b01110, 0b00100, 0b00000, 0b00000};
const byte FX_NOTE[8] PROGMEM  = {0b00110, 0b00101, 0b00100, 0b00100, 0b01100, 0b11100, 0b11000, 0b00000};

#define FX_NONE 0
#define FX_TEARS 1
#define FX_MAD 2
#define FX_SHY 3
#define FX_LOVE 4
#define FX_ZZZ 5
#define FX_HUM 6

// ---- expressions: eyeL, eyeR, mirror right eye, mouth, effect, can blink, voice pitch %, letter time % ----
const char NAMES[] PROGMEM = "idle|happy|joy|love|sad|cry|shock|angry|annoyed|smug|excited|dizzy|sleepy|sleep|kiss|tongue|evil|cute|confused";
const byte EX[][8] PROGMEM = {
  {DOT, DOT, 0, CAT, FX_NONE, 1, 100, 100},          // idle      ● ω ●
  {ARC, ARC, 0, SMILE, FX_NONE, 0, 115, 100},        // happy     ^ ‿ ^
  {DOT, DOT, 0, GRIN, FX_NONE, 1, 115, 88},          // joy       ● ▽ ●
  {HEART, HEART, 0, SMILE, FX_LOVE, 0, 110, 100},    // love      ♥ ‿ ♥
  {LINE, LINE, 0, FROWN, FX_TEARS, 0, 80, 130},      // sad       — ︵ —
  {TEE, TEE, 0, OH, FX_TEARS, 0, 80, 130},           // cry       T o T
  {RING, RING, 0, OH, FX_NONE, 1, 125, 100},         // shock     O o O
  {BROW, BROW, 1, FROWN, FX_MAD, 1, 90, 88},         // angry     ò ︵ ó
  {LID, LID, 0, RECT, FX_MAD, 0, 90, 100},           // annoyed   ⊖ ▭ ⊖
  {LID, LID, 0, SMIRK, FX_NONE, 0, 95, 110},         // smug      ⊖ / ⊖
  {CHEVRON, CHEVRON, 1, CAT, FX_SHY, 0, 115, 88},    // excited   > ω <
  {SPIRAL, SPIRAL, 1, WAVY, FX_NONE, 0, 100, 120},   // dizzy     @ ~ @
  {SLEEPY, SLEEPY, 0, V_SMALL, FX_NONE, 0, 75, 150}, // sleepy
  {LINE, LINE, 0, FLAT, FX_ZZZ, 0, 75, 150},         // sleep     — — z
  {LINE, LINE, 0, KISS, FX_SHY, 0, 110, 100},        // kiss      — 3 —
  {HALF, HALF, 0, TONGUE, FX_NONE, 0, 105, 100},     // tongue    ◡ ᴗ ◡
  {WEDGE, WEDGE, 1, CAT, FX_NONE, 0, 85, 100},       // evil      ◣ ω ◢
  {SMALL, SMALL, 0, TONGUE, FX_NONE, 1, 120, 95},    // cute      ° ᴗ °
  {SMALL, RING, 0, FLAT, FX_NONE, 1, 105, 110},      // confused  o — O
};
#define E_IDLE 0
#define E_HAPPY 1
#define E_JOY 2
#define E_LOVE 3
#define E_SAD 4
#define E_SHOCK 6
#define E_ANGRY 7
#define E_SLEEPY 12
#define E_SLEEP 13
#define E_CONFUSED 18

const char DEMO[] PROGMEM = "hello! i am glyph, a face for this little screen.|:happy|i can talk, blink and look around.|"
                            ":love|i love tiny pixels.|:angry|do not unplug me!|:dizzy|whoa...|"
                            ":sad|sorry, that was a lot.|:excited|WOW! can you hear me? type to me in the serial box...|:idle";

// vowel -> pitch step (% of voice, a musical step apart) and mouth shape
const char VOWELS[] = "aeiouy";
const byte V_STEP[] = {100, 112, 126, 89, 84, 119};
const byte V_MOUTH[] = {V_OPEN, V_WIDE, V_WIDE, OH, OH, V_WIDE};
const int HUM[] = {392, 440, 349};

// ---- buzzer voice: Timer1 hardware PWM on D9 (OC1A) ----
// Pitch = ICR1, loudness = duty (OCR1A). One interrupt per wave period runs the envelope
// (swell 20 %, hold, release 30 %), a +-1.6 % random wobble per period (breath), a ~40 ms
// glide into each new pitch, and the question slide. A note that starts while one is still
// sounding re-swells from 40 % instead of silence: legato babble, not beeps. It runs off the CPU's main path, so LCD bus stalls never chop the sound.
// SIM-REPLACE-BEGIN (the lab swaps this block for the same model in WebAudio)
volatile uint16_t vTop, vTarget, vLen, vPos, vA, vR;
volatile int16_t vSlide;
volatile uint8_t vSeed = 1;

void voiceStop() { TIMSK1 = 0; TCCR1A = 0; TCCR1B = 0; digitalWrite(BUZZER, LOW); }

void voiceStart(long freq, int ms, bool rise) {
  uint16_t top = 2000000UL / freq;             // Timer1 at 2 MHz (prescaler 8)
  uint16_t len = freq * ms / 1000;              // note length in wave periods
  noInterrupts();
  bool legato = TIMSK1;                        // previous note still sounding
  vTarget = top;
  if (!legato) vTop = top;
  vLen = len; vA = len / 5; vR = len - len * 3 / 10;
  vPos = legato ? vA * 2 / 5 : 0;
  vSlide = rise ? -(int16_t)(top / 12) : 0;
  if (legato) { interrupts(); return; }
  ICR1 = top; OCR1A = 0; TCNT1 = 0;
  TCCR1A = _BV(COM1A1) | _BV(WGM11);           // fast PWM, TOP = ICR1, output on D9
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11);
  TIMSK1 = _BV(TOIE1);
  interrupts();
}

ISR(TIMER1_OVF_vect) {
  uint16_t p = vPos++;
  if (p >= vLen) { voiceStop(); return; }
  uint8_t env = p < vA ? (uint32_t)255 * p / vA : (p > vR ? (uint32_t)255 * (vLen - p) / (vLen - vR) : 255);
  if (vSlide && (p & 31) == 31) vTarget += vSlide;              // question: word slides up
  vTop += ((int16_t)(vTarget - vTop)) / 8;                     // glide
  vSeed ^= vSeed << 3; vSeed ^= vSeed >> 5; vSeed ^= vSeed << 1;   // xorshift8
  uint16_t top = vTop + (((int16_t)(int8_t)vSeed * (int16_t)(vTop >> 6)) >> 7);
  ICR1 = top;
  OCR1A = ((uint32_t)top * env) >> 9;                           // duty 0..50 %
}
// SIM-REPLACE-END

// ---- state ----
#define MAXLEN 128
char say[MAXLEN];               // line being spoken
byte sayLen = 0, sayPos = 0;
char inLine[MAXLEN];            // line arriving on Serial
byte inLen = 0;
int8_t expr = E_IDLE;           // resting face
int8_t mood = E_IDLE;           // face while speaking / just after
int8_t flash = E_IDLE;          // short reaction (! ?)
unsigned long moodUntil = 0, flashUntil = 0;
bool asleep = false, demo = true, muted = false;
int8_t lookX = 0;               // pupil shift in px, -2..2
byte blinkStage = 0;            // 0 open, 1 half, 2 closed, 3 half
byte bob = 0;                   // breathing: face drops 1 px
unsigned long nextLook = 0, blinkAt = 0, bobAt = 0, humUntil = 0, nextHum = 0, hummAt = 0;
byte hummNote = 3;
unsigned long lastInput = 0, nextChar = 0, demoAt = 0;
int8_t viseme = -1;             // mouth override (talking), -1 = expression mouth
int demoPos = 0;                // offset into DEMO
int voiceBase = 220;            // Hz: a low hum; the square's harmonics ring the buzzer's ~2.7 kHz resonance
byte phraseK = 0;               // syllables into the current phrase (pitch drifts down)
bool phraseHigh = false, fading = false;
long sylFreq = 0;
unsigned long sylStart = 0, sylEnd = 0;
int8_t sylViseme = -1;
int8_t fxShown = -1;
byte fxFrame = 0;
unsigned long fxAt = 0;

byte cg[8][8];                  // what each CGRAM slot holds right now
byte g[8];
uint16_t rows[8];

// ---- drawing ----
byte ex(int face, int k) { return pgm_read_byte(&EX[face][k]); }

uint16_t mirror10(uint16_t v) {
  uint16_t r = 0;
  for (byte i = 0; i < 10; i++) if (v & (1 << i)) r |= 1 << (9 - i);
  return r;
}

void upload(byte slot) {       // send g[] to a CGRAM slot only if it changed
  if (memcmp(cg[slot], g, 8) == 0) return;
  memcpy(cg[slot], g, 8);
  lcd.createChar(slot, g);
}

void sendPair(byte slot) {     // rows[] (10 px) -> two 5 px slots
  for (byte r = 0; r < 8; r++) g[r] = (rows[r] >> 5) & 31;
  upload(slot);
  for (byte r = 0; r < 8; r++) g[r] = rows[r] & 31;
  upload(slot + 1);
}

void buildEye(byte shape, bool mirror, bool canBlink) {
  for (byte r = 0; r < 8; r++) {
    int8_t src = r - bob;
    uint16_t v = src >= 0 ? pgm_read_word(&EYES[shape][src]) : 0;
    if (mirror) v = mirror10(v);
    if (lookX > 0) v = v >> lookX;
    if (lookX < 0) v = (v << -lookX) & 0x3FF;
    if (canBlink && (blinkStage == 1 || blinkStage == 3) && (r < 3 + bob || r > 5 + bob)) v = 0;
    if (canBlink && blinkStage == 2) v = r == 4 + bob ? 0b0111111110 : 0;
    rows[r] = v;
  }
}

void buildMouth(byte shape) {
  for (byte r = 0; r < 8; r++) {
    int8_t src = r - bob;
    rows[r] = src >= 0 ? pgm_read_word(&MOUTHS[shape][src]) : 0;
  }
}

void putFx(byte slot, byte col, byte row) { lcd.setCursor(col, row); lcd.write(slot); }

void clearFx() {               // every cell an effect may use
  const byte cells[][2] = {{1, 0}, {14, 0}, {15, 0}, {2, 1}, {3, 1}, {12, 1}, {13, 1}};
  for (byte i = 0; i < 7; i++) { lcd.setCursor(cells[i][0], cells[i][1]); lcd.write(' '); }
}

void loadFx(byte slot, const byte *src, byte drop) {   // drop = shift down for falling tears
  for (byte r = 0; r < 8; r++) g[r] = r >= drop ? pgm_read_byte(&src[r - drop]) : 0;
  upload(slot);
}

void drawFx(byte fx, unsigned long now) {
  bool changed = fx != fxShown;
  if (changed) { clearFx(); fxShown = fx; fxFrame = 0; fxAt = now; }
  bool tick = changed || now >= fxAt;
  if (fx == FX_TEARS) {
    if (tick) { loadFx(6, FX_TEAR, fxFrame % 3); fxFrame++; fxAt = now + 220; }
    if (changed) { putFx(6, 3, 1); putFx(6, 12, 1); }
  } else if (fx == FX_MAD) {
    if (changed) { loadFx(7, FX_ANGER, 0); putFx(7, 14, 0); }
  } else if (fx == FX_SHY) {
    if (changed) { loadFx(7, FX_BLUSH, 0); putFx(7, 2, 1); putFx(7, 13, 1); }
  } else if (fx == FX_LOVE) {
    if (tick) {                            // heart hops between the corners
      if (changed) loadFx(7, FX_HEART, 0);
      lcd.setCursor(fxFrame % 2 ? 14 : 1, 0); lcd.write(' ');
      putFx(7, fxFrame % 2 ? 1 : 14, 0);
      fxFrame++; fxAt = now + 700;
    }
  } else if (fx == FX_ZZZ) {
    if (tick) {
      lcd.setCursor(14, 0);
      lcd.print(fxFrame % 3 == 0 ? F("z ") : (fxFrame % 3 == 1 ? F("zZ") : F(" Z")));
      fxFrame++; fxAt = now + 650;
    }
  } else if (fx == FX_HUM) {
    if (changed) { loadFx(7, FX_NOTE, 0); putFx(7, 14, 0); }
  }
}

void draw(unsigned long now) {
  byte face = expr;
  if (now < moodUntil) face = mood;
  if (now < flashUntil) face = flash;
  if (asleep) face = E_SLEEP;
  bool humming = now < humUntil && !asleep;
  if (humming) face = E_HAPPY;

  bool blinkOk = ex(face, 5);
  buildEye(ex(face, 0), false, blinkOk);
  sendPair(0);
  buildEye(ex(face, 1), ex(face, 2), blinkOk);
  sendPair(2);
  buildMouth(viseme >= 0 ? viseme : ex(face, 3));
  sendPair(4);
  drawFx(humming ? FX_HUM : ex(face, 4), now);
}

void layout() {
  lcd.clear();
  lcd.setCursor(3, 0);  lcd.write(0); lcd.write(1);
  lcd.setCursor(11, 0); lcd.write(2); lcd.write(3);
  lcd.setCursor(7, 1);  lcd.write(4); lcd.write(5);
}

// ---- speech: letter timer + mumble voice ----
int8_t vowel(char c) {           // index in VOWELS, -1 = not a vowel
  c = tolower(c);
  for (byte i = 0; i < 6; i++) if (VOWELS[i] == c) return i;
  return -1;
}

bool isEnd(char c) { return c == ',' || c == '.' || c == '!' || c == '?'; }

int charTime(char c) {           // letter ~1/13 s, comma 3x, sentence end 6x
  int t = 75;
  if (c == ',') t = 225;
  if (c == '.' || c == '!' || c == '?') t = 450;
  return t * ex(mood, 7) / 100;
}

bool has(byte a, byte b, const char* w) {   // case-insensitive search for flash word w in say[a..b)
  byte n = strlen_P(w);
  for (byte i = a; i + n <= b; i++) {
    byte k = 0;
    while (k < n && tolower(say[i + k]) == pgm_read_byte(&w[k])) k++;
    if (k == n) return true;
  }
  return false;
}

int8_t moodOf(byte a, byte b) {
  if (has(a, b, PSTR("love")) || has(a, b, PSTR("<3"))) return E_LOVE;
  if (has(a, b, PSTR("sad")) || has(a, b, PSTR("sorry")) || has(a, b, PSTR("cry"))) return E_SAD;
  if (has(a, b, PSTR("unplug")) || has(a, b, PSTR("angry")) || has(a, b, PSTR("hate"))) return E_ANGRY;
  if (has(a, b, PSTR("haha")) || has(a, b, PSTR("lol")) || has(a, b, PSTR("yay")) || has(a, b, PSTR("hello"))) return E_JOY;
  if (has(a, b, PSTR("wow")) || has(a, b, PSTR("whoa"))) return E_SHOCK;
  if (has(a, b, PSTR("hmm"))) return E_CONFUSED;
  return -1;
}

void startPhrase(byte from) {    // read ahead to the phrase end: sets tune and face
  byte j = from;
  while (j < sayLen && !isEnd(say[j])) j++;
  phraseK = 0;
  phraseHigh = j < sayLen && say[j] == '!';
  fading = j + 1 < sayLen && say[j] == '.' && say[j + 1] == '.';
  int8_t m = moodOf(from, j);
  if (m >= 0) mood = m;
}

bool wordIs(byte pos, bool shout) {   // shout: whole word in CAPS; else: word ends with ?
  byte a = pos, b = pos;
  while (a > 0 && isalpha(say[a - 1])) a--;
  while (b < sayLen && isalpha(say[b])) b++;
  if (!shout) return b < sayLen && say[b] == '?';
  if (b - a < 2) return false;
  for (byte i = a; i < b; i++) if (!isupper(say[i])) return false;
  return true;
}

void note(long f, unsigned long now, int len, int8_t mouth, bool rise) {
  sylFreq = f; sylStart = now; sylEnd = now + len; sylViseme = mouth;
  if (!muted) voiceStart(f, len, rise);
}

void startSyllable(byte pos, int8_t v, unsigned long now) {
  long f = (long)voiceBase * ex(mood, 6) / 100;
  f = f * V_STEP[v] / 100;
  f = f * (100 - min(phraseK * 3, 21)) / 100;   // the tune drifts down through the phrase
  phraseK++;
  if (phraseHigh) f = f * 120 / 100;
  if (fading) f = f * 85 / 100;
  if (wordIs(pos, true)) f = f * 130 / 100;
  f = f + f * random(-3, 4) / 100;               // no two buzzes alike
  note(f, now, (fading ? 110 : 180) * ex(mood, 7) / 100, V_MOUTH[v], wordIs(pos, false));   // longer than the gap: babble
}

void tickVoice(unsigned long now) {   // mouth follows the note's envelope; the timer plays it
  if (sylFreq > 0 && now >= sylEnd) { sylFreq = 0; if (sayLen == 0) viseme = -1; }
  if (sylFreq > 0) {
    unsigned long t = now - sylStart, len = sylEnd - sylStart;
    bool partOpen = t < len / 5 || t > len * 7 / 10;          // swell / release
    viseme = sylViseme < 0 ? -1 : (partOpen ? V_SMALL : sylViseme);
  } else if (sayLen > 0) {
    viseme = sayPos > 0 && isalpha(say[sayPos - 1]) ? V_SMALL : FLAT;
  }
}

void speak() {                   // inLine -> say
  memcpy(say, inLine, inLen);
  sayLen = inLen;
  sayPos = 0;
  mood = expr;
  moodUntil = 0xFFFFFFFF;
  nextChar = millis();
  startPhrase(0);
  Serial.print(F("> "));
}

void tickSpeech(unsigned long now) {
  if (sayLen == 0 || now < nextChar) return;
  if (sayPos >= sayLen) {        // finished: close mouth, hold the mood a moment
    sayLen = 0;
    viseme = -1;
    moodUntil = now + 1500;
    Serial.println();
    return;
  }
  char c = say[sayPos];
  int8_t v = vowel(c);
  if (v >= 0 && (sayPos == 0 || vowel(say[sayPos - 1]) < 0)) startSyllable(sayPos, v, now);
  sayPos++;
  Serial.write(c);
  if (c == '!') { flash = E_SHOCK; flashUntil = now + 450; }
  if (c == '?') { flash = E_CONFUSED; flashUntil = now + 700; }
  if (isEnd(c)) startPhrase(sayPos);
  nextChar = now + charTime(c);
  lastInput = now;
}

void tickHum(unsigned long now) {   // three soft notes on the same voice engine
  if (asleep || sayLen > 0) { hummNote = 3; return; }
  if (hummNote < 3 && now >= hummAt) { note(HUM[hummNote], now, 260, -1, false); hummNote++; hummAt = now + 300; }
}

// ---- input ----
bool isCmd(const char* w, bool prefix) {   // inLine == ":" + flash word w (or starts with it)
  byte n = strlen_P(w);
  if (inLen - 1 < n || (!prefix && inLen - 1 != n)) return false;
  for (byte i = 0; i < n; i++) if (inLine[i + 1] != pgm_read_byte(&w[i])) return false;
  return true;
}

int8_t findFace(bool list) {     // match inLine against NAMES in flash, or print them all
  int8_t k = 0;
  byte j = 1;
  bool ok = true;
  int n = strlen_P(NAMES);
  for (int i = 0; i <= n; i++) {
    char c = i < n ? pgm_read_byte(&NAMES[i]) : '|';
    if (list) Serial.write(c == '|' ? '\n' : c);
    if (c == '|') { if (ok && j == inLen) return k; k++; j = 1; ok = true; }
    else { if (j >= inLen || inLine[j] != c) ok = false; j++; }
  }
  return -1;
}

void handleLine() {
  unsigned long now = millis();
  lastInput = now;
  if (asleep) { asleep = false; flash = E_SHOCK; flashUntil = now + 600; }
  if (inLine[0] != ':') { speak(); return; }
  if (isCmd(PSTR("list"), false)) { findFace(true); return; }
  if (isCmd(PSTR("sleep"), false)) { asleep = true; return; }
  if (isCmd(PSTR("wake"), false)) return;
  if (isCmd(PSTR("mute"), false)) { muted = true; voiceStop(); return; }
  if (isCmd(PSTR("unmute"), false)) { muted = false; return; }
  if (isCmd(PSTR("pitch "), true)) {
    int v = 0;
    for (byte i = 7; i < inLen; i++) if (isdigit(inLine[i])) v = v * 10 + (inLine[i] - '0');
    voiceBase = constrain(v, 80, 3000);
    return;
  }
  int8_t f = findFace(false);
  if (f < 0) { Serial.println(F("unknown face, try :list")); return; }
  expr = f;
  moodUntil = 0;
}

void readSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inLen > 0) { demo = false; handleLine(); }
      inLen = 0;
    } else if (inLen < MAXLEN) {
      inLine[inLen++] = c;
    }
  }
}

// ---- idle director: blink, look around, breathe, hum, doze off ----
void tickIdle(unsigned long now) {
  if (blinkStage == 0 && now >= blinkAt) { blinkStage = 1; blinkAt = now + 40; }
  else if (blinkStage > 0 && now >= blinkAt) {
    blinkStage = (blinkStage + 1) % 4;
    blinkAt = blinkStage == 0 ? now + random(2000, 6000) : now + (blinkStage == 2 ? 90 : 40);
    if (blinkStage == 0 && random(0, 100) < 15) blinkAt = now + 250;   // double blink
  }
  if (now >= nextLook) {
    byte r = random(0, 5);
    lookX = r == 0 ? -2 : (r == 1 ? 2 : (r == 2 ? -1 : 0));
    nextLook = now + (lookX == 0 ? random(2500, 6000) : random(600, 1800));
  }
  if (now >= bobAt) { bob = 1 - bob; bobAt = now + (bob ? 500 : random(2500, 4000)); }
  if (now >= nextHum && sayLen == 0) { humUntil = now + 1500; hummAt = now; hummNote = 0; nextHum = now + random(20000, 40000); }

  unsigned long idle = now - lastInput;
  if (!demo && !asleep && idle > 60000) asleep = true;
  if (!demo && !asleep && idle > 30000 && expr == E_IDLE) { mood = E_SLEEPY; moodUntil = now + 100; }
}

void tickDemo(unsigned long now) {   // feed the next '|' piece of DEMO as if it came from Serial
  if (!demo || sayLen > 0 || now < demoAt) return;
  int n = strlen_P(DEMO);
  inLen = 0;
  while (demoPos < n) {
    char c = pgm_read_byte(&DEMO[demoPos++]);
    if (c == '|') break;
    inLine[inLen++] = c;
  }
  if (demoPos >= n) demoPos = 0;
  handleLine();
  inLen = 0;
  demoAt = now + 900;
}

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER, OUTPUT);
  lcd.init();
  lcd.backlight();
  memset(cg, 255, sizeof(cg));   // force first upload
  layout();
  randomSeed(analogRead(A0));
  unsigned long now = millis();
  blinkAt = now + 1500;
  nextLook = now + 2000;
  nextHum = now + 25000;
  demoAt = now + 1200;
  lastInput = now;
  Serial.println(F("GLYPH-LCD ready. Type text to talk, :list for faces."));
}

void loop() {
  unsigned long now = millis();
  readSerial();
  tickDemo(now);
  tickSpeech(now);
  tickVoice(now);
  tickHum(now);
  tickIdle(now);
  draw(now);
  delay(15);
}

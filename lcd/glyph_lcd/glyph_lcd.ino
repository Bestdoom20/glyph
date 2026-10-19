/*
  GLYPH-LCD: the GLYPH living kaomoji face on a 1602 I2C LCD (HD44780 + PCF8574 backpack).
  https://github.com/Bestdoom20/glyph  ·  Arduino Uno / Freenove Control Board V4

  Wiring: LCD GND -> GND, VCC -> 5V, SDA -> A4, SCL -> A5.

  Talk to it over Serial (9600 baud, newline ending):
    hello there!     speaks the text: mouth shape per letter, cues on ! ? and mood words
    :happy  :sad     sets the resting face (:list prints every name)
    :sleep  :wake    sleep / wake
  It plays a demo script until the first line arrives.

  Screen: eyes are 2 cells (10x8 px) each on row 0, the mouth is 2 cells on row 1.
  CGRAM slots: 0-1 left eye, 2-3 right eye, 4-5 mouth, 6-7 effects.
  The text never moves: animation = rewriting CGRAM, and only slots that changed are sent.
  Budget: one slot costs ~13 ms on the 100 kHz PCF8574 bus, LCD crystals need ~100-200 ms
  to settle, so ~8-12 face changes per second is the useful ceiling.
*/
#include <LiquidCrystal_I2C.h>

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

// ---- expressions: eyeL, eyeR, mirror right eye, mouth, effect, can blink ----
const char* NAMES[] = {"idle", "happy", "joy", "love", "sad", "cry", "shock", "angry", "annoyed", "smug",
                       "excited", "dizzy", "sleepy", "sleep", "kiss", "tongue", "evil", "cute", "confused"};
const byte EX[][6] PROGMEM = {
  {DOT, DOT, 0, CAT, FX_NONE, 1},          // idle      ● ω ●
  {ARC, ARC, 0, SMILE, FX_NONE, 0},        // happy     ^ ‿ ^
  {DOT, DOT, 0, GRIN, FX_NONE, 1},         // joy       ● ▽ ●
  {HEART, HEART, 0, SMILE, FX_LOVE, 0},    // love      ♥ ‿ ♥
  {LINE, LINE, 0, FROWN, FX_TEARS, 0},     // sad       — ︵ —
  {TEE, TEE, 0, OH, FX_TEARS, 0},          // cry       T o T
  {RING, RING, 0, OH, FX_NONE, 1},         // shock     O o O
  {BROW, BROW, 1, FROWN, FX_MAD, 1},       // angry     ò ︵ ó
  {LID, LID, 0, RECT, FX_MAD, 0},          // annoyed   ⊖ ▭ ⊖
  {LID, LID, 0, SMIRK, FX_NONE, 0},        // smug      ⊖ / ⊖
  {CHEVRON, CHEVRON, 1, CAT, FX_SHY, 0},   // excited   > ω <
  {SPIRAL, SPIRAL, 1, WAVY, FX_NONE, 0},   // dizzy     @ ~ @
  {SLEEPY, SLEEPY, 0, V_SMALL, FX_NONE, 0},// sleepy
  {LINE, LINE, 0, FLAT, FX_ZZZ, 0},        // sleep     — — z
  {LINE, LINE, 0, KISS, FX_SHY, 0},        // kiss      — 3 —
  {HALF, HALF, 0, TONGUE, FX_NONE, 0},     // tongue    ◡ ᴗ ◡
  {WEDGE, WEDGE, 1, CAT, FX_NONE, 0},      // evil      ◣ ω ◢
  {SMALL, SMALL, 0, TONGUE, FX_NONE, 1},   // cute      ° ᴗ °
  {SMALL, RING, 0, FLAT, FX_NONE, 1},      // confused  o — O
};
#define N_EX 19
#define E_IDLE 0
#define E_JOY 2
#define E_LOVE 3
#define E_SAD 4
#define E_SHOCK 6
#define E_ANGRY 7
#define E_SLEEPY 12
#define E_SLEEP 13
#define E_CONFUSED 18

const char* DEMO[] = {"hello! i am glyph, a face for this little screen.", ":happy", "i can talk, blink and look around.",
                      ":love", "i love tiny pixels.", ":angry", "do not unplug me!", ":dizzy", "whoa...",
                      ":sad", "sorry, that was a lot.", ":excited", "wow! type to me in the serial box!", ":idle"};
#define N_DEMO 14

// ---- state ----
int expr = E_IDLE;              // resting face
int mood = E_IDLE;              // face while speaking / just after
unsigned long moodUntil = 0;
int flash = E_IDLE;             // short reaction (! ?)
unsigned long flashUntil = 0;
bool asleep = false;
int lookX = 0;                  // pupil shift in px, -2..2
unsigned long nextLook = 0;
int blinkStage = 0;             // 0 open, 1 half, 2 closed, 3 half
unsigned long blinkAt = 0;
int bob = 0;                    // breathing: face drops 1 px
unsigned long bobAt = 0;
unsigned long humUntil = 0, nextHum = 0;
unsigned long lastInput = 0;
String say = "";
int sayPos = 0;
unsigned long nextChar = 0;
int viseme = -1;                // mouth while talking, -1 = expression mouth
bool demo = true;
int demoPos = 0;
unsigned long demoAt = 0;
String inLine = "";
int fxShown = -1;
unsigned long fxAt = 0;
int fxFrame = 0;

byte cg[8][8];                  // what each CGRAM slot holds right now
byte g[8];
uint16_t rows[8];

// ---- drawing ----
byte ex(int face, int k) { return pgm_read_byte(&EX[face][k]); }

uint16_t mirror10(uint16_t v) {
  uint16_t r = 0;
  for (int i = 0; i < 10; i++) if (v & (1 << i)) r |= 1 << (9 - i);
  return r;
}

void upload(int slot) {        // send g[] to a CGRAM slot only if it changed
  bool same = true;
  for (int r = 0; r < 8; r++) if (cg[slot][r] != g[r]) same = false;
  if (same) return;
  for (int r = 0; r < 8; r++) cg[slot][r] = g[r];
  lcd.createChar(slot, g);
}

void sendPair(int slot) {      // rows[] (10 px) -> two 5 px slots
  for (int r = 0; r < 8; r++) g[r] = (rows[r] >> 5) & 31;
  upload(slot);
  for (int r = 0; r < 8; r++) g[r] = rows[r] & 31;
  upload(slot + 1);
}

void buildEye(int shape, bool mirror, bool canBlink) {
  for (int r = 0; r < 8; r++) {
    int src = r - bob;
    uint16_t v = src >= 0 ? pgm_read_word(&EYES[shape][src]) : 0;
    if (mirror) v = mirror10(v);
    if (lookX > 0) v = v >> lookX;
    if (lookX < 0) v = (v << -lookX) & 0x3FF;
    if (canBlink && (blinkStage == 1 || blinkStage == 3) && (r < 3 + bob || r > 5 + bob)) v = 0;
    if (canBlink && blinkStage == 2) v = r == 4 + bob ? 0b0111111110 : 0;
    rows[r] = v;
  }
}

void buildMouth(int shape) {
  for (int r = 0; r < 8; r++) {
    int src = r - bob;
    rows[r] = src >= 0 ? pgm_read_word(&MOUTHS[shape][src]) : 0;
  }
}

void putFx(int slot, int col, int row) { lcd.setCursor(col, row); lcd.write(byte(slot)); }

void clearFx() {               // every cell an effect may use
  const byte cells[][2] = {{1, 0}, {14, 0}, {15, 0}, {2, 1}, {3, 1}, {12, 1}, {13, 1}};
  for (int i = 0; i < 7; i++) { lcd.setCursor(cells[i][0], cells[i][1]); lcd.print(" "); }
}

void loadFx(int slot, const byte *src, int drop) {   // drop = shift down for falling tears
  for (int r = 0; r < 8; r++) g[r] = r - drop >= 0 ? pgm_read_byte(&src[r - drop]) : 0;
  upload(slot);
}

void drawFx(int fx, unsigned long now) {
  bool changed = fx != fxShown;
  if (changed) { clearFx(); fxShown = fx; fxFrame = 0; fxAt = now; }
  if (fx == FX_TEARS) {
    if (changed || now >= fxAt) { loadFx(6, FX_TEAR, fxFrame % 3); fxFrame++; fxAt = now + 220; }
    if (changed) { putFx(6, 3, 1); putFx(6, 12, 1); }
  } else if (fx == FX_MAD) {
    if (changed) { loadFx(7, FX_ANGER, 0); putFx(7, 14, 0); }
  } else if (fx == FX_SHY) {
    if (changed) { loadFx(7, FX_BLUSH, 0); putFx(7, 2, 1); putFx(7, 13, 1); }
  } else if (fx == FX_LOVE) {
    if (changed || now >= fxAt) {          // heart hops between the corners
      if (changed) loadFx(7, FX_HEART, 0);
      lcd.setCursor(fxFrame % 2 ? 14 : 1, 0); lcd.print(" ");
      putFx(7, fxFrame % 2 ? 1 : 14, 0);
      fxFrame++; fxAt = now + 700;
    }
  } else if (fx == FX_ZZZ) {
    if (changed || now >= fxAt) {
      lcd.setCursor(14, 0);
      lcd.print(fxFrame % 3 == 0 ? "z " : (fxFrame % 3 == 1 ? "zZ" : " Z"));
      fxFrame++; fxAt = now + 650;
    }
  } else if (fx == FX_HUM) {
    if (changed) { loadFx(7, FX_NOTE, 0); putFx(7, 14, 0); }
  }
}

void draw(unsigned long now) {
  int face = expr;
  if (now < moodUntil) face = mood;
  if (now < flashUntil) face = flash;
  if (asleep) face = E_SLEEP;
  bool humming = now < humUntil && !asleep;
  if (humming) face = 1;     // happy eyes while humming

  bool blinkOk = ex(face, 5) == 1;
  buildEye(ex(face, 0), false, blinkOk);
  sendPair(0);
  buildEye(ex(face, 1), ex(face, 2) == 1, blinkOk);
  sendPair(2);
  buildMouth(viseme >= 0 ? viseme : ex(face, 3));
  sendPair(4);
  drawFx(humming ? FX_HUM : ex(face, 4), now);
}

void layout() {
  lcd.clear();
  lcd.setCursor(3, 0);  lcd.write(byte(0)); lcd.write(byte(1));
  lcd.setCursor(11, 0); lcd.write(byte(2)); lcd.write(byte(3));
  lcd.setCursor(7, 1);  lcd.write(byte(4)); lcd.write(byte(5));
}

// ---- speech ----
int visemeFor(char c) {
  c = tolower(c);
  if (c == 'a' || c == 'h') return V_OPEN;
  if (c == 'e' || c == 'i' || c == 'y') return V_WIDE;
  if (c == 'o' || c == 'u' || c == 'w' || c == 'q') return OH;
  if (c == 'm' || c == 'b' || c == 'p' || c == ' ') return FLAT;
  if (isalpha(c)) return V_SMALL;
  return FLAT;
}

int charTime(char c) {
  if (c == '.' || c == '!' || c == '?') return 380;
  if (c == ',') return 220;
  if (c == ' ') return 90;
  return 65;
}

int moodOf(String low) {
  if (low.indexOf("love") >= 0 || low.indexOf("<3") >= 0) return E_LOVE;
  if (low.indexOf("sad") >= 0 || low.indexOf("sorry") >= 0 || low.indexOf("cry") >= 0) return E_SAD;
  if (low.indexOf("unplug") >= 0 || low.indexOf("angry") >= 0 || low.indexOf("hate") >= 0) return E_ANGRY;
  if (low.indexOf("haha") >= 0 || low.indexOf("lol") >= 0 || low.indexOf("yay") >= 0 || low.indexOf("hello") >= 0) return E_JOY;
  if (low.indexOf("wow") >= 0 || low.indexOf("whoa") >= 0) return E_SHOCK;
  if (low.indexOf("hmm") >= 0) return E_CONFUSED;
  return expr;
}

void speak(String text) {
  String low = "";
  for (int i = 0; i < text.length(); i++) low += (char)tolower(text[i]);
  mood = moodOf(low);
  moodUntil = 0xFFFFFFFF;
  say = text;
  sayPos = 0;
  nextChar = millis();
}

void tickSpeech(unsigned long now) {
  if (say.length() == 0 || now < nextChar) return;
  if (sayPos >= say.length()) {           // finished: close mouth, hold the mood a moment
    say = "";
    viseme = -1;
    moodUntil = now + 1500;
    return;
  }
  char c = say[sayPos];
  sayPos++;
  viseme = visemeFor(c);
  if (c == '!') { flash = E_SHOCK; flashUntil = now + 450; }
  if (c == '?') { flash = E_CONFUSED; flashUntil = now + 700; }
  nextChar = now + charTime(c);
  lastInput = now;
}

// ---- input ----
void handleLine(String line) {
  unsigned long now = millis();
  lastInput = now;
  if (asleep) { asleep = false; flash = E_SHOCK; flashUntil = now + 600; }
  if (line.startsWith(":")) {
    String cmd = line.substring(1);
    if (cmd == "list") { for (int i = 0; i < N_EX; i++) Serial.println(NAMES[i]); return; }
    if (cmd == "sleep") { asleep = true; return; }
    if (cmd == "wake") return;
    for (int i = 0; i < N_EX; i++) if (cmd == NAMES[i]) { expr = i; moodUntil = 0; Serial.println("face: " + cmd); return; }
    Serial.println(F("unknown face, try :list"));
    return;
  }
  speak(line);
}

void readSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inLine.length() > 0) { demo = false; handleLine(inLine); }
      inLine = "";
    } else {
      inLine += c;
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
    int r = random(0, 5);
    lookX = r == 0 ? -2 : (r == 1 ? 2 : (r == 2 ? -1 : 0));
    nextLook = now + (lookX == 0 ? random(2500, 6000) : random(600, 1800));
  }
  if (now >= bobAt) { bob = 1 - bob; bobAt = now + (bob ? 500 : random(2500, 4000)); }
  if (now >= nextHum && say.length() == 0) { humUntil = now + 1500; nextHum = now + random(20000, 40000); }

  unsigned long idle = now - lastInput;
  if (!demo && !asleep && idle > 60000) asleep = true;
  if (!demo && !asleep && idle > 30000 && expr == E_IDLE) { mood = E_SLEEPY; moodUntil = now + 100; }
}

void tickDemo(unsigned long now) {
  if (!demo || say.length() > 0 || now < demoAt) return;
  handleLine(DEMO[demoPos]);
  demoPos = (demoPos + 1) % N_DEMO;
  demoAt = now + 900;
}

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  for (int s = 0; s < 8; s++) for (int r = 0; r < 8; r++) cg[s][r] = 255;   // force first upload
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
  tickIdle(now);
  draw(now);
  delay(15);
}

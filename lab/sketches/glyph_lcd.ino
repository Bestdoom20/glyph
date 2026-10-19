/*
  GLYPH-LCD: the GLYPH living kaomoji face on a 1602 I2C LCD (HD44780 + PCF8574 backpack).
  https://github.com/Bestdoom20/glyph  ·  Arduino Uno / Freenove Control Board V4

  Wiring: LCD GND -> GND, VCC -> 5V, SDA -> A4, SCL -> A5.
          Passive buzzer via S8050 NPN: D9 -> 1k -> base, emitter -> GND,
          collector -> buzzer (-), buzzer (+) -> SOUND switch -> 5V.
          SOUND switch (slide, SPDT): middle -> buzzer (+), one side -> 5V. Cuts sound in hardware.
          POWER switch (slide, SPDT): middle -> D3, one side -> GND. GND side = off: screen dark,
          voice stopped, ATmega in power-down sleep until it is switched back on.

  Voice = "mumble speech" (no TTS): text types out on a letter timer, and the buzzer speaks one
  vowel blip per vowel group (~10/s). Each blip is a stored one-period vowel waveform (buzz + 3
  formants, fitted to the reference voice) played at any pitch through a 62.5 kHz PWM "DAC".
  Text can stream in like LLM tokens: it is spoken as it arrives, at a pace that follows the arrival rate.
  Pitch drifts down through a phrase and resets at punctuation; ? rises, ! starts high, ... fades,
  CAPS shouts. The mouth opens on the vowel while it sounds, and each phrase's words set the face.

  Talk to it over Serial (9600 baud, newline ending):
    hello there!     speaks the text: mouth shape per letter, cues on ! ? and mood words
    :happy  :sad     sets the resting face (:list prints every name)
    :sleep  :wake    sleep / wake
    :mute  :unmute   buzzer off / on        :pitch 130   voice base pitch in Hz
    :voice buzzer    bright voice for the passive buzzer (default)
    :eyes <hex>      store a custom face (from the face builder); every expression is derived from it
    :mode custom     switch to it (also in the MODE button cycle once one is stored)
    :voice guard|default|woman|child|old   low hum voices (the reference tone; quiet on a buzzer)
    :rate 20         talk as if text arrived at 20 tokens/s (0 = normal); streamed text sets its own pace
  It plays a demo script until the first line arrives. Lines over 127 chars are cut.
  Every spoken line is echoed to Serial as "> text" while it types (a live caption).

  Screen: eyes are 2 cells (10x8 px) each on row 0, the mouth is 2 cells on row 1.
  CGRAM slots: 0-1 left eye, 2-3 right eye, 4-5 mouth, 6-7 effects.
  The text never moves: animation = rewriting CGRAM, and only slots that changed are sent.
  Budget: one slot costs ~3.5 ms on the 100 kHz PCF8574 bus (own batched driver), LCD crystals
  need ~100-200 ms to settle, so the crystals, not the bus, set the ~8-12 changes/s ceiling.
*/
#include <avr/sleep.h>
#include <EEPROM.h>

#define BUZZER 9
#define PWR_SW 3
#define MODE_BTN 2

uint16_t rs16 = 1;               // xorshift16: Arduino rnd() pulls in libc's 32-bit generator
long rnd(long lo, long hi) {
  rs16 = (rs16 ^ (rs16 << 7)) & 0xFFFF; rs16 ^= rs16 >> 9; rs16 = (rs16 ^ (rs16 << 8)) & 0xFFFF;
  return lo + rs16 % (hi - lo);
}

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
#define UCUP 14
#define STAR 15
#define BOX 16
#define VISOR 17
#define BR_N 18         // brows: flat brow + dot
#define BR_UP 19
#define BR_A 20
#define BR_S 21
#define BR_H 22
#define BR_T 23
#define BR_RD 24
#define FIRST_BROW 18   // rows 0-2 are a brow: blinks leave them alone

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
  {0b0000000000, 0b0000000000, 0b1000000001, 0b1000000001, 0b1100000011, 0b0110000110, 0b0011111100, 0b0000000000},  // UCUP U (UwU)
  {0b0000110000, 0b0000110000, 0b0001111000, 0b1111111111, 0b0001111000, 0b0011001100, 0b0110000110, 0b0000000000},  // STAR ✧
  {0b0000000000, 0b0111111110, 0b0111111110, 0b0111111110, 0b0111111110, 0b0111111110, 0b0000000000, 0b0000000000},  // BOX  ■ (robot)
  {0b0000000000, 0b0000000000, 0b0000000000, 0b0111111110, 0b0111111110, 0b0000000000, 0b0000000000, 0b0000000000},  // VISOR (robot low)
  {0b0000000000, 0b0111111110, 0b0000000000, 0b0000000000, 0b0001111000, 0b0011111100, 0b0001111000, 0b0000000000},  // BR_N  flat brow
  {0b0011111100, 0b0100000010, 0b0000000000, 0b0001111000, 0b0010000100, 0b0010000100, 0b0001111000, 0b0000000000},  // BR_UP raised, wide eye
  {0b1100000000, 0b0011100000, 0b0000011100, 0b0000000000, 0b0001110000, 0b0011111000, 0b0001110000, 0b0000000000},  // BR_A  angry (left; mirrored)
  {0b0000000111, 0b0000111000, 0b0111000000, 0b0000000000, 0b0000000000, 0b0001111000, 0b0011111100, 0b0000000000},  // BR_S  worried (left; mirrored)
  {0b0011111100, 0b0000000000, 0b0000000000, 0b0001111000, 0b0110000110, 0b1000000001, 0b0000000000, 0b0000000000},  // BR_H  raised + happy arc
  {0b0000000000, 0b0000000000, 0b0111111110, 0b0000000000, 0b1111111111, 0b0111111110, 0b0011111100, 0b0000000000},  // BR_T  low brow, heavy lid
  {0b0011111100, 0b0100000010, 0b0000000000, 0b0000000000, 0b0001111000, 0b0011111100, 0b0001111000, 0b0000000000},  // BR_RD one raised brow
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
#define ZIGZAG 15
#define DOTM 16

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
  {0b0000000000, 0b0000000000, 0b0100010001, 0b1010101010, 0b0001000100, 0b0000000000, 0b0000000000, 0b0000000000},  // ZIGZAG (robot)
  {0b0000000000, 0b0000000000, 0b0000110000, 0b0000110000, 0b0000000000, 0b0000000000, 0b0000000000, 0b0000000000},  // DOTM  .
};

// ---- effects: 5x8, one cell ----
const byte FX_TEAR[8] PROGMEM  = {0b00100, 0b00100, 0b01110, 0b01110, 0b11111, 0b11111, 0b01110, 0b00000};
const byte FX_ANGER[8] PROGMEM = {0b00000, 0b01010, 0b11011, 0b00000, 0b11011, 0b01010, 0b00000, 0b00000};
const byte FX_BLUSH[8] PROGMEM = {0b00000, 0b00000, 0b00000, 0b10101, 0b01010, 0b10101, 0b00000, 0b00000};
const byte FX_HEART[8] PROGMEM = {0b00000, 0b01010, 0b11111, 0b11111, 0b01110, 0b00100, 0b00000, 0b00000};
const byte FX_SPARK[8] PROGMEM = {0b00100, 0b00100, 0b11011, 0b00100, 0b00100, 0b00000, 0b00000, 0b00000};
const byte FX_NOTE[8] PROGMEM  = {0b00110, 0b00101, 0b00100, 0b00100, 0b01100, 0b11100, 0b11000, 0b00000};

#define FX_NONE 0
#define FX_TEARS 1
#define FX_MAD 2
#define FX_SHY 3
#define FX_LOVE 4
#define FX_ZZZ 5
#define FX_HUM 6
#define FX_SPARKLE 7
#define FX_THINK 8
#define FX_CURIOUS 9
#define FX_SWEAT 10

// ---- expressions: eyeL, eyeR, mirror right eye, mouth, effect, can blink, voice pitch %, letter time % ----
const char NAMES[] PROGMEM = "idle|happy|joy|love|sad|cry|shock|angry|annoyed|smug|excited|dizzy|sleepy|sleep|kiss|tongue|evil|cute|confused|"
                             "uwu|sparkle|owo|worried|robot|robohappy|roboalert|robosad|roboangry|grumpy|content|"
                             "browidle|browhappy|browwow|browangry|browsad|browskeptic|browtired|browcry";
const byte EX[][8] PROGMEM = {
  {DOT, DOT, 0, CAT, FX_NONE, 1, 100, 100},          // 0 idle      ● ω ●
  {ARC, ARC, 0, SMILE, FX_NONE, 0, 115, 100},        // 1 happy     ^ ‿ ^
  {DOT, DOT, 0, GRIN, FX_NONE, 1, 115, 88},          // 2 joy       ● ▽ ●
  {HEART, HEART, 0, SMILE, FX_LOVE, 0, 110, 100},    // 3 love      ♥ ‿ ♥
  {LINE, LINE, 0, FROWN, FX_TEARS, 0, 80, 130},      // 4 sad       — ︵ —
  {TEE, TEE, 0, OH, FX_TEARS, 0, 80, 130},           // 5 cry       T o T
  {RING, RING, 0, OH, FX_NONE, 1, 125, 100},         // 6 shock     O o O
  {BROW, BROW, 1, FROWN, FX_MAD, 1, 90, 88},         // 7 angry     ò ︵ ó
  {LID, LID, 0, RECT, FX_MAD, 0, 90, 100},           // 8 annoyed   ⊖ ▭ ⊖
  {LID, LID, 0, SMIRK, FX_NONE, 0, 95, 110},         // 9 smug      ⊖ / ⊖
  {CHEVRON, CHEVRON, 1, CAT, FX_SHY, 0, 115, 88},    // 10 excited  > ω <
  {SPIRAL, SPIRAL, 1, WAVY, FX_NONE, 0, 100, 120},   // 11 dizzy    @ ~ @
  {SLEEPY, SLEEPY, 0, V_SMALL, FX_NONE, 0, 75, 150}, // 12 sleepy
  {LINE, LINE, 0, FLAT, FX_ZZZ, 0, 75, 150},         // 13 sleep    — — z
  {LINE, LINE, 0, KISS, FX_SHY, 0, 110, 100},        // 14 kiss     — 3 —
  {HALF, HALF, 0, TONGUE, FX_NONE, 0, 105, 100},     // 15 tongue   ◡ ᴗ ◡
  {WEDGE, WEDGE, 1, CAT, FX_NONE, 0, 85, 100},       // 16 evil     ◣ ω ◢
  {SMALL, SMALL, 0, TONGUE, FX_NONE, 1, 120, 95},    // 17 cute     ° ᴗ °
  {SMALL, RING, 0, FLAT, FX_NONE, 1, 105, 110},      // 18 confused o — O
  {UCUP, UCUP, 0, CAT, FX_SHY, 0, 120, 95},          // 19 uwu      U ω U
  {STAR, STAR, 0, GRIN, FX_SPARKLE, 0, 125, 88},     // 20 sparkle  ✧ ▽ ✧
  {SMALL, SMALL, 0, OH, FX_NONE, 1, 120, 95},        // 21 owo      ° o °
  {DOT, DOT, 0, FROWN, FX_SWEAT, 1, 105, 105},       // 22 worried  ● ︵ ● ;
  {BOX, BOX, 0, FLAT, FX_NONE, 1, 100, 100},         // 23 robot    ■ — ■
  {BOX, BOX, 0, SMILE, FX_NONE, 1, 100, 95},         // 24 robohappy
  {BOX, BOX, 0, OH, FX_NONE, 1, 100, 90},            // 25 roboalert
  {VISOR, VISOR, 0, FROWN, FX_NONE, 0, 100, 110},    // 26 robosad
  {WEDGE, WEDGE, 1, ZIGZAG, FX_MAD, 1, 100, 90},     // 27 roboangry
  {LID, LID, 0, FROWN, FX_NONE, 0, 85, 115},         // 28 grumpy   ⊖ ︵ ⊖
  {ARC, ARC, 0, CAT, FX_NONE, 0, 100, 105},          // 29 content  ^ ω ^
  {BR_N, BR_N, 0, FLAT, FX_NONE, 1, 100, 100},       // 30 browidle
  {BR_H, BR_H, 0, SMILE, FX_NONE, 0, 112, 95},       // 31 browhappy
  {BR_UP, BR_UP, 0, OH, FX_NONE, 1, 122, 95},        // 32 browwow
  {BR_A, BR_A, 1, FROWN, FX_MAD, 1, 90, 88},         // 33 browangry
  {BR_S, BR_S, 1, FROWN, FX_NONE, 1, 85, 120},       // 34 browsad
  {BR_N, BR_RD, 0, SMIRK, FX_NONE, 1, 100, 105},     // 35 browskeptic  one brow up
  {BR_T, BR_T, 0, FLAT, FX_NONE, 0, 80, 135},        // 36 browtired
  {BR_S, BR_S, 1, OH, FX_TEARS, 0, 80, 130},         // 37 browcry
};
const int8_t EXV[][2] PROGMEM = {   // where each face sits in feeling space: valence, arousal (-100..100)
  {0, 0}, {55, 10}, {60, 50}, {90, 20}, {-60, -15}, {-90, 30}, {0, 80}, {-60, 70}, {-40, 30}, {30, -10},
  {70, 70}, {0, 60}, {0, -60}, {0, -95}, {60, -10}, {40, 30}, {-30, 40}, {50, 30}, {-10, 40},
  {60, 0}, {80, 70}, {10, 60}, {-35, 40}, {0, 0}, {55, 10}, {0, 80}, {-60, -15}, {-60, 70}, {-45, 0}, {50, -50},
  {0, 0}, {55, 10}, {10, 75}, {-60, 70}, {-55, -10}, {-10, 30}, {0, -60}, {-85, 30}};
#define N_EX 38
#define E_HAPPY 1
#define E_SHOCK 6
#define E_SLEEP 13
#define E_CONFUSED 18

// ---- modes: face per feeling cell (arousal high/mid/low x valence neg/neu/pos), love, cry,
//      resting valence, resting arousal, voice Hz / 2, flags, curious face ----
#define F_MONO 1                 // robot: flat tune, no wobble
#define F_BLUSH 2                // blush whenever no other effect
#define F_BOUNCY 4               // faster, bigger breathing bob
#define F_SIGH 8                 // sighs instead of humming
#define F_HUM 16                 // hums when idle
#define F_MOLE 32                // beauty mark on the right cheek
const char MODE_NAMES[] PROGMEM = "basic|kawaii|robot|grumpy|brows|custom";
const int8_t MODES[][16] PROGMEM = {
  {7, 6, 10,   4, 0, 1,     4, 12, 29,  3, 5,   10, 0,    65, F_HUM, 18},                     // basic
  {5, 21, 20,  22, 19, 10,  4, 12, 1,   3, 5,   30, 15,   100, F_BLUSH | F_BOUNCY | F_HUM | F_MOLE, 21},   // kawaii
  {27, 25, 24, 26, 23, 24,  26, 23, 23, 24, 26, 0, 0,     55, F_MONO, 25},                    // robot
  {7, 8, 9,    28, 28, 9,   4, 12, 28,  9, 4,   -25, -10, 50, F_SIGH, 8},                     // grumpy
  {33, 32, 31, 34, 30, 31,  34, 36, 31, 31, 37, 5, 5,     60, F_HUM, 35},                     // brows
  {7, 6, 10,   4, 0, 1,     4, 12, 29,  3, 5,   10, 0,    65, F_HUM, 18},                     // custom: your eyes, every expression derived
};
#define N_MODES 6
#define M_CUSTOM 5

// ---- feeling words: what a word does to valence / arousal, and what kind of word it is ----
// kind 0 plain · 1 sad/scared (if YOU say it: concern) · 2 hostile (hurts) · 3 its name (perks up) · 4 curious
const char LEX[] PROGMEM = "love|like|happy|yay|haha|lol|great|thank|cute|good|nice|wow|whoa|cool|awesome|glad|fun|hello|uwu|owo|"
                           "sad|sorry|cry|miss|lonely|bad|scared|afraid|help|"
                           "hate|angry|mad|stupid|dumb|shut|unplug|"
                           "tired|sleep|bored|glyph|hmm";
const int8_t LEXV[] PROGMEM = {50, 25, 40, 40, 35, 30, 30, 35, 40, 20, 25, 15, 10, 25, 40, 30, 30, 25, 45, 20,
                               -45, -25, -50, -30, -40, -30, -30, -30, -15,
                               -50, -40, -35, -45, -40, -30, -30,
                               -20, -5, -25, 25, 0};
const int8_t LEXA[] PROGMEM = {20, 5, 20, 50, 40, 30, 20, 10, 30, 5, 10, 60, 60, 20, 40, 10, 30, 25, 20, 50,
                               -20, -10, 10, -10, -20, 10, 60, 60, 40,
                               50, 60, 50, 40, 30, 40, 70,
                               -50, -40, -40, 30, 15};
const byte LEXK[] PROGMEM = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                             1, 1, 1, 1, 1, 1, 1, 1, 1,
                             2, 2, 2, 2, 2, 2, 2,
                             0, 0, 0, 3, 4};

// demo: ':' = command, '>' = something YOU say (it listens and reacts), anything else it says
const char DEMO[] PROGMEM = "hello! i am glyph, a little face for this screen.|> hi glyph!|hi! oh, you know my name!|"
                            "> i am kind of sad today|oh no. i am sorry. i am here for you.|> you are stupid|hey! that is not nice...|"
                            "> haha sorry, you are awesome|aww, thank you! that makes me happy!|> do you love pixels?|i LOVE tiny pixels so much!|"
                            "whoa... i think i need a nap.|tap my button to change my face. hold it to stop me talking!";

// vowel -> which VOX waveform plays (the vowel picks the sound, never the pitch) and mouth shape
const char VOWELS[] = "aeiouy";
const byte V_MOUTH[] = {V_OPEN, V_WIDE, V_WIDE, OH, OH, V_WIDE};
const int HUM[] = {196, 220, 175};
const int SIGH[] = {150, 125, 100};

// tilt k 0.505, mean tilt 39.2 dB (target 39.2)
// one period per vowel ("aeiouy"); the ISR interpolates between samples. Made by tools/make_vowels.py
const int8_t VOXH[6][64] PROGMEM = {   // low reference hum: harmonics 1-31 (above that it is 40-60 dB down)
  {-11, 2, 17, 40, 66, 76, 64, 41, 20, 10, 9, 9, 6, 5, 14, 32, 50, 58, 53, 39, 26, 17, 7, -6, -20, -30, -30, -22, -19, -27, -46, -65, -80, -88, -95, -104, -109, -90, -19, 74, 127, 122, 86, 44, 15, 0, -28, -74, -105, -97, -55, 1, 43, 55, 52, 54, 57, 46, 16, -26, -61, -69, -52, -28},  // a
  {48, 42, 26, 11, 3, -1, -5, -8, -1, 12, 25, 36, 47, 61, 71, 73, 71, 67, 62, 49, 33, 18, 6, -6, -19, -28, -32, -35, -43, -53, -62, -71, -83, -99, -112, -121, -127, -108, -59, -21, 6, 34, 70, 99, 97, 85, 74, 61, 35, -5, -32, -46, -58, -71, -77, -62, -41, -24, -9, 12, 37, 49, 51, 50},  // e
  {-49, -46, -41, -28, -16, -5, 9, 26, 40, 51, 64, 75, 81, 85, 89, 89, 84, 78, 72, 62, 50, 38, 26, 13, 0, -11, -21, -32, -44, -55, -65, -78, -89, -100, -110, -120, -127, -117, -99, -90, -72, -45, -19, -3, 19, 44, 58, 66, 75, 83, 78, 71, 65, 56, 39, 23, 12, -3, -20, -31, -37, -46, -52, -52},  // i
  {33, 26, 20, 14, 8, 2, -3, -8, -10, -9, -4, 5, 17, 30, 44, 55, 62, 64, 59, 50, 36, 20, 4, -10, -21, -28, -31, -31, -29, -27, -28, -32, -39, -50, -64, -78, -90, -97, -93, -74, -43, -2, 43, 84, 114, 127, 120, 95, 56, 11, -34, -69, -91, -98, -89, -69, -42, -14, 11, 29, 40, 44, 43, 39},  // o
  {-51, -48, -42, -30, -15, 2, 19, 36, 49, 60, 68, 75, 81, 85, 88, 87, 83, 76, 66, 53, 40, 28, 16, 5, -5, -14, -23, -31, -40, -48, -57, -66, -77, -88, -100, -111, -121, -127, -123, -106, -79, -47, -13, 17, 39, 52, 61, 66, 71, 76, 80, 79, 73, 59, 40, 18, -3, -20, -33, -41, -45, -47, -49, -51},  // u
  {-51, -46, -42, -29, -20, -7, 10, 20, 39, 50, 62, 74, 79, 87, 89, 88, 87, 80, 74, 64, 52, 42, 27, 16, 2, -10, -20, -33, -44, -55, -68, -78, -92, -101, -113, -122, -127, -112, -109, -89, -65, -55, -15, -9, 26, 39, 54, 72, 73, 81, 81, 72, 69, 54, 41, 31, 8, 3, -20, -27, -38, -47, -48, -53},  // y
};
const int8_t VOXB[6][32] PROGMEM = {   // bright buzzer voice: harmonics 1-15 (it uses up to 9)
  {0, 81, 127, 124, 88, 39, 0, -21, -24, -16, 0, 15, 26, 30, 25, 14, 0, -14, -25, -30, -26, -15, 0, 16, 24, 21, 0, -39, -88, -124, -127, -81},  // a
  {0, 84, 127, 118, 78, 35, 10, 9, 26, 45, 53, 47, 29, 9, -4, -5, 0, 5, 4, -9, -29, -47, -53, -45, -26, -9, -10, -35, -78, -118, -127, -84},  // e
  {0, 89, 127, 108, 62, 23, 7, 13, 25, 30, 23, 13, 10, 13, 14, 10, 0, -10, -14, -13, -10, -13, -23, -30, -25, -13, -7, -23, -62, -108, -127, -89},  // i
  {0, 70, 114, 127, 116, 98, 82, 72, 66, 61, 55, 47, 39, 29, 19, 9, 0, -9, -19, -29, -39, -47, -55, -61, -66, -72, -82, -98, -116, -127, -114, -70},  // o
  {0, 73, 119, 127, 107, 78, 53, 35, 22, 10, -2, -12, -16, -15, -10, -5, 0, 5, 10, 15, 16, 12, 2, -10, -22, -35, -53, -78, -107, -127, -119, -73},  // u
  {0, 90, 127, 112, 79, 49, 26, 15, 16, 24, 28, 27, 21, 14, 9, 5, 0, -5, -9, -14, -21, -27, -28, -24, -16, -15, -26, -49, -79, -112, -127, -90},  // y
};

// ---- buzzer voice: wavetable synth -> Timer1 8-bit PWM at 62.5 kHz on D9 ----
// Timer2 interrupts at 16 kHz. Each sample reads the vowel table at a phase step set by the pitch
// (pitch-shifting the stored wave moves its formants too, like resampling a recorded clip), adds
// breath noise and scales by the envelope. Every 64 samples (4 ms) it steps the envelope
// (swell 30 %, hold, release 30 %), the ~40 ms glide, the question slide and a +-1.5 % pitch wobble.
// A note that starts while one is sounding re-swells from 40 %: legato babble, not beeps.
// SIM-REPLACE-BEGIN (the lab swaps this block for the same model in WebAudio)
volatile uint16_t vPhase, vInc, vBase, vTarget;  // 16-bit table phase (one wrap = one period); step per sample
volatile uint16_t vLen, vPos, vA, vR, vEnv16, vUp, vDn;   // note in 4 ms ticks; envelope 8.8 and its steps
volatile uint8_t vEnv, vSub, vVowel, vSeed = 1;
volatile bool vRise;

void voiceStop() { TIMSK2 = 0; TCCR2B = 0; TCCR1A = 0; TCCR1B = 0; PORTB &= ~_BV(1); }   // D9 low

void voiceStart(long freq, int ms, bool rise, byte vowelIdx) {
  uint16_t inc = (uint32_t)freq * 4096 / 1000;   // freq * 128 * 512 / 16000 (~0.2 % pitch steps)
  uint16_t len = ms / 4;
  noInterrupts();
  bool legato = TIMSK2;
  vTarget = inc;
  if (!legato) { vBase = inc; vInc = inc; vPhase = 0; vEnv = 0; }
  vVowel = vowelIdx; vRise = rise; vSub = 0;
  vLen = len; vA = len * 3 / 10; vR = len - len * 3 / 10;
  vUp = 65280 / (vA ? vA : 1); vDn = 65280 / (vLen - vR ? vLen - vR : 1);   // steps precomputed: the ISR only adds
  vPos = legato ? vA * 2 / 5 : 0;
  vEnv16 = legato ? 26112 : 0;   // legato re-swells from 40 %
  if (!legato) {
    TCCR1A = _BV(COM1A1) | _BV(WGM10); TCCR1B = _BV(WGM12) | _BV(CS10);   // 8-bit fast PWM, 62.5 kHz
    OCR1A = 0;
    TCCR2A = _BV(WGM21); TCCR2B = _BV(CS21); OCR2A = 124;                  // CTC, 16 kHz
    TIMSK2 = _BV(OCIE2A);
  }
  interrupts();
}

ISR(TIMER2_COMPA_vect) {
  vPhase += vInc;
  int8_t w, a, b;                // read between two samples (linear interpolation): small tables, smooth wave
  if (vVowel < 6) {
    byte i = vPhase >> 10, f = vPhase >> 3;            // 64 samples: index = top 6 bits, fraction = next 7
    a = pgm_read_byte(&VOXH[vVowel][i]); b = pgm_read_byte(&VOXH[vVowel][(i + 1) & 63]);
    w = a + (((int16_t)(b - a) * (f & 127)) >> 7);
  } else {
    byte i = vPhase >> 11, f = vPhase >> 4;            // 32 samples
    a = pgm_read_byte(&VOXB[vVowel - 6][i]); b = pgm_read_byte(&VOXB[vVowel - 6][(i + 1) & 31]);
    w = a + (((int16_t)(b - a) * (f & 127)) >> 7);
  }
  vSeed ^= vSeed << 3; vSeed ^= vSeed >> 5; vSeed ^= vSeed << 1;          // xorshift8
  int8_t n = vSeed;
  int16_t mix = w - (w >> 3) + (((int16_t)n * vEnv) >> 11);               // breath ~ env^2: no hiss tail
  OCR1A = ((uint16_t)vEnv * (uint16_t)(128 + mix)) >> 8;                   // offset swells too: no pop
  if (++vSub < 64) return;
  vSub = 0;
  uint16_t p = vPos++;
  if (p >= vLen) { voiceStop(); return; }
  if (p < vA) vEnv16 = vEnv16 + vUp > 65280 ? 65280 : vEnv16 + vUp;      // swell
  else if (p > vR) vEnv16 = vEnv16 > vDn ? vEnv16 - vDn : 0;               // release
  else vEnv16 = 65280;                                                     // hold
  vEnv = vEnv16 >> 8;
  if (vRise) vTarget += (vTarget >> 8) + (vTarget >> 9);                   // question: word slides up ~0.6 %/tick
  vBase += ((int16_t)(vTarget - vBase)) >> 3;                              // glide
  vInc = vBase + (((int16_t)(int8_t)vSeed * (int16_t)(vBase >> 6)) >> 7);  // wobble
}

void sleepUntilOn() {          // power-down; a pin change on D3 wakes us
  Serial.flush();
  PCICR |= _BV(PCIE2); PCMSK2 |= _BV(PCINT19);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  while (!pinHigh(PWR_SW)) sleep_mode();
  PCMSK2 &= ~_BV(PCINT19);
}
ISR(PCINT2_vect) {}

void nap(byte ms) {              // idle-sleep instead of delay(): timers, I2C, serial and the voice keep
  unsigned long t = millis();    // running, the CPU core stops until the next interrupt (~1 ms tick)
  set_sleep_mode(SLEEP_MODE_IDLE);
  while (millis() - t < ms) sleep_mode();
}

// I2C master on the TWI hardware, blocking (the Wire library adds ~1.3 KB flash + ~160 B of buffers)
void twiInit() { PORTC |= _BV(4) | _BV(5); TWSR = 0; TWBR = 72; }   // pull-ups on SDA/SCL; (16 MHz / 100 kHz - 16) / 2
void twiWait() { while (!(TWCR & _BV(TWINT))); }
bool twiStart(byte a) {          // true if a device answered at address a
  while (TWCR & _BV(TWSTO));
  TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN); twiWait();
  TWDR = a << 1; TWCR = _BV(TWINT) | _BV(TWEN); twiWait();
  return (TWSR & 0xF8) == 0x18;
}
void twiWrite(byte b) { TWDR = b; TWCR = _BV(TWINT) | _BV(TWEN); twiWait(); }
void twiStop() { TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN); }

void ioInit() { DDRB |= _BV(1); PORTD |= _BV(2) | _BV(3); }   // D9 out; pull-ups on D2 (MODE), D3 (POWER)
bool pinHigh(byte bit) { return PIND & _BV(bit); }           // read D0-D7
// SIM-REPLACE-END

// ---- LCD: HD44780 behind a PCF8574 backpack (P0 RS, P1 RW, P2 EN, P3 backlight, P4-P7 = D4-D7) ----
// Own driver: one LCD byte = 4 expander writes (two EN pulses), up to 7 LCD bytes per I2C
// transaction (Wire buffer 32). A CGRAM slot costs ~3.5 ms on the bus instead of ~13 ms with
// LiquidCrystal_I2C, which opens a transaction per expander write. Each I2C byte takes ~90 us at
// 100 kHz, longer than the HD44780's 37 us per instruction, so no extra waits are needed.
byte lcdA = 0x27, lcdBl = 0x08, txN = 0;

void lcdOpen() { twiStart(lcdA); txN = 0; }
void lcdClose() { twiStop(); }
void lcdPut(byte v, byte rs) {   // queue one LCD byte
  if (txN == 7) { lcdClose(); lcdOpen(); }
  byte hi = (v & 0xF0) | lcdBl | rs, lo = ((v << 4) & 0xF0) | lcdBl | rs;
  twiWrite(hi | 4); twiWrite(hi); twiWrite(lo | 4); twiWrite(lo);
  txN++;
}
void lcdCmd(byte v) { lcdOpen(); lcdPut(v, 0); lcdClose(); if (v < 4) delay(2); }   // clear/home take 1.5 ms
void lcdNib(byte n) { lcdOpen(); twiWrite(n | lcdBl | 4); twiWrite(n | lcdBl); lcdClose(); }
void lcdChar(byte slot, const byte *bits) { lcdOpen(); lcdPut(0x40 | (slot << 3), 0); for (byte r = 0; r < 8; r++) lcdPut(bits[r], 1); lcdClose(); }
void lcdText(byte col, byte row, const char *s) {   // flash string at col,row
  lcdOpen(); lcdPut(0x80 | (col + row * 0x40), 0);
  for (byte i = 0; pgm_read_byte(&s[i]); i++) lcdPut(pgm_read_byte(&s[i]), 1);
  lcdClose();
}
void lcdLight(bool on) { lcdBl = on ? 0x08 : 0; lcdOpen(); twiWrite(lcdBl); lcdClose(); }
void lcdBegin() {
  twiInit();
  if (!twiStart(0x27)) lcdA = 0x3F;   // PCF8574T = 0x27, PCF8574AT = 0x3F
  twiStop();
  delay(50);
  lcdNib(0x30); delay(5); lcdNib(0x30); delay(1); lcdNib(0x30); lcdNib(0x20);   // reset into 4-bit mode
  lcdCmd(0x28); lcdCmd(0x0C); lcdCmd(0x01); lcdCmd(0x06);
}


// ---- state ----
#define MAXLEN 128
char say[MAXLEN];               // line being spoken
byte sayLen = 0, sayPos = 0;
bool talking = false, lineOpen = false, needPhrase = false, powered = true;
unsigned long lastRx = 0, firstRx = 0;
int pace = 100;                 // % of normal letter time, follows how fast text arrives
int ratePace = 100;             // pace for text that isn't streaming (whole lines, demo): set by :rate
byte rxCount = 0;
char inLine[MAXLEN];            // ":command" or "> what you say", arriving on Serial
byte inLen = 0;

// feeling: valence (sad..happy) and arousal (calm..excited), -100..100, drifting to the mode's resting point
int val = 0, aro = 0;
int bonus = 0;                  // praise memory: lifts resting valence, fades over minutes
byte mode = 0;
byte face = 0, target = 0;      // face on screen / face the feeling asks for
byte cand = 0;
unsigned long candSince = 0;
byte trans = 0;                 // transition: 1 squash old face, 2 squash (or pop) new face
unsigned long transAt = 0;
byte flash = 0, forced = 0;     // short reaction / face set by a :command
unsigned long flashUntil = 0, forcedUntil = 0;
unsigned long typingUntil = 0, thinkUntil = 0, curiousUntil = 0, nodUntil = 0, yawnUntil = 0;
unsigned long emoAt = 0, nextYawn = 0;

bool asleep = false, demo = true, muted = false;
int8_t lookX = 0, microX = 0;   // gaze in px, -2..2, and tiny saccade flicks
byte blinkStage = 0;            // 0 open, 1 half, 2 closed, 3 half
byte bob = 0;                   // breathing: face drops 1 px
bool squint = false;            // shouted syllable
unsigned long nextLook = 0, blinkAt = 0, bobAt = 0, humUntil = 0, nextHum = 0, hummAt = 0, nextSacc = 0, saccUntil = 0;
byte hummNote = 3;
unsigned long lastInput = 0, nextChar = 0, demoAt = 0;
int8_t viseme = -1;             // mouth override (talking), -1 = expression mouth
int demoPos = 0;                // offset into DEMO
int voiceBase = 130;            // Hz: the reference voices' median F0
bool wobble = false;
byte vset = 6;                  // vowel set: 6 = bright buzzer voice (default on hardware), 0 = low reference hum
byte phraseK = 0;               // syllables into the current phrase (pitch drifts down)
bool phraseHigh = false, fading = false;
long sylFreq = 0;
unsigned long sylStart = 0, sylEnd = 0;
int8_t sylViseme = -1;
int8_t fxShown = -1;
byte fxFrame = 0;
unsigned long fxAt = 0;
bool btnWas = true, btnLong = false;
unsigned long btnAt = 0, btnDown = 0;

byte cg[8][8];                  // what each CGRAM slot holds right now
byte g[8];
uint16_t rows[8];
uint32_t lastKey = 0xFFFFFFFF;  // everything that shapes the face last frame: unchanged = skip the work

// ---- tables ----
byte ex(byte f, byte k) { return pgm_read_byte(&EX[f][k]); }
int8_t md(byte k) { return (int8_t)pgm_read_byte(&MODES[mode][k]); }
byte flags() { return (byte)md(14); }
byte cell(byte c) { return md(c); }          // 0-8 grid, 9 love, 10 cry

// ---- feeling engine ----
void feel(int dv, int da) {
  val = constrain(val + dv, -100, 100);
  aro = constrain(aro + da, -100, 100);
}

void flashFace(byte f, int ms) { flash = f; flashUntil = millis() + ms; }

byte feelFace() {                // the face this feeling maps to in the current mode
  if (val > 75 && aro > -35 && aro < 35) return cell(9);
  if (val < -75) return cell(10);
  byte r = aro > 35 ? 0 : (aro < -35 ? 2 : 1);
  byte c = val < -30 ? 0 : (val > 30 ? 2 : 1);
  return cell(r * 3 + c);
}

void tickFeel(unsigned long now) {   // every 100 ms: drift home, boredom, fading praise
  if (now < emoAt) return;
  emoAt = now + 100;
  unsigned long idle = now - lastInput;
  int bored = idle > 20000 && !demo ? -(int)min((idle - 20000) / 500, 60UL) : 0;
  int hv = md(11) + bonus, ha = md(12) + bored;
  int dv = hv - val, da = ha - aro;
  val += dv / 12 + (dv > 0) - (dv < 0);
  aro += da / 12 + (da > 0) - (da < 0);
  if (bonus > 0 && now % 3000 < 100) bonus--;
}

char chAt(bool user, byte i) { return user ? inLine[i] : say[i]; }

bool found(bool user, byte a, byte b, int st, byte len) {   // whole word LEX[st..st+len) in text[a..b)?
  for (byte i = a; i + len <= b; i++) {
    if (i > a && isalpha(chAt(user, i - 1))) continue;
    byte k = 0;
    while (k < len && tolower(chAt(user, i + k)) == pgm_read_byte(&LEX[st + k])) k++;
    if (k == len && (i + len == b || !isalpha(chAt(user, i + len)))) return true;
  }
  return false;
}

void react(bool user, byte w) {  // a feeling word was found
  int dv = (int8_t)pgm_read_byte(&LEXV[w]), da = (int8_t)pgm_read_byte(&LEXA[w]);
  byte kind = pgm_read_byte(&LEXK[w]);
  if (!user) { feel(dv / 2, da / 2); return; }                // its own words colour it a little
  if (kind == 1) feel(dv / 4, 25);                            // you're sad or scared: it worries for you
  else if (kind == 3) { feel(25, 30); flashFace(cell(1), 250); }   // its name: perks up
  else if (kind == 4) { curiousUntil = millis() + 1500; feel(0, 15); }
  else feel(dv * 3 / 4, da * 3 / 4);
  if (dv > 0) bonus = min(bonus + dv / 5, 40);
}

void scanWords(bool user, byte a, byte b) {
  int n = strlen_P(LEX), st = 0;
  byte w = 0;
  for (int i = 0; i <= n; i++) {
    char c = i < n ? pgm_read_byte(&LEX[i]) : '|';
    if (c != '|') continue;
    if (found(user, a, b, st, i - st)) react(user, w);
    w++; st = i + 1;
  }
}

void hearYou() {                 // a whole "> ..." line from you has arrived
  unsigned long now = millis();
  scanWords(true, 1, inLen);
  bool shout = false, ask = false;
  byte caps = 0;
  for (byte i = 1; i < inLen; i++) {
    char c = inLine[i];
    caps = isupper(c) ? caps + 1 : 0;
    if (caps >= 3) shout = true;
    if (c == '?') ask = true;
    if (c == '!') feel(0, 10);
  }
  if (shout) { flashFace(E_SHOCK, 400); feel(0, 30); }
  if (ask) { curiousUntil = now + 1500; flashFace(cell(15), 600); }
  typingUntil = 0;
  thinkUntil = now + 700;        // a beat to "think" before answering
  lastInput = now;
}

// ---- universal expressions: any custom base eye -> every expression ----
// Each built-in eye shape stands for an expression; on a custom face the engine applies that
// expression's operator to YOUR base eye instead of drawing the built-in shape.
#define OP_BASE 0
#define OP_WIDE 1
#define OP_CLOSED 2
#define OP_HAPPY 3
#define OP_SLEEPY 4
#define OP_ANGRY 5
#define OP_SAD 6
#define OP_LID 7
#define OP_KEEP 8                // hearts, spirals, stars, chevrons stay universal overlays
const byte OP_OF[] PROGMEM = {OP_BASE, OP_WIDE, OP_CLOSED, OP_HAPPY, OP_HAPPY, OP_ANGRY, OP_KEEP, OP_KEEP, OP_KEEP, OP_LID,
                              OP_BASE, OP_SAD, OP_ANGRY, OP_SLEEPY, OP_HAPPY, OP_KEEP, OP_BASE, OP_SLEEPY,
                              OP_BASE, OP_WIDE, OP_ANGRY, OP_SAD, OP_HAPPY, OP_SLEEPY, OP_WIDE};   // per EYES index
const uint16_t CUT[2][3] PROGMEM = {{0b1111000000, 0b1111110000, 0b1111111100},     // angry: inner-top corner (left eye)
                                    {0b0000001111, 0b0000111111, 0b0011111111}};    // sad: outer-top corner
uint16_t cbL[8];                // custom base eyes (EEPROM): left
uint16_t cbR[8];                // right
bool hasCustom = false;
uint16_t er[8];

void derive(byte op, bool right) {   // er[] = op applied to the custom base eye
  const uint16_t *b = right ? cbR : cbL;
  uint16_t w = 0;
  byte top = 8;
  for (byte r = 0; r < 8; r++) { er[r] = b[r]; w |= b[r]; if (b[r] && top == 8) top = r; }
  if (op == OP_WIDE)
    for (byte r = 0; r < 8; r++) er[r] = (b[r] | b[r] << 1 | b[r] >> 1 | (r ? b[r - 1] : 0) | (r < 7 ? b[r + 1] : 0)) & 0x3FF;
  if (op == OP_CLOSED) for (byte r = 0; r < 8; r++) er[r] = r == 4 ? w : 0;
  if (op == OP_HAPPY)            // keep only the eye's top edge, lowered 2 rows: round eyes become ^
    for (int8_t r = 7; r >= 0; r--) er[r] = r >= 2 ? b[r - 2] & ~(r >= 3 ? b[r - 3] : 0) : 0;
  if (op == OP_SLEEPY || op == OP_LID) {
    byte lid = op == OP_SLEEPY ? 4 : 2;
    for (byte r = 0; r < lid; r++) er[r] = 0;
    er[lid] |= w;
  }
  if (op == OP_ANGRY || op == OP_SAD)
    for (byte i = 0; i < 3 && top + i < 8; i++) {
      uint16_t m = pgm_read_word(&CUT[op == OP_SAD ? 1 : 0][i]);
      er[top + i] &= right ? mirror10(m) : m;
    }
}

void loadCustom() {              // EEPROM: 'G', then 8 rows left eye, 8 rows right eye (uint16 little-endian)
  hasCustom = EEPROM.read(0) == 'G';
  for (byte i = 0; i < 16; i++) (i < 8 ? cbL : cbR)[i & 7] = EEPROM.read(1 + i * 2) | (EEPROM.read(2 + i * 2) << 8);
}

// ---- drawing ----
uint16_t mirror10(uint16_t v) {
  uint16_t r = 0;
  for (byte i = 0; i < 10; i++) if (v & (1 << i)) r |= 1 << (9 - i);
  return r;
}

void upload(byte slot) {       // send g[] to a CGRAM slot only if it changed
  if (memcmp(cg[slot], g, 8) == 0) return;
  memcpy(cg[slot], g, 8);
  lcdChar(slot, g);
}

void sendPair(byte slot) {     // rows[] (10 px) -> two 5 px slots
  for (byte r = 0; r < 8; r++) g[r] = (rows[r] >> 5) & 31;
  upload(slot);
  for (byte r = 0; r < 8; r++) g[r] = rows[r] & 31;
  upload(slot + 1);
}

void buildEye(byte shape, bool mirror, byte mask, int8_t look, bool right) {   // mask: 0 open, 1 half, 2 closed
  byte op = pgm_read_byte(&OP_OF[shape]);
  bool custom = mode == M_CUSTOM && op != OP_KEEP;
  if (custom) derive(op, right);
  for (byte r = 0; r < 8; r++) {
    int8_t src = r - bob;
    uint16_t v = src < 0 ? 0 : custom ? er[src] : pgm_read_word(&EYES[shape][src]);
    if (mirror && !custom) v = mirror10(v);
    if (look > 0) v = v >> look;
    if (look < 0) v = (v << -look) & 0x3FF;
    bool brow = shape >= FIRST_BROW && !custom;
    byte top = brow ? 4 : 3;                    // brow eyes sit lower, under the brow
    if (mask && brow && r < 3 + bob) {}         // brows stay put
    else if (mask == 1 && (r < top + bob || r > top + 2 + bob)) v = 0;
    else if (mask == 2) v = r == top + 1 + bob ? 0b0111111110 : 0;
    rows[r] = v;
  }
}

void buildMouth(byte shape) {
  for (byte r = 0; r < 8; r++) {
    int8_t src = r - bob;
    rows[r] = src >= 0 ? pgm_read_word(&MOUTHS[shape][src]) : 0;
  }
}

void putCh(char c, byte col, byte row) { lcdOpen(); lcdPut(0x80 | (col + row * 0x40), 0); lcdPut(c, 1); lcdClose(); }
void putFx(byte slot, byte col, byte row) { putCh(slot, col, row); }

void clearFx() {               // every cell an effect may use
  const byte cells[][2] = {{1, 0}, {13, 0}, {14, 0}, {15, 0}, {2, 1}, {3, 1}, {12, 1}, {13, 1}};
  for (byte i = 0; i < 8; i++) putCh(' ', cells[i][0], cells[i][1]);
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
  } else if (fx == FX_LOVE || fx == FX_SPARKLE) {
    if (tick) {                            // heart hops / sparkle twinkles between the corners
      if (changed) loadFx(7, fx == FX_LOVE ? FX_HEART : FX_SPARK, 0);
      putCh(' ', fxFrame % 2 ? 14 : 1, 0);
      putFx(7, fxFrame % 2 ? 1 : 14, 0);
      fxFrame++; fxAt = now + (fx == FX_LOVE ? 700 : 400);
    }
  } else if (fx == FX_ZZZ) {
    if (tick) {
      lcdText(14, 0, fxFrame % 3 == 0 ? PSTR("z ") : (fxFrame % 3 == 1 ? PSTR("zZ") : PSTR(" Z")));
      fxFrame++; fxAt = now + 650;
    }
  } else if (fx == FX_THINK) {             // thinking dots grow . .. ...
    if (tick) {
      lcdText(13, 0, fxFrame % 4 == 0 ? PSTR("   ") : (fxFrame % 4 == 1 ? PSTR(".  ") : (fxFrame % 4 == 2 ? PSTR(".. ") : PSTR("..."))));
      fxFrame++; fxAt = now + 350;
    }
  } else if (fx == FX_CURIOUS) {
    if (changed) putCh('?', 14, 0);
  } else if (fx == FX_SWEAT) {
    if (changed) { loadFx(6, FX_TEAR, 0); putFx(6, 14, 0); }
  } else if (fx == FX_HUM) {
    if (changed) { loadFx(7, FX_NOTE, 0); putFx(7, 14, 0); }
  }
}

byte pickFace(unsigned long now) {   // what should be on screen right now (before transitions)
  if (asleep) return E_SLEEP;
  if (now < flashUntil) return flash;
  if (now < forcedUntil) return forced;
  byte f = feelFace();
  if (f != cand) { cand = f; candSince = now; }
  return now - candSince > 250 ? cand : target;   // a feeling has to hold 250 ms to show: no flicker
}

void draw(unsigned long now) {
  byte want = pickFace(now);
  if (want != target) {          // anticipation: squash the old face, then squash (or pop) into the new
    target = want; trans = 1; transAt = now + 45;
  }
  if (trans == 1 && now >= transAt) { face = target; trans = 2; transAt = now + 60; }
  if (trans == 2 && now >= transAt) trans = 0;

  bool humming = now < humUntil && !asleep;
  bool yawning = now < yawnUntil;
  bool typing = now < typingUntil || now < thinkUntil;
  bool pop = trans == 2 && (int8_t)pgm_read_byte(&EXV[face][1]) > 50;   // into an excited face: eyes pop wide first
  byte mask = trans == 1 || (trans == 2 && !pop) ? 1 : 0;
  if (ex(face, 5) && blinkStage) mask = blinkStage == 2 ? 2 : 1;
  if (squint && mask == 0) mask = 1;
  if (humming && (flags() & F_SIGH)) mask = 2;    // eyes shut for a sigh
  int8_t look = (typing ? -2 : lookX) + (now < saccUntil ? microX : 0);

  byte eL = pop ? RING : ex(face, 0), eR = pop ? RING : ex(face, 1);
  if (yawning) { eL = LINE; eR = LINE; mask = 0; }
  if (humming && !(flags() & F_SIGH) && !yawning) { eL = ARC; eR = ARC; mask = 0; }
  if (now < nodUntil) bob = 1;
  bool mir = ex(face, 2) && !pop;
  byte mouth = yawning ? V_OPEN : (viseme >= 0 ? viseme : ex(face, 3));
  uint32_t key = eL | ((uint32_t)eR << 5) | ((uint32_t)mir << 10) | ((uint32_t)mask << 11) | ((uint32_t)(look + 4) << 13)
               | ((uint32_t)bob << 16) | ((uint32_t)mouth << 17) | ((uint32_t)mode << 22);
  if (key != lastKey) {
    lastKey = key;
    buildEye(eL, false, mask, look, false);
    sendPair(0);
    buildEye(eR, mir, mask, look, true);
    sendPair(2);
    buildMouth(mouth);
    sendPair(4);
  }

  byte fx = ex(face, 4);
  if (typing && !talking) fx = FX_THINK;
  else if (now < curiousUntil) fx = FX_CURIOUS;
  else if (humming && !(flags() & F_SIGH)) fx = FX_HUM;
  else if (fx == FX_NONE && (flags() & F_BLUSH)) fx = FX_SHY;
  drawFx(fx, now);
}

void layout() {                  // the screen text never changes: 3 cell pairs pointing at CGRAM 0-5
  lcdCmd(0x01);
  lcdOpen();
  lcdPut(0x80 | 3, 0); lcdPut(0, 1); lcdPut(1, 1);
  lcdPut(0x80 | 11, 0); lcdPut(2, 1); lcdPut(3, 1);
  lcdPut(0x80 | 0x47, 0); lcdPut(4, 1); lcdPut(5, 1);
  lcdClose();
}

// ---- speech: letter timer + mumble voice ----
int8_t vowel(char c) {           // index in VOWELS, -1 = not a vowel
  c = tolower(c);
  for (byte i = 0; i < 6; i++) if (VOWELS[i] == c) return i;
  return -1;
}

bool isEnd(char c) { return c == ',' || c == '.' || c == '!' || c == '?'; }

int charTime(char c) {           // letter ~1/13 s, comma 3x, sentence end 6x; excited talks faster
  int t = 75;
  if (c == ',') t = 225;
  if (c == '.' || c == '!' || c == '?') t = 450;
  return ((long)t * (ex(face, 7) - aro / 8) * 41) >> 12;   // x/100 as *41>>12
}

void startPhrase(byte from) {    // read ahead to the phrase end: sets the tune, its words move the feeling
  byte j = from;
  while (j < sayLen && !isEnd(say[j])) j++;
  phraseK = 0;
  phraseHigh = j < sayLen && say[j] == '!';
  fading = j + 1 < sayLen && say[j] == '.' && say[j + 1] == '.';
  scanWords(false, from, j);
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

void note(long f, unsigned long now, int len, int8_t mouth, bool rise, byte v) {
  sylFreq = f; sylStart = now; sylEnd = now + len; sylViseme = mouth;
  if (vset) f = min((f * 1382) >> 8, 1200L);     // buzzer voice: ~5.4x up, into the buzzer's loud range
  if (!muted) voiceStart(f, len, rise, v + vset);
}

void startSyllable(byte pos, int8_t v, unsigned long now) {
  bool mono = flags() & F_MONO;
  long f = (long)voiceBase * (ex(face, 6) + aro / 8) / 100;   // excited = higher, low = deeper
  bool shout = wordIs(pos, true);
  if (!mono) {
    if (wobble) {                                // old voice: slow steady ~2 Hz wobble, +-4 %
      int tri = now % 500; tri = tri < 250 ? tri : 500 - tri;
      f = f + f * (tri - 125) * 4 / 12500;
    }
    f = (f * (256 - min(phraseK * 8, 54))) >> 8;  // the tune drifts down through the phrase (-3 %/syllable, max -21 %)
    if (phraseHigh) f = (f * 307) >> 8;           // +20 %
    if (fading) f = (f * 218) >> 8;               // -15 %
    if (shout) f = (f * 333) >> 8;                // +30 %
    f = f + ((f * rnd(-8, 9)) >> 8);              // +-3 %: no two buzzes alike
  }
  phraseK++;
  squint = shout;
  note(f, now, max(50L, ((long)(fading ? 100 : 160) * ex(face, 7) * pace) / 10000), V_MOUTH[v], !mono && wordIs(pos, false), v);
}

void tickVoice(unsigned long now) {   // mouth follows the note's envelope; the timer plays it
  if (sylFreq > 0 && now >= sylEnd) { sylFreq = 0; squint = false; if (!talking) viseme = -1; }
  if (sylFreq > 0) {
    unsigned long t = now - sylStart, len = sylEnd - sylStart;
    bool partOpen = t < len * 3 / 10 || t > len * 7 / 10;     // swell / release
    viseme = sylViseme < 0 ? -1 : (partOpen ? V_SMALL : sylViseme);
  } else if (talking) {
    viseme = sayPos > 0 && sayPos == sayLen ? FLAT : (sayPos > 0 && isalpha(say[sayPos - 1]) ? V_SMALL : FLAT);
  }
}

int paceFor(long usPerChar) { return constrain(icbrt(usPerChar * 1000 / 75), 25, 160); }   // 100 * (g/75 ms)^(1/3)

int icbrt(long v) { int r = 0; while (r < 161 && (long)(r + 1) * (r + 1) * (r + 1) <= v) r++; return r; }

void hear(char c) {              // one character of its own speech arrives (stream or whole line)
  unsigned long now = millis();
  if (!talking) {                // new utterance
    talking = true; sayLen = sayPos = 0; needPhrase = true; firstRx = now; rxCount = 0; pace = ratePace;
    nextChar = max(now, thinkUntil); typingUntil = 0;
    Serial.print(F("> "));
  }
  if (sayLen == MAXLEN && sayPos > 0) {          // drop what was already said
    for (byte i = sayPos; i < sayLen; i++) say[i - sayPos] = say[i];
    sayLen -= sayPos; sayPos = 0;
  }
  if (sayLen < MAXLEN) say[sayLen++] = c;
  if (rxCount < 255) rxCount++;
  unsigned long span = now - firstRx;
  if (span >= 30 && rxCount > 1) {   // a stream (a whole line arrives as one ~instant burst: keep the rate pace)
    long g = span / (rxCount - 1);   // ms per arriving char; speech is ~75
    pace = paceFor(g * 1000);        // 5 tok/s ~87 %, 20 ~55 %, 100 ~32 %
  }
}

void tickSpeech(unsigned long now) {
  if (!talking || now < nextChar) return;
  if (sayPos >= sayLen) {        // caught up: wait for more, or finish when the stream goes quiet
    if (now - lastRx > (lineOpen ? 1500 : 300)) {
      talking = false; lineOpen = false;
      viseme = -1;
      Serial.println();
    }
    return;
  }
  if (needPhrase) { startPhrase(sayPos); needPhrase = false; }
  char c = say[sayPos];
  int8_t v = vowel(c);
  if (v >= 0 && (sayPos == 0 || vowel(say[sayPos - 1]) < 0)) startSyllable(sayPos, v, now);
  sayPos++;
  Serial.write(c);
  if (c == '!') { flashFace(cell(1), 450); feel(0, 8); }
  if (c == '?') flashFace(cell(15), 700);
  if (isEnd(c)) needPhrase = true;
  int p = sayLen - sayPos > 100 ? 25 : pace;      // buffer nearly full: catch up
  nextChar = now + (((long)charTime(c) * p * 41) >> 12);
  lastInput = now;
}

void tickHum(unsigned long now) {   // idle hum (or sigh) on the same voice engine
  if (asleep || talking) { hummNote = 3; return; }
  bool sigh = flags() & F_SIGH;
  if (hummNote < 3 && now >= hummAt) {
    note(sigh ? SIGH[hummNote] : HUM[hummNote], now, sigh ? 380 : 260, -1, false, sigh ? 0 : 4);
    hummNote++; hummAt = now + (sigh ? 400 : 300);
  }
}

// ---- input ----
int8_t findName(const char* names, byte from, bool list) {   // match inLine[from..] against "a|b|c" in flash
  int8_t k = 0;
  byte j = from;
  bool ok = true;
  int n = strlen_P(names);
  for (int i = 0; i <= n; i++) {
    char c = i < n ? pgm_read_byte(&names[i]) : '|';
    if (list) Serial.write(c == '|' ? '\n' : c);
    if (c == '|') { if (ok && j == inLen) return k; k++; j = from; ok = true; }
    else { if (j >= inLen || inLine[j] != c) ok = false; j++; }
  }
  return -1;
}

long numAt(byte from) {
  long n = 0;
  for (byte i = from; i < inLen; i++) if (isdigit(inLine[i])) n = n * 10 + (inLine[i] - '0');
  return n;
}

void setMode(byte m) {
  mode = m % N_MODES;
  if (mode == M_CUSTOM && !hasCustom) mode = 0;
  putCh(flags() & F_MOLE ? 0xA5 : ' ', 14, 1);   // beauty mark (ROM middle dot), right cheek
  voiceBase = (byte)md(13) * 2;
  wobble = false;
  val = md(11); aro = md(12);
  flashFace(cell(1), 400);
}

void wake(unsigned long now) {
  lastInput = now;
  if (asleep) { asleep = false; flashFace(E_SHOCK, 600); }
}

const char CMDS[] PROGMEM = "rate|typing|list|sleep|wake|mute|unmute|mode|voice|pitch|eyes";
const char VOICES[] PROGMEM = "guard|default|woman|child|old|buzzer";
const byte VOICE_HZ[] = {50, 65, 100, 130, 57, 65};   // Hz / 2 (buzzer: the profile pitch, played up in the loud range)

void handleCmd() {               // inLine holds ":word [arg]"
  unsigned long now = millis();
  byte sp = 1, full = inLen;
  while (sp < inLen && inLine[sp] != ' ') sp++;
  inLen = sp;
  int8_t c = findName(CMDS, 1, false);
  inLen = full;
  if (c == 0) {                  // :rate N  tokens/s for non-streamed text (0 = normal)
    long n = numAt(6);
    ratePace = n > 0 ? paceFor(250000 / n) : 100;   // ~4 chars per token
    pace = ratePace;
    return;
  }
  if (c == 1) { typingUntil = now + 8000; lastInput = now; return; }
  demo = false;                  // any other command takes over from the demo
  wake(now);
  switch (c) {
    case 2: findName(NAMES, 1, true); return;
    case 3: asleep = true; return;
    case 4: return;
    case 5: muted = true; voiceStop(); return;
    case 6: muted = false; return;
    case 7: { int8_t m = findName(MODE_NAMES, sp + 1, false); setMode(m >= 0 ? m : mode + 1); return; }   // unknown/next cycles
    case 8: { int8_t v = findName(VOICES, sp + 1, false); if (v < 0) v = 5; voiceBase = VOICE_HZ[v] * 2; wobble = v == 4; vset = v == 5 ? 6 : 0; return; }
    case 9: voiceBase = constrain(vset ? numAt(7) * 10 / 54 : numAt(7), 40, 3000); return;   // Hz you hear
    case 10: {                   // :eyes <24 hex = left eye, 3 per row> [<24 hex = right eye>]  (one eye = mirrored)
      byte h[48];
      byte n = 0;
      for (byte i = sp + 1; i < inLen && n < 48; i++) { char c = tolower(inLine[i]); if (isxdigit(c)) h[n++] = c <= '9' ? c - '0' : c - 'a' + 10; }
      if (n != 24 && n != 48) { Serial.println(F("bad hex")); return; }
      for (byte r = 0; r < 8; r++) {
        cbL[r] = ((h[r * 3] << 8) | (h[r * 3 + 1] << 4) | h[r * 3 + 2]) & 0x3FF;
        cbR[r] = n == 48 ? ((h[24 + r * 3] << 8) | (h[25 + r * 3] << 4) | h[26 + r * 3]) & 0x3FF : mirror10(cbL[r]);
      }
      EEPROM.update(0, 'G');
      for (byte i = 0; i < 16; i++) { uint16_t v = (i < 8 ? cbL : cbR)[i & 7]; EEPROM.update(1 + i * 2, v & 255); EEPROM.update(2 + i * 2, v >> 8); }
      hasCustom = true; setMode(M_CUSTOM); Serial.println(F("custom face saved"));
      return;
    }
  }
  int8_t f = findName(NAMES, 1, false);
  if (f < 0) { Serial.println(F("unknown, try :list")); return; }
  forced = f; forcedUntil = now + 5000;        // show it now, and move the feeling there
  val = (int8_t)pgm_read_byte(&EXV[f][0]); aro = (int8_t)pgm_read_byte(&EXV[f][1]);
}

void readSerial() {              // ":cmd" = command, "> text" = you talking to it, anything else = it speaks
  while (Serial.available() > 0) {
    char c = Serial.read();
    unsigned long now = millis();
    lastRx = now;
    bool eol = c == '\n' || c == '\r';
    if (inLen > 0) {             // inside a command or one of your lines
      if (!eol) {
        if (inLen < MAXLEN) inLine[inLen++] = c;
        if (inLine[0] == '>' && c == ' ') nodUntil = now + 140;   // nods along as you talk
        continue;
      }
      if (inLine[0] == '>') { if (demo) demo = false; hearYou(); } else handleCmd();
      inLen = 0;
    } else if (eol) {
      lineOpen = false;          // utterance complete: finish as soon as it is spoken
    } else if ((c == ':' || c == '>') && !lineOpen) {
      inLine[0] = c; inLen = 1;
      if (c == '>') {            // you start talking: it stops mid-sentence to listen
        wake(now); typingUntil = 0;
        if (talking) { talking = false; sayLen = sayPos = 0; sylFreq = 0; voiceStop(); viseme = -1; Serial.println(); }
        if (demo) demo = false;
      }
    } else {
      if (demo) { demo = false; talking = false; Serial.println(); }   // real speech cuts the demo off
      wake(now);
      lineOpen = true;
      hear(c);
    }
  }
}

// ---- idle director: body language follows the feeling ----
void tickIdle(unsigned long now) {
  bool hi = aro > 35, lo = aro < -35;
  if (blinkStage == 0 && now >= blinkAt) { blinkStage = 1; blinkAt = now + (lo ? 120 : 40); }
  else if (blinkStage > 0 && now >= blinkAt) {
    blinkStage = (blinkStage + 1) % 4;
    if (blinkStage == 0) blinkAt = now + (hi ? rnd(900, 3000) : (lo ? rnd(3500, 8000) : rnd(2000, 6000)));
    else blinkAt = now + (blinkStage == 2 ? (lo ? 260 : 90) : (lo ? 120 : 40));   // tired: slow blinks
    if (blinkStage == 0 && rnd(0, 100) < 15) blinkAt = now + 250;              // double blink
  }
  if (now >= nextLook) {         // excited: darting eyes; calm: long steady looks
    byte r = rnd(0, 5);
    lookX = r == 0 ? -2 : (r == 1 ? 2 : (r == 2 ? -1 : 0));
    nextLook = now + (hi ? rnd(300, 1200) : (lookX == 0 ? rnd(2500, 6000) : rnd(600, 1800)));
  }
  if (now >= nextSacc) {         // tiny 1 px flicks: the eyes are never perfectly still
    microX = rnd(0, 2) ? 1 : -1; saccUntil = now + 130;
    nextSacc = now + (hi ? rnd(500, 1500) : rnd(1500, 4000));
  }
  bool bouncy = flags() & F_BOUNCY;
  if (now >= bobAt) { bob = 1 - bob; bobAt = now + (bob ? (bouncy ? 250 : 500) : (bouncy ? rnd(700, 1200) : rnd(2500, 4000))); }
  if (now >= nextHum && !talking && (flags() & (F_HUM | F_SIGH))) {
    humUntil = now + 1500; hummAt = now; hummNote = 0;
    nextHum = now + (bouncy ? rnd(8000, 15000) : rnd(20000, 40000));
  }
  if (lo && !talking && !asleep && now >= nextYawn) {   // tired: a yawn now and then
    yawnUntil = now + 1400; nextYawn = now + rnd(15000, 40000);
    note(110, now, 700, -1, false, 0);
  }
  if (!demo && !asleep && now - lastInput > 60000) asleep = true;
}

void tickDemo(unsigned long now) {   // play the next '|' piece of DEMO
  if (!demo || talking || now < demoAt) return;
  int n = strlen_P(DEMO);
  char kind = pgm_read_byte(&DEMO[demoPos]);
  bool line = kind == ':' || kind == '>';
  inLen = 0;
  while (demoPos < n) {
    char c = pgm_read_byte(&DEMO[demoPos++]);
    if (c == '|') break;
    if (line) inLine[inLen++] = c; else hear(c);
  }
  if (demoPos >= n) demoPos = 0;
  if (kind == ':') { handleCmd(); demo = true; }   // the demo's own commands don't end it
  if (kind == '>') hearYou();
  inLen = 0;
  lastRx = now;
  demoAt = now + (kind == '>' ? 1200 : 900);
}

void tickPower() {               // POWER switch on D3: LOW = off
  bool on = pinHigh(PWR_SW);
  if (on == powered) return;
  powered = on;
  if (!on) {
    voiceStop(); sylFreq = 0;
    lcdCmd(0x08); lcdLight(false);
    sleepUntilOn();
  } else {
    lcdCmd(0x0C); lcdLight(true);
    unsigned long now = millis();
    flashFace(E_SHOCK, 600); lastInput = now;
  }
}

void tickButton(unsigned long now) {   // MODE button on D2: tap = wake / next profile, hold 1 s = non-stop talking on/off
  bool up = pinHigh(MODE_BTN);
  if (!up && btnWas && now - btnAt > 50) { btnDown = now; btnLong = false; lastInput = now; }   // press starts
  if (!up && btnDown && !btnLong && now - btnDown > 900) {                                     // held: toggle the demo loop
    btnLong = true;
    demo = !demo;
    if (asleep) asleep = false;
    if (demo) { demoAt = now + 400; flashFace(cell(2), 600); }
    else if (talking) { talking = false; sayLen = sayPos = 0; sylFreq = 0; voiceStop(); viseme = -1; Serial.println(); }
  }
  if (up && !btnWas && btnDown) {                                                              // released
    if (!btnLong) { if (asleep) wake(now); else setMode(mode + 1); }
    btnDown = 0; btnAt = now; lastInput = now;
  }
  btnWas = up;
}

void setup() {
  Serial.begin(9600);
  ioInit();
  lcdBegin();
  lcdLight(true);
  memset(cg, 255, sizeof(cg));   // force first upload
  loadCustom();
  lastKey = 0xFFFFFFFF;
  layout();
  rs16 = analogRead(A0) | 1;
  unsigned long now = millis();
  blinkAt = now + 1500;
  nextLook = now + 2000;
  nextHum = now + 25000;
  nextYawn = now + 20000;
  demoAt = now + 1200;
  lastInput = now;
  setMode(0);
  Serial.println(F("GLYPH-LCD ready. text / > you / :list"));
}

void loop() {
  tickPower();
  if (!powered) { nap(50); return; }
  unsigned long now = millis();
  readSerial();
  tickButton(now);
  tickDemo(now);
  tickFeel(now);
  tickSpeech(now);
  tickVoice(now);
  tickHum(now);
  tickIdle(now);
  draw(now);
  nap(15);
}

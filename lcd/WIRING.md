# GLYPH-LCD wiring

Freenove Control Board V4 (Uno), an 830-point breadboard, a 1602 I2C LCD, a passive buzzer, an S8050 NPN transistor, a 1k resistor, 2 slide switches and 1 push button.
Hole names use the kit breadboard's lettering: **row a = the bottom row, j = the top**, columns 1-63. Rails from top to bottom: red +, blue −, (rows j…a), red +, blue −.
The [Kit Lab](lab/) has an interactive build guide for both layouts: each wire glows on its own, with its holes marked.

## Layout A: as built (verified on hardware with `selftest/`)

| Step | Part / wire | Holes |
|---|---|---|
| 1 Power | board 5V → bottom red + rail · board GND → bottom blue − rail · bridges at the right end: + to +, − to − | |
| 2 LCD | GND → top − rail · VCC → top + rail · SDA → j15 · SCL → j17 · h15 → board SDA · h17 → board SCL | |
| 3 Voice | S8050 flat side toward row a: E e45, B e46, C e47 · buzzer + g38, − g40 · 1k b46–b53 · i40 → d47 · a45 → − rail · board D9 → a53 | |
| 4 SOUND switch | pins h30 h31 h32 · j30 → top + rail · i31 → i38 | |
| 5 POWER switch | pins c23 c24 c25 · a23 → − rail · b24 → board D3 | |
| 6 MODE button | legs d7 d9 g7 g9 · h7 → board D2 · c9 → − rail | |

About 21 jumpers.

## Layout B: compact (12 jumpers, columns 1-17, bottom rails only)

Parts share columns instead of jumpers: the buzzer − shares column 14 with the transistor's C, and the buzzer + shares column 16 with the SOUND switch's middle pin. One ground wire (a5) serves both the POWER switch and the button. The LCD's SDA/SCL plug straight into the board.

| Step | Part / wire | Holes |
|---|---|---|
| 1 Power | board 5V → bottom + rail (col 1) · board GND → bottom − rail (col 2) |
| 2 LCD | GND → bottom − rail · VCC → bottom + rail · SDA → board SDA pin · SCL → board SCL pin |
| 3 Voice | S8050 flat side toward row a: E e12, B e13, C e14 · buzzer − a14, + a16 · 1k d9–d13 · board D9 → a9 · d12 → − rail |
| 4 SOUND switch | pins d15 d16 d17 · e17 → + rail |
| 5 POWER switch | pins b5 b6 b7 · a5 → − rail (shared ground) · a6 → board D3 |
| 6 MODE button | legs e3 e5 (bottom) + f3 f5 (top) · h3 → board D2 |

## Check it

Flash `selftest/selftest.ino`. It checks the LCD on I2C, the D9 → 1k → transistor path, the MODE button and the POWER switch, and plays beeps for the buzzer and SOUND switch, with prompts on the LCD.

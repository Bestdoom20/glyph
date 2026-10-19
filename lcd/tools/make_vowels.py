"""Render one pitch period of each vowel (buzz + 3 formant resonators + spectral tilt) as int8.
Run: python3 make_vowels.py > vowels.h      (128 samples per period at F0 125 Hz = 16 kHz)

Formants and tilt are fitted to measurements of the reference devlog's babble demos
(LPC on voiced frames, see ../VOICE-NOTES.md): F0 ~130 Hz, 2-4 kHz peak ~39 dB under the
low harmonics, vowel clusters below."""
import numpy as np, scipy.signal as sg

FS, F0, N = 16000, 125, 128
VOWELS = {  # order matches "aeiouy"; (F1, F2, F3) Hz
    'a': (595, 1377, 2447), 'e': (433, 1829, 3268), 'i': (292, 2154, 3574),
    'o': (427, 664, 2450), 'u': (300, 870, 2240), 'y': (290, 3170, 3726)}
BW = (80, 100, 140)
TILT_DB = 39.2

def glottal(n):  # Rosenberg pulse train: the "buzz"
    t = np.arange(n) % N / N; open_, close = 0.4, 0.16
    return np.where(t < open_, 0.5 * (1 - np.cos(np.pi * t / open_)),
           np.where(t < open_ + close, np.cos(0.5 * np.pi * (t - open_) / close), 0.0))

def render(fm, pole):
    y = np.diff(glottal(N * 40), prepend=0)
    for f, bw in zip(fm, BW):
        r = np.exp(-np.pi * bw / FS); th = 2 * np.pi * f / FS
        y = sg.lfilter([1 - r], [1, -2 * r * np.cos(th), r * r], y)
    y = sg.lfilter([1, -pole], [1], y)                 # tilt: k > 0 brightens, k < 0 darkens
    return y[-N:] - y[-N:].mean()

def tilt(p):  # same measure as the analysis: low-harmonic peak minus 2-4 kHz peak, dB
    S = 20 * np.log10(np.abs(np.fft.rfft(np.tile(p, 8) * np.hanning(N * 8))) + 1e-9)
    f = np.fft.rfftfreq(N * 8, 1 / FS)
    return S[(f > 100) & (f < 500)].max() - S[(f > 2000) & (f < 4000)].max()

# solve the tilt coefficient so the average tilt matches the reference
lo, hi = -0.9, 0.9
for _ in range(30):
    pole = (lo + hi) / 2
    t = np.mean([tilt(render(fm, pole)) for fm in VOWELS.values()])
    lo, hi = (lo, pole) if t < TILT_DB else (pole, hi)

rows = []
for v, fm in VOWELS.items():
    p = render(fm, pole); p = np.round(p / np.abs(p).max() * 127).astype(int)
    rows.append('  {' + ', '.join(map(str, p)) + '},  // ' + v)
print(f'// tilt k {pole:.3f}, mean tilt {t:.1f} dB (target {TILT_DB})')
print('const int8_t VOX[6][128] PROGMEM = {  // one period per vowel, "aeiouy", made by tools/make_vowels.py')
print('\n'.join(rows)); print('};')

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

def buzzer_period(fm, f0=700, scale=1.6):
    """Band-limited bright vowel for a passive buzzer: additive harmonics of f0 up to the 16 kHz
    synth's Nyquist at the highest pitch it plays (~1.15 x f0), shaped by the vowel's formants
    (scaled up, like a small voice) and lifted around 2.7 kHz, so the energy lands where buzzers are loud."""
    n_harm = int(8000 / (f0 * 1.15))
    t = np.arange(N) / N
    out = np.zeros(N)
    for k in range(1, n_harm + 1):
        f, gain = k * f0, 1.0
        for fc, bw in zip(fm, BW):
            fc *= scale
            gain *= 1 / np.sqrt((1 - (f / fc) ** 2) ** 2 + (f * bw * 1.5 / fc ** 2) ** 2)
        lift = 1 / np.sqrt((1 - (f / 2700) ** 2) ** 2 + (f / 2700 / 1.4) ** 2)   # the buzzer's loud spot
        out += gain ** 0.35 * lift ** 2 * np.sin(2 * np.pi * k * t)   # vowel colours, buzzer decides loudness
    out -= out.mean()
    return np.round(out / np.abs(out).max() * 127).astype(int)

def shrink(p, n):
    """Band-limit a 128-sample period to n samples by FFT truncation (keeps harmonics < n/2 exactly)."""
    S = np.fft.rfft(p)[: n // 2]
    S[0] = 0
    out = np.fft.irfft(S, n) * n / len(p)
    return out

def q(p):
    return np.round(p / np.abs(p).max() * 127).astype(int)

hum = [q(shrink(render(fm, pole), 64)) for fm in VOWELS.values()]
buz = [q(shrink(buzzer_period(fm).astype(float), 32)) for fm in VOWELS.values()]
print(f'// tilt k {pole:.3f}, mean tilt {t:.1f} dB (target {TILT_DB})')
print('// one period per vowel ("aeiouy"); the ISR interpolates between samples. Made by tools/make_vowels.py')
print('const int8_t VOXH[6][64] PROGMEM = {   // low reference hum: harmonics 1-31 (above that it is 40-60 dB down)')
print('\n'.join('  {' + ', '.join(map(str, r)) + '},  // ' + v for r, v in zip(hum, VOWELS)))
print('};')
print('const int8_t VOXB[6][32] PROGMEM = {   // bright buzzer voice: harmonics 1-15 (it uses up to 9)')
print('\n'.join('  {' + ', '.join(map(str, r)) + '},  // ' + v for r, v in zip(buz, VOWELS)))
print('};')

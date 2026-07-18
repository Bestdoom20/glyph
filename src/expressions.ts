// GLYPH expression registry — add a face = add a string.
// Eye frames are templates: "%" marks the mouth slot. Beats may give literal full faces.

export type Frame = string;
export type Channel = { frames: Frame[]; ms: number };
export type Beat = { face?: string; bubble?: string; ms: number };
export type Expression = {
  eyes?: Channel;
  mouth?: Channel;
  bubble?: string;
  beats?: Beat[];
  wobble?: number;    // px jitter via CSS var
  cls?: string;       // extra css hook: blush / tears / glitch / pink
};

export const TALK_MOUTHS = ['_', 'o', 'O', 'w', 'v', 'ω', '‿', '-'];

export const EXPRESSIONS: Record<string, Expression> = {
  idle:     { eyes: { frames: ['・%・'], ms: 0 }, mouth: { frames: ['_'], ms: 0 } },
  blink:    { beats: [{ face: '-_-', ms: 90 }] },
  blink2:   { beats: [{ face: '-_-', ms: 80 }, { face: '・_・', ms: 120 }, { face: '-_-', ms: 80 }] },
  blinkSlow:{ beats: [{ face: '˘_˘', ms: 260 }] },
  lookAround: { beats: [{ face: '・_・', ms: 300 }, { face: '¬_・', ms: 450 }, { face: '・_¬', ms: 450 }, { face: '・_・', ms: 200 }] },
  think:    { beats: [{ face: '・_・', ms: 400 }, { face: '˘_˘', bubble: '💭', ms: 900 }], eyes: { frames: ['˘%˘', '・%・'], ms: 700 }, bubble: '💭' },
  aha:      { beats: [{ face: '・o・', bubble: '💡', ms: 700 }] },
  talk:     { eyes: { frames: ['・%・'], ms: 0 }, mouth: { frames: TALK_MOUTHS, ms: 120 } },
  happy:    { eyes: { frames: ['^%^', '◕%◕'], ms: 900 }, mouth: { frames: ['‿', 'ω'], ms: 300 }, bubble: '♪', cls: 'bounce' },
  love:     { eyes: { frames: ['♡%♡', '→%←'], ms: 800 }, mouth: { frames: ['‿'], ms: 0 }, bubble: '♥', cls: 'pink' },
  shock:    { beats: [{ face: 'O_O', bubble: '!', ms: 350 }, { face: '@_@', ms: 300 }, { face: '⊙▽⊙', ms: 400 }], cls: 'pop' },
  confused: { eyes: { frames: ['o%O', '・%・', '¬%¬'], ms: 800 }, mouth: { frames: ['_'], ms: 0 }, bubble: '?', cls: 'tilt' },
  sad:      { eyes: { frames: [';%;', 'T%T', '╥%╥'], ms: 1100 }, mouth: { frames: ['_'], ms: 0 }, cls: 'tears' },
  error:    { eyes: { frames: ['x%x', '>%<', '#%#'], ms: 350 }, mouth: { frames: ['_'], ms: 0 }, bubble: '⚡', cls: 'glitch', wobble: 2 },
  sleep:    { eyes: { frames: ['u%u', '˘%˘', '-%-'], ms: 1600 }, mouth: { frames: ['_'], ms: 0 }, bubble: 'z', cls: 'snore' },
  yawn:     { beats: [{ face: '-o-', ms: 500 }, { face: '-O-', ms: 600 }, { face: '-_-', ms: 300 }] },
  wake:     { beats: [{ face: '0o0', bubble: '!', ms: 450 }, { face: 'O□O', ms: 350 }, { face: '・_・', ms: 200 }], cls: 'pop' },
  wink:     { beats: [{ face: '^_-', ms: 500 }] },
  smug:     { eyes: { frames: ['¬%¬', '-%0'], ms: 1200 }, mouth: { frames: ['‿'], ms: 0 } },
  shy:      { eyes: { frames: ['>%<', '/%/'], ms: 900 }, mouth: { frames: ['_'], ms: 0 }, cls: 'blush tilt' },
  dizzy:    { eyes: { frames: ['@%@'], ms: 0 }, mouth: { frames: ['_'], ms: 0 }, cls: 'tilt', wobble: 1 },
  giggle:   { beats: [{ face: '^w^', ms: 250 }, { face: '^ω^', ms: 250 }, { face: '^w^', ms: 250 }], cls: 'bounce' },
  hum:      { eyes: { frames: ['˘%˘'], ms: 0 }, mouth: { frames: ['ω', '‿'], ms: 400 }, bubble: '♪' },
  sigh:     { beats: [{ face: '-_-', ms: 400 }, { face: '˘o˘', bubble: '…', ms: 600 }, { face: '・_・', ms: 200 }] },
  drowsy:   { eyes: { frames: ['˘%˘', '-%-'], ms: 1400 }, mouth: { frames: ['_'], ms: 0 } },
};

// Personality = data-only bias on idle weights + timing. tempo multiplies delays; sleepAt = ms until sleep.
export type Personality = { weights: Record<string, number>; tempo: number; sleepAt: number };

const BASE_WEIGHTS = { blink: 5, blink2: 1.5, lookAround: 2, wink: 0.7, smug: 0.5, hum: 0.6, sigh: 0.5, giggle: 0.3, none: 3 };

export const PERSONALITIES: Record<string, Personality> = {
  default: { weights: BASE_WEIGHTS, tempo: 1, sleepAt: 60_000 },
  sleepy:  { weights: { ...BASE_WEIGHTS, blinkSlow: 4, sigh: 2, blink: 3 }, tempo: 1.5, sleepAt: 25_000 },
  hyper:   { weights: { ...BASE_WEIGHTS, lookAround: 5, giggle: 2, wink: 2, none: 1 }, tempo: 0.45, sleepAt: 180_000 },
  shy:     { weights: { ...BASE_WEIGHTS, shy: 3, lookAround: 3, wink: 0.2, smug: 0 }, tempo: 1.1, sleepAt: 60_000 },
  sassy:   { weights: { ...BASE_WEIGHTS, smug: 4, wink: 2, sigh: 1.5, none: 2 }, tempo: 0.9, sleepAt: 60_000 },
};

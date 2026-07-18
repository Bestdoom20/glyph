// One runnable check per non-trivial piece: compose, idle stages, weighted pick, emotion heuristic, stream contract.
import { compose, idleStage, weightedPick } from '../src/engine';
import { analyze, mirrorEmoji } from '../src/emotion';
import { EXPRESSIONS, PERSONALITIES } from '../src/expressions';
import { drive } from '../src/stream';

const assert = (cond: unknown, msg: string) => { if (!cond) { console.error('FAIL:', msg); process.exit(1); } };

// compose
assert(compose('・%・', 'o') === '・o・', 'compose basic');
assert(compose('¬%¬', '‿') === '¬‿¬', 'compose smug');

// idle stages (default sleepAt 60s → drowsy at 30s)
assert(idleStage(10_000) === 'normal', 'stage normal');
assert(idleStage(35_000) === 'drowsy', 'stage drowsy');
assert(idleStage(61_000) === 'sleep', 'stage sleep');
assert(idleStage(26_000, 25_000) === 'sleep', 'personality sleepAt honored');

// weighted pick — deterministic with fixed rnd, respects zero weights
assert(weightedPick({ a: 1, b: 0, c: 1 }, 0.0) === 'a', 'pick first');
assert(weightedPick({ a: 1, b: 0, c: 1 }, 0.99) === 'c', 'pick last, skip zero');

// emotion heuristic
assert(analyze('the request failed with an error') === 'error', 'error cue');
assert(analyze('WOW!! incredible') === 'shock', 'shock cue');
assert(analyze('hmm, not sure about that') === 'confused', 'confused cue');
assert(analyze('1. apples 2. oranges 3. bananas') === null, 'dry list stays neutral');
assert(mirrorEmoji('great 😄 stuff') === 'happy', 'emoji mirror happy');
assert(mirrorEmoji('plain text') === null, 'emoji mirror none');

// registry sanity — every expression is renderable
for (const [name, ex] of Object.entries(EXPRESSIONS)) {
  if (ex.eyes) for (const f of ex.eyes.frames) assert(f.includes('%'), `eyes frame missing mouth slot in ${name}`);
  assert(ex.eyes || ex.beats, `${name} has neither eyes nor beats`);
}
for (const p of Object.values(PERSONALITIES)) assert(p.tempo > 0 && p.sleepAt > 0, 'personality timing sane');

// stream contract: think → say×n → done, exactly once, even on empty chunks
const calls: string[] = [];
const fake = { think: () => calls.push('think'), say: (c: string) => calls.push('say:' + c), done: () => calls.push('done') };
async function* toks() { yield 'Hel'; yield ''; yield { choices: [{ delta: { content: 'lo' } }] }; }
await drive(fake, toks());
assert(calls.join(',') === 'think,say:Hel,say:lo,done', 'stream contract order: ' + calls.join(','));

console.log('selfcheck OK — all assertions passed');

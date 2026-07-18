// GLYPH engine — vanilla DOM, zero deps. Two channels (eyes+mouth) + idle director + interaction.
import { EXPRESSIONS, PERSONALITIES, TALK_MOUTHS, type Expression, type Beat } from './expressions';
import { analyze, mirrorEmoji } from './emotion';

export const compose = (eyesTpl: string, mouth: string) => eyesTpl.replace('%', mouth);
export const idleStage = (ms: number, sleepAt = 60_000): 'normal' | 'drowsy' | 'sleep' =>
  ms >= sleepAt ? 'sleep' : ms >= sleepAt / 2 ? 'drowsy' : 'normal';

export function weightedPick(weights: Record<string, number>, rnd: number): string {
  const entries = Object.entries(weights).filter(([, w]) => w > 0);
  const total = entries.reduce((s, [, w]) => s + w, 0);
  let r = rnd * total;
  for (const [k, w] of entries) { r -= w; if (r <= 0) return k; }
  return entries[entries.length - 1][0];
}

const CSS = `
.glyph{position:relative;display:inline-flex;flex-direction:column;align-items:center;user-select:none;
  font:700 var(--glyph-size,56px)/1.1 ui-monospace,'Cascadia Mono',Menlo,monospace;color:var(--glyph-color,currentColor)}
.glyph-face{white-space:pre;transition:transform .18s ease;animation:glyph-breathe 3.2s ease-in-out infinite;
  transform:translate(var(--gx,0),var(--gy,0)) rotate(var(--gr,0deg))}
.glyph-bubble{position:absolute;top:-0.9em;right:-0.7em;font-size:.5em;opacity:0;transform:scale(.4);
  transition:opacity .15s,transform .18s cubic-bezier(.34,1.56,.64,1)}
.glyph-bubble.on{opacity:1;transform:scale(1)}
.glyph-bubble.pulse{animation:glyph-pulse 1.2s ease-in-out infinite}
.glyph.snore .glyph-bubble.on{animation:glyph-rise 2.4s ease-in infinite}
.glyph.bounce .glyph-face{animation:glyph-bounce .5s ease infinite}
.glyph.pop .glyph-face{animation:glyph-pop .3s ease}
.glyph.tilt .glyph-face{rotate:8deg}
.glyph.pink{color:#ff7eb6}
.glyph.blush .glyph-face{text-shadow:0 0 14px #ff7eb6}
.glyph.tears .glyph-face{animation:glyph-drip 1.6s ease-in-out infinite}
.glyph.glitch .glyph-face{animation:glyph-jitter .12s steps(2) infinite}
@keyframes glyph-breathe{50%{transform:translate(var(--gx,0),calc(var(--gy,0) - .04em)) rotate(var(--gr,0deg)) scaleY(1.02)}}
@keyframes glyph-bounce{50%{transform:translateY(-.12em)}}
@keyframes glyph-pop{0%{transform:scale(.7)}60%{transform:scale(1.15)}100%{transform:scale(1)}}
@keyframes glyph-pulse{50%{transform:scale(1.15)}}
@keyframes glyph-rise{0%{transform:translateY(0);opacity:1}100%{transform:translateY(-1.2em);opacity:0}}
@keyframes glyph-drip{50%{transform:translateY(.06em)}}
@keyframes glyph-jitter{50%{transform:translate(calc(var(--gwob,1px)*1),calc(var(--gwob,1px)*-1))}}
@media (prefers-reduced-motion:reduce){.glyph-face,.glyph .glyph-face,.glyph-bubble{animation:none!important;transition:none!important}}
`;

let cssDone = false;
function injectCss() {
  if (cssDone || typeof document === 'undefined') return;
  const s = document.createElement('style');
  s.textContent = CSS;
  document.head.appendChild(s);
  cssDone = true;
}

export type GlyphOptions = { personality?: string; interactive?: boolean; autoIdleMs?: number };

export class GlyphEngine {
  state = 'idle';
  private wrap!: HTMLElement; private faceEl!: HTMLElement; private bubbleEl!: HTMLElement;
  private timers: ReturnType<typeof setTimeout>[] = [];
  private lastActivity = Date.now();
  private sleeping = false;
  private flashing = false;
  private sayBuf = '';
  private lastCue = 0;
  private doneT?: ReturnType<typeof setTimeout>;
  private idleT?: ReturnType<typeof setTimeout>;
  private reduced: boolean;
  private persona = PERSONALITIES.default;
  private detach: (() => void)[] = [];
  private opts: GlyphOptions;

  constructor(host: HTMLElement, opts: GlyphOptions = {}) {
    this.opts = opts;
    this.reduced = typeof matchMedia !== 'undefined' && matchMedia('(prefers-reduced-motion: reduce)').matches;
    injectCss();
    this.wrap = document.createElement('span');
    this.wrap.className = 'glyph';
    this.bubbleEl = document.createElement('span');
    this.bubbleEl.className = 'glyph-bubble';
    this.faceEl = document.createElement('span');
    this.faceEl.className = 'glyph-face';
    this.wrap.append(this.bubbleEl, this.faceEl);
    host.appendChild(this.wrap);
    this.setPersonality(opts.personality || 'default');
    if (opts.interactive !== false) this.attachInteraction();
    this.apply('idle');
    this.scheduleIdle();
  }

  // ---- public API -------------------------------------------------------
  setState(name: string) { this.activity(); this.state = EXPRESSIONS[name] ? name : 'idle'; this.apply(this.state); }
  emote(name: string) { this.flash(name, 900); }
  think() { this.setState('think'); }
  say(chunk: string) {
    this.activity();
    if (typeof chunk !== 'string' || !chunk) return;
    if (this.state !== 'talk') { this.state = 'talk'; this.apply('talk'); }
    clearTimeout(this.doneT);
    this.doneT = setTimeout(() => this.done(), this.opts.autoIdleMs ?? 800);
    const mirror = mirrorEmoji(chunk);
    if (mirror) return this.flash(mirror, 450);
    if (/\.{3}\s*$/.test(chunk)) return this.slowMouth(600);
    if (/!\s*$/.test(chunk)) return this.flash('shock', 400);
    if (/\?\s*$/.test(chunk)) return this.flash('confused', 450);
    if (/[.,]\s*$/.test(chunk)) this.closeMouth(140);
    this.sayBuf += chunk;
    if (this.sayBuf.length > 40 && Date.now() - this.lastCue > 2500) {
      const cue = analyze(this.sayBuf);
      this.sayBuf = this.sayBuf.slice(-240);
      if (cue) { this.lastCue = Date.now(); this.flash(cue, 600); }
    }
  }
  done() {
    clearTimeout(this.doneT);
    this.sayBuf = '';
    this.lastActivity = Date.now();
    this.timers.push(setTimeout(() => { if (this.state === 'talk') { this.state = 'idle'; this.apply('idle'); } }, 250));
  }
  setPersonality(name: string) { this.persona = PERSONALITIES[name] || PERSONALITIES.default; }
  destroy() {
    this.clearAnim(); clearTimeout(this.idleT); clearTimeout(this.doneT);
    this.detach.forEach(f => f()); this.wrap.remove();
  }

  // ---- render core ------------------------------------------------------
  private clearAnim() { this.timers.forEach(clearTimeout); this.timers = []; }

  private apply(name: string) {
    const ex = EXPRESSIONS[name] || EXPRESSIONS.idle;
    this.clearAnim();
    this.wrap.className = 'glyph' + (ex.cls ? ' ' + ex.cls : '');
    this.wrap.style.setProperty('--gwob', (ex.wobble || 1) + 'px');
    this.bubble(ex.bubble);
    if (ex.beats?.length) this.playBeats(ex.beats, () => this.startChannels(ex));
    else this.startChannels(ex);
  }

  private startChannels(ex: Expression) {
    const eyes = ex.eyes || EXPRESSIONS.idle.eyes!;
    const mouth = ex.mouth || { frames: ['_'], ms: 0 };
    let ei = 0, mi = 0;
    const render = () => { this.faceEl.textContent = compose(eyes.frames[ei], mouth.frames[mi]); };
    render();
    if (this.reduced) return;
    const cycle = (ch: { frames: string[]; ms: number }, adv: () => void) => {
      if (ch.frames.length < 2 || ch.ms <= 0) return;
      const t = setInterval(() => { adv(); render(); }, ch.ms * this.persona.tempo);
      this.timers.push(t as unknown as ReturnType<typeof setTimeout>);
    };
    cycle(eyes, () => { ei = (ei + 1) % eyes.frames.length; });
    cycle(mouth, () => { mi = (mi + 1) % mouth.frames.length; });
  }

  private playBeats(beats: Beat[], then?: () => void) {
    let i = 0;
    const step = () => {
      if (i >= beats.length) return then?.();
      const b = beats[i++];
      if (b.face) this.faceEl.textContent = b.face;
      if (b.bubble !== undefined) this.bubble(b.bubble);
      this.timers.push(setTimeout(step, this.reduced ? 0 : b.ms * this.persona.tempo));
    };
    step();
  }

  private bubble(b?: string) {
    this.bubbleEl.textContent = b || '';
    this.bubbleEl.className = 'glyph-bubble' + (b ? ' on' : '') + (b === '💭' ? ' pulse' : '');
  }

  private flash(name: string, ms: number) {
    if (this.flashing || !EXPRESSIONS[name]) return;
    this.flashing = true;
    this.apply(name);
    this.timers.push(setTimeout(() => { this.flashing = false; this.apply(this.state); }, ms));
  }

  private closeMouth(ms: number) {
    const cur = this.faceEl.textContent || '・_・';
    this.faceEl.textContent = cur.replace(new RegExp('[' + TALK_MOUTHS.join('') + ']'), '_');
    // channels resume on their own next tick; brief hold reads as a natural pause
    void ms;
  }

  private slowMouth(ms: number) {
    this.closeMouth(ms);
    this.timers.push(setTimeout(() => { if (this.state === 'talk') this.apply('talk'); }, ms));
  }

  // ---- idle director ----------------------------------------------------
  private activity() {
    this.lastActivity = Date.now();
    if (this.sleeping) { this.sleeping = false; this.state = 'idle'; this.apply('wake'); }
  }

  private scheduleIdle() {
    clearTimeout(this.idleT);
    const delay = (1800 + Math.random() * 2700) * this.persona.tempo;
    this.idleT = setTimeout(() => this.idleTick(), delay);
  }

  private idleTick() {
    if (this.state !== 'idle' || this.reduced || this.flashing) return this.scheduleIdle();
    const stage = idleStage(Date.now() - this.lastActivity, this.persona.sleepAt);
    if (stage === 'sleep') {
      if (!this.sleeping) {
        this.sleeping = true;
        this.apply('yawn');
        this.timers.push(setTimeout(() => { if (this.sleeping) { this.state = 'sleep'; this.apply('sleep'); } }, 1500 * this.persona.tempo));
      }
    } else if (stage === 'drowsy') {
      this.flash(Math.random() < 0.5 ? 'blinkSlow' : 'drowsy', 900);
    } else {
      const pick = weightedPick(this.persona.weights, Math.random());
      if (pick !== 'none') this.flash(pick, 1000);
    }
    this.scheduleIdle();
  }

  // ---- interaction (P1.5) ----------------------------------------------
  private attachInteraction() {
    if (typeof window === 'undefined') return;
    let raf = 0;
    const onMove = (e: PointerEvent) => {
      if (this.reduced) return;
      cancelAnimationFrame(raf);
      raf = requestAnimationFrame(() => {
        const r = this.wrap.getBoundingClientRect();
        const dx = e.clientX - (r.left + r.width / 2), dy = e.clientY - (r.top + r.height / 2);
        const cl = (v: number, m: number) => Math.max(-m, Math.min(m, v));
        this.faceEl.style.setProperty('--gx', cl(dx * 0.02, 6) + 'px');
        this.faceEl.style.setProperty('--gy', cl(dy * 0.02, 4) + 'px');
        this.faceEl.style.setProperty('--gr', cl(dx * 0.008, 5) + 'deg');
      });
    };
    const onEnter = () => { if (this.state === 'idle' && !this.sleeping) this.flash('happy', 600); };
    let downAt = 0;
    const onDown = () => { downAt = Date.now(); this.activity(); };
    const onUp = () => {
      const held = Date.now() - downAt;
      if (held > 600) this.flash('dizzy', 1200);
      else this.flash(Math.random() < 0.5 ? 'shock' : 'giggle', 800);
    };
    window.addEventListener('pointermove', onMove);
    this.wrap.addEventListener('pointerenter', onEnter);
    this.wrap.addEventListener('pointerdown', onDown);
    this.wrap.addEventListener('pointerup', onUp);
    this.detach.push(() => {
      window.removeEventListener('pointermove', onMove);
      this.wrap.removeEventListener('pointerenter', onEnter);
      this.wrap.removeEventListener('pointerdown', onDown);
      this.wrap.removeEventListener('pointerup', onUp);
    });
  }
}

export const glyph = { mount: (el: HTMLElement, opts?: GlyphOptions) => new GlyphEngine(el, opts) };

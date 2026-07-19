"use strict";
(() => {
  // src/expressions.ts
  var TALK_MOUTHS = ["_", "o", "O", "w", "v", "\u03C9", "\u203F", "-"];
  var EXPRESSIONS = {
    idle: { eyes: { frames: ["\u30FB%\u30FB"], ms: 0 }, mouth: { frames: ["_"], ms: 0 } },
    blink: { beats: [{ face: "-_-", ms: 90 }] },
    blink2: { beats: [{ face: "-_-", ms: 80 }, { face: "\u30FB_\u30FB", ms: 120 }, { face: "-_-", ms: 80 }] },
    blinkSlow: { beats: [{ face: "\u02D8_\u02D8", ms: 260 }] },
    lookAround: { beats: [{ face: "\u30FB_\u30FB", ms: 300 }, { face: "\xAC_\u30FB", ms: 450 }, { face: "\u30FB_\xAC", ms: 450 }, { face: "\u30FB_\u30FB", ms: 200 }] },
    think: { beats: [{ face: "\u30FB_\u30FB", ms: 400 }, { face: "\u02D8_\u02D8", bubble: "\u{1F4AD}", ms: 900 }], eyes: { frames: ["\u02D8%\u02D8", "\u30FB%\u30FB"], ms: 700 }, bubble: "\u{1F4AD}" },
    aha: { beats: [{ face: "\u30FBo\u30FB", bubble: "\u{1F4A1}", ms: 700 }] },
    talk: { eyes: { frames: ["\u30FB%\u30FB"], ms: 0 }, mouth: { frames: TALK_MOUTHS, ms: 120 } },
    happy: { eyes: { frames: ["^%^", "\u25D5%\u25D5"], ms: 900 }, mouth: { frames: ["\u203F", "\u03C9"], ms: 300 }, bubble: "\u266A", cls: "bounce" },
    love: { eyes: { frames: ["\u2661%\u2661", "\u2192%\u2190"], ms: 800 }, mouth: { frames: ["\u203F"], ms: 0 }, bubble: "\u2665", cls: "pink" },
    shock: { beats: [{ face: "O_O", bubble: "!", ms: 350 }, { face: "@_@", ms: 300 }, { face: "\u2299\u25BD\u2299", ms: 400 }], cls: "pop" },
    confused: { eyes: { frames: ["o%O", "\u30FB%\u30FB", "\xAC%\xAC"], ms: 800 }, mouth: { frames: ["_"], ms: 0 }, bubble: "?", cls: "tilt" },
    sad: { eyes: { frames: [";%;", "T%T", "\u2565%\u2565"], ms: 1100 }, mouth: { frames: ["_"], ms: 0 }, cls: "tears" },
    error: { eyes: { frames: ["x%x", ">%<", "#%#"], ms: 350 }, mouth: { frames: ["_"], ms: 0 }, bubble: "\u26A1", cls: "glitch", wobble: 2 },
    sleep: { eyes: { frames: ["u%u", "\u02D8%\u02D8", "-%-"], ms: 1600 }, mouth: { frames: ["_"], ms: 0 }, bubble: "z", cls: "snore" },
    yawn: { beats: [{ face: "-o-", ms: 500 }, { face: "-O-", ms: 600 }, { face: "-_-", ms: 300 }] },
    wake: { beats: [{ face: "0o0", bubble: "!", ms: 450 }, { face: "O\u25A1O", ms: 350 }, { face: "\u30FB_\u30FB", ms: 200 }], cls: "pop" },
    wink: { beats: [{ face: "^_-", ms: 500 }] },
    smug: { eyes: { frames: ["\xAC%\xAC", "-%0"], ms: 1200 }, mouth: { frames: ["\u203F"], ms: 0 } },
    shy: { eyes: { frames: [">%<", "/%/"], ms: 900 }, mouth: { frames: ["_"], ms: 0 }, cls: "blush tilt" },
    dizzy: { eyes: { frames: ["@%@"], ms: 0 }, mouth: { frames: ["_"], ms: 0 }, cls: "tilt", wobble: 1 },
    giggle: { beats: [{ face: "^w^", ms: 250 }, { face: "^\u03C9^", ms: 250 }, { face: "^w^", ms: 250 }], cls: "bounce" },
    hum: { eyes: { frames: ["\u02D8%\u02D8"], ms: 0 }, mouth: { frames: ["\u03C9", "\u203F"], ms: 400 }, bubble: "\u266A" },
    sigh: { beats: [{ face: "-_-", ms: 400 }, { face: "\u02D8o\u02D8", bubble: "\u2026", ms: 600 }, { face: "\u30FB_\u30FB", ms: 200 }] },
    drowsy: { eyes: { frames: ["\u02D8%\u02D8", "-%-"], ms: 1400 }, mouth: { frames: ["_"], ms: 0 } }
  };
  var BASE_WEIGHTS = { blink: 5, blink2: 1.5, lookAround: 2, wink: 0.7, smug: 0.5, hum: 0.6, sigh: 0.5, giggle: 0.3, none: 3 };
  var PERSONALITIES = {
    default: { weights: BASE_WEIGHTS, tempo: 1, sleepAt: 6e4 },
    sleepy: { weights: { ...BASE_WEIGHTS, blinkSlow: 4, sigh: 2, blink: 3 }, tempo: 1.5, sleepAt: 25e3 },
    hyper: { weights: { ...BASE_WEIGHTS, lookAround: 5, giggle: 2, wink: 2, none: 1 }, tempo: 0.45, sleepAt: 18e4 },
    shy: { weights: { ...BASE_WEIGHTS, shy: 3, lookAround: 3, wink: 0.2, smug: 0 }, tempo: 1.1, sleepAt: 6e4 },
    sassy: { weights: { ...BASE_WEIGHTS, smug: 4, wink: 2, sigh: 1.5, none: 2 }, tempo: 0.9, sleepAt: 6e4 }
  };

  // src/emotion.ts
  var EMOJI = [
    [/[😄😀😊🙂😁😆🥳]/u, "happy"],
    [/[😢😭😞☹️🥺]/u, "sad"],
    [/[🤔💭]/u, "think"],
    [/[😲😮😱🤯]/u, "shock"],
    [/[😍🥰❤️♥💕]/u, "love"],
    [/[😵🥴😖]/u, "dizzy"],
    [/[😉😏]/u, "wink"]
  ];
  function mirrorEmoji(text) {
    for (const [re, name] of EMOJI) if (re.test(text)) return name;
    return null;
  }
  function analyze(text) {
    const t = text.slice(-240);
    const lower = t.toLowerCase();
    if (/\berror\b|\bfailed\b|\bexception\b|\bsorry\b|\bcannot\b|\bunable\b/.test(lower)) return "error";
    if (/!{2,}|\bwow\b|\bincredible\b|\bamazing\b/.test(lower) || /\b[A-Z]{4,}\b/.test(t)) return "shock";
    if (/\blove\b|\badore\b|♥|❤/.test(lower)) return "love";
    if (/\bgreat\b|\bawesome\b|\bperfect\b|\bnice\b|\bcongrats\b|\bthanks\b/.test(lower)) return "happy";
    if (/\bhmm\b|\bnot sure\b|\bmaybe\b|\bunclear\b|\?$/.test(lower)) return "confused";
    return null;
  }

  // src/engine.ts
  var compose = (eyesTpl, mouth) => eyesTpl.replace("%", mouth);
  var idleStage = (ms, sleepAt = 6e4) => ms >= sleepAt ? "sleep" : ms >= sleepAt / 2 ? "drowsy" : "normal";
  function weightedPick(weights, rnd) {
    const entries = Object.entries(weights).filter(([, w]) => w > 0);
    const total = entries.reduce((s, [, w]) => s + w, 0);
    let r = rnd * total;
    for (const [k, w] of entries) {
      r -= w;
      if (r <= 0) return k;
    }
    return entries[entries.length - 1][0];
  }
  var CSS = `
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
  var cssDone = false;
  function injectCss() {
    if (cssDone || typeof document === "undefined") return;
    const s = document.createElement("style");
    s.textContent = CSS;
    document.head.appendChild(s);
    cssDone = true;
  }
  var GlyphEngine = class {
    constructor(host, opts = {}) {
      this.state = "idle";
      this.timers = [];
      this.lastActivity = Date.now();
      this.sleeping = false;
      this.flashing = false;
      this.sayBuf = "";
      this.lastCue = 0;
      this.persona = PERSONALITIES.default;
      this.detach = [];
      this.opts = opts;
      this.reduced = typeof matchMedia !== "undefined" && matchMedia("(prefers-reduced-motion: reduce)").matches;
      injectCss();
      this.wrap = document.createElement("span");
      this.wrap.className = "glyph";
      this.bubbleEl = document.createElement("span");
      this.bubbleEl.className = "glyph-bubble";
      this.faceEl = document.createElement("span");
      this.faceEl.className = "glyph-face";
      this.wrap.append(this.bubbleEl, this.faceEl);
      host.appendChild(this.wrap);
      this.setPersonality(opts.personality || "default");
      if (opts.interactive !== false) this.attachInteraction();
      this.apply("idle");
      this.scheduleIdle();
    }
    // ---- public API -------------------------------------------------------
    setState(name) {
      this.activity();
      this.state = EXPRESSIONS[name] ? name : "idle";
      this.apply(this.state);
    }
    emote(name) {
      this.flash(name, 900);
    }
    think() {
      this.setState("think");
    }
    say(chunk) {
      this.activity();
      if (typeof chunk !== "string" || !chunk) return;
      if (this.state !== "talk") {
        this.state = "talk";
        this.apply("talk");
      }
      clearTimeout(this.doneT);
      this.doneT = setTimeout(() => this.done(), this.opts.autoIdleMs ?? 800);
      const mirror = mirrorEmoji(chunk);
      if (mirror) return this.flash(mirror, 450);
      if (/\.{3}\s*$/.test(chunk)) return this.slowMouth(600);
      if (/!\s*$/.test(chunk)) return this.flash("shock", 400);
      if (/\?\s*$/.test(chunk)) return this.flash("confused", 450);
      if (/[.,]\s*$/.test(chunk)) this.closeMouth(140);
      this.sayBuf += chunk;
      if (this.sayBuf.length > 40 && Date.now() - this.lastCue > 2500) {
        const cue = analyze(this.sayBuf);
        this.sayBuf = this.sayBuf.slice(-240);
        if (cue) {
          this.lastCue = Date.now();
          this.flash(cue, 600);
        }
      }
    }
    done() {
      clearTimeout(this.doneT);
      this.sayBuf = "";
      this.lastActivity = Date.now();
      this.timers.push(setTimeout(() => {
        if (this.state === "talk") {
          this.state = "idle";
          this.apply("idle");
        }
      }, 250));
    }
    setPersonality(name) {
      this.persona = PERSONALITIES[name] || PERSONALITIES.default;
    }
    destroy() {
      this.clearAnim();
      clearTimeout(this.idleT);
      clearTimeout(this.doneT);
      this.detach.forEach((f) => f());
      this.wrap.remove();
    }
    // ---- render core ------------------------------------------------------
    clearAnim() {
      this.timers.forEach(clearTimeout);
      this.timers = [];
    }
    apply(name) {
      const ex = EXPRESSIONS[name] || EXPRESSIONS.idle;
      this.clearAnim();
      this.wrap.className = "glyph" + (ex.cls ? " " + ex.cls : "");
      this.wrap.style.setProperty("--gwob", (ex.wobble || 1) + "px");
      this.bubble(ex.bubble);
      if (ex.beats?.length) this.playBeats(ex.beats, () => this.startChannels(ex));
      else this.startChannels(ex);
    }
    startChannels(ex) {
      const eyes = ex.eyes || EXPRESSIONS.idle.eyes;
      const mouth = ex.mouth || { frames: ["_"], ms: 0 };
      let ei = 0, mi = 0;
      const render = () => {
        this.faceEl.textContent = compose(eyes.frames[ei], mouth.frames[mi]);
      };
      render();
      if (this.reduced) return;
      const cycle = (ch, adv) => {
        if (ch.frames.length < 2 || ch.ms <= 0) return;
        const t = setInterval(() => {
          adv();
          render();
        }, ch.ms * this.persona.tempo);
        this.timers.push(t);
      };
      cycle(eyes, () => {
        ei = (ei + 1) % eyes.frames.length;
      });
      cycle(mouth, () => {
        mi = (mi + 1) % mouth.frames.length;
      });
    }
    playBeats(beats, then) {
      let i = 0;
      const step = () => {
        if (i >= beats.length) return then?.();
        const b = beats[i++];
        if (b.face) this.faceEl.textContent = b.face;
        if (b.bubble !== void 0) this.bubble(b.bubble);
        this.timers.push(setTimeout(step, this.reduced ? 0 : b.ms * this.persona.tempo));
      };
      step();
    }
    bubble(b) {
      this.bubbleEl.textContent = b || "";
      this.bubbleEl.className = "glyph-bubble" + (b ? " on" : "") + (b === "\u{1F4AD}" ? " pulse" : "");
    }
    flash(name, ms) {
      if (this.flashing || !EXPRESSIONS[name]) return;
      this.flashing = true;
      this.apply(name);
      this.timers.push(setTimeout(() => {
        this.flashing = false;
        this.apply(this.state);
      }, ms));
    }
    closeMouth(ms) {
      const cur = this.faceEl.textContent || "\u30FB_\u30FB";
      this.faceEl.textContent = cur.replace(new RegExp("[" + TALK_MOUTHS.join("") + "]"), "_");
    }
    slowMouth(ms) {
      this.closeMouth(ms);
      this.timers.push(setTimeout(() => {
        if (this.state === "talk") this.apply("talk");
      }, ms));
    }
    // ---- idle director ----------------------------------------------------
    activity() {
      this.lastActivity = Date.now();
      if (this.sleeping) {
        this.sleeping = false;
        this.state = "idle";
        this.apply("wake");
      }
    }
    scheduleIdle() {
      clearTimeout(this.idleT);
      const delay = (1800 + Math.random() * 2700) * this.persona.tempo;
      this.idleT = setTimeout(() => this.idleTick(), delay);
    }
    idleTick() {
      if (this.state !== "idle" || this.reduced || this.flashing) return this.scheduleIdle();
      const stage = idleStage(Date.now() - this.lastActivity, this.persona.sleepAt);
      if (stage === "sleep") {
        if (!this.sleeping) {
          this.sleeping = true;
          this.apply("yawn");
          this.timers.push(setTimeout(() => {
            if (this.sleeping) {
              this.state = "sleep";
              this.apply("sleep");
            }
          }, 1500 * this.persona.tempo));
        }
      } else if (stage === "drowsy") {
        this.flash(Math.random() < 0.5 ? "blinkSlow" : "drowsy", 900);
      } else {
        const pick = weightedPick(this.persona.weights, Math.random());
        if (pick !== "none") this.flash(pick, 1e3);
      }
      this.scheduleIdle();
    }
    // ---- interaction (P1.5) ----------------------------------------------
    attachInteraction() {
      if (typeof window === "undefined") return;
      let raf = 0;
      const onMove = (e) => {
        if (this.reduced) return;
        cancelAnimationFrame(raf);
        raf = requestAnimationFrame(() => {
          const r = this.wrap.getBoundingClientRect();
          const dx = e.clientX - (r.left + r.width / 2), dy = e.clientY - (r.top + r.height / 2);
          const cl = (v, m) => Math.max(-m, Math.min(m, v));
          this.faceEl.style.setProperty("--gx", cl(dx * 0.02, 6) + "px");
          this.faceEl.style.setProperty("--gy", cl(dy * 0.02, 4) + "px");
          this.faceEl.style.setProperty("--gr", cl(dx * 8e-3, 5) + "deg");
        });
      };
      const onEnter = () => {
        if (this.state === "idle" && !this.sleeping) this.flash("happy", 600);
      };
      let downAt = 0;
      const onDown = () => {
        downAt = Date.now();
        this.activity();
      };
      const onUp = () => {
        const held = Date.now() - downAt;
        if (held > 600) this.flash("dizzy", 1200);
        else this.flash(Math.random() < 0.5 ? "shock" : "giggle", 800);
      };
      window.addEventListener("pointermove", onMove);
      this.wrap.addEventListener("pointerenter", onEnter);
      this.wrap.addEventListener("pointerdown", onDown);
      this.wrap.addEventListener("pointerup", onUp);
      this.detach.push(() => {
        window.removeEventListener("pointermove", onMove);
        this.wrap.removeEventListener("pointerenter", onEnter);
        this.wrap.removeEventListener("pointerdown", onDown);
        this.wrap.removeEventListener("pointerup", onUp);
      });
    }
  };
  var glyph = { mount: (el, opts) => new GlyphEngine(el, opts) };

  // src/stream.ts
  async function reveal(g2, text, onWord, msPerWord = 65) {
    const words = text.split(/(\s+)/);
    for (const w of words) {
      if (!w) continue;
      g2.say(w);
      onWord?.(w);
      await new Promise((r) => setTimeout(r, /\s/.test(w) ? 0 : msPerWord));
    }
    g2.done();
  }

  // src/element.ts
  var GlyphFace = class extends HTMLElement {
    static {
      this.observedAttributes = ["state", "emotion", "personality"];
    }
    connectedCallback() {
      if (this.engine) return;
      this.engine = new GlyphEngine(this, {
        personality: this.getAttribute("personality") || void 0,
        interactive: this.getAttribute("interactive") !== "false"
      });
      const s = this.getAttribute("state");
      if (s) this.engine.setState(s);
    }
    disconnectedCallback() {
      this.engine?.destroy();
      this.engine = void 0;
    }
    attributeChangedCallback(name, _old, val) {
      if (!this.engine || val === null) return;
      if (name === "state") this.engine.setState(val === "thinking" ? "think" : val === "talking" ? "talk" : val);
      if (name === "emotion") this.engine.emote(val);
      if (name === "personality") this.engine.setPersonality(val);
    }
    // convenience passthroughs
    think() {
      this.engine?.think();
    }
    say(chunk) {
      this.engine?.say(chunk);
    }
    done() {
      this.engine?.done();
    }
  };
  if (typeof customElements !== "undefined" && !customElements.get("glyph-face")) {
    customElements.define("glyph-face", GlyphFace);
  }

  // demo/main.ts
  var $ = (id) => document.getElementById(id);
  var g = glyph.mount($("stage"));
  var states = Object.keys(EXPRESSIONS);
  for (const s of states) {
    const b = document.createElement("button");
    b.textContent = s;
    b.onclick = () => g.setState(s);
    $("states").appendChild(b);
  }
  for (const p of Object.keys(PERSONALITIES)) {
    const o = document.createElement("option");
    o.value = o.textContent = p;
    $("persona").appendChild(o);
  }
  $("persona").onchange = (e) => g.setPersonality(e.target.value);
  $("fake").onclick = async () => {
    const text = "Hello! I am GLYPH... a tiny living face. WOW this actually works!! Sometimes things go wrong and I show an error \u{1F622} but mostly I am happy \u{1F604} to talk with you.";
    await reveal(g, text, (w) => {
      $("transcript").textContent += w;
    });
  };
  $("chat").onsubmit = async (e) => {
    e.preventDefault();
    const input = $("prompt");
    const prompt = input.value.trim();
    if (!prompt) return;
    input.value = "";
    $("transcript").textContent = `you: ${prompt}

glyph: `;
    g.think();
    try {
      const r = await fetch("/api/chat", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ prompt })
      });
      const d = await r.json();
      if (!r.ok || d.error) throw new Error(d.error || `HTTP ${r.status}`);
      await reveal(g, d.content, (w) => {
        $("transcript").textContent += w;
      });
      $("meta").textContent = d.model ? `${d.model} \xB7 $${(+d.cost || 0).toFixed(5)}` : "";
    } catch (err) {
      g.setState("error");
      $("transcript").textContent += `[${err.message}]`;
      setTimeout(() => g.setState("idle"), 2500);
    }
  };
})();

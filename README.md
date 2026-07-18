# GLYPH

<img src="assets/glyph.svg" alt="GLYPH — a living kaomoji face" width="360">

A living kaomoji face that replaces the "..." typing dots in any AI chat.

`・_・` breathes, blinks, looks around, follows your cursor, and falls asleep if you ignore it. When your AI generates, it talks — `・o・ ・ω・ ・‿・` — for exactly as long as tokens flow, reacting to what it's saying: `WOW!!` → `O_O!`, an error → `>_< ⚡`, an emoji in the stream gets mirrored on its face.

Zero runtime dependencies. Text + CSS only. **Add a face = add a string.**

## Install

```sh
npm install @bestdoom20/glyph
```

## Use — under 5 lines

```js
import { glyph } from '@bestdoom20/glyph';
const g = glyph.mount(document.querySelector('#face'));
g.think();                                      // 💭 waiting for first token
for await (const chunk of stream) g.say(chunk); // mouth runs while tokens flow
g.done();                                       // idles, blinks, eventually sleeps
```

Or as a web component (React/Vue/Svelte/plain HTML):

```html
<glyph-face state="idle" personality="hyper"></glyph-face>
```

React wrapper:

```jsx
import { Glyph } from '@bestdoom20/glyph/react';
<Glyph state="thinking" engineRef={g => (ref.current = g)} />
```

OpenAI-style streams, one call:

```js
import { drive } from '@bestdoom20/glyph';
await drive(g, openaiStream); // think → say per delta → done
```

Non-streaming API? `reveal(g, fullText)` plays it back word-by-word so the face still lives.

## What it does on its own

- **Breathing + random blinks** — always on, the core "alive" cue
- **Idle director** — look-arounds, winks, hums ♪, sighs; 30s idle → drowsy `˘_˘`; 60s → yawns and sleeps `u_u z`; any activity → startle-wake `0o0 !`
- **Cursor tracking** — face leans toward your pointer; hover → `^‿^`; click → `O_O!` or `^w^`; long-press → `@_@`
- **Talk realism** — pauses on `.` `,`; pops on `!` `?`; slows on `...`; mirrors emoji; local text-cue heuristic flashes happy/shock/error/confused mid-sentence
- **Personalities** — `sleepy` `hyper` `shy` `sassy` (data-only presets)
- Respects `prefers-reduced-motion` — static readable face, no animation

## API

| Call | Effect |
|------|--------|
| `glyph.mount(el, {personality, interactive, autoIdleMs})` | create engine |
| `g.think()` / `g.say(chunk)` / `g.done()` | the streaming contract |
| `g.setState(name)` / `g.emote(name)` | force a state / flash an emotion |
| `g.setPersonality(name)` | swap personality live |
| `g.destroy()` | clean up |

~17 expressions built in (`idle think talk happy love shock confused sad error sleep wake wink smug shy dizzy giggle hum` …). Add your own: one entry in the `EXPRESSIONS` registry — eyes template + mouth frames, `%` marks the mouth slot.

Host emotion tag is optional — GLYPH is purely text-reactive by default; `emotion="proud"` overrides the heuristic when you *do* know better.

## Demo

```sh
npm install && npm run demo   # → http://localhost:8199
```

Playground: every state as a button, personality switcher, fake token stream, and live chat (`/api/chat` proxies to your LLM server-side — API key never touches the browser).

## License

MIT © [Bestdoom20](https://github.com/Bestdoom20)

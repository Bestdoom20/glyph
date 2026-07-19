<div align="center">

<img src="assets/glyph.svg" width="400" alt="GLYPH — a living kaomoji face">

# G L Y P H

**A living kaomoji face that replaces the "..." typing dots in any AI chat.**

[![MIT license](https://img.shields.io/badge/license-MIT-blue?style=flat-square)](LICENSE)
[![zero dependencies](https://img.shields.io/badge/dependencies-zero-brightgreen?style=flat-square)](package.json)
[![web component](https://img.shields.io/badge/%3Cglyph--face%3E-web%20component-7ee0ff?style=flat-square)](#use--under-5-lines)
[![react ready](https://img.shields.io/badge/react-ready-61dafb?style=flat-square)](#use--under-5-lines)
[![PRs welcome](https://img.shields.io/badge/PRs-welcome-ff69b4?style=flat-square)](https://github.com/Bestdoom20/glyph/pulls)

**[▶ Live Playground](https://bestdoom20.github.io/glyph/)** · [Install](#install) · [API](#api) · [Demo](#demo)

`・_・` &nbsp;→&nbsp; `-_-` &nbsp;→&nbsp; `・o・` &nbsp;→&nbsp; `・ω・` &nbsp;→&nbsp; `^‿^` &nbsp;→&nbsp; `u_u z`

</div>

---

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

## The cast

<div align="center">

| `・_・` | `˘_˘ 💭` | `・ω・` | `^‿^ ♪` | `♡_♡ ♥` | `O_O !` |
|:---:|:---:|:---:|:---:|:---:|:---:|
| idle | think | talk | happy | love | shock |

| `o_O ?` | `;_;` | `>_< ⚡` | `u_u z` | `¬‿¬` | `@_@` |
|:---:|:---:|:---:|:---:|:---:|:---:|
| confused | sad | error | sleep | smug | dizzy |

</div>

~17 expressions built in. Add your own: one entry in the `EXPRESSIONS` registry — eyes template + mouth frames, `%` marks the mouth slot.

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

Host emotion tag is optional — GLYPH is purely text-reactive by default; `emotion="proud"` overrides the heuristic when you *do* know better.

## Demo

```sh
npm install && npm run demo   # → http://localhost:8199
```

Playground: every state as a button, personality switcher, fake token stream, and live chat (`/api/chat` proxies to your LLM server-side — API key never touches the browser).

---

<div align="center">

MIT © [Bestdoom20](https://github.com/Bestdoom20)

`(=^･ω･^=)` *thanks for stopping by*

</div>

# GLYPH

A living kaomoji face that replaces the "..." typing dots in any AI chat.

`・_・` blinks, looks around, falls asleep — and when your AI generates, it talks: `・o・ ・ω・ ・‿・` — reacting to the content as it streams.

**Status: scaffold (P0).** Engine, expressions, idle director, web component, React wrapper, stream helper landing in phases.

## Quick start (target API)

```html
<glyph-face state="idle"></glyph-face>
```

```js
import { glyph } from '@bestdoom20/glyph';
const g = glyph.mount(document.querySelector('#face'));
g.think();                                    // 💭 waiting for first token
for await (const chunk of stream) g.say(chunk); // mouth runs while tokens flow
g.done();                                     // back to idle, blinks, eventually sleeps
```

## Defaults chosen (noted per brief)

- npm name: `@bestdoom20/glyph` (scoped, collision-safe)
- Context depth: purely text-reactive; optional `emotion="..."` host tag override — hosts never *have* to emit tags

MIT © Bestdoom20

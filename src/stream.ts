// Drive a GlyphEngine from any async iterable: plain strings or OpenAI-style chunks.
import type { GlyphEngine } from './engine';

type Chunk = string | { choices?: { delta?: { content?: string | null } }[] };

export async function drive(g: Pick<GlyphEngine, 'think' | 'say' | 'done'>, stream: AsyncIterable<Chunk>) {
  g.think();
  try {
    for await (const c of stream) {
      const text = typeof c === 'string' ? c : c?.choices?.[0]?.delta?.content ?? '';
      if (text) g.say(text);
    }
  } finally {
    g.done();
  }
}

// Reveal a complete (non-streaming) response progressively, word by word.
export async function reveal(
  g: Pick<GlyphEngine, 'think' | 'say' | 'done'>,
  text: string,
  onWord?: (word: string) => void,
  msPerWord = 65,
) {
  const words = text.split(/(\s+)/);
  for (const w of words) {
    if (!w) continue;
    g.say(w);
    onWord?.(w);
    await new Promise(r => setTimeout(r, /\s/.test(w) ? 0 : msPerWord));
  }
  g.done();
}

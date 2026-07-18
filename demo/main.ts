// GLYPH playground — every state + fake stream + live chat via /api/chat.
import { glyph, EXPRESSIONS, PERSONALITIES, reveal } from '../src/index';
import '../src/element';

const $ = (id: string) => document.getElementById(id)!;
const g = glyph.mount($('stage'));

// state buttons
const states = Object.keys(EXPRESSIONS);
for (const s of states) {
  const b = document.createElement('button');
  b.textContent = s;
  b.onclick = () => g.setState(s);
  $('states').appendChild(b);
}

// personality select
for (const p of Object.keys(PERSONALITIES)) {
  const o = document.createElement('option');
  o.value = o.textContent = p;
  $('persona').appendChild(o);
}
($('persona') as HTMLSelectElement).onchange = e => g.setPersonality((e.target as HTMLSelectElement).value);

// fake token stream — P1 acceptance: talks until done, then idles
$('fake').onclick = async () => {
  const text = 'Hello! I am GLYPH... a tiny living face. WOW this actually works!! ' +
    'Sometimes things go wrong and I show an error 😢 but mostly I am happy 😄 to talk with you.';
  await reveal(g, text, w => { $('transcript').textContent += w; });
};

// live chat via server proxy (P3)
($('chat') as HTMLFormElement).onsubmit = async e => {
  e.preventDefault();
  const input = $('prompt') as HTMLInputElement;
  const prompt = input.value.trim();
  if (!prompt) return;
  input.value = '';
  $('transcript').textContent = `you: ${prompt}\n\nglyph: `;
  g.think();
  try {
    const r = await fetch('/api/chat', {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({ prompt }),
    });
    const d = await r.json();
    if (!r.ok || d.error) throw new Error(d.error || `HTTP ${r.status}`);
    await reveal(g, d.content, w => { $('transcript').textContent += w; });
    $('meta').textContent = d.model ? `${d.model} · $${(+d.cost || 0).toFixed(5)}` : '';
  } catch (err) {
    g.setState('error');
    $('transcript').textContent += `[${(err as Error).message}]`;
    setTimeout(() => g.setState('idle'), 2500);
  }
};

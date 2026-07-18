// <glyph-face> web component — wraps GlyphEngine. Works in plain HTML, React, Vue, Svelte.
import { GlyphEngine } from './engine';

export class GlyphFace extends HTMLElement {
  engine?: GlyphEngine;
  static observedAttributes = ['state', 'emotion', 'personality'];

  connectedCallback() {
    if (this.engine) return;
    this.engine = new GlyphEngine(this, {
      personality: this.getAttribute('personality') || undefined,
      interactive: this.getAttribute('interactive') !== 'false',
    });
    const s = this.getAttribute('state');
    if (s) this.engine.setState(s);
  }
  disconnectedCallback() { this.engine?.destroy(); this.engine = undefined; }
  attributeChangedCallback(name: string, _old: string | null, val: string | null) {
    if (!this.engine || val === null) return;
    if (name === 'state') this.engine.setState(val === 'thinking' ? 'think' : val === 'talking' ? 'talk' : val);
    if (name === 'emotion') this.engine.emote(val);
    if (name === 'personality') this.engine.setPersonality(val);
  }
  // convenience passthroughs
  think() { this.engine?.think(); }
  say(chunk: string) { this.engine?.say(chunk); }
  done() { this.engine?.done(); }
}

if (typeof customElements !== 'undefined' && !customElements.get('glyph-face')) {
  customElements.define('glyph-face', GlyphFace);
}

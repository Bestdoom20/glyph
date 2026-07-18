// Thin React wrapper — 21st.dev-style DX. Core stays vanilla.
import { useEffect, useRef } from 'react';
import { GlyphEngine, type GlyphOptions } from './engine';

export type GlyphProps = {
  state?: string;
  emotion?: string;
  personality?: string;
  interactive?: boolean;
  engineRef?: (g: GlyphEngine) => void;
  className?: string;
  style?: React.CSSProperties;
};

export function Glyph({ state, emotion, personality, interactive, engineRef, className, style }: GlyphProps) {
  const host = useRef<HTMLDivElement>(null);
  const eng = useRef<GlyphEngine>();

  useEffect(() => {
    const opts: GlyphOptions = { personality, interactive };
    eng.current = new GlyphEngine(host.current!, opts);
    engineRef?.(eng.current);
    return () => eng.current?.destroy();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => { if (state) eng.current?.setState(state === 'thinking' ? 'think' : state === 'talking' ? 'talk' : state); }, [state]);
  useEffect(() => { if (emotion) eng.current?.emote(emotion); }, [emotion]);
  useEffect(() => { if (personality) eng.current?.setPersonality(personality); }, [personality]);

  return <div ref={host} className={className} style={style} />;
}
